#include "qmtpubex_parse_exec_handler.h"
#include "../parse_and_exec_helpers.h"
#include "string_handling.h"
#include "../parse_and_exec_tmp_buff.h"
#include "mqtt_service.h"


//==============================
// Public functions definition
//==============================
AT_BUFF_SIZE_T qmtpubex_test_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    handler_tmp_buff_t *handler_tmp_buff = alloc_handler_tmp_buff(arg_length);
    if (MAX_NUM_OF_MQTT_CLIENT > 1)
    {
        sprintf(handler_tmp_buff->tmp_resp_buff, "<0-%d>", 
            MAX_NUM_OF_MQTT_CLIENT - 1);
    }
    else
    {
        sprintf(handler_tmp_buff->tmp_resp_buff, "<0>");
    }
    sprintf(at_resp, 
        "+QMTPUBEX %s",
        handler_tmp_buff->tmp_resp_buff);
    //This is possible value of retain ('0' or '1')
    sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
    sprintf(at_resp, 
        "%s,<%d-%d>,<%d-%d>,\"topic\",\"message\"\r\n\r\nOK",
        handler_tmp_buff->tmp_resp_buff,
        MQTT_QOS_AT_MOST_ONCE, MQTT_QOS_EXACTLY_ONCE,
        false, true);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}


AT_BUFF_SIZE_T qmtpubex_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    long int parsed_qos;
    long int parsed_is_retain;
    const char *parsed_topic;
    const char *parsed_msg;
    INTIALIZE_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp, client_index);
    TOKENIZE_AND_ASSIGN_REQUIRED_LINT_PARAM(parsed_qos, MQTT_QOS_AT_MOST_ONCE, 
        MQTT_QOS_EXACTLY_ONCE, NULL, token, tokenize_context, 
        handler_tmp_buff, at_resp);
    TOKENIZE_AND_ASSIGN_REQUIRED_LINT_PARAM(parsed_is_retain, 0, 
        1, NULL, token, tokenize_context, handler_tmp_buff, at_resp);
    TOKENIZE_AND_ASSIGN_REQUIRED_QUOTED_STRING(parsed_topic, NULL, token,
        tokenize_context, handler_tmp_buff, at_resp);
    TOKENIZE_AND_ASSIGN_REQUIRED_QUOTED_STRING(parsed_msg, NULL, token,
        tokenize_context, handler_tmp_buff, at_resp);

    mqtt_service_pkt_status_t status = mqtt_service_publish(client_index, 
        parsed_topic, parsed_msg, (mqtt_qos_t)parsed_qos, (bool)parsed_is_retain);
    sprintf(at_resp,"OK\r\n+QMTPUBEX: %ld,%d", client_index, status);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}
