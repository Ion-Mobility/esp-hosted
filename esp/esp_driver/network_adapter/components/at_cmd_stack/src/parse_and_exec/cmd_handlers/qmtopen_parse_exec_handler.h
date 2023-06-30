#ifndef _QMTOPEN_PARSE_EXEC_HANDLER_H_
#define _QMTOPEN_PARSE_EXEC_HANDLER_H_

#include "at_cmd_stack_types.h"

extern AT_BUFF_SIZE_T qmtopen_test_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_size, char *at_resp);

extern AT_BUFF_SIZE_T qmtopen_read_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_size, char *at_resp);

extern AT_BUFF_SIZE_T qmtopen_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_size, char *at_resp);

extern AT_BUFF_SIZE_T qmtopen_exec_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_size, char *at_resp);


#endif