#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/event_groups.h"
/* Attributes State Machine */
enum
{
    IDX_SVC,
    IDX_CHAR_A,
    IDX_CHAR_VAL_A,
    IDX_CHAR_CFG_A,

    IDX_CHAR_B,
    IDX_CHAR_VAL_B,

    IDX_CHAR_C,
    IDX_CHAR_VAL_C,

    HRS_IDX_NB,
};

typedef enum {
    CONNECTING = BIT0,          //ESP_GATTS_CONNECT_EVT
    REQUEST_TO_PAIR = BIT1,     //ESP_GAP_BLE_NC_REQ_EVT
    CONFIRM_PASSKEY = BIT2,     //ESP_GAP_BLE_KEY_EVT
    START_ADVERTISE = BIT3,
    DISCONNECT = BIT4,
    CONNECTED = BIT5
} ion_ble_event_t;

extern EventGroupHandle_t ion_ble_event_group;

extern void tm_ble_start_advertising(void);
extern void tm_ble_gatts_server_init(void);
