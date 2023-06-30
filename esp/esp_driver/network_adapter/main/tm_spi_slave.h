#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void spi_slave_task_init(void);
extern void spi_slave_send_ble_connect_event(void);
extern void spi_slave_display_6digits_pairing_number(void);
extern void spi_slave_send_ble_disconnect_event(void);