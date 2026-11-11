####
# BootReporter: reports the previous boot's FW_ASSERT / hard fault and the SD card mount result.
####
register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/BootReporter.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/BootReporter.cpp"
)
