module billeeScm {
    enum relayId: U8 {
        RELAY1 = 1 @< Science Control Module Pump 1
        RELAY2 = 2 @< Science Control Module Pump 2
        RELAY3 = 3 @< Science Control Module Pump 3
        RELAY4 = 4 @< Science Control Module Pump 4
    }

    struct relay {
        relayNum: relayId @< pump id for pump control
        relayState: Fw.Enabled @< pump state 
    }

}