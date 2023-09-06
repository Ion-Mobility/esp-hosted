#include "qmtconn_parse_exec_handler.h"
#include "common_helpers.h"
#include "../parse_and_exec_helpers.h"
#include "string_handling.h"
#include "../parse_and_exec_tmp_buff.h"
#include "mqtt_service.h"
#include <string.h>

//================================
// Private functions declaration
//================================



//==============================
// Public functions definition
//==============================
AT_BUFF_SIZE_T qmtconn_test_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    handler_tmp_buff_t *handler_tmp_buff = 
        alloc_handler_tmp_buff(arg_length);
    if (MAX_NUM_OF_MQTT_CLIENT > 1)
    {
        sprintf(handler_tmp_buff->tmp_resp_buff, "<0-%d>", MAX_NUM_OF_MQTT_CLIENT - 1);
    }
    else
    {
        sprintf(handler_tmp_buff->tmp_resp_buff, "<0>");
    }
    sprintf(at_resp, 
        "+QMTCONN %s,\"hostname\",\"port\",\"clientid\"[,\"username\",\"password\"]\r\n\r\nOK",
        handler_tmp_buff->tmp_resp_buff);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}

AT_BUFF_SIZE_T qmtconn_read_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    handler_tmp_buff_t *handler_tmp_buff = alloc_handler_tmp_buff(arg_length);
    sprintf(handler_tmp_buff->tmp_resp_buff, "+QMTCONN:");
    memcpy(at_resp, handler_tmp_buff->tmp_resp_buff, 
        strlen(handler_tmp_buff->tmp_resp_buff) + 1);
    FOREACH_CLIENT_IN_MQTT_SERVICE(index)
    {
        char prepend_char = (index == 0) ? ' ' : ',';
        sprintf(handler_tmp_buff->tmp_resp_buff, "%s%c%d", at_resp, prepend_char,
            mqtt_service_get_connection_status(index));
        memcpy(at_resp, handler_tmp_buff->tmp_resp_buff, 
            strlen(handler_tmp_buff->tmp_resp_buff) + 1);
    }
    sprintf(handler_tmp_buff->tmp_resp_buff, "%s\r\nOK", at_resp);
    memcpy(at_resp, handler_tmp_buff->tmp_resp_buff, 
        strlen(handler_tmp_buff->tmp_resp_buff) + 1);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}

AT_BUFF_SIZE_T qmtconn_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    INTIALIZE_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp, client_index);
    const char *parsed_hostname;
    const char *parsed_client_id;
    const char *parsed_username;
    const char *parsed_password;
    long int parsed_port_num;

    TOKENIZE_AND_ASSIGN_REQUIRED_QUOTED_STRING(parsed_hostname, NULL, 
        token, tokenize_context, handler_tmp_buff, at_resp);

    TOKENIZE_AND_ASSIGN_REQUIRED_LINT_PARAM(parsed_port_num, 1, 65535, NULL,
        token, tokenize_context, handler_tmp_buff, at_resp);

    TOKENIZE_AND_ASSIGN_REQUIRED_QUOTED_STRING(parsed_client_id, NULL,
        token, tokenize_context, handler_tmp_buff, at_resp);
    
    char *possible_parsed_username = NULL;
    TOKENIZE_AND_ASSIGN_OPTIONAL_QUOTED_STRING(possible_parsed_username, 
        NULL, token, tokenize_context, handler_tmp_buff, at_resp);
    if (possible_parsed_username != NULL)
    {
        parsed_username = possible_parsed_username;
        AT_STACK_LOGI("+ qmtconn: have optional username and password"); 
        TOKENIZE_AND_ASSIGN_REQUIRED_QUOTED_STRING(parsed_password, NULL,
            token, tokenize_context, handler_tmp_buff, at_resp);

    }
    else
    {
        AT_STACK_LOGI("+ qmtconn: don't have optional username and password");
        parsed_username = NULL;
        parsed_password = NULL;
    }

    esp_mqtt_connect_return_code_t connect_ret_code;
    mqtt_service_pkt_status_t connect_result = mqtt_service_connect(client_index,
        parsed_client_id, parsed_hostname, parsed_username, parsed_password,
        (uint16_t)parsed_port_num, &connect_ret_code);
    sprintf(at_resp, "OK\r\n\r\n+QMTCONN: %lu,%d,%d", client_index,
        connect_result, connect_ret_code);

    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}

AT_BUFF_SIZE_T qmtconn_exec_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    RETURN_RESPONSE_UNSUPPORTED(at_resp);
}
