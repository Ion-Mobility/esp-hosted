#include "qmtopen_parse_exec_handler.h"
#include "../parse_and_exec_types.h"
#include "../parse_and_exec_helpers.h"
#include "string_handling.h"
#include "../parse_and_exec_tmp_buff.h"
#include "mqtt_service_configs.h"
#include "mqtt_service_helpers.h"


//==============================
// Public functions definition
//==============================
AT_BUFF_SIZE_T qmtdump_read_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    for (int index = 0; index < MAX_NUM_OF_MQTT_CLIENT; index++)
    {
        print_at_mqtt_client_config(index);
    }
    RETURN_RESPONSE_OK(at_resp);
}
