#include "qmtrecv_parse_exec_handler.h"
#include "../parse_and_exec_helpers.h"
#include "mqtt_service.h"

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
    recv_buffer_group_t read_recv_buff_group = {0};
    mqtt_service_status_t service_status;
    FOREACH_CLIENT_IN_MQTT_SERVICE(client_idx)
    {
        service_status = mqtt_service_get_recv_buff_group_status(client_idx,
            &read_recv_buff_group);

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
            recv_buffer_t read_recv_buff = read_recv_buff_group.buff[buff_idx];
            sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
            sprintf(at_resp, "%s,%d",handler_tmp_buff->tmp_resp_buff, read_recv_buff.is_unread);
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

    recv_buffer_group_t read_recv_buff_group;
    mqtt_service_status_t service_status = mqtt_service_get_recv_buff_group(
        client_index, &read_recv_buff_group);
    if (service_status == MQTT_SERVICE_STATUS_ERROR)
    {
        RETURN_RESPONSE_ERROR(at_resp);
    }
    service_status = mqtt_service_clear_recv_buff_group(client_index);
    sprintf(at_resp,"+QMTRECV:\r\n%ld", client_index);
    FOREACH_BUFFER_IN_RECV_BUFF_GROUP(buff_idx)
    {
        recv_buffer_t *buff_to_resp = 
            &read_recv_buff_group.buff[buff_idx];
        if (!buff_to_resp->is_unread)
            break;

        sprintf(handler_tmp_buff->tmp_resp_buff,"%s", at_resp);
        AT_STACK_LOGD("copy at resp to tmp_resp_buff. topic = %p, msg = %p",
            buff_to_resp->topic,
            buff_to_resp->msg);
        sprintf(at_resp,"%s,\"%s\",\"%s\"", handler_tmp_buff->tmp_resp_buff, 
            buff_to_resp->topic,
            buff_to_resp->msg);
        AT_STACK_LOGD("at resp appends! Response = '%s'", at_resp);
        sys_mem_free(buff_to_resp->topic);
        sys_mem_free(buff_to_resp->msg);
        AT_STACK_LOGD("done free topic and msg buffer");
    }
    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}
