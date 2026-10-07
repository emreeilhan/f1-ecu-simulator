set(ECU_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
idf_component_register(
    SRCS "uart_main.c"
         "${ECU_ROOT}/src/ecu_core.c" "${ECU_ROOT}/src/ecu_protocol.c"
         "${ECU_ROOT}/src/ecu_time.c" "${ECU_ROOT}/src/ecu_policy.c"
         "${ECU_ROOT}/src/ecu_transport.c"
    INCLUDE_DIRS "${ECU_ROOT}/include"
    REQUIRES esp_driver_uart esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
