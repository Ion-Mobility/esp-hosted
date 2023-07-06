#include "qmtconn_parse_exec_handler.h"
#include "common_helpers.h"
#include "../parse_and_exec_helpers.h"
#include "esp_log.h"
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
    char *tmp_buff = sys_mem_calloc(MAX_AT_RESP_LENGTH + 1, 1);
    if (MAX_NUM_OF_MQTT_CLIENT > 1)
    {
        sprintf(tmp_buff, "<0-%d>", MAX_NUM_OF_MQTT_CLIENT - 1);
    }
    else
    {
        sprintf(tmp_buff, "<0>");
    }
    sprintf(at_resp, 
        "+QMTCONN %s,\"hostname\",\"port\",\"clientid\"[,\"username\",\"password\"]\r\n\r\nOK",
        tmp_buff);
    sys_mem_free(tmp_buff);
    return strlen(at_resp);
}

AT_BUFF_SIZE_T qmtconn_read_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    char *tmp_buff = sys_mem_calloc(MAX_AT_RESP_LENGTH + 1, 1);
    sprintf(tmp_buff, "+QMTCONN:");
    memcpy(at_resp, tmp_buff, strlen(tmp_buff) + 1);
    FOREACH_CLIENT_IN_MQTT_SERVICE(index)
    {
        char prepend_char = (index == 0) ? ' ' : ',';
        sprintf(tmp_buff, "%s%c%d", at_resp, prepend_char,
            mqtt_service_get_connection_status(index));
        memcpy(at_resp, tmp_buff, strlen(tmp_buff) + 1);
    }
    sprintf(tmp_buff, "%s\r\nOK", at_resp);
    memcpy(at_resp, tmp_buff, strlen(tmp_buff) + 1);
    sys_mem_free(tmp_buff);
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
        token, tokenize_context, tmp_buff, at_resp);

    TOKENIZE_AND_ASSIGN_REQUIRED_LINT_PARAM(parsed_port_num, 1, 65535, NULL,
        token, tokenize_context, tmp_buff, at_resp);

    TOKENIZE_AND_ASSIGN_REQUIRED_QUOTED_STRING(parsed_client_id, NULL,
        token, tokenize_context, tmp_buff, at_resp);
    
    char *possible_parsed_username = NULL;
    TOKENIZE_AND_ASSIGN_OPTIONAL_QUOTED_STRING(possible_parsed_username, 
        NULL, token, tokenize_context, tmp_buff, at_resp);
    if (possible_parsed_username != NULL)
    {
        parsed_username = possible_parsed_username;
        DEEP_DEBUG("+ qmtconn: have optional username and password\n"); 
        token = sys_strtok(NULL, param_delim, &tokenize_context);
        TOKENIZE_AND_ASSIGN_REQUIRED_QUOTED_STRING(parsed_password, NULL,
            token, tokenize_context, tmp_buff, at_resp);

    }
    else
    {
        DEEP_DEBUG("+ qmtconn: don't have optional username and password\n");
        parsed_username = NULL;
        parsed_password = NULL;
    }

    esp_mqtt_connect_return_code_t connect_ret_code;
    mqtt_service_pkt_status_t connect_result = mqtt_service_connect(client_index,
        parsed_client_id, parsed_hostname, parsed_username, parsed_password,
        (uint16_t)parsed_port_num, &connect_ret_code);
    sprintf(at_resp, "OK\r\n\r\n+QMTCONN: %lu,%d,%d", client_index,
        connect_result, connect_ret_code);

    sys_mem_free(tmp_buff);
    return strlen(at_resp);
}

AT_BUFF_SIZE_T qmtconn_exec_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    RETURN_RESPONSE_UNSUPPORTED(at_resp);
}
