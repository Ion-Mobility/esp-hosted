#ifndef _AT_LIB_H
#define _AT_LIB_H

#define MAX_AT_BUFFER_SIZE (200 * sizeof(char))
#define MAX_NUMBER_OF_SUPPORTED_AT_COMMANDS 1


#ifndef BUFF_SIZE_T
#define BUFF_SIZE_T unsigned int
#endif // BUFF_SIZE_T

#ifndef TIME_SIZE_T
#define TIME_SIZE_T unsigned int
#endif // BUFF_SIZE_T


/**
 * @brief Handler function that handling argument parts of AT command. 
 * In order word, handler assume that consumer knows the type of command 
 * 
 * @param arg pointer to argument portion behind the AT command. Usually start with "?", "=", "=?" [in]
 * @param arg_size size of AT command buffer [in]
 * @param at_resp ponter to null-terminated AT response [out]
 * 
 * @return size of AT response not including null character
 */
typedef BUFF_SIZE_T (*command_handler_t)(const char *arg, BUFF_SIZE_T arg_size, char *at_resp);


typedef struct {
    char *at_command;
    command_handler_t command_handler;
} at_command_list_entry_t;

extern at_command_list_entry_t supported_at_cmds_list[MAX_NUMBER_OF_SUPPORTED_AT_COMMANDS];


/**
 * @brief Handling an AT command then get the response
 * 
 * @param at_cmd pointer to AT command string buffer like 'AT+HELLO?' [in]
 * @param cmd_total_length length of AT command string [in]
 * @param at_resp  pointer to already-allocated AT response buffer [out]
 * @return length of AT response 
 */
extern BUFF_SIZE_T ATSlave_HandlingCommand(const char *at_cmd, const BUFF_SIZE_T cmd_total_length, char* at_resp);


/**
 * @brief Convert normal AT NULL terminated command or response like "AT+Hello\0" 
 * or "OK" to Quectel AT command & responsee
 * 
 * @param at_normal_message pointer to null-termined AT command or reponse [in]
 * @param at_normal_message_len length of  null-termined AT command or reponse [in]
 * @param at_quectel_message [out]
 * 
 * @return size of AT Quectel command or response
 */
extern BUFF_SIZE_T AT_NormalString_To_QuecTelString(const char* at_normal_message, BUFF_SIZE_T normal_string_length, char* quectel_string);


/**
 * @brief Convert Quectel AT command & responsee like "<Lf><CR>OK<LF><CR>" 
 * to normal AT command & response string like "OK\0"
 * 
 * @param at_quectel_message pointer to Quectel AT command or response. Can begin with <LF><CR> [in]
 * @param at_quectel_message_len length of Quectel AT command or response, including <LF><CR> [in]
 * @param at_normal_message pointer to allocated null-termined AT command or response [out]
 * 
 * @return length of AT Quectel command or response, not include NULL terminator character
 */
extern BUFF_SIZE_T AT_QuecTelString_To_NormalString(const char* at_quectel_message, BUFF_SIZE_T at_quectel_message_len, char* at_normal_message);

#endif