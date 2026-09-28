module billeeScm{
    enum stirrerId: U8{
        STIRRER1 = 1 @< SCM Stirrer 1
        STIRRER2 = 2 @< SCM Stirrer 2
        STIRRER3 = 3 @< SCM Stirrer 3
        STIRRER4 = 4 @< SCM Stirrer 4
    }

    struct stirrer {
        stirrerNum: stirrerId @< Stirrer id for pump control
        stirrerState: Fw.Enabled
    }
}