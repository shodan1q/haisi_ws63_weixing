#===============================================================================
# LCD demo for the 4T_HRM_QS2 board (Hi3863/WS63 family).
# Appends source files to the parent samples component.
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts.c"
    PARENT_SCOPE)
