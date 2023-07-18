#ifndef _COMMON_HELPERS_H_
#define _COMMON_HELPERS_H_

#include "sys_common_funcs.h"
#include "esp_log.h"
#include "sdkconfig.h"
#define AT_LOG_FORMAT(format)   "(AT-%s) (%u) %s:%d: %s() " format "\n"
#define AT_LOG_ARGS(level_string,...)   #level_string, SYS_GET_TIMESTAMP, \
    __FILENAME__,__LINE__, __func__, ##__VA_ARGS__

#ifndef AT_STACK_LOGE
#define AT_STACK_LOGE(format,...)
#endif 

#ifndef AT_STACK_LOGW
#define AT_STACK_LOGW(format,...)
#endif

#ifndef AT_STACK_LOGI
#define AT_STACK_LOGI(format,...)
#endif

#ifndef AT_STACK_LOGD
#define AT_STACK_LOGD(format,...)
#endif

#ifndef AT_STACK_LOGV
#define AT_STACK_LOGV(format,...)
#endif

#define MUST_BE_CORRECT_OR_EXIT(cond, ret_val)  do { \
    if (!(cond)) { \
    AT_STACK_LOGE("Condition not satisfy!"); \
    return ret_val; \
    }\
} while(0)

#define VALIDATE_ARGS(arg_condition, ret_val) do {\
    if (!(arg_condition))\
    {\
        AT_STACK_LOGE("Argument is not valid!");\
        return ret_val;\
    }\
} while(0)

#if CONFIG_TURN_ON_DEEP_DEBUG
#if CONFIG_LOG_DEFAULT_LEVEL >= 1
#undef AT_STACK_LOGE
#define AT_STACK_LOGE(format,...) \
    sys_printf(AT_LOG_FORMAT(format), AT_LOG_ARGS(ERROR, ##__VA_ARGS__))
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 2
#undef AT_STACK_LOGW
#define AT_STACK_LOGW(format,...) \
    sys_printf(AT_LOG_FORMAT(format), AT_LOG_ARGS(WARN, ##__VA_ARGS__));
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 3
#undef AT_STACK_LOGI
#define AT_STACK_LOGI(format,...) \
    sys_printf(AT_LOG_FORMAT(format), AT_LOG_ARGS(INFO, ##__VA_ARGS__));
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 4
#undef AT_STACK_LOGD
#define AT_STACK_LOGD(format,...) \
    sys_printf(AT_LOG_FORMAT(format), AT_LOG_ARGS(DEBUG, ##__VA_ARGS__));
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 5
#undef AT_STACK_LOGV
#define AT_STACK_LOGV(format,...) \
    sys_printf(AT_LOG_FORMAT(format), AT_LOG_ARGS(VERBOSE, ##__VA_ARGS__));
#endif
#endif

#endif