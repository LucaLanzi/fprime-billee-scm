module billeeScm {
    enum pumpId: U8 {
        PUMP1 = 1 @< Science Control Module Pump 1
        PUMP2 = 2 @< Science Control Module Pump 2
        PUMP3 = 3 @< Science Control Module Pump 3
        PUMP4 = 4 @< Science Control Module Pump 4
    }

    struct pump {
        pumpNum: pumpId @< pump id for pump control
        pumpState: Fw.Enabled @< pump state
    }

}
