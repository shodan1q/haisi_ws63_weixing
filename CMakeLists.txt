#===============================================================================
# WS63 SG90 servo demo on GPIO_10 (software-bit-banged 50 Hz PWM).
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/app_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/servo/sg90_control.c"
    PARENT_SCOPE)
