#include "parse_and_exec_tmp_buff.h"
#include "sys_common_funcs.h"


//==============================
// Public functions definition
//==============================
handler_tmp_buff_t *alloc_handler_tmp_buff(AT_BUFF_SIZE_T arg_length)
{
    handler_tmp_buff_t *ret_buff = sys_mem_calloc((sizeof(handler_tmp_buff_t)), 1);
    if (ret_buff == NULL)
    {
        return NULL;
    }

    ret_buff->cpy_of_arg = sys_mem_calloc(arg_length, 1);
    if (ret_buff->cpy_of_arg == NULL)
    {
        sys_mem_free(ret_buff);
        return NULL;
    }

    ret_buff->tmp_resp_buff = sys_mem_calloc(MAX_AT_RESP_LENGTH, 1);
    if (ret_buff->tmp_resp_buff == NULL)
    {
        sys_mem_free(ret_buff);
        sys_mem_free(ret_buff->cpy_of_arg);
        return NULL;
    }
    
    return ret_buff;
}

void free_handler_tmp_buff(handler_tmp_buff_t* tmp_buff_handle)
{
    sys_mem_free(tmp_buff_handle->cpy_of_arg);
    sys_mem_free(tmp_buff_handle->tmp_resp_buff);
    sys_mem_free(tmp_buff_handle);
}