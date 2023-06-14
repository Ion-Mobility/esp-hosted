#include "at-lib.h"
#include <stdio.h>
#include <string.h>

const char basic_response[3] = "OK\0";
static BUFF_SIZE_T handler_hello(const char* arg, const BUFF_SIZE_T arg_size, char *at_resp);

#define IS_TEST_COMMAND(arg, arg_size) (!memcmp(arg, "=?", arg_size)) && (arg_size == 2)
#define IS_READ_COMMAND(arg, arg_size) (!memcmp(arg, "?", arg_size)) && (arg_size == 1)
#define IS_WRITE_COMMAND(arg, arg_size) (!memcmp(arg, "=", 1)) && (arg_size >= 1)
#define IS_EXECUTION_COMMAND(arg, arg_size) (arg_size == 0)

at_command_list_entry_t supported_at_cmds_list[MAX_NUMBER_OF_SUPPORTED_AT_COMMANDS] =
{
    {
        .at_command = "AT+HELLO",
        .command_handler = handler_hello,
    },
};

static BUFF_SIZE_T handler_hello(const char* arg, const BUFF_SIZE_T arg_size, char *at_resp)
{
    if (IS_TEST_COMMAND(arg, arg_size))
    {
        printf("Test command!\n");
        memcpy(at_resp, "NO WRITE", sizeof("NO WRITE"));
        printf("done copy\n");
        return strlen("NO WRITE");
    } else if (IS_READ_COMMAND(arg, arg_size)) 
    {
        printf("Read command!\n");   
        memcpy(at_resp, basic_response, 3);
        printf("done copy\n");
        return strlen(basic_response);
    }
    else if (IS_WRITE_COMMAND(arg, arg_size)) 
    {
        printf("Write command!\n");
        memcpy(at_resp, "i do mean NO WRITE", sizeof("i do mean NO WRITE"));
        printf("done copy\n");
        return strlen("i do mean NO WRITE");
    }
    else if (IS_EXECUTION_COMMAND(arg, arg_size)) 
    {
        printf("Execution command!\n");
        memcpy(at_resp, "No execution available", sizeof("No execution available"));
        printf("done copy\n");
        return strlen("No execution available");
    }
    return 0;
}