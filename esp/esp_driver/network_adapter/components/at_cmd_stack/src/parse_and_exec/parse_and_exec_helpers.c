#include "parse_and_exec_helpers.h"

int convert_and_validate(const char* str, 
    long int* converted_number)
{
    *converted_number = strtol(str, NULL, 10);
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
