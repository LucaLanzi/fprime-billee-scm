####
# RelayManager: logic-level relays (pumps 1-4, UV light, magnetic stirrer)
####
register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/RelayManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/RelayManager.cpp"
    DEPENDS
        Types
)
