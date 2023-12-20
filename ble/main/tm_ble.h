#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/queue.h"

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
    PHONE_BLE_BATTERY           = 0x03,
    PHONE_BLE_LAST_TRIP         = 0x04,
    PHONE_BLE_STEERING          = 0x05,
    PHONE_BLE_PING_BIKE         = 0x06,
    PHONE_BLE_OPEN_SEAT         = 0x07,
    PHONE_BLE_DIAG              = 0x08,
    PHONE_BLE_BIKE_INFO         = 0x09,
    PHONE_BLE_POWER_ON          = 0x0A,
    PHONE_BLE_CELLULAR          = 0x0B,
    PHONE_BLE_LOCATION          = 0x0C,
    PHONE_BLE_RIDE_TRACKING     = 0x0D,
    //add new command here

    // between tm & ble
    TM_BLE_PAIRING              = 0x50,
    TM_BLE_SESSION              = 0x51,
    TM_BLE_DISCONNECT           = 0x52,
    TM_BLE_BATTERY              = 0x53,
    TM_BLE_LAST_TRIP            = 0x54,
    TM_BLE_STEERING             = 0x55,
    TM_BLE_PING_BIKE            = 0x56,
    TM_BLE_OPEN_SEAT            = 0x57,
    TM_BLE_DIAG                 = 0x58,
    TM_BLE_BIKE_INFO            = 0x59,
    TM_BLE_POWER_ON             = 0x5A,
    TM_BLE_CELLULAR             = 0x5B,
    TM_BLE_LOCATION             = 0x5C,
    TM_BLE_RIDE_TRACKING        = 0x5D,
    // add new msg here
    TM_BLE_FAIL                 = 0x9E,
    TM_BLE_OK                   = 0x9F,

    BLE_START_ADVERTISE         = 0xA0,
    BLE_DISCONNECT              = 0xA1,
    BLE_CONNECT                 = 0xA2
} ble_msg_id_t;

extern QueueHandle_t ble_queue;
extern void tm_ble_init(void);
extern void tm_ble_start_advertise(void);