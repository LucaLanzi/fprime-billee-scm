// ======================================================================
// \title  PCA9685Manager.hpp
// \author luca_lanzi
// \brief  PCA9685 16-channel servo board over I2C
// ======================================================================
#ifndef billeeScm_PCA9685Manager_HPP
#define billeeScm_PCA9685Manager_HPP

#include "Components/PCA9685Manager/PCA9685ManagerComponentAc.hpp"
#include <PCA9685.h>
// PCA9685.h pulls in Arduino's headers, which #define HIGH/LOW as plain macros that would clobber Fw::Logic::HIGH/LOW.
// FprimeArduino.hpp captures them as Arduino::DEF_HIGH/DEF_LOW and #undefs the raw macros.
#include <Arduino/config/FprimeArduino.hpp>
#include <new>  // placement new, see m_pcaStorage

namespace billeeScm {

class PCA9685Manager final : public PCA9685ManagerComponentBase {
  public:
    //! Number of servo channels (SERVO_COUNT in Types/PCA9685Types.fpp)
    static constexpr U8 NUM_SERVOS = billeeScm::ServoStates::SIZE;

    //! Full-scale speed of SERVO_SET, in percent
    static constexpr U8 SPEED_FULL_SCALE = 100;

    PCA9685Manager(const char* const compName);
    ~PCA9685Manager();

    //! Bind to the PCA9685 on `wire` (address pins A5..A0 = a5a0) and initialize it. Call once during topology
    //! setup, before the rate group starts ticking this component.
    void configure(TwoWire* wire, U8 a5a0, uint32_t i2cSpeed = 100000);

  private:
    void run_handler(FwIndexType portNum, U32 context) override;
    void SERVO_SET_cmdHandler(FwOpcodeType opCode,
                              U32 cmdSeq,
                              billeeScm::ServoId servo,
                              billeeScm::MotorDir direction,
                              U8 speed) override;
    void SERVO_OFF_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::ServoId servo) override;
    void SERVO_PULSE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, billeeScm::ServoId servo, U16 counts) override;
    void STOP_ALL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void REINIT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;

    //! Pulse (counts) for a direction/speed on a channel, clamped to the channel's calibrated limits
    U16 pulseFor(U8 channel, billeeScm::MotorDir direction, U8 speed) const;

    //! Record a channel's commanded state (republished every tick)
    void setState(U8 channel, billeeScm::MotorDir direction, U8 speed, U16 pulse);

    //! Write one channel (0 = release) and check the bus. Returns true on success.
    bool writeChannel(U8 channel, U16 counts);

    //! Read the library's last I2C error; count it and log I2cFault when the code changes. Returns true if no error.
    bool checkI2c(U8 channel);

    //! Initialize the PCA9685 (50 Hz) and release every channel. Returns true if every transaction succeeded.
    bool initDevice();

    void respond(FwOpcodeType opCode, U32 cmdSeq, bool ok);

    // PCA9685 driver, constructed in configure() with placement new into this storage rather than on the heap, so
    // its lifetime and memory are fixed at build time.
    alignas(PCA9685) uint8_t m_pcaStorage[sizeof(PCA9685)];
    PCA9685* m_pca = nullptr;

    bool m_initOk = false;            //!< True once initDevice() has fully succeeded
    billeeScm::ServoStates m_states;  //!< Last commanded state per servo
    U32 m_i2cErrorCount = 0;          //!< Failed I2C transactions since boot
    U8 m_lastI2cError = 0;            //!< Wire code of the most recent transaction (0 = ok)
};

}  // namespace billeeScm

#endif
