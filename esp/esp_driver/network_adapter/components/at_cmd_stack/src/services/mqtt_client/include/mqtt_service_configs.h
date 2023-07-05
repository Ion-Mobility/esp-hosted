#ifndef _AT_MQTT_CLIENT_CONFIG_H_
#define _AT_MQTT_CLIENT_CONFIG_H_

#include "mqtt_service_types.h"

#define MAX_NUM_OF_MQTT_CLIENT  1

extern mqtt_service_client_cfg_t 
    mqtt_service_client_cfg_table[MAX_NUM_OF_MQTT_CLIENT];

#endif
