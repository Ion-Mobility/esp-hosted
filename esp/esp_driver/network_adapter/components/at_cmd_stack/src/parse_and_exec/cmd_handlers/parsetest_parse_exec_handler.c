#include "parsetest_parse_exec_handler.h"
#include "../parse_and_exec_helpers.h"
#include "mqtt_service.h"


//==============================
// Public functions definition
//==============================
AT_BUFF_SIZE_T parsetest_test_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    sprintf(at_resp, 
        "+PARSETEST: <0-1>,<required_quoted_string>[,<0-65535>[,<optional_quoted_string>]]\r\n\r\nOK");
    return strlen(at_resp);
}

AT_BUFF_SIZE_T parsetest_read_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    sprintf(at_resp, 
        "+PARSETEST: this is READ cmd");
    return strlen(at_resp);
}

AT_BUFF_SIZE_T parsetest_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    handler_tmp_buff_t *handler_tmp_buff = alloc_handler_tmp_buff(arg_length);
    memcpy(handler_tmp_buff->cpy_of_arg, arg+1, arg_length);
    char* tokenize_context;
    char* token;
    long int parsed_required_lint, parsed_optional_lint = 1000000;
    char *parsed_required_string, *parsed_optional_string = NULL;
    TOKENIZE_AND_ASSIGN_REQUIRED_LINT_PARAM(parsed_required_lint, 0, 1, 
        handler_tmp_buff->cpy_of_arg,
        token, tokenize_context, handler_tmp_buff, at_resp);
    TOKENIZE_AND_ASSIGN_REQUIRED_QUOTED_STRING(parsed_required_string, NULL,
        token, tokenize_context, handler_tmp_buff, at_resp);
    TOKENIZE_AND_ASSIGN_OPTIONAL_UNSIGNED_LINT_PARAM(parsed_optional_lint, 0, 65535, 
        NULL, token, tokenize_context, handler_tmp_buff, at_resp);
    TOKENIZE_AND_ASSIGN_OPTIONAL_QUOTED_STRING(parsed_optional_string, NULL,
        token, tokenize_context, handler_tmp_buff, at_resp);
    sprintf(at_resp, "+PARSETEST: found required '%ld','%s'", 
        parsed_required_lint, parsed_required_string);
    if (parsed_optional_lint != 1000000)
    {
        sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
        sprintf(at_resp, "%s; found optional '%ld'", 
            handler_tmp_buff->tmp_resp_buff, parsed_optional_lint);
    }
    if (parsed_optional_string != NULL)
    {
        sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
        sprintf(at_resp, "%s,'%s'", 
            handler_tmp_buff->tmp_resp_buff, parsed_optional_string);
    }
    sprintf(handler_tmp_buff->tmp_resp_buff, "%s",at_resp);
    sprintf(at_resp, "%s\r\n\r\nOK", handler_tmp_buff->tmp_resp_buff);

    free_handler_tmp_buff(handler_tmp_buff);
    return strlen(at_resp);
}


AT_BUFF_SIZE_T parsetest_exec_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{

    sprintf(at_resp, 
        "+PARSETEST: this is EXECUTE cmd");
    return strlen(at_resp);
}