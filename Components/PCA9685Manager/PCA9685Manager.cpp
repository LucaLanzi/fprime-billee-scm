// ======================================================================
// \title  PCA9685Manager.cpp
// \author luca_lanzi
// \brief  cpp file for PCA9685Manager component implementation class
// ======================================================================

#include "Components/PCA9685Manager/PCA9685Manager.hpp"

namespace billeeScm {

static_assert(billeeScm::ServoAngles::SIZE == PCA9685Manager::NUM_SERVO_CHANNELS,
              "ServoAngles (Types/PCA9685Types.fpp) and NUM_SERVO_CHANNELS must have the same size");

namespace {

using ServoType = PCA9685Manager::ServoType;

//! Per-channel servo type and pulse limits, in PCA9685 counts (4096 counts per 20 ms frame at 50 Hz).
//! minPwm/maxPwm are the servo's safe mechanical end points: no command can ever send a pulse outside them
//! (a positional servo driven past its range can break its limiter pin). midPwm is the calibrated center
//! (POSITIONAL) or the stop pulse (CONTINUOUS). Defaults are the nominal 2.5%/7.5%/12.5% of the frame.
//! Bench-tune with setChannelPWM, then copy the values into this table.
struct ServoCal {
    ServoType type;
    U16 minPwm;  //!< Pulse at -90 degrees / full reverse
    U16 midPwm;  //!< Pulse at 0 degrees / stopped
    U16 maxPwm;  //!< Pulse at +90 degrees / full forward
};

constexpr ServoCal kDefaultCal = {ServoType::POSITIONAL, 102, 307, 512};

constexpr ServoCal kServoCal[PCA9685Manager::NUM_SERVO_CHANNELS] = {
    kDefaultCal,  // channel 0
    kDefaultCal,  // channel 1
    kDefaultCal,  // channel 2
    kDefaultCal,  // channel 3
    kDefaultCal,  // channel 4
    kDefaultCal,  // channel 5
    kDefaultCal,  // channel 6
    kDefaultCal,  // channel 7
    kDefaultCal,  // channel 8
    kDefaultCal,  // channel 9
    kDefaultCal,  // channel 10
    kDefaultCal,  // channel 11
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
    billeeScm::ServoAngles angles;
    for (U8 channel = 0; channel < NUM_SERVO_CHANNELS; channel++) {
        angles[channel] = this->m_values[channel];
    }
    this->tlmWrite_servoAngles(angles);
    this->tlmWrite_i2cErrorCount(this->m_i2cErrorCount);
    this->tlmWrite_lastI2cError(this->m_lastI2cError);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PCA9685Manager ::setServoAngle_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 channel, F32 angle) {
    // Anything invalid (bad channel, wrong servo type, out-of-range or NaN angle) never reaches the bus.
    // Written as !(in range) so NaN is rejected too.
    if (channel >= NUM_SERVO_CHANNELS || kServoCal[channel].type != ServoType::POSITIONAL ||
        !(angle >= -90.0f && angle <= 90.0f)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (!this->m_initOk) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    const bool ok = this->writeChannel(channel, this->pwmForPosition(channel, angle / 90.0f));
    if (ok) {
        this->m_values[channel] = angle;
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::setServoSpeed_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 channel, F32 speed) {
    if (channel >= NUM_SERVO_CHANNELS || kServoCal[channel].type != ServoType::CONTINUOUS ||
        !(speed >= -1.0f && speed <= 1.0f)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (!this->m_initOk) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    const bool ok = this->writeChannel(channel, this->pwmForPosition(channel, speed));
    if (ok) {
        this->m_values[channel] = speed;
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::setChannelPWM_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 channel, U16 pwm) {
    // 0 releases the channel; anything else must be inside the channel's calibrated pulse limits, so a
    // bring-up command can never drive a limited-range servo past its end stops (4096 "full on" is never valid).
    if (channel >= NUM_SERVO_CHANNELS ||
        (pwm != 0 && (pwm < kServoCal[channel].minPwm || pwm > kServoCal[channel].maxPwm))) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (!this->m_initOk) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    const bool ok = this->writeChannel(channel, pwm);
    if (ok) {
        // A raw pulse has no meaningful angle/speed, so show 0 (unknown) rather than a wrong number.
        this->m_values[channel] = 0.0f;
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::releaseChannel_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 channel) {
    if (channel >= NUM_SERVO_CHANNELS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    // Deliberately NOT gated on m_initOk: a stop command must always try the bus. (Only refused if configure() never ran.)
    if (this->m_pca == nullptr) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    const bool ok = this->writeChannel(channel, 0);
    if (ok) {
        this->m_values[channel] = 0.0f;
    }
    this->respond(opCode, cmdSeq, ok);
}

void PCA9685Manager ::releaseAll_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Deliberately NOT gated on m_initOk: this is the software stop (OE is not wired), so it must always try the bus.
    // (Only refused if configure() never ran.)
    if (this->m_pca == nullptr) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->m_pca->setAllChannelsPWM(0);
    const bool ok = this->checkI2c(ALL_CHANNELS, true);
    if (ok) {
        for (U8 channel = 0; channel < NUM_SERVO_CHANNELS; channel++) {
            this->m_values[channel] = 0.0f;
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

U16 PCA9685Manager ::pwmForPosition(U8 channel, F32 position) const {
    const ServoCal& cal = kServoCal[channel];
    // Piecewise linear through (-1, min), (0, mid), (+1, max). Each half has its own slope, so a servo whose
    // center is not exactly halfway between its end points is still centred at 0.
    const F32 span = (position < 0.0f) ? static_cast<F32>(cal.midPwm - cal.minPwm)
                                       : static_cast<F32>(cal.maxPwm - cal.midPwm);
    F32 pwm = static_cast<F32>(cal.midPwm) + position * span + 0.5f;
    // Defensive clamp: the callers validate position, but a pulse outside [min,max] must be impossible.
    if (pwm < static_cast<F32>(cal.minPwm)) {
        pwm = static_cast<F32>(cal.minPwm);
    }
    if (pwm > static_cast<F32>(cal.maxPwm)) {
        pwm = static_cast<F32>(cal.maxPwm);
    }
    return static_cast<U16>(pwm);
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
            this->m_values[channel] = 0.0f;
        }
    }
    return ok;
}

void PCA9685Manager ::respond(FwOpcodeType opCode, U32 cmdSeq, bool ok) {
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

}  // namespace billeeScm
