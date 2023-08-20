#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/event_groups.h"

extern void tm_ble_gatts_start_advertise(void);
extern void tm_ble_gatts_server_init(void);
extern void update_read_response(uint8_t *data, int len);
