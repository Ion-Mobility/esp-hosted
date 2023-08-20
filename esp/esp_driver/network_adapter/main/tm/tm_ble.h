#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/queue.h"

#define BLE_MSG_MAX_LEN         512

typedef enum {
    //from phone or internal ble
    BLE_START_ADVERTISE         =0,
    BLE_DISCONNECT              =1,
    BLE_CONNECTING              =2,
    BLE_CONNECTED               =3,
    BLE_AUTHEN                  =4,
    BLE_PAIRING                 =5,
    BLE_SESSION                 =6,
    BLE_PHONE_READ              =7,
    BLE_PHONE_WRITE             =8,
    // from tm to ble
    TM_BLE_LOGIN                =9,
    TM_BLE_CHARGE               =10,
    TM_BLE_BATTERY              =11,
    TM_BLE_LAST_TRIP            =12,
    TM_BLE_LOCK                 =13,
    TM_BLE_UNLOCK               =14,
    //add new msg code here
    BLE_INVALID_MSG             =15
} ble_msg_id_t;

typedef struct {
    uint8_t msg_id;
    int len;
    uint8_t data[BLE_MSG_MAX_LEN];
} ble_msg_t;

extern QueueHandle_t ble_queue;
extern void tm_ble_init(void);
extern void tm_ble_start_advertise(void);