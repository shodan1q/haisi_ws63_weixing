#===============================================================================
# WS63 temperature/humidity/pressure transmitter + MQTT publisher.
# Branch: temp-humid-sensor
#   SHT30 + BMP280 on bit-bang I2C (GPIO_7 SDA, GPIO_8 SCL)
#   TM1640 7-seg tube on (GPIO_14 DIN, GPIO_13 CLK)
#   WiFi + MQTT telemetry to tcp://121.41.23.138:1883
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/app_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensors/i2c_bb.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensors/sht30.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensors/bmp280.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensors/tm1640.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/net/wifi_connect.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/net/sensors_mqtt.c"
    PARENT_SCOPE)

set(PUBLIC_HEADER "${PUBLIC_HEADER}"
    "${CMAKE_CURRENT_SOURCE_DIR}/net"
    "${CMAKE_CURRENT_SOURCE_DIR}/sensors"
    PARENT_SCOPE)
