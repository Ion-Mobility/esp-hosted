#ifndef _PARSE_EXEC_HELPER_H_
#define _PARSE_EXEC_HELPER_H_
#include <string.h>
#include "sys_common_funcs.h"
#include "parse_and_exec_constants.h"
#include "common_helpers.h"


#define RETURN_RESPONSE_UNSUPPORTED(resp)  do { \
    memcpy(resp, unsupported_string, strlen(unsupported_string)); \
    DEEP_DEBUG("Return 'ERR' because of unsupported AT commands!\n"); \
    return strlen(unsupported_string); \
} while (0)

#define RETURN_RESPONSE_ERROR(resp)  do { \
    memcpy(resp, error_string, strlen(error_string)); \
    DEEP_DEBUG("Return 'ERR' because of AT command is invalid!\n"); \
    return strlen(error_string); \
} while (0)

#define RETURN_RESPONSE_OK(resp)  do { \
    memcpy(resp, ok_string, strlen(ok_string)); \
    return strlen(ok_string); \
} while (0)

#define MUST_BE_CORRECT_OR_RESPOND_ERROR(condition, resp)  do { \
    if (!(condition)) { \
        DEEP_DEBUG("Condition not satisfy. Return 'ERR' response!\n"); \
        RETURN_RESPONSE_ERROR(resp); \
    } \
} while (0)

#define FREE_BUFF_AND_RETURN_VAL(buff, val) do { \
    free(buff); \
    return val; \
} while(0)





#endif // _PARSE_EXEC_HELPER_H_