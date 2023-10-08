#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/queue.h"

#define BLE_MSG_MAX_LEN         235

typedef enum {
    UNPAIRED            = 0,
    PAIRED              = 1,
    SESSION_CREATED     = 2
} pair_status_t;

typedef enum {
    // between phone & ble
    PHONE_BLE_PAIRING           = 0x00,
    PHONE_BLE_SESSION           = 0x01,
    PHONE_BLE_BATTERY           = 0x02,
    PHONE_BLE_CHARGE            = 0x03,
    PHONE_BLE_LAST_TRIP         = 0x04,
    PHONE_BLE_STEERING_LOCK     = 0x05,
    PHONE_BLE_STEERING_UNLOCK   = 0x06,
    PHONE_BLE_PING_BIKE         = 0x07,
    PHONE_BLE_OPEN_SEAT         = 0x08,
    PHONE_BLE_DIAG              = 0x09,

    // between tm & ble
    TM_BLE_BATTERY              = 0x50,
    TM_BLE_CHARGE               = 0x51,
    TM_BLE_LAST_TRIP            = 0x52,
    TM_BLE_STEERING_LOCK        = 0x53,
    TM_BLE_STEERING_UNLOCK      = 0x54,
    TM_BLE_PING_BIKE            = 0x55,
    TM_BLE_OPEN_SEAT            = 0x56,
    TM_BLE_DIAG                 = 0x57,
    TM_BLE_PAIRING              = 0x58,
    TM_BLE_PAIRED               = 0x59,
    TM_BLE_DISCONNECT           = 0x5A,
    // add new msg here
    TM_BLE_INVALID              = 0x5B,

    BLE_START_ADVERTISE         = 0xA0,
    BLE_DISCONNECT              = 0xA1,
    BLE_CONNECT                 = 0xA2
} ble_msg_id_t;

typedef struct {
    int msg_id;
    int len;
    uint8_t data[BLE_MSG_MAX_LEN];
} ble_msg_t;

extern QueueHandle_t ble_queue;
extern void tm_ble_init(void);
extern void tm_ble_start_advertise(void);