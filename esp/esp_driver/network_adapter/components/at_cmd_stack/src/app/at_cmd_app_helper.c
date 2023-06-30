#include "at_cmd_app_helper.h"
#include "common_helpers.h"
#include <string.h>

const char quectel_terminate_pair[] = {'\r', '\n'};

//================================
// Public functions definition
//================================
AT_BUFF_SIZE_T AT_NormalString_To_QuecTelString(
    const char* at_normal_message, AT_BUFF_SIZE_T normal_string_length, char* quectel_string
    )
{
    VALIDATE_ARGS(at_normal_message, 0);
    VALIDATE_ARGS(normal_string_length, 0);
    VALIDATE_ARGS(quectel_string, 0);
    AT_BUFF_SIZE_T total_length = 0;
    memcpy(quectel_string, quectel_terminate_pair, sizeof(quectel_terminate_pair));
    total_length += sizeof(quectel_terminate_pair);

    memcpy(quectel_string + total_length, at_normal_message, normal_string_length);
    total_length += normal_string_length;

    memcpy(quectel_string + total_length, quectel_terminate_pair, sizeof(quectel_terminate_pair));
    total_length += sizeof(quectel_terminate_pair);

    return total_length;

}

AT_BUFF_SIZE_T AT_QuecTelString_To_NormalString(
    const char* at_quectel_message, AT_BUFF_SIZE_T at_quectel_message_len,
    char* at_normal_message)
{
    VALIDATE_ARGS(at_quectel_message, 0);
    VALIDATE_ARGS(at_quectel_message_len > sizeof(quectel_terminate_pair), 0);
    VALIDATE_ARGS(at_normal_message, 0);
    AT_BUFF_SIZE_T start_character_pos = 0, end_character_pos = at_quectel_message_len-2;
    VALIDATE_ARGS(!memcmp(&at_quectel_message[end_character_pos], 
        quectel_terminate_pair, sizeof(quectel_terminate_pair)), 0);

    if (!memcmp(at_quectel_message, quectel_terminate_pair, 
        sizeof(quectel_terminate_pair)))
    {
        start_character_pos = 2;
    }
    
    end_character_pos--;
    AT_BUFF_SIZE_T normal_message_length = end_character_pos - start_character_pos + 1; 
    memcpy(at_normal_message, &at_quectel_message[start_character_pos], normal_message_length);
    at_normal_message[normal_message_length] = '\0';
    return normal_message_length;
}
