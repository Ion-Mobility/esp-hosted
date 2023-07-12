#ifndef _PARSE_EXEC_HANDLERS_LIST_
#define _PARSE_EXEC_HANDLERS_LIST_

#include "parse_and_exec_types.h"
#include "at_cmd_stack_types.h"

#define MAX_NUMBER_OF_SUPPORTED_AT_COMMANDS 9

typedef struct {
    char *at_command_family;
    command_handler_t test_cmd_parse_exec_handler;
    command_handler_t read_cmd_parse_exec_handler;
    command_handler_t write_cmd_parse_exec_handler;
    command_handler_t exec_cmd_parse_exec_handler;
} parse_exec_handler_entry_t;

extern const parse_exec_handler_entry_t parse_exec_handlers_table
    [MAX_NUMBER_OF_SUPPORTED_AT_COMMANDS];

#endif