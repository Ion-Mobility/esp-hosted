#include "mqtt_service_configs.h"



mqtt_service_client_cfg_t 
    mqtt_service_client_cfg_table[MAX_NUM_OF_MQTT_CLIENT] = 
{
    {
        .mqtt_version = MQTT_CLIENT_VERSION_DEFAULT,
        .pdp_cid = MQTT_CLIENT_PDPCID_DEFAULT,
        .keepalive_time_s = MQTT_CLIENT_KEEPALIVE_S_DEFAULT,
        .session_type = MQTT_CLIENT_SESSION_TYPE_DEFAULT,
        .timeout_info = {
            .pkt_timeout_s = MQTT_CLIENT_PACKET_TIMEOUT_S_DEFAULT,
            .max_num_of_retries = MQTT_CLIENT_RETRY_TIMES_DEFAULT,
            .is_report_timeout_msg = MQTT_CLIENT_TIMEOUT_NOTICE_DEFAULT,
        },
        .will_info = {
            .is_require_will_flag = MQTT_CLIENT_WILL_FG_DEFAULT,
            .is_will_retain = MQTT_CLIENT_WILL_RETAIN_DEFAULT,
            .will_qos = MQTT_CLIENT_WILL_QOS_DEFAULT,
        },
        .recv_config = {
            .recv_mode = MQTT_CLIENT_RECV_MODE_DEFAULT,
            .recv_length_mode = MQTT_CLIENT_RECV_LENGTH_MODE_DEFAULT,
        },
        .send_mode = MQTT_CLIENT_SEND_MODE_DEFAULT,
    }
};
