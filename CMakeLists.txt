#===============================================================================
# WS63 PN532 NFC reader test (UART1 / GPIO_26/27, 115200 8N1).
# Branch: nfc-5321
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/app_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/nfc/pn532.c"
    PARENT_SCOPE)
