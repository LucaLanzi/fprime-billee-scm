####
# LoadCellManager: HX711 load-cell ADCs (Sample Cache weight sensors)
####
register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/LoadCellManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/LoadCellManager.cpp"
    DEPENDS
        Types
)
