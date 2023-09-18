#include "qmtrecv_parse_exec_handler.h"
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
AT_BUFF_SIZE_T qmtrecv_test_cmd_parse_exec_handler(const char *arg, 
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
        "+QMTRECV %s\r\n\r\nOK",
        handler_tmp_buff->tmp_resp_buff);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}


AT_BUFF_SIZE_T qmtrecv_read_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    handler_tmp_buff_t *handler_tmp_buff = alloc_handler_tmp_buff(arg_length);
    sprintf(at_resp, "+QMTRECV\r\n");
    FOREACH_CLIENT_IN_MQTT_SERVICE(client_idx)
    {
        unsigned int get_num_of_filled_recv_buffs;
        mqtt_service_status_t service_status = 
            mqtt_service_get_num_of_filled_recv_buffs(client_idx,
            &get_num_of_filled_recv_buffs);

        sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
        sprintf(at_resp, "%s%d",handler_tmp_buff->tmp_resp_buff, client_idx);
        sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
        sprintf(at_resp, "%s,%d",handler_tmp_buff->tmp_resp_buff, 
            get_num_of_filled_recv_buffs? 1 : 0);
        sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
        sprintf(at_resp, "%s\r\n",handler_tmp_buff->tmp_resp_buff);
    }
    sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
    sprintf(at_resp, "%s\r\nOK",handler_tmp_buff->tmp_resp_buff);
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}

AT_BUFF_SIZE_T qmtrecv_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    INTIALIZE_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp, client_index);
    sprintf(at_resp,"+QMTRECV:\r\n%ld", client_index);
    // Although at this time, +QMTRECV only appends one buffer, there is no
    // telling that this requirement can change in the future, so still keep it
    // in the loop
    for (unsigned int num_of_append_buff = 0; 
        num_of_append_buff < MAX_NUM_OF_RECV_BUFFERS_IN_RESP;
        num_of_append_buff++)
    {
        recv_buffer_t read_recv_buff;
        mqtt_service_status_t read_buff_status = 
            mqtt_service_read_current_filled_recv_buff(client_index, &read_recv_buff);
        if ((read_buff_status != MQTT_SERVICE_STATUS_OK) || 
            (read_recv_buff.topic == NULL) || (read_recv_buff.msg == NULL))
            break;
        AT_STACK_LOGD("copy at resp to tmp_resp_buff. topic = %p, msg = %p",
            read_recv_buff.topic,
            read_recv_buff.msg);
        PREPARE_STRING_AND_APPEND_TO_RESP(read_recv_buff.topic,
            handler_tmp_buff->tmp_resp_buff, at_resp);
        PREPARE_STRING_AND_APPEND_TO_RESP(read_recv_buff.msg,
            handler_tmp_buff->tmp_resp_buff, at_resp);
        AT_STACK_LOGD("at resp appends! Response = '%s'", at_resp);
        sys_mem_free(read_recv_buff.topic);
        sys_mem_free(read_recv_buff.msg);
        AT_STACK_LOGD("done free topic and msg buffer");
        if (mqtt_service_clear_current_filled_recv_buff(client_index, 0) != 
            MQTT_SERVICE_STATUS_OK)
            break;
    }
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}


