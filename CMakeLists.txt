#===============================================================================
# WS63 two-board SLE throughput demo. Imports SDK official sle_speed_*
# from src/application/samples/bt/sle/ into our custom/ tree.
# app_demo.c picks role at compile time. Both files compile every build;
# the chosen role's entry function is the only one called at boot.
#===============================================================================
set(SOURCES "${SOURCES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/app_demo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lcd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fonts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_speed_server/src/sle_speed_server.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_speed_server/src/sle_speed_server_adv.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/sle_speed_client/src/sle_speed_client.c"
    PARENT_SCOPE)
