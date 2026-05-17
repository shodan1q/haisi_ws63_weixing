#===============================================================================
# Append main.c into the parent samples component's SOURCES.
# Follows the same convention as src/application/samples/peripheral/helloworld.
#===============================================================================
set(SOURCES "${SOURCES}" "${CMAKE_CURRENT_SOURCE_DIR}/main.c" PARENT_SCOPE)
