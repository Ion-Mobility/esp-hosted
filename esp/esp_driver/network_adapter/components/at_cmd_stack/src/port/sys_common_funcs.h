#ifndef _SYS_COMMON_FUNCS_H_
#define _SYS_COMMON_FUNCS_H_
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pthread.h"

#define sys_printf(...) printf(__VA_ARGS__)
#define sys_strtok(buff, delim, context) strtok_r(buff, delim, context)
#define sys_mem_malloc(size) malloc(size)
#define sys_mem_calloc(block, block_size) calloc(block, block_size)
#define sys_mem_free(ptr) if (ptr != NULL) \
{ \
    free(ptr); \
    ptr = NULL; \
}

#define sys_task_sleep_us(sleep_us) usleep(sleep_us)

#endif