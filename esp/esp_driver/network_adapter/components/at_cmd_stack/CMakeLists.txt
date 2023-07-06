set(CMD_HANDLERS_SRCS
"src/parse_and_exec/cmd_handlers/qmtcfg_parse_exec_handler.c"
"src/parse_and_exec/cmd_handlers/qmtopen_parse_exec_handler.c"
"src/parse_and_exec/cmd_handlers/qmtconn_parse_exec_handler.c"
"src/parse_and_exec/cmd_handlers/qmtdump_parse_exec_handler.c"
)

set(APP_SRCS
    "src/app/at_cmd_app.c"
    "src/app/at_cmd_app_helper.c"
)

set(PARSE_AND_EXEC_SRCS
    "src/parse_and_exec/parse_and_exec_constants.c"
    "src/parse_and_exec/parse_and_exec_core.c"
    "src/parse_and_exec/parse_and_exec_cmd_handlers_list.c"
    "src/parse_and_exec/parse_and_exec_helpers.c"
    ${CMD_HANDLERS_SRCS}
)

set(SERVICES_SRCS
    "src/services/mqtt_client/mqtt_service_configs.c"
    "src/services/mqtt_client/mqtt_service_helpers.c"
    "src/services/mqtt_client/mqtt_service_core.c"
)

set(COMPONENT_SRCS 
    ${APP_SRCS}
    ${PARSE_AND_EXEC_SRCS}
    ${SERVICES_SRCS}
)
set(COMPONENT_ADD_INCLUDEDIRS 
    "include"
    "../../main")

set(COMPONENT_ADD_PRIVATE_INCLUDEDIRS 
    "src/port"
    "src/parse_and_exec/include"
    "src/services/mqtt_client/include"
    "src/common/include"
    "../../../../../common/include")

idf_component_register(SRCS "${COMPONENT_SRCS}"
                    INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
                    PRIV_INCLUDE_DIRS "${COMPONENT_ADD_PRIVATE_INCLUDEDIRS}"
                    REQUIRES mqtt
                    )
