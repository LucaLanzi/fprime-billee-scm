// ======================================================================
// \title  PCA9685Manager.hpp
// \author luca_lanzi
// \brief  hpp file for PCA9685Manager component implementation class
// ======================================================================

#ifndef billeeScm_PCA9685Manager_HPP
#define billeeScm_PCA9685Manager_HPP

#include "Components/PCA9685Manager/PCA9685ManagerComponentAc.hpp"
#include <PCA9685.h>
// PCA9685.h pulls in Arduino.h, which #defines HIGH/LOW as plain macros that would otherwise clobber
// Fw::Logic::HIGH/LOW in any later include. FprimeArduino.hpp captures them as Arduino::DEF_HIGH/DEF_LOW
// and #undefs the raw macros -- same fix RoboclawManager.hpp uses.
#include <Arduino/config/FprimeArduino.hpp>
#include <new>  // for placement new, see m_pcaStorage below

namespace billeeScm {

class PCA9685Manager final : public PCA9685ManagerComponentBase {
  public:
    //! Servo channels exposed (the chip has 16). Must equal billeeScm::ServoAngles::SIZE (Types/PCA9685Types.fpp).
    static constexpr U8 NUM_SERVO_CHANNELS = 12;

    //! What a channel's servo is. Decides which command may drive it (see the calibration table in the .cpp).
    enum class ServoType : U8 {
        POSITIONAL,  //!< Limited-range angle servo: setServoAngle only
        CONTINUOUS   //!< Continuous-rotation servo: setServoSpeed only
    };

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PCA9685Manager object
    PCA9685Manager(const char* const compName  //!< The component name
    );

    //! Destroy PCA9685Manager object
    ~PCA9685Manager();

    //! Bind this instance to its I2C bus and initialise the PCA9685 (50 Hz servo frame, all channels released).
    //! Call once during topology init, before the rate group starts ticking this component. Calls
    //! wire->begin() itself. a5a0 is the value of the board's A5..A0 address pins (0-61), NOT the 7-bit I2C
    //! address: the library adds the 0x40 base itself, so a default board is 0 (-> 0x40).
    void configure(TwoWire* wire, U8 a5a0, uint32_t i2cSpeed = 100000);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for run
    //!
    //! Input port for run handler
    void run_handler(FwIndexType portNum,  //!< The port number
                     U32 context           //!< The call order
                     ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command setServoAngle
    void setServoAngle_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                  U32 cmdSeq,           //!< The command sequence number
                                  U8 channel,           //!< Servo channel, 0-11
                                  F32 angle             //!< Angle in degrees, -90.0..+90.0
                                  ) override;

    //! Handler implementation for command setServoSpeed
    void setServoSpeed_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                  U32 cmdSeq,           //!< The command sequence number
                                  U8 channel,           //!< Servo channel, 0-11
                                  F32 speed             //!< Speed, -1.0..+1.0
                                  ) override;

    //! Handler implementation for command setChannelPWM
    void setChannelPWM_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                  U32 cmdSeq,           //!< The command sequence number
                                  U8 channel,           //!< Channel, 0-11
                                  U16 pwm               //!< 0 = release, else PWM counts within the channel's limits
                                  ) override;

    //! Handler implementation for command releaseChannel
    void releaseChannel_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                   U32 cmdSeq,           //!< The command sequence number
                                   U8 channel            //!< Channel, 0-11
                                   ) override;

    //! Handler implementation for command releaseAll
    void releaseAll_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq            //!< The command sequence number
                               ) override;

    //! Handler implementation for command reinit
    void reinit_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                           U32 cmdSeq            //!< The command sequence number
                           ) override;

  private:
    // ----------------------------------------------------------------------
    // Helpers
    // ----------------------------------------------------------------------

    //! Piecewise-linear map of position in [-1,+1] to a PWM count using the channel's calibrated
    //! min/mid/max (-1 -> min, 0 -> mid, +1 -> max). Never returns a value outside [min,max].
    //! Deliberately NOT PCA9685_ServoEval: see the comment on m_pcaStorage.
    U16 pwmForPosition(U8 channel, F32 position) const;

    //! Send one PWM value (0 = release) to one channel, then check the transaction result.
    //! @return true if the I2C transaction succeeded
    bool writeChannel(U8 channel, U16 pwm);

    //! Read the library's last I2C result, update the error counters/telemetry state, and (if logFault) emit
    //! I2cFault once per new fault, not once per repeated failure, to avoid flooding eventLogger's shallow queue.
    //! @return true if the last transaction succeeded
    bool checkI2c(U8 channel, bool logFault);

    //! init() + 50 Hz servo frame + release all channels.
    //! @return true if every step's I2C transaction succeeded
    bool initDevice(bool logFaults);

    //! Send the command response: OK if ok, else EXECUTION_ERROR.
    void respond(FwOpcodeType opCode, U32 cmdSeq, bool ok);

    // PCA9685 driver instance, constructed in configure() via placement new into this static storage.
    // NOT heap-allocated, and PCA9685_ServoEval is NOT used, because this deployment never calls
    // Os::Baremetal::OverrideNewDelete::registerMemAllocator(): the global operator new/new[] override's
    // FW_ASSERT(pAllocator != nullptr) fires on the very first heap allocation and halts the board (see the
    // matching comment in RoboclawManager.hpp). PCA9685_ServoEval's constructors all do `new float[]`, so using
    // them would assert at boot; the PCA9685 class itself never allocates.
    alignas(PCA9685) uint8_t m_pcaStorage[sizeof(PCA9685)];
    PCA9685* m_pca = nullptr;  //!< Points into m_pcaStorage once configure() runs

    bool m_initOk = false;               //!< True once initDevice() has fully succeeded
    F32 m_values[NUM_SERVO_CHANNELS] = {};  //!< Last commanded value per channel (degrees / speed / 0 = released)
    U32 m_i2cErrorCount = 0;             //!< Failed I2C transactions since boot
    U8 m_lastI2cError = 0;               //!< Wire code of the most recent transaction (0 = ok)
};

}  // namespace billeeScm

#endif
