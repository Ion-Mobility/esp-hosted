#include "qmtrecv_parse_exec_handler.h"
#include "../parse_and_exec_helpers.h"
#include "mqtt_service.h"

#define PREPARE_STRING_AND_APPEND_TO_RESP(str, tmp_buff, resp) do { \
    memset(handler_tmp_buff->tmp_resp_buff, 0, MAX_AT_RESP_LENGTH); \
    sprintf(tmp_buff, "%s", str); \
    add_escape_characters_in_string(tmp_buff); \
    add_quote_characters_to_string(tmp_buff); \
    strcat(resp, ","); \
    strcat(resp, tmp_buff); \
} while (0)

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
        if (service_status == MQTT_SERVICE_STATUS_ERROR)
        {
            sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
            sprintf(at_resp, "%s,NO_CONN",handler_tmp_buff->tmp_resp_buff);
            continue;
        }

        FOREACH_BUFFER_IN_RECV_BUFF_GROUP(buff_idx)
        {
            if (buff_idx < get_num_of_filled_recv_buffs)
            {
                sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
                sprintf(at_resp, "%s,1",handler_tmp_buff->tmp_resp_buff);
            }
            else
            {
                sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
                sprintf(at_resp, "%s,0",handler_tmp_buff->tmp_resp_buff);
            }
        }
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
    while (1)
    {
        recv_buffer_t read_recv_buff;
        mqtt_service_status_t read_buff_status = 
            mqtt_service_read_current_filled_recv_buff(client_index, &read_recv_buff);
        if ((read_buff_status != MQTT_SERVICE_STATUS_OK) || 
            (read_recv_buff.topic == NULL) || (read_recv_buff.msg == NULL))
            break;
        AT_BUFF_SIZE_T appending_string_length = 2 * strlen(",") + 4 * strlen("\"") + 
            read_recv_buff.topic_len + read_recv_buff.msg_len;
        AT_BUFF_SIZE_T calculated_length_after_appended = 
            strlen(at_resp) + appending_string_length;
        if (calculated_length_after_appended > MAX_AT_RESP_LENGTH)
        {
            AT_STACK_LOGD("Stop append new data because next recv buff will cause response bigger than max length");
            break;
        }
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
        if (mqtt_service_clear_current_filled_recv_buff(client_index) != 
            MQTT_SERVICE_STATUS_OK)
            break;
    }
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}
