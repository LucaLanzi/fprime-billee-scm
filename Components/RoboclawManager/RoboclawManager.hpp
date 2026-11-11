// ======================================================================
// \title  RoboclawManager.hpp
// \author luca_lanzi
// \brief  All Roboclaw boards and motors of the Science Control Module, with limit switches
// ======================================================================
#ifndef billeeScm_RoboclawManager_HPP
#define billeeScm_RoboclawManager_HPP

#include "Components/RoboclawManager/RoboclawManagerComponentAc.hpp"
#include <RoboClaw.h>
// RoboClaw.h pulls in Arduino's HardwareSerial.h, which #defines HIGH/LOW as plain macros that would clobber
// Fw::Logic::HIGH/LOW. FprimeArduino.hpp captures them as Arduino::DEF_HIGH/DEF_LOW and #undefs the raw macros.
#include <Arduino/config/FprimeArduino.hpp>
#include <new>  // placement new, see Board::storage

namespace billeeScm {

class RoboclawManager final : public RoboclawManagerComponentBase {
  public:
    static constexpr FwIndexType NUM_BOARDS = billeeScm::RoboclawBoardStates::SIZE;
    static constexpr FwIndexType NUM_MOTORS = billeeScm::RoboclawMotorStates::SIZE;
    static constexpr U8 SPEED_FULL_SCALE = 100;   //!< Speed is 0-100 % duty
    static constexpr I32 MAX_DUTY = 32767;        //!< Full duty of the Roboclaw's signed duty commands
    static constexpr U32 MAX_MOTION_MS = 300000;  //!< Longest RUN_FOR / TO_LIMIT (under cmdSeq's 600 s timeout)
    static constexpr U32 PROBE_PERIOD_TICKS = 100;  //!< Offline boards are probed every 2 s at 50 Hz
    static constexpr U32 TLM_PERIOD_TICKS = 5;      //!< Telemetry published at 10 Hz from the 50 Hz tick

    RoboclawManager(const char* const compName);
    ~RoboclawManager();

    //! Bind a board to its serial port and packet-serial address. Call once per board during topology setup.
    void configureBoard(billeeScm::RoboclawId board, HardwareSerial* serial, U8 address, uint32_t baud = 38400);

    //! invert = true when the motor's positive duty moves it toward its LOWER switch, so that FORWARD always moves
    //! toward the upper one. Also negates its encoder count. Default false.
    void configureMotor(billeeScm::RoboclawMotor motor, bool invert);

  private:
    void run_handler(FwIndexType portNum, U32 context) override;
    void MOVE_cmdHandler(FwOpcodeType opCode,
                         U32 cmdSeq,
                         billeeScm::RoboclawMotor motor,
                         billeeScm::MotorDir direction,
                         U8 speed) override;
    void RUN_FOR_cmdHandler(FwOpcodeType opCode,
                            U32 cmdSeq,
                            billeeScm::RoboclawMotor motor,
                            billeeScm::MotorDir direction,
                            U8 speed,
                            U32 durationMs) override;
    void TO_LIMIT_cmdHandler(FwOpcodeType opCode,
                             U32 cmdSeq,
                             billeeScm::RoboclawMotor motor,
                             billeeScm::MotorDir direction,
                             U8 speed,
                             U32 timeoutMs) override;
    void STOP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::RoboclawMotor motor) override;
    void STOP_ALL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void RESET_ENCODERS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::RoboclawId roboclaw) override;

    struct Board {
        alignas(RoboClaw) uint8_t storage[sizeof(RoboClaw)];  //!< RoboClaw driver, placement-new'd in configureBoard
        RoboClaw* rc = nullptr;
        U8 address = 0;
        bool online = false;
        U32 nextProbeTick = 0;
        U16 batteryDv = 0;
        U32 errorBits = 0;
    };

    struct Motor {
        U8 board = 0;    //!< RoboclawId of its board
        U8 channel = 1;  //!< 1 = M1, 2 = M2 on that board
        bool invert = false;
        billeeScm::MotorMode::T mode = billeeScm::MotorMode::IDLE;
        billeeScm::MotorDir::T direction = billeeScm::MotorDir::STOPPED;
        U8 speed = 0;
        bool lowerLimit = false;
        bool upperLimit = false;
        I32 encoder = 0;
        I32 currentMa = 0;
        // Command waiting for the motion to end (RUN_FOR / TO_LIMIT)
        bool pending = false;
        FwOpcodeType opCode = 0;
        U32 cmdSeq = 0;
        Fw::Time start;
        U32 limitMs = 0;
    };

    //! Validate and start a motion. Answers the command itself (now for MOVE, at the end for RUN_FOR / TO_LIMIT).
    void startMotion(FwOpcodeType opCode,
                     U32 cmdSeq,
                     FwIndexType m,
                     billeeScm::MotorMode::T mode,
                     billeeScm::MotorDir::T direction,
                     U8 speed,
                     U32 limitMs);

    //! Send duty 0 to a motor, set it IDLE, emit MotorStopped and answer its pending command (if any) with
    //! `response`. Returns false if the stop was not acknowledged by a board that may have the motor running.
    bool stopMotor(FwIndexType m, billeeScm::StopReason reason, Fw::CmdResponse response);

    //! Answer and forget a motor's pending command (no-op if none)
    void finishPending(FwIndexType m, Fw::CmdResponse response);

    //! Read every connected limit switch into the motor table
    void readLimitSwitches();

    //! True if the switch in `direction` is tripped
    bool limitTripped(FwIndexType m, billeeScm::MotorDir::T direction) const;

    //! True if a limit switch is wired in `direction`
    bool hasLimit(FwIndexType m, billeeScm::MotorDir::T direction);

    //! Send a signed duty to one motor. Updates the board's online state. Returns true if acknowledged.
    bool sendDuty(FwIndexType m, I32 duty);

    //! One telemetry transaction (or offline probe) per tick
    void pollTelemetry();

    void markOnline(FwIndexType b);
    void markOffline(FwIndexType b);

    //! Index of the motor on board b, channel c (1/2), or -1 if that channel drives nothing
    FwIndexType motorAt(FwIndexType b, U8 channel) const;

    void publishTelemetry();

    Board m_boards[NUM_BOARDS];
    Motor m_motors[NUM_MOTORS];
    U32 m_tick = 0;
    U32 m_pollSlot = 0;  //!< Round-robin position over (board, telemetry item)
};

}  // namespace billeeScm

#endif
