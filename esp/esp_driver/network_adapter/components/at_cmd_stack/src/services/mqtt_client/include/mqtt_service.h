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

#endif