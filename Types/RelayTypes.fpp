module billeeScm {
    @ Number of logic-level relays on the Science Control Module
    constant RELAY_COUNT = 6

    @ Logic-level relays on the Science Control Module. The value is the index of RelayManager's relaySet output
    @ port; the topology wires each index to the GPIO named in SciencePins.hpp (Science Hardware Architecture 2027).
    enum RelayId: U8 {
        PUMP1 = 0    @< Chemical Management Pump 1, T_GPIO_39, +12V
        PUMP2 = 1    @< Chemical Management Pump 2, T_GPIO_38, +12V
        PUMP3 = 2    @< Chemical Management Pump 3, T_GPIO_37, +12V
        PUMP4 = 3    @< Chemical Management Pump 4, T_GPIO_36, +12V
        UV_LIGHT = 4 @< UV Spectroscopy UV light, T_GPIO_40, +22.2V
        STIRRER = 5  @< Chemical Management magnetic stirrer MS1, T_GPIO_34, +12V
    }

    @ State of every relay, indexed by RelayId
    array RelayStates = [RELAY_COUNT] Fw.On
}
