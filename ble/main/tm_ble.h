#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/queue.h"
#include "tm_ble_gatts_server.h"

typedef enum {
    UNPAIRED            = 0,
    PAIRED              = 1,
    SESSION_CREATED     = 2
} pair_status_t;

typedef enum {
    // between phone & ble
    PHONE_BLE_PAIRING           = 0x00,
    PHONE_BLE_SESSION           = 0x01,
    //below commands only available after session is successfully created
    PHONE_BLE_COMMAND           = 0x02,

    TM_BLE_GET_TIME             = 0xFA,
    BLE_START_ADVERTISE         = 0xFB,
    BLE_CONNECT                 = BLE_CONNECT_EVENT,
    BLE_DISCONNECT              = BLE_DISCONNECT_EVENT,
    TM_BLE_FAIL                 = 0xFE,
    TM_BLE_OK                   = 0xFF
} ble_msg_id_t;

extern QueueHandle_t ble_queue;
extern void tm_ble_init(void);
extern void tm_ble_start_advertise(void);
extern void send_to_phone(ble_msg_t *pble_msg);