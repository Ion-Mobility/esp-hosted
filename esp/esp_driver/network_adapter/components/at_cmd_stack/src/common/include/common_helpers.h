#ifndef _COMMON_HELPERS_H_
#define _COMMON_HELPERS_H_

#include "sys_common_funcs.h"
#include "esp_log.h"
#define AT_LOG_FORMAT(format)   "(%s) at_cmd_stack: %s:%d: " format "\n"


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
#define AT_STACK_LOGE(format,...) do { \
    if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_INFO) \
    { \
        sys_printf(AT_LOG_FORMAT(format), "ERROR", __FILENAME__,__LINE__, ##__VA_ARGS__); \
    } \
} while(0)
#else
#define AT_STACK_LOGE(format,...)
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 2
#define AT_STACK_LOGW(format,...) do { \
    if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_WARN) \
    { \
        sys_printf(AT_LOG_FORMAT(format), "WARN", __FILENAME__,__LINE__, ##__VA_ARGS__); \
    } \
} while(0)
#else
#define AT_STACK_LOGW(format,...)
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 3
#define AT_STACK_LOGI(format,...) do { \
    if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_INFO) \
    { \
        sys_printf(AT_LOG_FORMAT(format), "INFO", __FILENAME__,__LINE__, ##__VA_ARGS__); \
    } \
} while(0)
#else
#define AT_STACK_LOGI(format,...)
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 4
#define AT_STACK_LOGD(format,...) do { \
    if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG) \
    { \
        sys_printf(AT_LOG_FORMAT(format), "DEBUG", __FILENAME__,__LINE__, ##__VA_ARGS__); \
    } \
} while(0)
#else
#define AT_STACK_LOGD(format,...)
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 5
#define AT_STACK_LOGV(format,...) do { \
    if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_VERBOSE) \
    { \
        sys_printf(AT_LOG_FORMAT(format), "VERBOSE", __FILENAME__,__LINE__, ##__VA_ARGS__); \
    } \
} while(0)
#else
#define AT_STACK_LOGV(format,...)
#endif
#endif

#endif