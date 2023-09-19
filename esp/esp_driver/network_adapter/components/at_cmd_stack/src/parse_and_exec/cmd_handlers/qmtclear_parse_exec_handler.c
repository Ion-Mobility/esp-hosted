#include "qmtclear_parse_exec_handler.h"
#include "../parse_and_exec_helpers.h"
#include "string_handling.h"
#include "../parse_and_exec_tmp_buff.h"
#include "mqtt_service.h"

#define MAX_NUM_OF_RECV_BUFFERS_IN_RESP 1

#define PREPARE_STRING_AND_APPEND_TO_RESP(str, tmp_buff, resp) do { \
    memset(tmp_buff, 0, MAX_AT_RESP_LENGTH); \
    sprintf(tmp_buff, "%s", str); \
    add_escape_characters_in_string(tmp_buff, \
        MAX_AT_RESP_LENGTH); \
    add_quote_characters_to_string(tmp_buff, \
        MAX_AT_RESP_LENGTH); \
    strcat(resp, ","); \
    strcat(resp, tmp_buff); \
} while (0)

//================================
// Private functions declaration
//================================

//==============================
// Public functions definition
//==============================
AT_BUFF_SIZE_T qmtclear_test_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    handler_tmp_buff_t *handler_tmp_buff = alloc_handler_tmp_buff(arg_length);
    if (MAX_NUM_OF_MQTT_CLIENT > 1)
    {
        sprintf(handler_tmp_buff->tmp_resp_buff, "<0-%d>", MAX_NUM_OF_MQTT_CLIENT - 1);
    }
    else
    {
        sprintf(handler_tmp_buff->tmp_resp_buff, "<0>");
    }
    sprintf(at_resp, 
        "+QMTCLEAR %s,<options>\r\n\r\nOK",
        handler_tmp_buff->tmp_resp_buff);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}


AT_BUFF_SIZE_T qmtclear_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    long int clear_unread;
    int cmd_status = 0;
    INTIALIZE_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp, client_index);
    

    TOKENIZE_AND_ASSIGN_REQUIRED_LINT_PARAM(clear_unread, 0, 1, NULL,
        token, tokenize_context, handler_tmp_buff, at_resp);

    mqtt_service_status_t status = mqtt_service_clear_current_filled_recv_buff(
        client_index, (int) clear_unread); 
    
    if (status != MQTT_SERVICE_STATUS_OK)
        cmd_status = 1;
        
    sprintf(at_resp,"+QMTCLEAR:\r\n%ld,%d", client_index, cmd_status);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}


