module billeeScm{
    enum stirrerId: U8{
        STIRRER1 = 1 @< SCM Magnetic Stirrer 1
        STIRRER2 = 2 @< SCM Magnetic Stirrer 2
    }

    struct stirrer {
        stirrerNum: stirrerId @< Stirrer id for stirrer control
        stirrerState: Fw.Enabled @< stirrer state
    }
}
