#include "qmtdisc_parse_exec_handler.h"
#include "common_helpers.h"
#include "../parse_and_exec_helpers.h"
#include "mqtt_service.h"
#include <string.h>

//================================
// Private functions declaration
//================================



//==============================
// Public functions definition
//==============================
AT_BUFF_SIZE_T qmtdisc_test_cmd_parse_exec_handler(const char *arg, 
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
        "+QMTCONN %s\r\nOK",
        handler_tmp_buff->tmp_resp_buff);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}

AT_BUFF_SIZE_T qmtdisc_read_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    RETURN_RESPONSE_UNSUPPORTED(at_resp);
}

AT_BUFF_SIZE_T qmtdisc_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    INTIALIZE_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp, client_index);

    mqtt_service_status_t disconnect_result = 
        mqtt_service_disconnect(client_index);

    sprintf(at_resp, "OK\r\n+QMTCONN: %lu,%d", client_index,
        disconnect_result);

    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}

AT_BUFF_SIZE_T qmtdisc_exec_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    RETURN_RESPONSE_UNSUPPORTED(at_resp);
}
