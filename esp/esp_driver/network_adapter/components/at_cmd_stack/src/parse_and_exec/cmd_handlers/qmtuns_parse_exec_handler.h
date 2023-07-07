#ifndef _QMTUNS_PARSE_EXEC_HANDLER_H_
#define _QMTUNS_PARSE_EXEC_HANDLER_H_

#include "at_cmd_stack_types.h"

extern AT_BUFF_SIZE_T qmtuns_test_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp);

extern AT_BUFF_SIZE_T qmtuns_read_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp);

extern AT_BUFF_SIZE_T qmtuns_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp);

extern AT_BUFF_SIZE_T qmtuns_exec_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp);


#endif