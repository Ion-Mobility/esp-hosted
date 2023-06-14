#include "at-lib.h"
#include <string.h>
#include "stdio.h"

#ifndef SANITY_CHECK
#define SANITY_CHECK(cond, ret_val)  do { \
    if (!(cond)) { \
    printf("File %s line %d has error!\n",__FILE__, __LINE__); \
    return ret_val; \
    }\
} while(0)
#endif

const char quectel_terminate_pair[] = {0x0D, 0x0A};

char handling_cmd_arg[MAX_AT_BUFFER_SIZE];

/**
 * @brief Get the proper index in supported AT commands list for specified AT command
 * 
 * @param at_cmd pointer to AT command string buffer like 'AT+HELLO?' [in]
 * @param cmd_total_length length of AT command string [in]
 * @retval index in supported AT commands list 
 * @retval -1 if not found
 */
static int get_index_in_supported_at_cmds_list(const char *at_cmd, const BUFF_SIZE_T cmd_total_length)
{
    int ret_index = -1;
    for (int index = 0; index < MAX_NUMBER_OF_SUPPORTED_AT_COMMANDS; index++)
    {
        const char *target_cmd_to_compare = supported_at_cmds_list[index].at_command;
        BUFF_SIZE_T length_of_target_cmd = strlen(target_cmd_to_compare);
        if (!strncmp(at_cmd, target_cmd_to_compare, length_of_target_cmd))
        {
            ret_index = index;
            break;
        }
    }
    return ret_index;
}


BUFF_SIZE_T ATSlave_HandlingCommand(const char *at_cmd, const BUFF_SIZE_T cmd_total_length, char* at_resp)
{
    SANITY_CHECK(((at_cmd) && (cmd_total_length > 0)), 0);
    BUFF_SIZE_T ret_resp_length = 0;
    int index_of_this_cmd = get_index_in_supported_at_cmds_list(at_cmd, cmd_total_length);
    if (index_of_this_cmd != -1)
    {
        command_handler_t found_handler = supported_at_cmds_list[index_of_this_cmd].command_handler;
        BUFF_SIZE_T cmd_only_length = strlen(supported_at_cmds_list[index_of_this_cmd].at_command);
        BUFF_SIZE_T arg_size = cmd_total_length - cmd_only_length;
        const char* start_of_cmd_arg = at_cmd + cmd_only_length;
        memset(handling_cmd_arg, 0, MAX_AT_BUFFER_SIZE);
        memcpy(handling_cmd_arg, start_of_cmd_arg, arg_size);
        if (found_handler)
        {
            ret_resp_length = found_handler(handling_cmd_arg, arg_size, at_resp);
        }
    }
    return ret_resp_length;
}

BUFF_SIZE_T AT_NormalString_To_QuecTelString(
    const char* at_normal_message, BUFF_SIZE_T normal_string_length, char* quectel_string
    )
{
    BUFF_SIZE_T total_length = 0;
    memcpy(quectel_string, quectel_terminate_pair, sizeof(quectel_terminate_pair));
    total_length += sizeof(quectel_terminate_pair);

    memcpy(quectel_string + total_length, at_normal_message, normal_string_length);
    total_length += normal_string_length;

    memcpy(quectel_string + total_length, quectel_terminate_pair, sizeof(quectel_terminate_pair));
    total_length += sizeof(quectel_terminate_pair);

    return total_length;

}

BUFF_SIZE_T AT_QuecTelString_To_NormalString(const char* at_quectel_message, BUFF_SIZE_T at_quectel_message_len, char* at_normal_message)
{
    BUFF_SIZE_T start_character_pos = 0, end_character_pos = at_quectel_message_len-2;
    BUFF_SIZE_T normal_message_length;
    if (!memcmp(at_quectel_message, quectel_terminate_pair, sizeof(quectel_terminate_pair)))
    {
        start_character_pos = 2;
    }

    while (1)
    {
        if (!memcmp(&at_quectel_message[end_character_pos], quectel_terminate_pair, sizeof(quectel_terminate_pair)))
        {
            end_character_pos--;
            break;
        }
        end_character_pos--;
    }
    normal_message_length = end_character_pos - start_character_pos + 1; 
    memcpy(at_normal_message, &at_quectel_message[start_character_pos], normal_message_length);
    at_normal_message[normal_message_length] = '\0';
    return normal_message_length;
}