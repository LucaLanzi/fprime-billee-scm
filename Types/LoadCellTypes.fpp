module billeeScm {
    @ Number of HX711 load-cell ADCs
    constant LOADCELL_COUNT = 2

    @ Sample Cache weight sensors, each a load cell on an HX711 Wheatstone-bridge ADC (block diagram "Dat/CLK Bus 1/2").
    @ Pins in SciencePins.hpp.
    enum LoadCellId: U8 {
        CELL1 = 0 @< Weight sensor on Dat/CLK bus 1
        CELL2 = 1 @< Weight sensor on Dat/CLK bus 2
    }

    @ Which side of a threshold WAIT_MASS waits for
    enum MassCompare: U8 {
        ABOVE = 0 @< Mass >= threshold
        BELOW = 1 @< Mass <= threshold
    }

    @ State of one load cell
    struct LoadCellState {
        connected: bool    @< A sample arrived within the last second
        raw: I32           @< Latest raw 24-bit reading (channel A, gain 128)
        grams: F32         @< (raw - tareOffset) / countsPerGram
        tareOffset: I32    @< Raw reading that means 0 g (set by TARE)
        countsPerGram: F32 @< Scale (set by CALIBRATE or SET_SCALE)
    }

    @ State of every load cell, indexed by LoadCellId
    array LoadCellStates = [LOADCELL_COUNT] LoadCellState
}
