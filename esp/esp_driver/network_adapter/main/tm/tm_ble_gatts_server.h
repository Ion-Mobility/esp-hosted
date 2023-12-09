#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/event_groups.h"

extern void ble_gatts_start_advertise(void);
extern void tm_ble_gatts_server_init(void);
extern void tm_ble_gatts_send_to_phone(uint8_t *data, int len);
extern void tm_ble_gatts_kill_connection(void);
extern uint32_t EndianConverter(uint32_t *byteArray);
extern size_t serialize_data(uint8_t *buf, size_t pos, uint8_t *data, size_t len);
