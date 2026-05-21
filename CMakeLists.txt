#===============================================================================
# WS63 temperature/humidity/pressure transmitter
# Branch: temp-humid-sensor
#   SHT30 + BMP280 on bit-bang I2C (GPIO_11 SDA, GPIO_12 SCL)
#   TM1640 7-seg tube on (GPIO_13 DIN, GPIO_3 CLK)
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/app_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensors/i2c_bb.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensors/sht30.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensors/bmp280.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensors/tm1640.c"
    PARENT_SCOPE)
