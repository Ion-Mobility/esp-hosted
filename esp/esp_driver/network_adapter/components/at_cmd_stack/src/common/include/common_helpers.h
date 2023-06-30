#ifndef _COMMON_HELPERS_H_
#define _COMMON_HELPERS_H_

#include "sys_common_funcs.h"

#define MUST_BE_CORRECT_OR_EXIT(cond, ret_val)  do { \
    if (!(cond)) { \
    DEEP_DEBUG("Condition not satisfy!\n"); \
    return ret_val; \
    }\
} while(0)

#define VALIDATE_ARGS(arg_condition, ret_val) do {\
    if (!(arg_condition))\
    {\
        DEEP_DEBUG("Argument is not valid!\n");\
        return ret_val;\
    }\
} while(0)

#if CONFIG_TURN_ON_DEEP_DEBUG
#define DEEP_DEBUG(...) sys_printf("[File %s, line %d] ", __FILE__, __LINE__); \
    sys_printf(__VA_ARGS__)
#else
#define DEEP_DEBUG(...)
#endif

#endif