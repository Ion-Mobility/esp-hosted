#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLE_TO_TM_MSG_MAX_LEN   128

typedef struct {
    bool state;
    uint16_t vol;    //mV
    uint16_t cur;    //mA
    uint16_t cycle;
    uint16_t time_to_full;
} charge_t;

typedef struct {
    uint8_t level;
    uint16_t estimate_range;
} battery_t;

typedef struct {
    uint16_t distance;
    uint16_t ride_time;
    uint16_t elec_used;
} trip_t;

typedef struct {
    int msg_id;
    int len;
    uint8_t data[BLE_TO_TM_MSG_MAX_LEN];
} ble_to_tm_msg_t;

extern void tm_atcmd_tasks_init(void);
extern void send_to_tm_queue(int msg_id, uint8_t *data, int len);