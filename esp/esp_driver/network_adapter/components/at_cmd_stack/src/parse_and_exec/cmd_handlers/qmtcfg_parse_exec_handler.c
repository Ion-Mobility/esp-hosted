#include <stdlib.h>
#include "at_cmd_stack_types.h"
#include "../parse_and_exec_types.h"
#include "../parse_and_exec_helpers.h"
#include "mqtt_service_configs.h"
#include <stdbool.h>

// This constant value is assigned to any config fields that is not changed
// since we don't know how to check unchange field in staging config
static const long int predefined_invalid_value = 0x3A3B3C3D;


#define QMTCFG_WRITE_PARSE_HANLDER(param) \
    static AT_BUFF_SIZE_T qmtcfg_write_parse_ ## param

#define QMTCFG_WRITE_PARSE_HANLDER_FUNC_NAME(param) \
    qmtcfg_write_parse_ ## param


#define INTIALIZE_QMTCFG_WRITE_PARSE_EXEC_HANDLER(arg, arg_len, resp) \
    handler_tmp_buff_t *handler_tmp_buff= alloc_handler_tmp_buff(arg_len); \
    memcpy(handler_tmp_buff->cpy_of_arg, arg, arg_length); \
    char* tokenize_context; \
    char* token; \
    token = sys_strtok(handler_tmp_buff->cpy_of_arg, param_delim, \
        &tokenize_context); \
    long int client_idx; \
    long int tmp_parsed_value; \
    TOKENIZE_AND_ASSIGN_REQUIRED_LINT_PARAM(client_idx, 0, \
        MAX_NUM_OF_MQTT_CLIENT-1, NULL, token, tokenize_context, \
        handler_tmp_buff, resp); \
    mqtt_service_client_cfg_t staging_mqtt_service_client_cfg = \
        mqtt_service_client_cfg_table[client_idx]

#define FINALIZE_WRITE_PARSE_EXEC_HANDLE(resp) \
    mqtt_service_client_cfg_table[client_idx] = staging_mqtt_service_client_cfg; \
    free_handler_tmp_buff(handler_tmp_buff); \
    RETURN_RESPONSE_OK(resp);

#define GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(field, field_type, min, \
    max, handler_tmp_buff, resp) \
    tmp_parsed_value = predefined_invalid_value; \
    TOKENIZE_AND_ASSIGN_OPTIONAL_UNSIGNED_LINT_PARAM(tmp_parsed_value, min, \
        max, NULL, token, tokenize_context, handler_tmp_buff, resp); \
    if (tmp_parsed_value != predefined_invalid_value) \
    { \
        staging_mqtt_service_client_cfg.field = (field_type)tmp_parsed_value; \
    } \


#define APPEND_START_OF_USAGE(target, param_key) \
    char *tmp_buff = sys_mem_calloc(MAX_AT_RESP_LENGTH, 1); \
    sprintf(tmp_buff, "%s+QMTCFG: \"%s\"", target, param_key); \
    memcpy(target, tmp_buff, strlen(tmp_buff)); \
    APPEND_RANGE_STRING(target, 0, MAX_NUM_OF_MQTT_CLIENT-1) \


#define APPEND_RANGE_STRING(target, min, max) do { \
    if (min == max) \
    { \
        sprintf(tmp_buff, "%s,<%d>", target, min); \
        memcpy(target, tmp_buff, strlen(tmp_buff)); \
    } \
    else \
    { \
        sprintf(tmp_buff, "%s,<%d-%d>", target, min, max); \
        memcpy(target, tmp_buff, strlen(tmp_buff)); \
    } \
} while (0)

#define APPEND_END_OF_USAGE(target) \
    sprintf(tmp_buff, "%s\r\n", target); \
    memcpy(target, tmp_buff, strlen(tmp_buff)); \
    sys_mem_free(tmp_buff) \

typedef struct {
    const char* param_key;
    int min_num_of_param_values;
    int max_num_of_param_values;
    command_handler_t qmtcfg_write_parse_exec_handler;
} param_key_based_params_parser_table_entry_t;

#define QMTCFG_WRITE_PARSE_HANDLER_NUM_ENTRY 7
#define RANGE_STRING_BUFF_SIZE 9
#define PARAM_KEY_MAX_BUFF_SIZE 9


QMTCFG_WRITE_PARSE_HANLDER(version) (const char *arg,
    AT_BUFF_SIZE_T arg_length, char *at_resp);
QMTCFG_WRITE_PARSE_HANLDER(pdpcid) (const char *arg,
    AT_BUFF_SIZE_T arg_length, char *at_resp);
QMTCFG_WRITE_PARSE_HANLDER(keepalive) (const char *arg,
    AT_BUFF_SIZE_T arg_length, char *at_resp);
QMTCFG_WRITE_PARSE_HANLDER(session) (const char *arg,
    AT_BUFF_SIZE_T arg_length, char *at_resp);
QMTCFG_WRITE_PARSE_HANLDER(timeout) (const char *arg,
    AT_BUFF_SIZE_T arg_length, char *at_resp);
QMTCFG_WRITE_PARSE_HANLDER(recv_mode) (const char *arg,
    AT_BUFF_SIZE_T arg_length, char *at_resp);
QMTCFG_WRITE_PARSE_HANLDER(send_mode) (const char *arg,
    AT_BUFF_SIZE_T arg_length, char *at_resp);


const param_key_based_params_parser_table_entry_t 
    qmtcfg_write_parse_exec_table [QMTCFG_WRITE_PARSE_HANDLER_NUM_ENTRY] =
{
    {
        .param_key = "\"version\"",
        .min_num_of_param_values = 1,
        .max_num_of_param_values = 2,
        .qmtcfg_write_parse_exec_handler = 
            QMTCFG_WRITE_PARSE_HANLDER_FUNC_NAME(version),
    },
    {
        .param_key = "\"pdpcid\"",
        .min_num_of_param_values = 1,
        .max_num_of_param_values = 2,
        .qmtcfg_write_parse_exec_handler = 
            QMTCFG_WRITE_PARSE_HANLDER_FUNC_NAME(pdpcid),
    },
    {
        .param_key = "\"keepalive\"",
        .min_num_of_param_values = 1,
        .max_num_of_param_values = 2,
        .qmtcfg_write_parse_exec_handler = 
            QMTCFG_WRITE_PARSE_HANLDER_FUNC_NAME(keepalive),
    },
    {
        .param_key = "\"session\"",
        .min_num_of_param_values = 1,
        .max_num_of_param_values = 2,
        .qmtcfg_write_parse_exec_handler = 
            QMTCFG_WRITE_PARSE_HANLDER_FUNC_NAME(session),
    },
    {
        .param_key = "\"timeout\"",
        .min_num_of_param_values = 1,
        .max_num_of_param_values = 4,
        .qmtcfg_write_parse_exec_handler = 
            QMTCFG_WRITE_PARSE_HANLDER_FUNC_NAME(timeout),
    },
    {
        .param_key = "\"recv/mode\"",
        .min_num_of_param_values = 1,
        .max_num_of_param_values = 3,
        .qmtcfg_write_parse_exec_handler = 
            QMTCFG_WRITE_PARSE_HANLDER_FUNC_NAME(recv_mode),
    },
    {
        .param_key = "\"send/mode\"",
        .min_num_of_param_values = 1,
        .max_num_of_param_values = 2,
        .qmtcfg_write_parse_exec_handler = 
            QMTCFG_WRITE_PARSE_HANLDER_FUNC_NAME(send_mode),
    },
};

//=================================
// Private functions declaration
//=================================
static void append_qmtcfg_version_usage(char *at_resp);
static void append_qmtcfg_pdpcid_usage(char *at_resp);
static void append_qmtcfg_keepalive_usage(char *at_resp);
static void append_qmtcfg_session_usage(char *at_resp);
static void append_qmtcfg_timeout_usage(char *at_resp);
static void append_qmtcfg_recv_mode_usage(char *at_resp);
static void append_qmtcfg_send_mode_usage(char *at_resp);
static void extract_param_key_from_at_cmd(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char* extracted_param_key);
static int find_qmtcfg_write_parse_exec_handler_for_param_key(
        char* param_key);
static bool is_num_of_param_value_valid(const char *arg, 
    AT_BUFF_SIZE_T arg_length,  int min_num_of_param_values, 
    int max_num_of_param_values);


//==============================
// Public functions definition
//==============================
AT_BUFF_SIZE_T qmtcfg_test_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    char *tmp_buff = sys_mem_calloc(MAX_AT_RESP_LENGTH, 1);
    sprintf(at_resp, "%s", tmp_buff);
    append_qmtcfg_version_usage(at_resp);
    append_qmtcfg_pdpcid_usage(at_resp);
    append_qmtcfg_keepalive_usage(at_resp);
    append_qmtcfg_session_usage(at_resp);
    append_qmtcfg_timeout_usage(at_resp);
    append_qmtcfg_recv_mode_usage(at_resp);
    append_qmtcfg_send_mode_usage(at_resp);
    memset(tmp_buff, 0, MAX_AT_RESP_LENGTH);
    sprintf(tmp_buff, "%sOK", at_resp);
    memcpy(at_resp, tmp_buff, strlen(tmp_buff) + 1);
    
    sys_mem_free(tmp_buff);
    return strlen(at_resp);
}


AT_BUFF_SIZE_T qmtcfg_write_cmd_parse_exec_handler(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    char param_key[PARAM_KEY_MAX_BUFF_SIZE];

    extract_param_key_from_at_cmd(arg, arg_length, param_key);

    int write_parse_exec_handler_entry_index = 
        find_qmtcfg_write_parse_exec_handler_for_param_key(param_key);

    bool is_write_parse_exec_handler_found = 
        (write_parse_exec_handler_entry_index != -1) ? true: false;

    if (!is_write_parse_exec_handler_found)
        RETURN_RESPONSE_UNSUPPORTED(at_resp);
        
    param_key_based_params_parser_table_entry_t found_entry = 
        qmtcfg_write_parse_exec_table[write_parse_exec_handler_entry_index];
    
    if (!is_num_of_param_value_valid(arg, arg_length, 
        found_entry.min_num_of_param_values, 
        found_entry.max_num_of_param_values
    ))
        RETURN_RESPONSE_ERROR(at_resp);

    return found_entry.qmtcfg_write_parse_exec_handler(arg, arg_length, at_resp);
    
    
}

//=================================
// Private functions definition
//=================================
QMTCFG_WRITE_PARSE_HANLDER(version)(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    INTIALIZE_QMTCFG_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp);

    GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(mqtt_version, mqtt_vsn_t,
        MQTT_VERSION_V3_1, MQTT_VERSION_V3_1_1, handler_tmp_buff, at_resp);
    
    FINALIZE_WRITE_PARSE_EXEC_HANDLE(at_resp);

}

QMTCFG_WRITE_PARSE_HANLDER(pdpcid)(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    INTIALIZE_QMTCFG_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp);

    GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(pdp_cid, uint8_t,
        MQTT_CID_MIN, MQTT_CID_MAX, handler_tmp_buff, at_resp);

    FINALIZE_WRITE_PARSE_EXEC_HANDLE(at_resp);
}

QMTCFG_WRITE_PARSE_HANLDER(keepalive) (const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    INTIALIZE_QMTCFG_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp);

    GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(keepalive_time_s, uint8_t,
        0, MQTT_MAX_KEEPALIVE_TIME_s, handler_tmp_buff, at_resp);

    FINALIZE_WRITE_PARSE_EXEC_HANDLE(at_resp);
}

QMTCFG_WRITE_PARSE_HANLDER(session) (const char *arg,
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    INTIALIZE_QMTCFG_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp);

    GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(session_type, uint8_t,
        SESSION_TYPE_STORE_AFTER_DISCONNECT, SESSION_TYPE_CLEAR_AFTER_DISCONNECT,
        handler_tmp_buff, at_resp);

    FINALIZE_WRITE_PARSE_EXEC_HANDLE(at_resp);
}

QMTCFG_WRITE_PARSE_HANLDER(timeout)(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    INTIALIZE_QMTCFG_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp);

    GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(timeout_info.pkt_timeout_s, uint8_t,
        MQTT_PACKET_MIN_TIMEOUT, MQTT_PACKET_MAX_TIMEOUT, handler_tmp_buff, at_resp);
    GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(timeout_info.max_num_of_retries,
        uint8_t, 0, MQTT_MAX_POSSIBLE_RETRIES, handler_tmp_buff, at_resp);
    GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(timeout_info.is_report_timeout_msg,
        bool, false, true, handler_tmp_buff, at_resp);

    FINALIZE_WRITE_PARSE_EXEC_HANDLE(at_resp);
}

QMTCFG_WRITE_PARSE_HANLDER(recv_mode) (const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    INTIALIZE_QMTCFG_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp);

    GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(recv_config.recv_mode,
        mqtt_recv_mode_t, MQTT_RX_MESS_CONTAINED_IN_URC,
        MQTT_RX_MESS_NOT_CONTAINED_IN_URC, 
        handler_tmp_buff, at_resp);
    GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(recv_config.recv_length_mode,
        mqtt_recv_length_mode_t, MQTT_RX_MESS_LENGTH_NOT_CONTAINED_IN_URC,
        MQTT_RX_MESS_LENGTH_CONTAINED_IN_URC, 
        handler_tmp_buff, at_resp);

    FINALIZE_WRITE_PARSE_EXEC_HANDLE(at_resp);
}

QMTCFG_WRITE_PARSE_HANLDER(send_mode) (const char *arg, 
    AT_BUFF_SIZE_T arg_length, char *at_resp)
{
    INTIALIZE_QMTCFG_WRITE_PARSE_EXEC_HANDLER(arg, arg_length, at_resp);

    GET_OPTIONAL_PARAM_AND_ASSIGN_TO_CONFIG_FIELD(send_mode, mqtt_send_mode_t,
        MQTT_SEND_MODE_STRING, MQTT_SEND_MODE_HEX, handler_tmp_buff, at_resp);

    FINALIZE_WRITE_PARSE_EXEC_HANDLE(at_resp);
}

//===============================
// Private functions definition
//===============================
static void append_qmtcfg_version_usage(char *at_resp)
{
    APPEND_START_OF_USAGE(at_resp, "version");
    APPEND_RANGE_STRING(at_resp, MQTT_VERSION_V3_1, MQTT_VERSION_V3_1_1);
    APPEND_END_OF_USAGE(at_resp);
}

static void append_qmtcfg_pdpcid_usage(char *at_resp)
{
    APPEND_START_OF_USAGE(at_resp, "pdpcid");
    APPEND_RANGE_STRING(at_resp, MQTT_CID_MIN, MQTT_CID_MAX);
    APPEND_END_OF_USAGE(at_resp);
}

static void append_qmtcfg_keepalive_usage(char *at_resp)
{
    APPEND_START_OF_USAGE(at_resp, "keepalive");
    APPEND_RANGE_STRING(at_resp, MQTT_CID_MIN, MQTT_CID_MAX);
    APPEND_END_OF_USAGE(at_resp);
}

static void append_qmtcfg_session_usage(char *at_resp)
{
    APPEND_START_OF_USAGE(at_resp, "keepalive");
    APPEND_RANGE_STRING(at_resp, SESSION_TYPE_STORE_AFTER_DISCONNECT, 
        SESSION_TYPE_CLEAR_AFTER_DISCONNECT);
    APPEND_END_OF_USAGE(at_resp);
}

static void append_qmtcfg_timeout_usage(char *at_resp)
{
    APPEND_START_OF_USAGE(at_resp, "timeout");
    APPEND_RANGE_STRING(at_resp, MQTT_PACKET_MIN_TIMEOUT, 
        MQTT_PACKET_MAX_TIMEOUT);
    APPEND_RANGE_STRING(at_resp, 0, 
        MQTT_MAX_POSSIBLE_RETRIES);
    APPEND_RANGE_STRING(at_resp, false, 
        true);
    APPEND_END_OF_USAGE(at_resp);
}
static void append_qmtcfg_recv_mode_usage(char *at_resp)
{
    APPEND_START_OF_USAGE(at_resp, "recv/mode");
    APPEND_RANGE_STRING(at_resp, MQTT_RX_MESS_CONTAINED_IN_URC, 
        MQTT_RX_MESS_NOT_CONTAINED_IN_URC);
    APPEND_RANGE_STRING(at_resp, MQTT_RX_MESS_LENGTH_NOT_CONTAINED_IN_URC, 
        MQTT_RX_MESS_LENGTH_CONTAINED_IN_URC);
    APPEND_END_OF_USAGE(at_resp);
}
static void append_qmtcfg_send_mode_usage(char *at_resp)
{
    APPEND_START_OF_USAGE(at_resp, "send/mode");
    APPEND_RANGE_STRING(at_resp, MQTT_SEND_MODE_STRING, 
        MQTT_SEND_MODE_HEX);
    APPEND_END_OF_USAGE(at_resp);
}

static void extract_param_key_from_at_cmd(const char *arg, 
    AT_BUFF_SIZE_T arg_length, char* extracted_param_key)
{
    const char *arg_without_equal_sign = arg + 1 * sizeof (char);
    AT_BUFF_SIZE_T arg_length_after_trim_equal_sign = arg_length -  
        1 * sizeof (char);
    char *tmp_buff = sys_mem_calloc(arg_length + 1, 1);
    char *tokenize_context;
    char *token;
    memcpy(tmp_buff, arg_without_equal_sign, arg_length_after_trim_equal_sign);

    token = sys_strtok(tmp_buff, param_delim, &tokenize_context);

    memcpy(extracted_param_key, token, strlen(token));

    free(tmp_buff);
}

static int find_qmtcfg_write_parse_exec_handler_for_param_key(
        char* param_key)
{
    for (int entry_index = 0; entry_index < QMTCFG_WRITE_PARSE_HANDLER_NUM_ENTRY;
        entry_index++)
    {
        param_key_based_params_parser_table_entry_t checking_entry = 
            qmtcfg_write_parse_exec_table[entry_index];
        if (!strcmp(checking_entry.param_key, param_key))
            return entry_index;
    }
    return -1;
}

static bool is_num_of_param_value_valid(const char *arg, 
    AT_BUFF_SIZE_T arg_length,  int min_num_of_param_values, 
    int max_num_of_param_values)
{
    char *tmp_buff = sys_mem_calloc(arg_length + 1, 1);
    char *tokenize_context;
    char *token;
    memcpy(tmp_buff, arg, arg_length);

    token = sys_strtok(tmp_buff, param_delim, &tokenize_context);

    for (int try = 0; try < min_num_of_param_values; try++)
    {
        token = sys_strtok(NULL, param_delim, &tokenize_context);
        if (token == NULL)
        {
            FREE_BUFF_AND_RETURN_VAL(tmp_buff, 
                false);
        }
    }

    for (int try = min_num_of_param_values; try < max_num_of_param_values; try++)
    {
        token = sys_strtok(NULL, param_delim, &tokenize_context);
        if (token == NULL)
        {
            FREE_BUFF_AND_RETURN_VAL(tmp_buff, true);
        }
    }
    token = sys_strtok(NULL, param_delim, &tokenize_context);
    if (token != NULL)
    {
        FREE_BUFF_AND_RETURN_VAL(tmp_buff, 
            false);
    }
    else
    {
        FREE_BUFF_AND_RETURN_VAL(tmp_buff, true);
    }

}