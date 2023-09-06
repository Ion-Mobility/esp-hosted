#ifndef _APP_TO_SERVICE_H_
#define _APP_TO_SERVICE_H_
#include "at_cmd_stack_types.h"
#include "mqtt_service_types.h"

/**
 * @brief Check if upper application can accept incoming MQTT receive buff
 * 
 * @param client_index index of client [in]
 * @param recv_buff pointer to receive buff from MQTT service [in]
 * @retval 1 if upper application CAN accept
 * @retval 0 if upper application CANNOT accept
 */
extern int can_app_accept_incoming_recv_buff(int client_index, 
    const recv_buffer_t *recv_buff);

#endif
