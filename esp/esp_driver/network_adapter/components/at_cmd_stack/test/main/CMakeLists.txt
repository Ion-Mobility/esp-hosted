set(COMPONENT_SRCS 
    "app_main.c"
    "parse_test.c"
    "mqtt_test.c"
    "common_test_helpers.c"
)

set(AT_COMPONENT_ROOT "../..")

set(COMPONENT_ADD_PRIVATE_INCLUDEDIRS 
    "${AT_COMPONENT_ROOT}/src/port"
    "${AT_COMPONENT_ROOT}/src/parse_and_exec/include"
    "${AT_COMPONENT_ROOT}/src/services/mqtt_client/include"
    "${AT_COMPONENT_ROOT}/src/common/include")


idf_component_register(SRCS "${COMPONENT_SRCS}"
    PRIV_INCLUDE_DIRS "${COMPONENT_ADD_PRIVATE_INCLUDEDIRS}"
    REQUIRES cmock mqtt at_cmd_stack protocol_examples_common nvs_flash
    )
