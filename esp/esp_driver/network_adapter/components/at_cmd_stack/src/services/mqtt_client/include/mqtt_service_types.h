#ifndef _AT_MQTT_CLIENT_TYPES_H_
#define _AT_MQTT_CLIENT_TYPES_H_

#include <stdint.h>
#include <stdbool.h>


#define MQTT_CID_MIN            1
#define MQTT_CID_MAX            7

#define MQTT_MAX_POSSIBLE_RETRIES  10

#define MQTT_MAX_KEEPALIVE_TIME_s  3600

#define MQTT_PACKET_MIN_TIMEOUT 1
#define MQTT_PACKET_MAX_TIMEOUT 60

#define MQTT_CLIENT_VERSION_DEFAULT MQTT_VERSION_V3_1
#define MQTT_CLIENT_PDPCID_DEFAULT 1
#define MQTT_CLIENT_WILL_FG_DEFAULT false
#define MQTT_CLIENT_WILL_QOS_DEFAULT MQTT_QOS_AT_MOST_ONCE
#define MQTT_CLIENT_WILL_RETAIN_DEFAULT false
#define MQTT_CLIENT_PACKET_TIMEOUT_S_DEFAULT 5
#define MQTT_CLIENT_RETRY_TIMES_DEFAULT 3
#define MQTT_CLIENT_TIMEOUT_NOTICE_DEFAULT false
#define MQTT_CLIENT_SESSION_TYPE_DEFAULT \
    SESSION_TYPE_CLEAR_AFTER_DISCONNECT
#define MQTT_CLIENT_KEEPALIVE_S_DEFAULT 120
#define MQTT_CLIENT_RECV_MODE_DEFAULT MQTT_RX_MESS_CONTAINED_IN_URC
#define MQTT_CLIENT_RECV_LENGTH_MODE_DEFAULT \
    MQTT_RX_MESS_LENGTH_NOT_CONTAINED_IN_URC
#define MQTT_CLIENT_RECV_LENGTH_MODE_DEFAULT \
    MQTT_RX_MESS_LENGTH_NOT_CONTAINED_IN_URC
#define MQTT_CLIENT_SEND_MODE_DEFAULT \
    MQTT_SEND_MODE_STRING



typedef enum {
    MQTT_VERSION_V3_1 = 3,
    MQTT_VERSION_V3_1_1 = 4,
} mqtt_vsn_t;

typedef enum {
    SESSION_TYPE_STORE_AFTER_DISCONNECT = 0,
    SESSION_TYPE_CLEAR_AFTER_DISCONNECT,
} mqtt_session_type_t;

typedef enum {
    MQTT_QOS_AT_MOST_ONCE = 0,
    MQTT_QOS_AT_LEAST_ONCE,
    MQTT_QOS_EXACTLY_ONCE,
} mqtt_qos_t;


typedef enum {
    MQTT_RX_MESS_CONTAINED_IN_URC = 0,
    MQTT_RX_MESS_NOT_CONTAINED_IN_URC,
} mqtt_recv_mode_t;

typedef enum {
    MQTT_RX_MESS_LENGTH_NOT_CONTAINED_IN_URC = 0,
    MQTT_RX_MESS_LENGTH_CONTAINED_IN_URC,
} mqtt_recv_length_mode_t;

typedef enum {
    MQTT_SEND_MODE_STRING = 0,
    MQTT_SEND_MODE_HEX,
} mqtt_send_mode_t;


typedef enum {
    MQTT_SERVICE_PACKET_STATUS_OK = 0,
    MQTT_SERVICE_PACKET_STATUS_RETRANSMISSION = 1,
    MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND = 2,
} mqtt_service_pkt_status_t;

typedef enum {
    MQTT_CONNECTION_STATUS_NOT_CONNECTED = 0,
    MQTT_CONNECTION_STATUS_CONNECTED = 1,
} mqtt_client_connection_status_t;

typedef struct {
    uint8_t pkt_timeout_s;
    uint8_t max_num_of_retries;
    bool is_report_timeout_msg;
} timeout_info_t;

typedef struct {
    bool is_require_will_flag;
    mqtt_qos_t will_qos;
    bool is_will_retain;
    char will_topic[256];
    char will_message[256];
    uint16_t will_message_length;
} will_info_t;

typedef struct {
    mqtt_recv_mode_t recv_mode;
    mqtt_recv_length_mode_t recv_length_mode;
} mqtt_recv_info_t;

typedef struct {
    mqtt_vsn_t mqtt_version;
    uint8_t pdp_cid;
    uint16_t keepalive_time_s;
    mqtt_session_type_t session_type;
    timeout_info_t timeout_info;
    will_info_t will_info;
    mqtt_recv_info_t recv_config;
    mqtt_send_mode_t send_mode;
} mqtt_service_client_cfg_t;

#endif
