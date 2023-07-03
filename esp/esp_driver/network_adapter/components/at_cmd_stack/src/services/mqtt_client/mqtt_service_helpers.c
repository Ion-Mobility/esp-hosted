#include "sys_common_funcs.h"
#include "mqtt_service_helpers.h"
#include "mqtt_service_configs.h"
#include "mqtt_service_types.h"

static const char * ver_string[] =
{
    [0] = "dummy",
    [1] = "dummy",
    [2] = "dummy",
    [MQTT_VERSION_V3_1] = "v3.1",
    [MQTT_VERSION_V3_1_1] = "v3.1.1",
};

static const char * session_type_string[] =
{
    [SESSION_TYPE_STORE_AFTER_DISCONNECT] = 
        "STORE client info after disconnect",
    [SESSION_TYPE_CLEAR_AFTER_DISCONNECT] = 
        "CLEAR client info after disconnect",
};

static const char * recv_mode_string[] =
{
    [MQTT_RX_MESS_CONTAINED_IN_URC] = 
        "message contained in URC",
    [MQTT_RX_MESS_NOT_CONTAINED_IN_URC] = 
        "message NOT contained in URC",
};

static const char * recv_len_mode_string[] =
{
    [MQTT_RX_MESS_LENGTH_NOT_CONTAINED_IN_URC] = 
        "message length NOT contained in URC",
    [MQTT_RX_MESS_LENGTH_CONTAINED_IN_URC] = 
        "message length contained in URC",
};

static const char * send_mode_string[] =
{
    [MQTT_SEND_MODE_STRING] = 
        "send mode STRING",
    [MQTT_SEND_MODE_HEX] = 
        "send mode HEX",
};

static const char * will_qos_string[] =
{
    [MQTT_QOS_AT_MOST_ONCE] = 
        "at most once",
    [MQTT_QOS_AT_LEAST_ONCE] = 
        "at least once",
    [MQTT_QOS_EXACTLY_ONCE] = 
        "exactly once",
};

static const char * yes_no_string[] =
{
    [0] = "No",
    [1] = "Yes",
};


static void print_timeout_info(timeout_info_t timeout_info);
static void print_will_info(will_info_t will_info);
static void print_recv_config(mqtt_recv_info_t recv_info);


void print_at_mqtt_client_config(int client_index)
{
    mqtt_service_client_cfg_t *get_client_cfg = 
        &mqtt_service_client_cfg_table[client_index];
    sys_printf("=== Print out AT MQTT client config #%d ===\n", client_index);
    sys_printf("> Version: %s (%d)\n", 
        ver_string[get_client_cfg->mqtt_version], 
        get_client_cfg->mqtt_version);

    sys_printf("> PDPCID: %d\n",
        get_client_cfg->pdp_cid);

    sys_printf("> Keep alive time (s): %d\n",
        get_client_cfg->keepalive_time_s);

    sys_printf("> Session type: %s (%d)\n",
        session_type_string[get_client_cfg->session_type],
        get_client_cfg->session_type);

    print_timeout_info(get_client_cfg->timeout_info);
    print_will_info(get_client_cfg->will_info);
    print_recv_config(get_client_cfg->recv_config);

    sys_printf("> Send mode: %s (%d)\n", 
        send_mode_string[get_client_cfg->send_mode], 
        get_client_cfg->send_mode);
    sys_printf("=== Done printing ===\n");
}

static void print_timeout_info(timeout_info_t timeout_info)
{
    sys_printf("> Timeout info:\n");
    sys_printf("  > Packet timeout: %d seconds\n",
        timeout_info.pkt_timeout_s);
    sys_printf("  > Max retries timeout: %d\n",
        timeout_info.max_num_of_retries);
    sys_printf("  > Timeout notice: %s\n",
        yes_no_string[timeout_info.is_report_timeout_msg]);
}

static void print_will_info(will_info_t will_info)
{
    sys_printf("> Will info:\n");
    sys_printf("  > Is required will flag: %s\n",
        yes_no_string[will_info.is_require_will_flag]);
    sys_printf("  > Will QOS: %s (%d)\n",
        will_qos_string[will_info.will_qos],
        will_info.will_qos);
    sys_printf("  > Is will retained: %s\n",
        yes_no_string[will_info.is_will_retain]);
    
    if (will_info.will_topic != NULL)
    {
        sys_printf("  > WiLL topic: %s\n",
            will_info.will_topic);
    }
    if (will_info.will_message != NULL)
    {    
        sys_printf("  > WiLL message: %s\n",
            will_info.will_message);
    }
    sys_printf("  > WiLL message length: %d\n", 
        will_info.will_message_length);
}

static void print_recv_config(mqtt_recv_info_t recv_info)
{
    sys_printf("> Receive info:\n");
    sys_printf("  > Receive mode: %s (%d)\n",
        recv_mode_string[recv_info.recv_mode],
        recv_info.recv_mode);
    sys_printf("  > Receive length mode: %s (%d)\n",
        recv_len_mode_string[recv_info.recv_length_mode],
        recv_info.recv_length_mode);
}