#ifndef _MQTT_SERVICE_H_
#define _MQTT_SERVICE_H_

#include "mqtt_service_configs.h"
#include "mqtt_service_types.h"
#include "at_cmd_stack_types.h"
#include "mqtt_client.h"
#include <stdint.h>

typedef enum {
    MQTT_SERVICE_STATUS_OK = 0,
    MQTT_SERVICE_STATUS_ERROR
} mqtt_service_status_t;


/**
 * @brief Initialize MQTT service
 * 
 * @return MQTT_SERVICE_STATUS_OK if initialize successfully 
 */
extern mqtt_service_status_t mqtt_service_init();

/**
 * @brief Get connection status of particular client
 * 
 * @param client_index index of MQTT client
 * @retval MQTT_CONNECTION_STATUS_NOT_CONNECTED if MQTT client does not connect
 * to any MQTT broker
 * @retval MQTT_CONNECTION_STATUS_CONNECTED if MQTT client has connected to a 
 * MQTT broker
 */
extern mqtt_client_connection_status_t mqtt_service_get_connection_status(
    int client_index);


/**
 * @brief Issue a MQTT client with given index to connect to MQTT broker
 * 
 * @param client_index index of MQTT client [in]
 * @param client_id client ID of this MQTT connect [in]
 * @param hostname hostname of MQTT broker [in]
 * @param username (optional) username of connecting MQTT broker. 
 * Can be NULL if not used [in]
 * @param pass (optional) password of connecting MQTT broker. Can be NULL
 * if not used [in]
 * @param port port of MQTT broker [in]
 * @param connect_ret_code return code of connection request [out]
 * @retval MQTT_SERVICE_PACKET_STATUS_OK if connection to MQTT broker is good
 * @retval MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND if MQTT client failed to send
 */
extern mqtt_service_pkt_status_t mqtt_service_connect(int client_index,
    const char *client_id, const char *hostname, const char *username,
    const char *pass, uint16_t port,
    esp_mqtt_connect_return_code_t *connect_ret_code);


/**
 * @brief Issue a MQTT client to connect to specified topic and QOS level
 * 
 * @param client_index index of MQTT client [in]
 * @param topic NULL-terminated string of topic [in]
 * @param qos QOS level, can be MQTT_QOS_AT_MOST_ONCE, MQTT_QOS_AT_LEAST_ONCE
 * or MQTT_QOS_EXACTLY_ONCE [in]
 * @retval MQTT_SERVICE_PACKET_STATUS_OK if subscribe topic is good
 * @retval MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND if MQTT client failed to
 * send subscribtion packet due to connectivity, not connect to broker or no
 * ACK from server
 */
extern mqtt_service_pkt_status_t mqtt_service_subscribe(int client_index,
    const char *topic, mqtt_qos_t qos);

/**
 * @brief Issue a MQTT client to publish a message to particular topic
 * 
 * @param client_index index of MQTT client [in]
 * @param topic NULL-terminated string of topic [in]
 * @param msg NULL-terminated string of message need to publish [in]
 * @param qos QOS level, can be MQTT_QOS_AT_MOST_ONCE, MQTT_QOS_AT_LEAST_ONCE
 * or MQTT_QOS_EXACTLY_ONCE [in]
 * @param is_retain whether published is retained or not [in]
 * @retval MQTT_SERVICE_PACKET_STATUS_OK if publish message to topic is good
 * @retval MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND if MQTT client failed to
 * send publish packet due to connectivity, not connect to broker or no
 * ACK from server
 */
extern mqtt_service_pkt_status_t mqtt_service_publish(int client_index,
    const char *topic, const char* msg, mqtt_qos_t qos,
    bool is_retain);

/**
 * @brief Only get the status of a MQTT client's receive buffer group status
 * 
 * @param client_index index of MQTT client [in]
 * @param out_recv_buff_group pointer to output receive buffer group. Only
 * status part 'is_unread` is updated [out]
 * @retval MQTT_SERVICE_STATUS_OK if get receive buffer successfully 
 * @retval MQTT_SERVICE_STATUS_ERROR if MQTT service is not initialized or
 * no connection to MQTT broker
 */
extern mqtt_service_status_t mqtt_service_get_recv_buff_group_status(
    int client_index, recv_buffer_group_t *out_recv_buff_group);

/**
 * @brief Transfer receive buffer group of a client from MQTT service to caller
 * This APIs also de-init MQTT service receive buffer group of that client
 * @note Caller must free the topic and message buffer
 * 
 * @param client_index index of MQTT client [in]
 * @param out_recv_buff_group pointer to output receive buffer group [out]
 * @retval MQTT_SERVICE_STATUS_OK if get receive buffer successfully 
 * @retval MQTT_SERVICE_STATUS_ERROR if MQTT service is not initialized or
 * no connection to MQTT broker
 */
extern mqtt_service_status_t mqtt_service_get_recv_buff_group(
    int client_index, recv_buffer_group_t *out_recv_buff_group);

/**
 * @brief Clear the receive buffer group of a MQTT client
 * 
 * @param client_index index of MQTT client [in]
 * @param out_recv_buff_group pointer to output receive buffer group [out]
 * @retval MQTT_SERVICE_STATUS_OK if get receive buffer successfully 
 * @retval MQTT_SERVICE_STATUS_ERROR if MQTT service is not initialized or
 * no connection to MQTT broker
 */
extern mqtt_service_status_t mqtt_service_clear_recv_buff_group(
    int client_index);

#endif