// ======================================================================
// \title  RoboclawManager.cpp
// \author luca_lanzi
// \brief  All Roboclaw boards and motors of the Science Control Module, with limit switches
// ======================================================================
#include "Components/RoboclawManager/RoboclawManager.hpp"
#include "Components/Common/Elapsed.hpp"

namespace billeeScm {

static_assert(billeeScm::RoboclawMotor::NUM_CONSTANTS == RoboclawManager::NUM_MOTORS, "RoboclawMotor must name every motor");
static_assert(billeeScm::RoboclawId::NUM_CONSTANTS == RoboclawManager::NUM_BOARDS, "RoboclawId must name every board");

namespace {

//! Which board and channel drives each RoboclawMotor (the block diagram's R1_M1 ... R3_M1)
struct MotorWiring {
    U8 board;
    U8 channel;
};
constexpr MotorWiring kWiring[RoboclawManager::NUM_MOTORS] = {
    {0, 1},  // R1_M1
    {0, 2},  // R1_M2
    {1, 1},  // R2_M1
    {1, 2},  // R2_M2
    {2, 1},  // R3_M1
};

//! Telemetry items polled per board, one per tick
enum PollItem : U32 { POLL_ENCODERS = 0, POLL_CURRENTS, POLL_BATTERY, POLL_ERROR, POLL_ITEMS };

constexpr FwIndexType LOWER = 0;
constexpr FwIndexType UPPER = 1;

}  // namespace

RoboclawManager ::RoboclawManager(const char* const compName) : RoboclawManagerComponentBase(compName) {
    for (FwIndexType m = 0; m < NUM_MOTORS; m++) {
        this->m_motors[m].board = kWiring[m].board;
        this->m_motors[m].channel = kWiring[m].channel;
    }
    // Stagger the first probes so the three boards' 30 ms timeouts don't land on the same tick
    for (FwIndexType b = 0; b < NUM_BOARDS; b++) {
        this->m_boards[b].nextProbeTick = 10 + static_cast<U32>(b) * (PROBE_PERIOD_TICKS / NUM_BOARDS);
    }
}

RoboclawManager ::~RoboclawManager() {}

void RoboclawManager ::configureBoard(billeeScm::RoboclawId board, HardwareSerial* serial, U8 address, uint32_t baud) {
    FW_ASSERT(board.isValid(), board.e);
    FW_ASSERT(serial != nullptr);
    Board& b = this->m_boards[board.e];
    b.address = address;
    // 10 ms per attempt; the library makes 3 attempts, so a board that does not answer costs 30 ms per call
    b.rc = new (b.storage) RoboClaw(serial, 10000);
    b.rc->begin(baud);
}

void RoboclawManager ::configureMotor(billeeScm::RoboclawMotor motor, bool invert) {
    FW_ASSERT(motor.isValid(), motor.e);
    this->m_motors[motor.e].invert = invert;
}

// ----------------------------------------------------------------------
// Tick
// ----------------------------------------------------------------------

void RoboclawManager ::run_handler(FwIndexType portNum, U32 context) {
    this->m_tick++;
    this->readLimitSwitches();

    const Fw::Time now = this->getTime();
    for (FwIndexType m = 0; m < NUM_MOTORS; m++) {
        Motor& motor = this->m_motors[m];
        if (motor.mode == billeeScm::MotorMode::IDLE) {
            continue;
        }
        if (this->limitTripped(m, motor.direction)) {
            // Reaching the switch is the goal of TO_LIMIT and a fault for RUN_FOR (sequence should stop there)
            const Fw::CmdResponse response = (motor.mode == billeeScm::MotorMode::TO_LIMIT)
                                                 ? Fw::CmdResponse::OK
                                                 : Fw::CmdResponse::EXECUTION_ERROR;
            (void)this->stopMotor(m, billeeScm::StopReason::LIMIT_SWITCH, response);
            continue;
        }
        if (motor.mode == billeeScm::MotorMode::RUN_FOR || motor.mode == billeeScm::MotorMode::TO_LIMIT) {
            if (elapsedMs(motor.start, now) >= static_cast<I64>(motor.limitMs)) {
                const bool done = (motor.mode == billeeScm::MotorMode::RUN_FOR);
                (void)this->stopMotor(m, done ? billeeScm::StopReason::TIME_UP : billeeScm::StopReason::TIMEOUT,
                                      done ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
            }
        }
    }

    this->pollTelemetry();
    if (this->m_tick % TLM_PERIOD_TICKS == 0) {
        this->publishTelemetry();
    }
}

// ----------------------------------------------------------------------
// Commands
// ----------------------------------------------------------------------

void RoboclawManager ::MOVE_cmdHandler(FwOpcodeType opCode,
                                       U32 cmdSeq,
                                       billeeScm::RoboclawMotor motor,
                                       billeeScm::MotorDir direction,
                                       U8 speed) {
    if (!motor.isValid() || !direction.isValid() || speed > SPEED_FULL_SCALE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    if (direction == billeeScm::MotorDir::STOPPED || speed == 0) {
        const bool ok = this->stopMotor(motor.e, billeeScm::StopReason::COMMANDED, Fw::CmdResponse::EXECUTION_ERROR);
        this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->startMotion(opCode, cmdSeq, motor.e, billeeScm::MotorMode::MOVE, direction.e, speed, 0);
}

void RoboclawManager ::RUN_FOR_cmdHandler(FwOpcodeType opCode,
                                          U32 cmdSeq,
                                          billeeScm::RoboclawMotor motor,
                                          billeeScm::MotorDir direction,
                                          U8 speed,
                                          U32 durationMs) {
    if (!motor.isValid() || !direction.isValid() || direction == billeeScm::MotorDir::STOPPED || speed == 0 ||
        speed > SPEED_FULL_SCALE || durationMs == 0 || durationMs > MAX_MOTION_MS) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    this->startMotion(opCode, cmdSeq, motor.e, billeeScm::MotorMode::RUN_FOR, direction.e, speed, durationMs);
}

void RoboclawManager ::TO_LIMIT_cmdHandler(FwOpcodeType opCode,
                                           U32 cmdSeq,
                                           billeeScm::RoboclawMotor motor,
                                           billeeScm::MotorDir direction,
                                           U8 speed,
                                           U32 timeoutMs) {
    if (!motor.isValid() || !direction.isValid() || direction == billeeScm::MotorDir::STOPPED || speed == 0 ||
        speed > SPEED_FULL_SCALE || timeoutMs == 0 || timeoutMs > MAX_MOTION_MS ||
        !this->hasLimit(motor.e, direction.e)) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    this->startMotion(opCode, cmdSeq, motor.e, billeeScm::MotorMode::TO_LIMIT, direction.e, speed, timeoutMs);
}

void RoboclawManager ::STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::RoboclawMotor motor) {
    if (!motor.isValid()) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    const bool ok = this->stopMotor(motor.e, billeeScm::StopReason::COMMANDED, Fw::CmdResponse::EXECUTION_ERROR);
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void RoboclawManager ::STOP_ALL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    bool ok = true;
    for (FwIndexType b = 0; b < NUM_BOARDS; b++) {
        Board& board = this->m_boards[b];
        bool anyRunning = false;
        for (FwIndexType m = 0; m < NUM_MOTORS; m++) {
            anyRunning = anyRunning || (this->m_motors[m].board == b && this->m_motors[m].mode != billeeScm::MotorMode::IDLE);
        }
        // Always try (one packet stops both channels), even if the board looks offline
        const bool acked = (board.rc != nullptr) && board.rc->DutyM1M2(board.address, 0, 0);
        if (acked) {
            this->markOnline(b);
        } else if (board.online) {
            this->markOffline(b);
        }
        // An unanswered stop is a failure only if a motor on that board may be running
        ok = ok && (acked || !anyRunning);
        for (FwIndexType m = 0; m < NUM_MOTORS; m++) {
            Motor& motor = this->m_motors[m];
            if (motor.board == b && motor.mode != billeeScm::MotorMode::IDLE) {
                motor.mode = billeeScm::MotorMode::IDLE;
                motor.direction = billeeScm::MotorDir::STOPPED;
                motor.speed = 0;
                this->finishPending(m, Fw::CmdResponse::EXECUTION_ERROR);
                this->log_ACTIVITY_HI_MotorStopped(static_cast<billeeScm::RoboclawMotor::T>(m),
                                                   billeeScm::StopReason::COMMANDED);
            }
        }
    }
    this->publishTelemetry();
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void RoboclawManager ::RESET_ENCODERS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::RoboclawId roboclaw) {
    if (!roboclaw.isValid()) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    Board& board = this->m_boards[roboclaw.e];
    const bool ok = (board.rc != nullptr) && board.rc->ResetEncoders(board.address);
    ok ? this->markOnline(roboclaw.e) : this->markOffline(roboclaw.e);
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

// ----------------------------------------------------------------------
// Motion
// ----------------------------------------------------------------------

void RoboclawManager ::startMotion(FwOpcodeType opCode,
                                   U32 cmdSeq,
                                   FwIndexType m,
                                   billeeScm::MotorMode::T mode,
                                   billeeScm::MotorDir::T direction,
                                   U8 speed,
                                   U32 limitMs) {
    Motor& motor = this->m_motors[m];
    const billeeScm::RoboclawMotor motorId = static_cast<billeeScm::RoboclawMotor::T>(m);

    // Decide on fresh switch readings, not the last tick's
    this->readLimitSwitches();
    if (this->limitTripped(m, direction)) {
        if (mode == billeeScm::MotorMode::TO_LIMIT) {
            // Already where it was asked to go
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        } else {
            this->log_WARNING_LO_LimitBlocked(motorId, direction);
            this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        }
        return;
    }

    // A new command on the motor supersedes the one still running on it
    this->finishPending(m, Fw::CmdResponse::EXECUTION_ERROR);

    const I32 magnitude = (static_cast<I32>(speed) * MAX_DUTY + SPEED_FULL_SCALE / 2) / SPEED_FULL_SCALE;
    const bool positive = (direction == billeeScm::MotorDir::FORWARD) != motor.invert;
    if (!this->sendDuty(m, positive ? magnitude : -magnitude)) {
        motor.mode = billeeScm::MotorMode::IDLE;
        motor.direction = billeeScm::MotorDir::STOPPED;
        motor.speed = 0;
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    motor.mode = mode;
    motor.direction = direction;
    motor.speed = speed;
    this->log_ACTIVITY_HI_MotorStarted(motorId, mode, direction, speed);
    this->publishTelemetry();

    if (mode == billeeScm::MotorMode::MOVE) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
        return;
    }
    // Answered by run_handler (or stopMotor) when the motion ends
    motor.pending = true;
    motor.opCode = opCode;
    motor.cmdSeq = cmdSeq;
    motor.start = this->getTime();
    motor.limitMs = limitMs;
}

bool RoboclawManager ::stopMotor(FwIndexType m, billeeScm::StopReason reason, Fw::CmdResponse response) {
    Motor& motor = this->m_motors[m];
    const bool wasRunning = (motor.mode != billeeScm::MotorMode::IDLE);
    // Always send the stop: the motor may still be turning after a failed command or a firmware reset
    const bool acked = this->sendDuty(m, 0);
    motor.mode = billeeScm::MotorMode::IDLE;
    motor.direction = billeeScm::MotorDir::STOPPED;
    motor.speed = 0;
    // An unacknowledged stop of a running motor is a failure even if the motion itself ended as intended
    const bool ok = acked || !wasRunning;
    this->finishPending(m, ok ? response : Fw::CmdResponse(Fw::CmdResponse::EXECUTION_ERROR));
    if (wasRunning) {
        this->log_ACTIVITY_HI_MotorStopped(static_cast<billeeScm::RoboclawMotor::T>(m), reason);
    }
    this->publishTelemetry();
    return ok;
}

void RoboclawManager ::finishPending(FwIndexType m, Fw::CmdResponse response) {
    Motor& motor = this->m_motors[m];
    if (motor.pending) {
        motor.pending = false;
        this->cmdResponse_out(motor.opCode, motor.cmdSeq, response);
    }
}

bool RoboclawManager ::sendDuty(FwIndexType m, I32 duty) {
    const Motor& motor = this->m_motors[m];
    Board& board = this->m_boards[motor.board];
    if (board.rc == nullptr) {
        return false;
    }
    // The library takes the signed duty as its raw 16-bit word (two's complement for negative)
    const uint16_t word = static_cast<uint16_t>(static_cast<I16>(duty));
    const bool acked = (motor.channel == 1) ? board.rc->DutyM1(board.address, word)
                                            : board.rc->DutyM2(board.address, word);
    acked ? this->markOnline(motor.board) : this->markOffline(motor.board);
    return acked;
}

// ----------------------------------------------------------------------
// Limit switches
// ----------------------------------------------------------------------

void RoboclawManager ::readLimitSwitches() {
    for (FwIndexType m = 0; m < NUM_MOTORS; m++) {
        for (FwIndexType side = LOWER; side <= UPPER; side++) {
            const FwIndexType port = 2 * m + side;
            bool tripped = false;
            if (this->isConnected_limitSwGet_OutputPort(port)) {
                Fw::Logic level;
                // Closes to GND when tripped (pins use the internal pull-up)
                tripped = (this->limitSwGet_out(port, level) == Drv::GpioStatus::OP_OK) && (level == Fw::Logic::LOW);
            }
            (side == LOWER ? this->m_motors[m].lowerLimit : this->m_motors[m].upperLimit) = tripped;
        }
    }
}

bool RoboclawManager ::limitTripped(FwIndexType m, billeeScm::MotorDir::T direction) const {
    if (direction == billeeScm::MotorDir::FORWARD) {
        return this->m_motors[m].upperLimit;
    }
    if (direction == billeeScm::MotorDir::REVERSE) {
        return this->m_motors[m].lowerLimit;
    }
    return false;
}

bool RoboclawManager ::hasLimit(FwIndexType m, billeeScm::MotorDir::T direction) {
    const FwIndexType side = (direction == billeeScm::MotorDir::FORWARD) ? UPPER : LOWER;
    return this->isConnected_limitSwGet_OutputPort(2 * m + side);
}

// ----------------------------------------------------------------------
// Board telemetry
// ----------------------------------------------------------------------

void RoboclawManager ::pollTelemetry() {
    // At most one serial transaction per tick: a board that does not answer blocks for 30 ms, and the TaskRunner is
    // cooperative.
    for (FwIndexType b = 0; b < NUM_BOARDS; b++) {
        Board& board = this->m_boards[b];
        if (board.rc != nullptr && !board.online && this->m_tick >= board.nextProbeTick) {
            board.nextProbeTick = this->m_tick + PROBE_PERIOD_TICKS;
            bool valid = false;
            const U16 volts = board.rc->ReadMainBatteryVoltage(board.address, &valid);
            if (valid) {
                board.batteryDv = volts;
                this->markOnline(b);
            }
            return;
        }
    }
    const U32 slots = static_cast<U32>(NUM_BOARDS) * POLL_ITEMS;
    for (U32 tries = 0; tries < slots; tries++) {
        const U32 slot = this->m_pollSlot++ % slots;
        const FwIndexType b = static_cast<FwIndexType>(slot / POLL_ITEMS);
        Board& board = this->m_boards[b];
        if (board.rc == nullptr || !board.online) {
            continue;
        }
        bool ok = false;
        switch (slot % POLL_ITEMS) {
            case POLL_ENCODERS: {
                uint32_t enc1 = 0;
                uint32_t enc2 = 0;
                ok = board.rc->ReadEncoders(board.address, enc1, enc2);
                if (ok) {
                    for (U8 channel = 1; channel <= 2; channel++) {
                        const FwIndexType m = this->motorAt(b, channel);
                        if (m >= 0) {
                            const I32 count = static_cast<I32>(channel == 1 ? enc1 : enc2);
                            this->m_motors[m].encoder = this->m_motors[m].invert ? -count : count;
                        }
                    }
                }
                break;
            }
            case POLL_CURRENTS: {
                int16_t c1 = 0;
                int16_t c2 = 0;
                ok = board.rc->ReadCurrents(board.address, c1, c2);
                if (ok) {
                    for (U8 channel = 1; channel <= 2; channel++) {
                        const FwIndexType m = this->motorAt(b, channel);
                        if (m >= 0) {
                            this->m_motors[m].currentMa = 10 * static_cast<I32>(channel == 1 ? c1 : c2);  // 10 mA units
                        }
                    }
                }
                break;
            }
            case POLL_BATTERY: {
                const U16 volts = board.rc->ReadMainBatteryVoltage(board.address, &ok);
                if (ok) {
                    board.batteryDv = volts;
                }
                break;
            }
            default: {
                const U32 bits = board.rc->ReadError(board.address, &ok);
                if (ok) {
                    board.errorBits = bits;
                }
                break;
            }
        }
        if (!ok) {
            this->markOffline(b);
        }
        return;
    }
}

void RoboclawManager ::markOnline(FwIndexType b) {
    if (!this->m_boards[b].online) {
        this->m_boards[b].online = true;
        this->log_ACTIVITY_HI_BoardOnline(static_cast<billeeScm::RoboclawId::T>(b));
    }
}

void RoboclawManager ::markOffline(FwIndexType b) {
    Board& board = this->m_boards[b];
    if (board.online) {
        board.online = false;
        this->log_WARNING_HI_BoardOffline(static_cast<billeeScm::RoboclawId::T>(b));
    }
    board.nextProbeTick = this->m_tick + PROBE_PERIOD_TICKS;
}

FwIndexType RoboclawManager ::motorAt(FwIndexType b, U8 channel) const {
    for (FwIndexType m = 0; m < NUM_MOTORS; m++) {
        if (this->m_motors[m].board == b && this->m_motors[m].channel == channel) {
            return m;
        }
    }
    return -1;
}

void RoboclawManager ::publishTelemetry() {
    billeeScm::RoboclawMotorStates motors;
    for (FwIndexType m = 0; m < NUM_MOTORS; m++) {
        const Motor& motor = this->m_motors[m];
        motors[m] = billeeScm::RoboclawMotorState(motor.mode, motor.direction, motor.speed, motor.lowerLimit,
                                                  motor.upperLimit, motor.encoder, motor.currentMa);
    }
    billeeScm::RoboclawBoardStates boards;
    for (FwIndexType b = 0; b < NUM_BOARDS; b++) {
        const Board& board = this->m_boards[b];
        boards[b] = billeeScm::RoboclawBoardState(board.online, board.batteryDv, board.errorBits);
    }
    this->tlmWrite_motorStates(motors);
    this->tlmWrite_boardStates(boards);
}

}  // namespace billeeScm
