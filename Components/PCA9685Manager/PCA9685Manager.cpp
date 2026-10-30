// ======================================================================
// \title  PCA9685Manager.cpp
// \author luca_lanzi
// \brief  cpp file for PCA9685Manager component implementation class
// ======================================================================

#include "Components/PCA9685Manager/PCA9685Manager.hpp"

namespace billeeScm {

static_assert(billeeScm::ServoStates::SIZE == PCA9685Manager::NUM_SERVO_CHANNELS,
              "ServoStates (Types/PCA9685Types.fpp) and NUM_SERVO_CHANNELS must have the same size");
static_assert(billeeScm::ServoId::NUM_CONSTANTS == PCA9685Manager::NUM_SERVO_CHANNELS,
              "ServoId (Types/PCA9685Types.fpp) must name every servo channel");

namespace {

//! Per-channel pulse limits, in PCA9685 counts (4096 counts per 20 ms frame at 50 Hz).
//! minPwm/maxPwm are the servo's safe mechanical end points: no command can ever send a pulse outside them
//! (a positional servo driven past its range can break its limiter pin). midPwm is the neutral pulse: the
//! calibrated center of a positional servo, the stop pulse of a continuous one. PWM magnitude scales the
//! distance from mid toward max (FORWARD) or min (REVERSE). reversed swaps the two, so FORWARD can mean the
//! same physical direction however the servo is mounted. Defaults are the nominal 2.5%/7.5%/12.5% of the frame.
//! Bench-tune with setChannelPWM, then copy the values into this table.
struct ServoCal {
    U16 minPwm;     //!< Pulse at full REVERSE (end stop / full reverse speed)
    U16 midPwm;     //!< Neutral pulse: centred / stopped
    U16 maxPwm;     //!< Pulse at full FORWARD (end stop / full forward speed)
    bool reversed;  //!< Swap FORWARD and REVERSE for this servo
};

constexpr ServoCal kDefaultCal = {102, 307, 512, false};

constexpr ServoCal kServoCal[PCA9685Manager::NUM_SERVO_CHANNELS] = {
    kDefaultCal,  // SERVO0
    kDefaultCal,  // SERVO1
    kDefaultCal,  // SERVO2
    kDefaultCal,  // SERVO3
    kDefaultCal,  // SERVO4
    kDefaultCal,  // SERVO5
    kDefaultCal,  // SERVO6
    kDefaultCal,  // SERVO7
    kDefaultCal,  // SERVO8
    kDefaultCal,  // SERVO9
    kDefaultCal,  // SERVO10
    kDefaultCal,  // SERVO11
};

//! Channel value reported in I2cFault when the failure is not tied to one channel (init / release all).
constexpr U8 ALL_CHANNELS = 255;

}  // namespace

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PCA9685Manager ::PCA9685Manager(const char* const compName) : PCA9685ManagerComponentBase(compName) {}

PCA9685Manager ::~PCA9685Manager() {}

void PCA9685Manager ::configure(TwoWire* wire, U8 a5a0, uint32_t i2cSpeed) {
    FW_ASSERT(wire != nullptr);
    wire->begin();
    // Placement new: constructs into m_pcaStorage (declared in the header), not the heap -- see the comment there.
    this->m_pca = new (this->m_pcaStorage) PCA9685(a5a0, *wire, i2cSpeed);
    // Events are not logged here: configure() runs before the rate group starts and eventLogger has not drained
    // its queue yet. A failure is still visible in i2cErrorCount/lastI2cError telemetry, and reinit() retries.
    this->m_initOk = this->initDevice(false);
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PCA9685Manager ::run_handler(FwIndexType portNum, U32 context) {
    // Publish every cycle (not only on change): gives the channels a value from the first tick and keeps them
    // visible to a GDS that connects mid-run. This handler does no I2C.
    this->tlmWrite_servoStates(this->m_states);
    this->tlmWrite_i2cErrorCount(this->m_i2cErrorCount);
    this->tlmWrite_lastI2cError(this->m_lastI2cError);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PCA9685Manager ::setServo_cmdHandler(FwOpcodeType opCode,
                                          U32 cmdSeq,
                                          billeeScm::ServoId servo,
                                          U16 pwm,
                                          billeeScm::motorDir direction) {
    // Anything invalid (bad servo or direction, magnitude past full scale) never reaches the bus
    if (!servo.isValid() || !direction.isValid() || pwm > PWM_FULL_SCALE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (!this->m_initOk) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    const U8 channel = static_cast<U8>(servo.e);
    const U16 pulse = this->pulseFor(channel, pwm, direction);
    const bool ok = this->writeChannel(channel, pulse);
    if (ok) {
        // A STOPPED command's magnitude is ignored, so record 0 rather than a number that was never applied
        this->setState(channel, direction, (direction == billeeScm::motorDir::STOPPED) ? 0 : pwm, pulse);
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::setChannelPWM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::ServoId servo, U16 pwm) {
    // 0 releases the channel; anything else must be inside the channel's calibrated pulse limits, so a
    // bring-up command can never drive a limited-range servo past its end stops (4096 "full on" is never valid).
    if (!servo.isValid()) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    const U8 channel = static_cast<U8>(servo.e);
    if (pwm != 0 && (pwm < kServoCal[channel].minPwm || pwm > kServoCal[channel].maxPwm)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (!this->m_initOk) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    const bool ok = this->writeChannel(channel, pwm);
    if (ok) {
        // A raw pulse has no direction/magnitude; the pulse itself is still reported
        this->setState(channel, billeeScm::motorDir::STOPPED, 0, pwm);
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::releaseChannel_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::ServoId servo) {
    if (!servo.isValid()) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    // Deliberately NOT gated on m_initOk: a stop command must always try the bus. (Only refused if configure() never ran.)
    if (this->m_pca == nullptr) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    const U8 channel = static_cast<U8>(servo.e);
    const bool ok = this->writeChannel(channel, 0);
    if (ok) {
        this->setState(channel, billeeScm::motorDir::STOPPED, 0, 0);
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::hardStop_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Deliberately NOT gated on m_initOk: this is the software stop (OE is not wired), so it must always try the bus.
    // (Only refused if configure() never ran.) One transaction releases every channel at once.
    if (this->m_pca == nullptr) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->m_pca->setAllChannelsPWM(0);
    const bool ok = this->checkI2c(ALL_CHANNELS, true);
    if (ok) {
        for (U8 channel = 0; channel < NUM_SERVO_CHANNELS; channel++) {
            this->setState(channel, billeeScm::motorDir::STOPPED, 0, 0);
        }
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::reinit_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (this->m_pca == nullptr) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->m_initOk = this->initDevice(true);
    this->respond(opCode, cmdSeq, this->m_initOk);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

U16 PCA9685Manager ::pulseFor(U8 channel, U16 pwm, billeeScm::motorDir direction) const {
    const ServoCal& cal = kServoCal[channel];
    if (direction == billeeScm::motorDir::STOPPED || pwm == 0) {
        return cal.midPwm;
    }
    const bool forward = (direction == billeeScm::motorDir::FORWARD) != cal.reversed;
    // Each side of mid has its own span, so a servo whose neutral is not exactly halfway between its end points
    // still maps 0 to neutral and full scale to each end point. Integer math, rounded to the nearest count.
    const U32 span = forward ? static_cast<U32>(cal.maxPwm - cal.midPwm) : static_cast<U32>(cal.midPwm - cal.minPwm);
    const U32 offset = (static_cast<U32>(pwm) * span + PWM_FULL_SCALE / 2) / PWM_FULL_SCALE;
    U32 pulse = forward ? cal.midPwm + offset : cal.midPwm - offset;
    // Defensive clamp: the caller validates pwm, but a pulse outside [min,max] must be impossible.
    if (pulse < cal.minPwm) {
        pulse = cal.minPwm;
    }
    if (pulse > cal.maxPwm) {
        pulse = cal.maxPwm;
    }
    return static_cast<U16>(pulse);
}

void PCA9685Manager ::setState(U8 channel, billeeScm::motorDir direction, U16 pwm, U16 pulse) {
    this->m_states[channel] = billeeScm::ServoState(direction, pwm, pulse);
}

bool PCA9685Manager ::writeChannel(U8 channel, U16 pwm) {
    if (pwm == 0) {
        this->m_pca->setChannelOff(channel);
    } else {
        this->m_pca->setChannelPWM(channel, pwm);
    }
    return this->checkI2c(channel, true);
}

bool PCA9685Manager ::checkI2c(U8 channel, bool logFault) {
    const U8 error = this->m_pca->getLastI2CError();
    if (error != 0) {
        this->m_i2cErrorCount++;
        // Only log when the fault is new (first failure, or a different code): a dead bus would otherwise emit one
        // event per command and overflow eventLogger's depth-3 queue, which is an FW_ASSERT.
        if (logFault && error != this->m_lastI2cError) {
            this->log_WARNING_HI_I2cFault(channel, error);
        }
    }
    this->m_lastI2cError = error;
    return error == 0;
}

bool PCA9685Manager ::initDevice(bool logFaults) {
    // Each library call restarts its own error state, so check after every one. init() with no arguments is
    // totem-pole outputs, normal polarity, low when disabled, update after STOP, no phase balancer.
    this->m_pca->init();
    bool ok = this->checkI2c(ALL_CHANNELS, logFaults);
    this->m_pca->setPWMFreqServo();  // 50 Hz = standard 20 ms servo frame
    ok = this->checkI2c(ALL_CHANNELS, logFaults) && ok;
    // The PCA9685 keeps running across a Teensy reset (no reset line), so servos would hold their last pulse
    // while our telemetry says 0. Release everything so firmware and hardware start from the same known state.
    this->m_pca->setAllChannelsPWM(0);
    ok = this->checkI2c(ALL_CHANNELS, logFaults) && ok;
    if (ok) {
        for (U8 channel = 0; channel < NUM_SERVO_CHANNELS; channel++) {
            this->setState(channel, billeeScm::motorDir::STOPPED, 0, 0);
        }
    }
    return ok;
}

void PCA9685Manager ::respond(FwOpcodeType opCode, U32 cmdSeq, bool ok) {
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

}  // namespace billeeScm
