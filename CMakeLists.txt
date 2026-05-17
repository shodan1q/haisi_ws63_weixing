#===============================================================================
# 4T_HRM_QS2 combined demo (LCD + LEDs + buzzer + ADC buttons).
# Appends source files to the parent samples component.
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/app_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts.c"
    PARENT_SCOPE)
