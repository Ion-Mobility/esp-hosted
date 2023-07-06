#include "at_cmd_parse_and_exec.h"
#include <stdbool.h>
#include "parse_and_exec_helpers.h"
#include "parse_and_exec_cmd_handlers_list.h"



#define IS_TEST_COMMAND(arg, arg_length) (!memcmp(arg, "=?", arg_length)) && (arg_length == 2)
#define IS_READ_COMMAND(arg, arg_length) (!memcmp(arg, "?", arg_length)) && (arg_length == 1)
#define IS_WRITE_COMMAND(arg, arg_length) (!memcmp(arg, "=", 1)) && (arg_length >= 1)
#define IS_EXECUTION_COMMAND(arg, arg_length) (arg_length == 0)


typedef enum {
    AT_CMD_TEST,
    AT_CMD_READ,
    AT_CMD_WRITE,
    AT_CMD_EXEC,
} at_cmd_type_t;

//================================
// Private functions declaration
//================================
static int find_parse_exec_handler_entry_for_at_cmd(const char *at_cmd, 
    AT_BUFF_SIZE_T cmd_total_length);

static at_cmd_type_t determine_type_of_at_cmd_and_parse_exec(int entry_index,
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, char* at_resp);

static AT_BUFF_SIZE_T find_argument_part_of_at_cmd(int entry_index,
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, const char** ret_arg_ptr);

static at_cmd_type_t determine_type_of_at_cmd(const char *arg, 
    AT_BUFF_SIZE_T arg_length);

static AT_BUFF_SIZE_T parse_and_exec_test_at_cmd(int entry_index, 
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, char* at_resp);

static AT_BUFF_SIZE_T parse_and_exec_read_at_cmd(int entry_index, 
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, char* at_resp);

static AT_BUFF_SIZE_T parse_and_exec_write_at_cmd(int entry_index, 
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, char* at_resp);

static AT_BUFF_SIZE_T parse_and_exec_exec_at_cmd(int entry_index, 
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, char* at_resp);


//================================
// Public functions definition
//================================
AT_BUFF_SIZE_T parse_and_exec_at_cmd(const char *at_cmd, const AT_BUFF_SIZE_T 
    cmd_total_length, char* at_resp)
{
    VALIDATE_ARGS(at_cmd != NULL, 0);
    VALIDATE_ARGS(at_resp != NULL, 0);
    VALIDATE_ARGS((cmd_total_length > 0) && 
        (cmd_total_length < MAX_AT_CMD_LENGTH), 0);

    int parse_exec_hanlder_index = find_parse_exec_handler_entry_for_at_cmd(
            at_cmd, cmd_total_length
        );
    bool is_handler_found = (parse_exec_hanlder_index != -1)? true : false;
    
    if (!is_handler_found)
        RETURN_RESPONSE_UNSUPPORTED(at_resp);

    return determine_type_of_at_cmd_and_parse_exec(parse_exec_hanlder_index,
        at_cmd, cmd_total_length, at_resp);
}

//===============================
// Private functions definition
//===============================
static AT_BUFF_SIZE_T determine_type_of_at_cmd_and_parse_exec(int entry_index,
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, char* at_resp)
{
    const char* arg;
    AT_BUFF_SIZE_T arg_length = find_argument_part_of_at_cmd(entry_index, at_cmd, 
        cmd_total_length, &arg);
    at_cmd_type_t type_of_cmd = determine_type_of_at_cmd(arg, arg_length);
    switch (type_of_cmd)
    {
    case AT_CMD_TEST:
        return parse_and_exec_test_at_cmd(entry_index, at_cmd, 
            cmd_total_length, at_resp);
        break;

    case AT_CMD_READ:
        return parse_and_exec_read_at_cmd(entry_index, at_cmd, 
            cmd_total_length, at_resp);
        break;

    case AT_CMD_WRITE:
        return parse_and_exec_write_at_cmd(entry_index, at_cmd, 
            cmd_total_length, at_resp);
        break;

    case AT_CMD_EXEC:
        return parse_and_exec_exec_at_cmd(entry_index, at_cmd, 
            cmd_total_length, at_resp);
        break;
    }

    // cannot reach here, but still do to avoid compiler error
    RETURN_RESPONSE_OK(at_resp);
}

static AT_BUFF_SIZE_T find_argument_part_of_at_cmd(int entry_index,
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, const char** ret_arg_ptr)
{
    parse_exec_handler_entry_t checking_entry = 
        parse_exec_handlers_table[entry_index];


    AT_BUFF_SIZE_T at_cmd_family_len = strlen(checking_entry.at_command_family);
    AT_BUFF_SIZE_T arg_length = cmd_total_length - at_cmd_family_len;
    *ret_arg_ptr = &at_cmd[at_cmd_family_len];

    return arg_length;
}

static AT_BUFF_SIZE_T parse_and_exec_test_at_cmd(int entry_index, 
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, char* at_resp)
{
    parse_exec_handler_entry_t parse_exec_handler = 
        parse_exec_handlers_table[entry_index];
    
    const char *arg;
    AT_BUFF_SIZE_T arg_length = find_argument_part_of_at_cmd(entry_index,
        at_cmd, cmd_total_length, &arg
    );

    if (parse_exec_handler.test_cmd_parse_exec_handler == NULL)
        RETURN_RESPONSE_UNSUPPORTED(at_resp);

    return parse_exec_handler.test_cmd_parse_exec_handler(arg, 
        arg_length, at_resp);
}

static AT_BUFF_SIZE_T parse_and_exec_read_at_cmd(int entry_index, 
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, char* at_resp)
{
    parse_exec_handler_entry_t parse_exec_handler = 
        parse_exec_handlers_table[entry_index];

    const char *arg;
    AT_BUFF_SIZE_T arg_length = find_argument_part_of_at_cmd(entry_index,
        at_cmd, cmd_total_length, &arg
    );

    if (parse_exec_handler.read_cmd_parse_exec_handler == NULL)
        RETURN_RESPONSE_UNSUPPORTED(at_resp);

    return parse_exec_handler.read_cmd_parse_exec_handler(arg, 
        arg_length, at_resp);
}

static AT_BUFF_SIZE_T parse_and_exec_write_at_cmd(int entry_index, 
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, char* at_resp)
{
    parse_exec_handler_entry_t parse_exec_handler = 
        parse_exec_handlers_table[entry_index];
    const char *arg;
    AT_BUFF_SIZE_T arg_length = find_argument_part_of_at_cmd(entry_index,
        at_cmd, cmd_total_length, &arg
    );
    
    if (parse_exec_handler.write_cmd_parse_exec_handler == NULL)
        RETURN_RESPONSE_UNSUPPORTED(at_resp);

    return parse_exec_handler.write_cmd_parse_exec_handler(arg, 
        arg_length, at_resp);
}

static AT_BUFF_SIZE_T parse_and_exec_exec_at_cmd(int entry_index, 
    const char *at_cmd, AT_BUFF_SIZE_T cmd_total_length, char* at_resp)
{
    parse_exec_handler_entry_t parse_exec_handler = 
        parse_exec_handlers_table[entry_index];
    
    if (parse_exec_handler.exec_cmd_parse_exec_handler == NULL)
        RETURN_RESPONSE_UNSUPPORTED(at_resp);

    return parse_exec_handler.exec_cmd_parse_exec_handler(at_cmd, 
        cmd_total_length, at_resp);
}

static int find_parse_exec_handler_entry_for_at_cmd(const char *at_cmd, 
    AT_BUFF_SIZE_T cmd_total_length)
{
    for (int index = 0; index < MAX_NUMBER_OF_SUPPORTED_AT_COMMANDS; index++)
    {
        const char *target_cmd_family_to_compare = 
            parse_exec_handlers_table[index].at_command_family;
        AT_BUFF_SIZE_T target_cmd_family_len = strlen(target_cmd_family_to_compare);
        if (!strncmp(at_cmd, target_cmd_family_to_compare, target_cmd_family_len))
        {
            return index;
        }
    }
    return -1;
}

static at_cmd_type_t determine_type_of_at_cmd(const char *arg, 
    AT_BUFF_SIZE_T arg_length)
{
    if (IS_TEST_COMMAND(arg, arg_length))
    {
        return AT_CMD_TEST;
    } else if (IS_READ_COMMAND(arg, arg_length)) 
    {
        return AT_CMD_READ;
    }
    else if (IS_WRITE_COMMAND(arg, arg_length)) 
    {
        return AT_CMD_WRITE;
    }
    else if (IS_EXECUTION_COMMAND(arg, arg_length)) 
    {
        return AT_CMD_EXEC;
    }
    return 0;
}