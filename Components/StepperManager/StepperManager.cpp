// ======================================================================
// \title  StepperManager.cpp
// \brief  Sample Cache carousel stepper (step / direction / motor-free driver)
// ======================================================================
#include "Components/StepperManager/StepperManager.hpp"

#include <Arduino.h>
#include <IntervalTimer.h>
#include <cmath>

namespace billeeScm {

namespace {

// State shared with the step timer interrupt. One stepper per firmware (configure() asserts that).
IntervalTimer s_timer;
U8 s_stepPin = 0;
volatile bool s_running = false;
volatile I32 s_position = 0;
volatile I32 s_stepDelta = 1;   //!< +1 FORWARD, -1 REVERSE
volatile U32 s_stepsDone = 0;
volatile U32 s_stepsTotal = 0;
volatile F32 s_speed = 0;       //!< Current step rate, steps/s
F32 s_cruise = 0;               //!< Cruise step rate of the current move
F32 s_start = 0;                //!< Ramp start/end step rate of the current move
F32 s_accel = 4000;             //!< steps/s^2
bool s_pulseLow = false;        //!< PU- is being held on (low) for the first half of a step

//! Steps needed to slow from `speed` to the ramp-end speed
inline F32 stepsToStop(F32 speed) {
    return (speed * speed - s_start * s_start) / (2.0f * s_accel);
}

//! Runs at twice the step rate: the first call of each step pulls PU- low (input on), the second releases it, counts
//! the step and works out the next step's rate (trapezoid: accelerate, cruise, decelerate to land on the target).
void stepIsr() {
    if (!s_pulseLow) {
        digitalWriteFast(s_stepPin, LOW);
        s_pulseLow = true;
        return;
    }
    digitalWriteFast(s_stepPin, HIGH);  // open-drain: high-impedance = input off
    s_pulseLow = false;
    s_position = s_position + s_stepDelta;
    const U32 done = s_stepsDone + 1;
    s_stepsDone = done;
    if (done >= s_stepsTotal) {
        s_timer.end();
        s_speed = 0;
        s_running = false;
        return;
    }
    const F32 remaining = static_cast<F32>(s_stepsTotal - done);
    F32 speed = s_speed;
    if (remaining <= stepsToStop(speed)) {
        speed = sqrtf(fmaxf(speed * speed - 2.0f * s_accel, s_start * s_start));
    } else if (speed < s_cruise) {
        speed = fminf(sqrtf(speed * speed + 2.0f * s_accel), s_cruise);
    }
    s_speed = speed;
    s_timer.update(500000.0f / speed);  // half a step period, microseconds
}

}  // namespace

StepperManager ::StepperManager(const char* const compName) : StepperManagerComponentBase(compName) {}

StepperManager ::~StepperManager() {}

void StepperManager ::configure(U8 stepPin, U8 dirPin, U8 freePin, F32 accelStepsPerS2, bool invertDirection) {
    FW_ASSERT(!this->m_configured);
    FW_ASSERT(accelStepsPerS2 > 0.0f);
    s_stepPin = stepPin;
    s_accel = accelStepsPerS2;
    this->m_stepPin = stepPin;
    this->m_dirPin = dirPin;
    this->m_freePin = freePin;
    this->m_invertDirection = invertDirection;
    // Open-drain: LOW sinks current through the driver's optocoupler (input on); HIGH leaves the pin high-impedance
    // (input off). Driving HIGH to 3.3 V instead would leave 1.7 V across an input wired to a +5 V XCOM, which can
    // keep the optocoupler partly on.
    for (const U8 pin : {stepPin, dirPin, freePin}) {
        digitalWriteFast(pin, HIGH);
        pinMode(pin, OUTPUT_OPENDRAIN);
        digitalWriteFast(pin, HIGH);
    }
    this->m_configured = true;
    this->setHold(true);  // hold position from boot
}

// ----------------------------------------------------------------------
// Tick
// ----------------------------------------------------------------------

void StepperManager ::run_handler(FwIndexType portNum, U32 context) {
    if (this->m_pending && !s_running) {
        const I32 position = s_position;
        this->log_ACTIVITY_HI_MoveDone(position);
        this->finishMove(position == this->m_target ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
    }
    this->publishTelemetry();
}

// ----------------------------------------------------------------------
// Commands
// ----------------------------------------------------------------------

void StepperManager ::MOVE_STEPS_cmdHandler(FwOpcodeType opCode,
                                            U32 cmdSeq,
                                            billeeScm::MotorDir direction,
                                            U32 steps,
                                            U32 speedHz) {
    if (!direction.isValid() || direction == billeeScm::MotorDir::STOPPED || steps == 0 || steps > MAX_STEPS ||
        speedHz == 0 || speedHz > MAX_SPEED_HZ) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    this->startMove(opCode, cmdSeq, direction == billeeScm::MotorDir::FORWARD, steps, speedHz);
}

void StepperManager ::MOVE_TO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, I32 position, U32 speedHz) {
    if (speedHz == 0 || speedHz > MAX_SPEED_HZ) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    // Stop first so the distance is measured from where the motor really is
    if (this->haltTimer()) {
        this->log_WARNING_LO_MoveStopped(s_position);
        this->finishMove(Fw::CmdResponse::EXECUTION_ERROR);
    }
    const I64 distance = static_cast<I64>(position) - static_cast<I64>(s_position);
    if (distance == 0) {
        this->m_target = position;
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }
    const U64 steps = static_cast<U64>(distance > 0 ? distance : -distance);
    if (steps > MAX_STEPS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    this->startMove(opCode, cmdSeq, distance > 0, static_cast<U32>(steps), speedHz);
}

void StepperManager ::STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (this->haltTimer()) {
        this->log_WARNING_LO_MoveStopped(s_position);
    }
    this->finishMove(Fw::CmdResponse::EXECUTION_ERROR);
    this->m_target = s_position;
    this->publishTelemetry();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void StepperManager ::HOLD_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Fw::On onOff) {
    if (!onOff.isValid() || !this->m_configured) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (s_running) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->setHold(onOff == Fw::On::ON);
    this->publishTelemetry();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void StepperManager ::ZERO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (s_running) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    s_position = 0;
    this->m_target = 0;
    this->publishTelemetry();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void StepperManager ::startMove(FwOpcodeType opCode, U32 cmdSeq, bool forward, U32 steps, U32 speedHz) {
    if (!this->m_configured) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    // A new move supersedes the one in progress
    if (this->haltTimer()) {
        this->log_WARNING_LO_MoveStopped(s_position);
    }
    this->finishMove(Fw::CmdResponse::EXECUTION_ERROR);
    if (!this->m_holding) {
        this->setHold(true);
    }
    // DR- on means the "inverted" direction; give the driver time to see DR- before the first pulse
    const bool dirOn = (forward == this->m_invertDirection);
    digitalWriteFast(this->m_dirPin, dirOn ? LOW : HIGH);
    delayMicroseconds(20);

    const I32 from = s_position;
    this->m_target = forward ? from + static_cast<I32>(steps) : from - static_cast<I32>(steps);
    s_stepDelta = forward ? 1 : -1;
    s_stepsDone = 0;
    s_stepsTotal = steps;
    s_cruise = static_cast<F32>(speedHz);
    s_start = fminf(START_SPEED_HZ, s_cruise);
    s_speed = s_start;
    s_pulseLow = false;
    s_running = true;
    s_timer.begin(stepIsr, 500000.0f / s_speed);

    this->m_pending = true;
    this->m_opCode = opCode;
    this->m_cmdSeq = cmdSeq;
    this->log_ACTIVITY_HI_MoveStarted(from, this->m_target, speedHz);
    this->publishTelemetry();
}

bool StepperManager ::haltTimer() {
    noInterrupts();
    const bool wasRunning = s_running;
    s_timer.end();
    s_running = false;
    s_speed = 0;
    if (s_pulseLow) {
        digitalWriteFast(s_stepPin, HIGH);  // release PU- mid-pulse; that step is not counted
        s_pulseLow = false;
    }
    interrupts();
    return wasRunning;
}

void StepperManager ::finishMove(Fw::CmdResponse response) {
    if (this->m_pending) {
        this->m_pending = false;
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, response);
    }
}

void StepperManager ::setHold(bool hold) {
    // MF- on (pin LOW) frees the motor; off (high-impedance) energizes it
    digitalWriteFast(this->m_freePin, hold ? HIGH : LOW);
    this->m_holding = hold;
}

void StepperManager ::publishTelemetry() {
    this->tlmWrite_carousel(billeeScm::StepperState(s_running, this->m_holding, s_position, this->m_target,
                                                 static_cast<U32>(s_speed)));
}

}  // namespace billeeScm
