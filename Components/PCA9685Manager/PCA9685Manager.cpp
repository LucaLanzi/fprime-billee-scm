// ======================================================================
// \title  PCA9685Manager.cpp
// \author luca_lanzi
// \brief  PCA9685 16-channel servo board over I2C
// ======================================================================

#include "Components/PCA9685Manager/PCA9685Manager.hpp"

namespace billeeScm {

static_assert(billeeScm::ServoId::NUM_CONSTANTS == PCA9685Manager::NUM_SERVOS,
              "ServoId (Types/PCA9685Types.fpp) must name every servo channel");

namespace {

//! Per-channel pulse limits, in PCA9685 counts (4096 counts per 20 ms frame at 50 Hz).
//! minCounts/maxCounts are the servo's safe end points: no command can send a pulse outside them (a positional servo
//! driven past its range can break its end stop). midCounts is the neutral pulse: the calibrated center of a
//! positional servo, the stop pulse of a continuous one. Speed scales the distance from mid toward max (FORWARD) or
//! min (REVERSE); `reversed` swaps the two so FORWARD can mean the same physical direction however the servo is
//! mounted. Defaults are the nominal 0.5/1.5/2.5 ms pulses. Bench-tune with SERVO_PULSE, then copy the values here.
struct ServoCal {
    U16 minCounts;  //!< Pulse at full REVERSE (end stop / full reverse speed)
    U16 midCounts;  //!< Neutral pulse: centred / stopped
    U16 maxCounts;  //!< Pulse at full FORWARD (end stop / full forward speed)
    bool reversed;  //!< Swap FORWARD and REVERSE for this servo
};

constexpr ServoCal kDefaultCal = {102, 307, 512, false};

constexpr ServoCal kServoCal[PCA9685Manager::NUM_SERVOS] = {
    kDefaultCal,  // S1  soil cup tilting servo
    kDefaultCal,  // S2  drape lowering servo
    kDefaultCal,  // S3  cache lid servo
    kDefaultCal,  // S4  funnel flap servo
    kDefaultCal,  // S5
    kDefaultCal,  // S6
    kDefaultCal,  // S7
    kDefaultCal,  // S8
    kDefaultCal,  // S9
    kDefaultCal,  // S10
    kDefaultCal,  // S11
    kDefaultCal,  // S12
    kDefaultCal,  // S13
    kDefaultCal,  // S14
    kDefaultCal,  // S15
    kDefaultCal,  // S16
};

//! Channel reported in I2cFault when the failure is not tied to one channel (init / release all)
constexpr U8 ALL_CHANNELS = 255;

}  // namespace

PCA9685Manager ::PCA9685Manager(const char* const compName) : PCA9685ManagerComponentBase(compName) {
    for (U8 channel = 0; channel < NUM_SERVOS; channel++) {
        this->setState(channel, billeeScm::MotorDir::STOPPED, 0, 0);
    }
}

PCA9685Manager ::~PCA9685Manager() {}

void PCA9685Manager ::configure(TwoWire* wire, U8 a5a0, uint32_t i2cSpeed) {
    FW_ASSERT(wire != nullptr);
    wire->begin();
    this->m_pca = new (this->m_pcaStorage) PCA9685(a5a0, *wire, i2cSpeed);
    // A failure shows in i2cErrorCount/lastI2cError telemetry (and I2cFault); REINIT retries.
    this->m_initOk = this->initDevice();
}

void PCA9685Manager ::run_handler(FwIndexType portNum, U32 context) {
    // Every tick, so a GDS that connects later still sees the state. No I2C here.
    this->tlmWrite_servoStates(this->m_states);
    this->tlmWrite_i2cErrorCount(this->m_i2cErrorCount);
    this->tlmWrite_lastI2cError(this->m_lastI2cError);
}

void PCA9685Manager ::SERVO_SET_cmdHandler(FwOpcodeType opCode,
                                           U32 cmdSeq,
                                           billeeScm::ServoId servo,
                                           billeeScm::MotorDir direction,
                                           U8 speed) {
    if (!servo.isValid() || !direction.isValid() || speed > SPEED_FULL_SCALE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (!this->m_initOk) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    const U8 channel = static_cast<U8>(servo.e);
    const U16 pulse = this->pulseFor(channel, direction, speed);
    const bool ok = this->writeChannel(channel, pulse);
    if (ok) {
        this->setState(channel, direction, (direction == billeeScm::MotorDir::STOPPED) ? 0 : speed, pulse);
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::SERVO_OFF_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::ServoId servo) {
    if (!servo.isValid()) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    // Not gated on m_initOk: a stop must always try the bus
    if (this->m_pca == nullptr) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    const U8 channel = static_cast<U8>(servo.e);
    const bool ok = this->writeChannel(channel, 0);
    if (ok) {
        this->setState(channel, billeeScm::MotorDir::STOPPED, 0, 0);
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::SERVO_PULSE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::ServoId servo, U16 counts) {
    if (!servo.isValid()) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    const U8 channel = static_cast<U8>(servo.e);
    if (counts != 0 && (counts < kServoCal[channel].minCounts || counts > kServoCal[channel].maxCounts)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (!this->m_initOk) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    const bool ok = this->writeChannel(channel, counts);
    if (ok) {
        this->setState(channel, billeeScm::MotorDir::STOPPED, 0, counts);
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::STOP_ALL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Not gated on m_initOk: this is the software stop (OE is not wired), so it must always try the bus
    if (this->m_pca == nullptr) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->m_pca->setAllChannelsPWM(0);
    const bool ok = this->checkI2c(ALL_CHANNELS);
    if (ok) {
        for (U8 channel = 0; channel < NUM_SERVOS; channel++) {
            this->setState(channel, billeeScm::MotorDir::STOPPED, 0, 0);
        }
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::REINIT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (this->m_pca == nullptr) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->m_initOk = this->initDevice();
    this->respond(opCode, cmdSeq, this->m_initOk);
}

U16 PCA9685Manager ::pulseFor(U8 channel, billeeScm::MotorDir direction, U8 speed) const {
    const ServoCal& cal = kServoCal[channel];
    if (direction == billeeScm::MotorDir::STOPPED || speed == 0) {
        return cal.midCounts;
    }
    const bool forward = (direction == billeeScm::MotorDir::FORWARD) != cal.reversed;
    // Each side of neutral has its own span, so a servo whose neutral is not exactly halfway between its end points
    // still maps 0 to neutral and 100 % to each end point. Integer math, rounded to the nearest count.
    const U32 span = forward ? static_cast<U32>(cal.maxCounts - cal.midCounts)
                             : static_cast<U32>(cal.midCounts - cal.minCounts);
    const U32 offset = (static_cast<U32>(speed) * span + SPEED_FULL_SCALE / 2) / SPEED_FULL_SCALE;
    U32 pulse = forward ? cal.midCounts + offset : cal.midCounts - offset;
    // Defensive clamp: speed is validated, but a pulse outside [min,max] must be impossible
    if (pulse < cal.minCounts) {
        pulse = cal.minCounts;
    }
    if (pulse > cal.maxCounts) {
        pulse = cal.maxCounts;
    }
    return static_cast<U16>(pulse);
}

void PCA9685Manager ::setState(U8 channel, billeeScm::MotorDir direction, U8 speed, U16 pulse) {
    this->m_states[channel] = billeeScm::ServoState(direction, speed, pulse);
}

bool PCA9685Manager ::writeChannel(U8 channel, U16 counts) {
    if (counts == 0) {
        this->m_pca->setChannelOff(channel);
    } else {
        this->m_pca->setChannelPWM(channel, counts);
    }
    return this->checkI2c(channel);
}

bool PCA9685Manager ::checkI2c(U8 channel) {
    const U8 error = this->m_pca->getLastI2CError();
    if (error != 0) {
        this->m_i2cErrorCount++;
        if (error != this->m_lastI2cError) {
            this->log_WARNING_HI_I2cFault(channel, error);
        }
    }
    this->m_lastI2cError = error;
    return error == 0;
}

bool PCA9685Manager ::initDevice() {
    // Each library call restarts its own error state, so check after every one. init() with no arguments is
    // totem-pole outputs, normal polarity, low when disabled, update after STOP, no phase balancer.
    this->m_pca->init();
    bool ok = this->checkI2c(ALL_CHANNELS);
    this->m_pca->setPWMFreqServo();  // 50 Hz = standard 20 ms servo frame
    ok = this->checkI2c(ALL_CHANNELS) && ok;
    // The PCA9685 keeps running across a Teensy reset (no reset line), so servos would hold their last pulse while
    // telemetry says released. Release everything so firmware and hardware start from the same state.
    this->m_pca->setAllChannelsPWM(0);
    ok = this->checkI2c(ALL_CHANNELS) && ok;
    if (ok) {
        for (U8 channel = 0; channel < NUM_SERVOS; channel++) {
            this->setState(channel, billeeScm::MotorDir::STOPPED, 0, 0);
        }
    }
    return ok;
}

void PCA9685Manager ::respond(FwOpcodeType opCode, U32 cmdSeq, bool ok) {
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

}  // namespace billeeScm
