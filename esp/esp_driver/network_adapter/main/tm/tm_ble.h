#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/queue.h"

#define BLE_MSG_MAX_LEN         512

typedef enum {
    UNPAIRED            = 0,
    PAIRED              = 1,
    SESSION_VERIFIED    = 2,
    RD_DATA_READY       = 3
} phone_request_status_t;

typedef enum {
    //from phone or internal ble
    BLE_START_ADVERTISE         =0,
    BLE_DISCONNECT              =1,
    BLE_CONNECTING              =2,
    BLE_CONNECTED               =3,
    BLE_PHONE_READ              =4,
    BLE_PHONE_WRITE             =5,
    // from tm to ble
    TM_BLE_LOGIN                =6,
    TM_BLE_CHARGE               =7,
    TM_BLE_BATTERY              =8,
    TM_BLE_LAST_TRIP            =9,
    TM_BLE_LOCK                 =10,
    TM_BLE_UNLOCK               =11,
    //add new msg code here
    BLE_INVALID_MSG             =12
} ble_msg_id_t;

typedef struct {
    uint8_t msg_id;
    int len;
    uint8_t data[BLE_MSG_MAX_LEN];
} ble_msg_t;

extern QueueHandle_t ble_queue;
extern void tm_ble_init(void);
extern void tm_ble_start_advertise(void);
extern uint8_t tm_ble_get_phone_request_status(void);
extern void tm_ble_set_phone_request_status(uint8_t status);