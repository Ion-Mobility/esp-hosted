#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/queue.h"

#define BLE_MSG_MAX_LEN         512

typedef enum {
    BLE_START_ADVERTISE         =0,
    BLE_DISCONNECT              =1,
    BLE_CONNECTING              =2,
    BLE_CONNECTED               =3,
    BLE_AUTHEN                  =4,
    BLE_PAIRING                 =5,
    BLE_SESSION                 =6,
    BLE_PHONE_READ              =7,
    BLE_PHONE_WRITE             =8,
    BLE_TM_MSG                  =9,
    BLE_INVALID_MSG             =10
} ble_msg_id_t;

typedef struct {
    uint8_t msg_id;
    int len;
    uint8_t data[BLE_MSG_MAX_LEN];
} ble_msg_t;

extern QueueHandle_t ble_queue;
extern void tm_ble_init(void);
extern void tm_ble_start_advertise(void);