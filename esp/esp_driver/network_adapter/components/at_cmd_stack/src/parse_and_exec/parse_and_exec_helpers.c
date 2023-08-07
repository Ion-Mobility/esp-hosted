#include "parse_and_exec_helpers.h"
#include <string.h>


int convert_and_validate(const char* str, 
    long int* converted_number)
{
    *converted_number = strtol(str, NULL, 10);
    for (int pos = 0; pos < strlen(str); pos++)
    {
        if ((str[pos] < '0') || (str[pos] > '9'))
        {
            AT_STACK_LOGE("original string '%s' has wrong integer format", str);
            return 1;
        }
    }
    if (*converted_number != 0) return 0;

    for (int pos = 0; pos < strlen(str); pos++)
    {
        if (str[pos] != '0')
        {
            AT_STACK_LOGE("original string '%s' is not properly \"0\"", str);
            return 1;
        }
    }
    return 0;
}

int tokenize_quoted_string(const char *str, const char *delim,
    char **out_token, char **token_ctx)
{
    if ((str == NULL) && (*token_ctx == NULL))
        goto end_of_string;

    char *process_buff = *token_ctx;
    // if token context is a NULL terminate character, it means end of string
    if (process_buff[0] == '\0')
        goto end_of_string;

    if (process_buff[0] != '"')
        goto invalid_token;
    bool is_quoted_string_found = false;
    bool is_valid_add_quote_characters_to_string_in_the_end = false;
    unsigned int end_quote_pos, delim_pos;
    for (end_quote_pos = 1; end_quote_pos < strlen(process_buff); 
        end_quote_pos++)
    {
        if ((process_buff[end_quote_pos] == '"') && 
            (process_buff[end_quote_pos - 1] != '\\'))
        {
            delim_pos = end_quote_pos + 1;
            if (!memcmp(&process_buff[delim_pos], delim, strlen(delim)))
            {
                is_quoted_string_found = true;
            }
            else if (delim_pos == strlen(process_buff))
            {
                is_quoted_string_found = true;
                is_valid_add_quote_characters_to_string_in_the_end = true;
            }
            break;
        }
    }
    if (!is_quoted_string_found)
        goto invalid_token;

    *out_token = *token_ctx;
    if (!is_valid_add_quote_characters_to_string_in_the_end)
    {
        process_buff[delim_pos] = '\0';
        *token_ctx = &process_buff[delim_pos + 1];
    }
    else
    {
        *token_ctx = &process_buff[delim_pos];
    }
    return 0;
    
end_of_string:
    *out_token = NULL;
    return 0;
invalid_token:
    *out_token = NULL;
    return -1;
}

void remove_escape_characters_in_string(char* string_to_remove)
{
    for (unsigned int pos = 0; pos < strlen(string_to_remove); pos++)
    {
        if (string_to_remove[pos] == '\\')
        {
            memmove(&string_to_remove[pos], &string_to_remove[pos+1], 
                strlen(string_to_remove) - pos);
        }
    }
}
