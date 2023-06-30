#ifndef _PARSE_EXEC_CORE_H_
#define _PARSE_EXEC_CORE_H_

#include "at_cmd_stack_types.h"

/**
 * @brief Parse AT command and execute it, then return response
 * 
 * @param at_cmd pointer to AT command buffer [in]
 * @param cmd_total_length length of AT command buffer, not include NULL-terminator [in]
 * @param at_resp pointer to AT response [out]
 * @retval length of AT response, not include NULL-terminator 
 * @retval 0 if one of arguments is invalid
 */
extern AT_BUFF_SIZE_T parse_and_exec_at_cmd(const char *at_cmd, const AT_BUFF_SIZE_T 
    cmd_total_length, char* at_resp);


#endif