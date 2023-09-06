#include "at_cmd_stack_types.h"
#include "mqtt_service.h"
#include "common_helpers.h"
#include "string_handling.h"

//================================
// Private functions declaration
//================================
static int prepare_quoted_string(char *dst_quoted_string, 
    AT_BUFF_SIZE_T dst_quoted_string_size, const char *src_string);
static bool can_append_next_recv_buff(const recv_buffer_t *recv_buff, 
    const char *resp, AT_BUFF_SIZE_T resp_max_len);

//==============================
// Public functions definition
//==============================
int can_app_accept_incoming_recv_buff(int client_index, 
    const recv_buffer_t *recv_buff)
{
    static char testing_resp[MAX_AT_RESP_LENGTH];
    memset(testing_resp, 0, MAX_AT_RESP_LENGTH);
    sprintf(testing_resp,"+QMTRECV:\r\n%u", client_index);
    if (!can_append_next_recv_buff(recv_buff, testing_resp, MAX_AT_RESP_LENGTH))
    {
        AT_STACK_LOGI("AT command stack app CANNOT accept this recv buff!");
        return 0;
    }
    AT_STACK_LOGI("AT command stack app CAN accept this recv buff!");
    return 1;
}

//===============================
// Private functions definition
//===============================
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
    const char *resp, AT_BUFF_SIZE_T resp_max_len)
{
    static char topic_string_in_at_resp[MAX_AT_RESP_LENGTH];
    static char msg_string_in_at_resp[MAX_AT_RESP_LENGTH];
    AT_BUFF_SIZE_T required_comma_and_quote_len = 2 * strlen(",") + 4 * strlen("\"");
    if (prepare_quoted_string(topic_string_in_at_resp, MAX_AT_RESP_LENGTH,
        recv_buff->topic))
    {
        AT_STACK_LOGD("Cannot append this receive buffer because possible topic length is longer than maximum response");
        return false;
    }
    if (prepare_quoted_string(msg_string_in_at_resp, MAX_AT_RESP_LENGTH,
        recv_buff->msg))
    {
        AT_STACK_LOGD("Cannot append this receive buffer because possible message length is longer than maximum response");
        return false;
    }
    AT_STACK_LOGD("quoted topic is '%s'", topic_string_in_at_resp);
    AT_STACK_LOGD("quoted message is '%s'", msg_string_in_at_resp);

    AT_BUFF_SIZE_T calculated_length_after_appended = 
        strlen(resp)+ strlen(topic_string_in_at_resp) + strlen(msg_string_in_at_resp) +
        required_comma_and_quote_len;
    AT_STACK_LOGD("Calculated length after append recv buff to response: %u bytes, compare to max allowed response length %u bytes",
        calculated_length_after_appended, resp_max_len);

    if (calculated_length_after_appended > resp_max_len)
    {
        AT_STACK_LOGE("Cannot append this receive buffer because it will make response longer than maximum size");
        return false;
    }
    return true;
}
