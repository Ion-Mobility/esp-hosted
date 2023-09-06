#ifndef _PARSE_EXEC_HELPER_H_
#define _PARSE_EXEC_HELPER_H_
#include <string.h>
#include "sys_common_funcs.h"
#include "parse_and_exec_constants.h"
#include "common_helpers.h"
#include "parse_and_exec_tmp_buff.h"


#define RETURN_RESPONSE_UNSUPPORTED(resp)  do { \
    memcpy(resp, unsupported_string, strlen(unsupported_string)); \
    AT_STACK_LOGE("Return 'ERROR' because of unsupported AT commands!"); \
    return strlen(unsupported_string); \
} while (0)

#define RETURN_RESPONSE_ERROR(resp)  do { \
    memcpy(resp, error_string, strlen(error_string)); \
    AT_STACK_LOGE("Return 'ERROR' because of AT command is invalid!"); \
    return strlen(error_string); \
} while (0)

#define RETURN_RESPONSE_OK(resp)  do { \
    memcpy(resp, ok_string, strlen(ok_string)); \
    return strlen(ok_string); \
} while (0)

/**
 * @brief Check if condition is satisfy. If not, free temporary buffer of
 * parse and exec handler and response 'ERROR'
 * 
 * @param condition the condition to check [in]
 * @param handler_tmp_buff temporary buffer in usual parse and exec handler.
 * When exit due to invalid value, temporary buffer must be free [out]
 * @param resp pointer to AT response, in case fail to validate [out]
 * 
 */
#define MUST_BE_CORRECT_OR_RESPOND_ERROR(condition, \
    handler_tmp_buff, resp)  do { \
    if (!(condition)) { \
        AT_STACK_LOGE("Condition not satisfy. Return 'ERROR' response!"); \
        free_handler_tmp_buff(handler_tmp_buff); \
        RETURN_RESPONSE_ERROR(resp); \
    } \
} while (0)

#define FREE_BUFF_AND_RETURN_VAL(buff, val) do { \
    free(buff); \
    return val; \
} while(0)


/**
 * @brief Get next token, validate and assign to a unsigned long integer. 
 * If no token is found or valus is out of range, return 'ERROR' response.
 * 
 * @note Can only be used in parse and exec handler
 * 
 * @param dest the value to assign to. MUST NOT a pointer [out]
 * @param min the minimum value must meet [in]
 * @param max the maximum value must meet [in]
 * @param input_str the string to tokenize [in,out]
 * @param token token variable [in]
 * @param tokenize_context tokenize context. That mean origial string must 
 * be tokenized once [in,out]
 * @param handler_tmp_buff temporary buffer in usual parse and 
 * exec handler. When exit due to invalid value, temporary buffer must be 
 * free [in]
 * @param resp pointer to AT response, in case fail to validate [out]
 */
#define TOKENIZE_AND_ASSIGN_REQUIRED_LINT_PARAM(dest, \
    min, max, input_str, token, tokenize_context, handler_tmp_buff, \
    resp)  do { \
    token = sys_strtok(input_str, param_delim, &tokenize_context); \
    if (token != NULL) \
    { \
        int is_cond_satisfy = !convert_and_validate(token, (long int*)&dest); \
        if (!is_cond_satisfy) \
        { \
            AT_STACK_LOGE("Converted lint stage for token '%s' wrong", token); \
        } \
        MUST_BE_CORRECT_OR_RESPOND_ERROR(is_cond_satisfy, \
            handler_tmp_buff, resp); \
        is_cond_satisfy = ((dest >= min) && (dest <= max)); \
        if (!is_cond_satisfy) \
        { \
            AT_STACK_LOGE("converted lint %ld not within range (%ld,%ld)", \
                dest, (long int)min, (long int)max); \
        } \
        MUST_BE_CORRECT_OR_RESPOND_ERROR(is_cond_satisfy, \
            handler_tmp_buff, resp); \
    } \
    else \
    { \
        AT_STACK_LOGE("Need to tokenize an long integer value but not found. Report 'ERROR'!"); \
        free_handler_tmp_buff(handler_tmp_buff); \
        RETURN_RESPONSE_ERROR(resp); \
    } \
} while (0)

/**
 * @brief Get next token, validate and assign to a unsigned long integer. 
 * If token is found and invalid, return 'ERROR' response. 
 * Continue if no token is found
 * 
 * @note Can only be used in parse and exec handler
 * 
 * @param dest the value to assign to. MUST NOT a pointer [out]
 * @param min the minimum value must meet [in]
 * @param max the maximum value must meet [in]
 * @param input_str the string to tokenize [in,out]
 * @param token token variable [in]
 * @param tokenize_context tokenize context. That mean origial string must 
 * be tokenized once [in,out]
 * @param handler_tmp_buff temporary buffer in usual parse and exec handler. When
 * exit due to invalid value, temporary buffer must be free [in]
 * @param resp pointer to AT response, in case fail to validate [out]
 */
#define TOKENIZE_AND_ASSIGN_OPTIONAL_UNSIGNED_LINT_PARAM(dest, min, max, \
    input_str, token, tokenize_context, handler_tmp_buff, resp)  do { \
    token = sys_strtok(input_str, param_delim, &tokenize_context); \
    if (token != NULL) \
    { \
        int is_cond_satisfy = !convert_and_validate(token, (long int*)&dest); \
        if (!is_cond_satisfy) \
        { \
            AT_STACK_LOGE("Converted lint stage for token '%s' wrong", token); \
        } \
        MUST_BE_CORRECT_OR_RESPOND_ERROR(is_cond_satisfy, \
            handler_tmp_buff, resp); \
        is_cond_satisfy = ((dest >= min) && (dest <= max)); \
        if (!is_cond_satisfy) \
        { \
            AT_STACK_LOGE("converted lint %ld not within range (%ld,%ld)", \
                dest, (long int)min, (long int)max); \
        } \
        MUST_BE_CORRECT_OR_RESPOND_ERROR(is_cond_satisfy, \
            handler_tmp_buff, resp); \
    } \
    else \
    { \
        AT_STACK_LOGD("Not found optional integer token!"); \
    } \
} while (0)


/**
 * @brief Get next token, validate and assign to a character pointer from
 * quoted string. If no token is found or not properly quoted, 
 * return 'ERROR' response
 * 
 * @note Can only be used in parse and exec handler
 * 
 * @param dest the pointer to token to assign to [out]
 * @param input_str the string to tokenize [in,out]
 * @param token token variable [in]
 * @param tokenize_context tokenize context. That mean origial string must 
 * be tokenized once [in,out]
 * @param handler_tmp_buff pointer to temporary buffer for 
 * free in case of invalid tokenize [out]
 * @param resp pointer to AT response, in case fail to validate [out]
 */
#define TOKENIZE_AND_ASSIGN_REQUIRED_QUOTED_STRING(dest, input_str, \
    token, token_ctx, handler_tmp_buff, resp)  do { \
    int tok_err = tokenize_quoted_string(NULL, param_delim, \
        &token, &token_ctx); \
    if (tok_err) \
    { \
        AT_STACK_LOGE("Tokenize invalid quoted string"); \
    } \
    else if (token == NULL) \
    { \
        AT_STACK_LOGE("Need to find quote string but reach the end of tokenize string"); \
    } \
    MUST_BE_CORRECT_OR_RESPOND_ERROR((!tok_err) && \
        (token != NULL), handler_tmp_buff, resp); \
    remove_escape_characters_in_string(token); \
    dest = &token[1]; \
    token[strlen(token) - 1] = '\0'; \
} while (0)

/**
 * @brief Get next token, validate and assign to a character pointer from
 * quoted string. If token is found but not properly quoted, return 'ERROR' 
 * response. Continue if no token is found
 * 
 * @note Can only be used in parse and exec handler
 * 
 * @param dest the pointer to token to assign to [out]
 * @param input_str the string to tokenize [in,out]
 * @param token token variable [in]
 * @param tokenize_context tokenize context. That mean origial string must 
 * be tokenized once [in,out]
 * @param handler_tmp_buff pointer to temporary buffer for free in case  
 * @param resp pointer to AT response, in case fail to validate [out]
 */
#define TOKENIZE_AND_ASSIGN_OPTIONAL_QUOTED_STRING(dest, input_str, \
    token, token_ctx, handler_tmp_buff, resp)  do { \
    int tok_err = tokenize_quoted_string(NULL, param_delim, \
        &token, &token_ctx); \
    if ((!tok_err) && (token != NULL)) \
    { \
        remove_escape_characters_in_string(token); \
        dest = &token[1]; \
        token[strlen(token) - 1] = '\0'; \
    } \
    else if ((!tok_err) && (token == NULL)) \
    { \
        AT_STACK_LOGD("Not found optional quoted string token!"); \
    } \
    else if (tok_err) \
    { \
        AT_STACK_LOGE("Tokenize invalid quoted string"); \
        MUST_BE_CORRECT_OR_RESPOND_ERROR(!tok_err, handler_tmp_buff, resp); \
    } \
} while (0)

/**
 * @brief Write parse and exec handler usually have the same procedure:
 * allocate temporary buffer and copy argument to this buffer, define token 
 * and tokenize context, find client index. So this macro is used to shorten 
 * these steps
 * 
 * @param arg pointer to argument [in]
 * @param arg_len argument length [in]
 * @param resp pointer to AT reponse [out]
 * @param client_index name of client index variable [in]
 * 
 */
#define INTIALIZE_WRITE_PARSE_EXEC_HANDLER(arg, arg_len, resp, client_index) \
    handler_tmp_buff_t *handler_tmp_buff= alloc_handler_tmp_buff(arg_len); \
    memcpy(handler_tmp_buff->cpy_of_arg, arg+1, arg_len); \
    char* tokenize_context; \
    char* token; \
    long int client_index; \
    TOKENIZE_AND_ASSIGN_REQUIRED_LINT_PARAM(client_index, 0, \
        MAX_NUM_OF_MQTT_CLIENT - 1, handler_tmp_buff->cpy_of_arg, \
        token, tokenize_context, handler_tmp_buff, resp);




#endif // _PARSE_EXEC_HELPER_H_