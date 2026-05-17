#===============================================================================
# 4T_HRM_QS2 SLE provisioning demo — server + client roles in one tree.
# app_demo.c selects which role this build acts as via WS63_ROLE.
# Both server.c and client.c compile; only the chosen role's entry runs.
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/app_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/wifi_sta.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_server/src/SLE_Distribute_Network_Server.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_server/src/SLE_Distribute_Network_Server_adv.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_client/src/SLE_Distribute_Network_Client.c"
    PARENT_SCOPE)
