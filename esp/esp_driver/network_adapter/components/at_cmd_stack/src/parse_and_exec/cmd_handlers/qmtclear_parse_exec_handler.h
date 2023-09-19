#ifndef _QMTCLEAR_PARSE_EXEC_HANDLER_H_
#define _QMTCLEAR_PARSE_EXEC_HANDLER_H_

#include "at_cmd_stack_types.h"

extern AT_BUFF_SIZE_T qmtclear_test_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp);

extern AT_BUFF_SIZE_T qmtclear_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp);



#endif