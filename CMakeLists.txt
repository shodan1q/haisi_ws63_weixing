#===============================================================================
# 4T_HRM_QS2 SLE (NearLink) provisioning demo.
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/app_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/wifi_sta.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_server/src/SLE_Distribute_Network_Server.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_server/src/SLE_Distribute_Network_Server_adv.c"
    PARENT_SCOPE)
