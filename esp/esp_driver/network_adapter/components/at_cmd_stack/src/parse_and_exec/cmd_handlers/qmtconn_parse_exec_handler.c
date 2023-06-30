#include "qmtconn_parse_exec_handler.h"
#include "common_helpers.h"
#include "../parse_and_exec_helpers.h"
#include "esp_log.h"
#include "mqtt_service.h"
#include <string.h>

#define TOKENIZE_AND_VALIDATE_UNSIGNED_LINT_PARAM(val, min, max, input_str, token, tokenize_context, resp)  do { \
    token = sys_strtok(input_str, param_delim, &tokenize_context); \
    if (token != NULL) \
    { \
        DEEP_DEBUG("Get token %s\n", token); \
        MUST_BE_CORRECT_OR_RESPOND_ERROR(!unsigned_convert_and_validate(token, \
            (unsigned long int*)&val), resp); \
        MUST_BE_CORRECT_OR_RESPOND_ERROR((val >= min) && (val <= max), resp); \
    } \
    else \
    { \
        RETURN_RESPONSE_ERROR(resp); \
    } \
} while (0)


#define INTIALIZE_WRITE_PARSE_EXEC_HANDLER(arg, arg_len, resp, client_index) \
    char *tmp_buff = sys_mem_calloc(arg_len + 1, 1); \
    memcpy(tmp_buff, arg+1, arg_len); \
    char* tokenize_context; \
    char* token; \
    int client_index; \
    TOKENIZE_AND_VALIDATE_UNSIGNED_LINT_PARAM(client_index, \
        0, MAX_NUM_OF_MQTT_CLIENT - 1, tmp_buff, token, tokenize_context, resp)

#define TOKENIZE_REQUIRED_QUOTED_STRING_AND_ASSIGN(token, token_ctx, dest, \
        tmp_buff, resp)  do { \
    token = sys_strtok(NULL, param_delim, &token_ctx); \
    if ((token == NULL) || \
        ((token[0] != '"') || (token[strlen(token) - 1] != '"'))) \
    { \
        DEEP_DEBUG("Required string token but not found token or not properly quoted!\n"); \
        sys_mem_free(tmp_buff); \
        RETURN_RESPONSE_ERROR(resp); \
    } \
    dest = &token[1]; \
    token[strlen(token) - 1] = '\0'; \
} while (0)

#define FOREACH_MQTT_CLIENT(num_of_mqtt_clients, index) \
    for (int index = 0; index < num_of_mqtt_clients; index++) \

//================================
// Private functions declaration
//================================


static int unsigned_convert_and_validate(const char* str, 
    unsigned long int* converted_number);

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
    FOREACH_MQTT_CLIENT(MAX_NUM_OF_MQTT_CLIENT, index)
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
    unsigned long int parsed_port_num;

    TOKENIZE_REQUIRED_QUOTED_STRING_AND_ASSIGN(token, tokenize_context, 
        parsed_hostname, tmp_buff, at_resp);

    TOKENIZE_AND_VALIDATE_UNSIGNED_LINT_PARAM(parsed_port_num, 1, 65535, NULL, token, 
        tokenize_context, at_resp);

    TOKENIZE_REQUIRED_QUOTED_STRING_AND_ASSIGN(token, tokenize_context, 
        parsed_client_id, tmp_buff, at_resp);
    
    token = sys_strtok(NULL, param_delim, &tokenize_context);
    if (token != NULL)
    {
        DEEP_DEBUG("+ qmtconn: have optional username and password\n"); 
        parsed_username = token;
        token = sys_strtok(NULL, param_delim, &tokenize_context);
        TOKENIZE_REQUIRED_QUOTED_STRING_AND_ASSIGN(token, tokenize_context, 
            parsed_password, tmp_buff, at_resp);

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
    sprintf(at_resp, "OK\r\n\r\n+QMTCONN: %d,%d,%d", client_index,
        connect_result, connect_ret_code);

    sys_mem_free(tmp_buff);
    return strlen(at_resp);
}

AT_BUFF_SIZE_T qmtconn_exec_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    RETURN_RESPONSE_UNSUPPORTED(at_resp);
}



static int unsigned_convert_and_validate(const char* str, 
    unsigned long int* converted_number)
{
    *converted_number = strtoul(str, NULL, 10);
    if (*converted_number != 0) return 0;

    for (int pos = 0; pos < strlen(str); pos++)
    {
        if (str[pos] != '0')
        {
            DEEP_DEBUG("original string is not properly \"0\"\n");
            return 1;
        }
    }
    return 0;
}

