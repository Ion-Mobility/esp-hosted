#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/queue.h"

#define BLE_MSG_MAX_LEN         504

typedef enum {
    UNPAIRED            = 0,
    PAIRED              = 1,
    SESSION_CREATED     = 2
} pair_status_t;

typedef enum {
    // between phone & ble
    PHONE_BLE_PAIRING           =0,
    PHONE_BLE_SESSION           =1,
    PHONE_BLE_GENERAL           =2,
    PHONE_BLE_BATTERY           =3,
    PHONE_BLE_CHARGE            =4,
    PHONE_BLE_LAST_TRIP         =5,
    // between tm & ble
    TM_BLE_GENERAL              =6,
    TM_BLE_BATTERY              =7,
    TM_BLE_CHARGE               =8,
    TM_BLE_LAST_TRIP            =9,
    TM_BLE_LOCK                 =10,
    TM_BLE_UNLOCK               =11,

    BLE_START_ADVERTISE         =12,
    BLE_DISCONNECT              =13,
    BLE_CONNECT                 =14,

    BLE_INVALID_MSG             =15
} ble_msg_id_t;

typedef struct {
    int msg_id;
    int len;
    uint8_t data[BLE_MSG_MAX_LEN];
} ble_msg_t;

extern QueueHandle_t ble_queue;
extern void tm_ble_init(void);
extern void tm_ble_start_advertise(void);