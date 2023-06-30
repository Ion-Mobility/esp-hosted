#ifndef _AT_CMD_APP_HELPER_H_
#define _AT_CMD_APP_HELPER_H_

#include "at_cmd_stack_types.h"

/**
 * @brief Convert normal AT NULL terminated command or response like "AT+Hello\0" 
 * or "OK" to Quectel AT command & response
 * 
 * @param at_normal_message pointer to null-termined AT command or response [in]
 * @param at_normal_message_len length of null-termined AT command or response, not including NULL terminator [in]
 * @param at_quectel_message pointer to allocated Quectel AT command or response [out]
 * 
 * @retval size of AT Quectel command or response, including <CR><LF>
 * @retval 0 if arguments are wrong: NULL pointer or length of string is 0)
 */
extern AT_BUFF_SIZE_T AT_NormalString_To_QuecTelString(
    const char* at_normal_message, AT_BUFF_SIZE_T normal_string_length,
    char* quectel_string
    );
/**
 * @brief Convert Quectel AT command & responsee like "[<CR><LF>]OK<CR><LF>" 
 * to normal AT command & response string like "OK\0"
 * 
 * @param at_quectel_message pointer to Quectel AT command or response. Must 
 * have valid Quectel format [<CR><LF>]<at_msg><CR><LF> [in]
 * @param at_quectel_message_len length of Quectel AT command or response, including <CR><LF> [in]
 * @param at_normal_message pointer to allocated null-termined AT command or response [out]
 * 
 * @retval length of AT Quectel command or response, not include NULL terminator character
 * @retval 0 if arguments are wrong: NULL pointer or (at_quectel_message_len <= 2)
 * @retval 0 if input 'at_quectel_message' does not have valid Quectel format 
 * [<CR><LF>]<at_msg><CR><LF>
 */
extern AT_BUFF_SIZE_T AT_QuecTelString_To_NormalString(
    const char* at_quectel_message, AT_BUFF_SIZE_T at_quectel_message_len,
    char* at_normal_message);

#endif