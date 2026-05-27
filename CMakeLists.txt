#===============================================================================
# WS63 dual-servo (GPIO_8 / GPIO_9) + WiFi + MQTT controller.
#   - servo/servo_dual.c   two SG90 driven synchronously
#   - net/wifi_connect.c   blocking WiFi STA helper (HiHope)
#   - net/mqtt_app.c       Paho MQTT client (esp32watch broker config)
# MQTTClient.h comes from the always-built open_source/mqtt component.
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/app_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/servo/servo_dual.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/laser.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/net/wifi_connect.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/net/mqtt_app.c"
    PARENT_SCOPE)

set(PUBLIC_HEADER "${PUBLIC_HEADER}"
    "${CMAKE_CURRENT_SOURCE_DIR}/servo"
    "${CMAKE_CURRENT_SOURCE_DIR}/net"
    PARENT_SCOPE)
