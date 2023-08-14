#include "qmtrecv_parse_exec_handler.h"
#include "../parse_and_exec_helpers.h"
#include "mqtt_service.h"

#define MAX_NUM_OF_RECV_BUFFERS_IN_RESP 1

#define PREPARE_STRING_AND_APPEND_TO_RESP(str, tmp_buff, resp) do { \
    prepare_quoted_string(tmp_buff, MAX_AT_RESP_LENGTH, str); \
    strcat(resp, ","); \
    strcat(resp, tmp_buff); \
} while (0)

//================================
// Private functions declaration
//================================
static int prepare_quoted_string(char *dst_quoted_string, 
    AT_BUFF_SIZE_T dst_quoted_string_size, const char *src_string);
static bool can_append_next_recv_buff(const recv_buffer_t *recv_buff, 
    const char *resp);
static void discard_recv_buffs_until_found_valid(int client_index);
static int discard_next_recv_buff_if_invalid(int client_index);

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
        discard_recv_buffs_until_found_valid(client_idx);
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
    discard_recv_buffs_until_found_valid(client_index);
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
        if (!can_append_next_recv_buff(&read_recv_buff, at_resp))
        {
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

static int prepare_quoted_string(char *dst_quoted_string, 
    AT_BUFF_SIZE_T dst_quoted_string_size, const char *src_string)
{
    memset(dst_quoted_string, 0, dst_quoted_string_size); \
    sprintf(dst_quoted_string, "%s", src_string); \
    if (add_escape_characters_in_string(dst_quoted_string, 
        dst_quoted_string_size))
        return -1;
    if (add_quote_characters_to_string(dst_quoted_string,
        dst_quoted_string_size))
        return -1;
    return 0;
}

static bool can_append_next_recv_buff(const recv_buffer_t *recv_buff, 
    const char *resp)
{
    static char topic_string_in_at_cmd[MAX_AT_RESP_LENGTH];
    static char msg_string_in_at_cmd[MAX_AT_RESP_LENGTH];
    AT_BUFF_SIZE_T required_comma_and_quote_len = 2 * strlen(",") + 4 * strlen("\"");
    if (prepare_quoted_string(topic_string_in_at_cmd, MAX_AT_RESP_LENGTH,
        recv_buff->topic))
    {
        AT_STACK_LOGD("Cannot append this receive buffer because possible topic length is longer than maximum response");
        return false;
    }
    if (prepare_quoted_string(msg_string_in_at_cmd, MAX_AT_RESP_LENGTH,
        recv_buff->msg))
    {
        AT_STACK_LOGD("Cannot append this receive buffer because possible message length is longer than maximum response");
        return false;
    }
    AT_BUFF_SIZE_T calculated_length_after_appended = 
        strlen(topic_string_in_at_cmd) + strlen(msg_string_in_at_cmd) +
        required_comma_and_quote_len;
    if (calculated_length_after_appended > MAX_AT_RESP_LENGTH)
    {
        AT_STACK_LOGD("Cannot append this receive buffer because it will make response longer than maximum size");
        return false;
    }
    return true;
}

static void discard_recv_buffs_until_found_valid(int client_index)
{
    while(!(discard_next_recv_buff_if_invalid(client_index)));
}

static int discard_next_recv_buff_if_invalid(int client_index)
{
    static char testing_resp[MAX_AT_RESP_LENGTH];
    recv_buffer_t recv_buff;
    memset(testing_resp, 0, MAX_AT_RESP_LENGTH);
    sprintf(testing_resp,"+QMTRECV:\r\n%u", client_index);
    mqtt_service_status_t err =
        mqtt_service_read_current_filled_recv_buff(client_index, &recv_buff);
    if ((err) || (recv_buff.msg == NULL) || 
        (recv_buff.topic == NULL))
        return -1;
    if (!can_append_next_recv_buff(&recv_buff, testing_resp))
    {
        mqtt_service_clear_current_filled_recv_buff(client_index);
        return 0;
    }
    return -1;
}
