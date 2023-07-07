#ifndef _PARSE_AND_EXEC_TMP_BUFF_H_H
#define _PARSE_AND_EXEC_TMP_BUFF_H_H

#include "at_cmd_stack_types.h"


typedef struct {

    // Each handler have a copy of argument to perform tokenize because
    // tokenizing on original argument buffer is not good idea
    char *cpy_of_arg;

    // Each handler also have a buffer for temporary storaged of output response
    char *tmp_resp_buff;
    
} handler_tmp_buff_t;

/**
 * @brief Allocate temporary buffer for parse and exec handler
 * 
 * @param arg_length length of argument length from caller [in]
 * 
 * @return pointer to allocated temporary buffer, NULL if it's not possible
 */
extern handler_tmp_buff_t *alloc_handler_tmp_buff(AT_BUFF_SIZE_T arg_length);

/**
 * @brief Free temporary buffer for parse and exec handler
 * 
 */
extern void free_handler_tmp_buff(handler_tmp_buff_t* tmp_buff_handle);

#endif
