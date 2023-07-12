#ifndef _COMMON_HELPERS_H_
#define _COMMON_HELPERS_H_

#include "sys_common_funcs.h"
#include "esp_log.h"
#include "sdkconfig.h"
#define AT_LOG_FORMAT(format)   "(AT-%s) %s:%d: %s() " format "\n"

#ifndef AT_STACK_LOGE(format,...)
#define AT_STACK_LOGE(format,...)
#endif 

#ifndef AT_STACK_LOGW(format,...)
#define AT_STACK_LOGW(format,...)
#endif

#ifndef AT_STACK_LOGI(format,...)
#define AT_STACK_LOGI(format,...)
#endif

#ifndef AT_STACK_LOGD(format,...)
#define AT_STACK_LOGD(format,...)
#endif

#ifndef AT_STACK_LOGV(format,...)
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
#undef AT_STACK_LOGE(format,...)
#define AT_STACK_LOGE(format,...) do { \
    if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_INFO) \
    { \
        sys_printf(AT_LOG_FORMAT(format), "ERROR", __FILENAME__,__LINE__, \
        __func__, ##__VA_ARGS__); \
    } \
} while(0)
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 2
#undef AT_STACK_LOGW(format,...)
#define AT_STACK_LOGW(format,...) do { \
    if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_WARN) \
    { \
        sys_printf(AT_LOG_FORMAT(format), "WARN", __FILENAME__,__LINE__, \
        __func__, ##__VA_ARGS__); \
    } \
} while(0)
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 3
#undef AT_STACK_LOGI(format,...)
#define AT_STACK_LOGI(format,...) do { \
    if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_INFO) \
    { \
        sys_printf(AT_LOG_FORMAT(format), "INFO", __FILENAME__,__LINE__, \
        __func__, ##__VA_ARGS__); \
    } \
} while(0)
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 4
#undef AT_STACK_LOGD(format,...)
#define AT_STACK_LOGD(format,...) do { \
    if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_DEBUG) \
    { \
        sys_printf(AT_LOG_FORMAT(format), "DEBUG", __FILENAME__,__LINE__, \
        __func__, ##__VA_ARGS__); \
    } \
} while(0)
#endif

#if CONFIG_LOG_DEFAULT_LEVEL >= 5
#undef AT_STACK_LOGV(format,...)
#define AT_STACK_LOGV(format,...) do { \
    if (CONFIG_LOG_DEFAULT_LEVEL >= ESP_LOG_VERBOSE) \
    { \
        sys_printf(AT_LOG_FORMAT(format), "VERBOSE", __FILENAME__,__LINE__, \
        __func__, ##__VA_ARGS__); \
    } \
} while(0)
#endif
#endif

#endif