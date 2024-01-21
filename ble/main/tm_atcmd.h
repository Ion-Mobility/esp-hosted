#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLE_TO_TM_MSG_MAX_LEN   128

typedef enum {
    OFF = 0,
    ON = 1,
    STATE = 2
} on_off_state_t;

typedef struct {
    uint8_t level;
    uint16_t estimate_range;
    uint16_t time_to_full;
} battery_t;

typedef struct {
    int msg_id;
    int len;
    uint8_t data[BLE_TO_TM_MSG_MAX_LEN];
} ble_to_tm_msg_t;

extern void tm_atcmd_tasks_init(void);
extern void send_to_tm_queue(ble_to_tm_msg_t *msg);