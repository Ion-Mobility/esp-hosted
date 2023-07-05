#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int8_t battery;
    uint16_t est_range;
    uint32_t odo;
    uint16_t last_trip_distance;
    uint16_t last_trip_time;
    uint16_t last_trip_elec_used;
    uint16_t last_charge_level;
    uint16_t distance_since_last_charge;
} login_t;

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

extern void tm_atcmd_tasks_init(void);