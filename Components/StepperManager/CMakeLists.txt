####
# StepperManager: Sample Cache carousel stepper (step / direction / motor-free driver)
####
register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/StepperManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/StepperManager.cpp"
    DEPENDS
        Types
)
