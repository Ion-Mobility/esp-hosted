#ifndef _PARSE_AND_EXEC_TYPES_H_
#define _PARSE_AND_EXEC_TYPES_H_

#include "at_cmd_stack_types.h"

/**
 * @brief Handler function that handling argument parts of AT command. 
 * In order word, handler assume that consumer knows the type of command 
 * 
 * @param arg pointer to argument portion behind the AT command. Usually start with "?", "=", "=?" [in]
 * @param arg_length size of AT command buffer [in]
 * @param at_resp ponter to null-terminated AT response [out]
 * 
 * @return size of AT response not including null character
 */
typedef AT_BUFF_SIZE_T (*command_handler_t)(const char *arg, AT_BUFF_SIZE_T arg_length,
    char *at_resp);

#endif