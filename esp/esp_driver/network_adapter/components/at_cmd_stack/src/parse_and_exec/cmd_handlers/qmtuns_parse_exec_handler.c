#include "qmtuns_parse_exec_handler.h"
#include "../parse_and_exec_helpers.h"
#include "mqtt_service.h"


//==============================
// Public functions definition
//==============================
AT_BUFF_SIZE_T qmtuns_test_cmd_parse_exec_handler(const char *arg, 
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
        "+QMTSUB %s,\"topic\"\r\n\r\nOK",
        handler_tmp_buff->tmp_resp_buff);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}


AT_BUFF_SIZE_T qmtuns_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    const char *parsed_topic;
    INTIALIZE_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp, client_index);
    
    TOKENIZE_AND_ASSIGN_REQUIRED_QUOTED_STRING(parsed_topic, NULL, token,
        tokenize_context, handler_tmp_buff, at_resp);

    mqtt_service_pkt_status_t status = mqtt_service_unsubscribe(
        client_index, parsed_topic);
    
    sprintf(at_resp,"OK\r\n+QMTUNS: %ld,%d", client_index, status);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}
