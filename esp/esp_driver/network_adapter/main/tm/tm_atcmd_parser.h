#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define CMD_PREFIX              "AT+"
#define CMD_MIN_LEN             strlen(CMD_PREFIX)
#define CMD_MAX_LEN             255
#define CMD_START_CHAR_INDEX    CMD_MIN_LEN

// esp32-ble is self-hosted, communication between esp32 & tm148 is mainly for data/event base exchange
// esp32-ble will request system info (battery, user button press, last trip, last charge...) and handle data transfering to phone

typedef enum {
    EVENT_START         =0,         //AT+?  (query handshake reasons)
    DATA_LOGIN          =1,         //AT+LOGIN,battery(%),                                  (1 byte)
                                    //        estimate range(km),                           (2 bytes)
                                    //        odo(km),                                      (4 bytes)
                                    //        last trip distance(km),                       (2 bytes)
                                    //        last trip time (mins),                        (2 bytes)
                                    //        last trip electric used(kWh),                 (2 bytes)
                                    //        last charge level (%),                        (1 bytes)
                                    //        distance since last charge (km)               (2 bytes)

    DATA_CHARGE         =2,         //AT+CHARGE,state(on/off),                              (1 byte)
                                    // below info available if charge state = on
                                    //          charge voltage,                             (1 byte)
                                    //          charge current,                             (1 byte)
                                    //          charge cycle,                               (2 bytes)
                                    //          time to full(mins)                          (2 bytes)

    DATA_BATTERY        =3,         //AT+BATTERY,battery (%),                               (1 byte)
                                    //           estimate range(km)                         (2 bytes)

    DATA_TRIP           =4,         //AT+TRIP,distance(km)                                  (2 bytes)
                                    //        ride time(mins)                               (2 bytes)
                                    //        electric used(kWh)                            (2 bytes)

    EVENT_LOCK          =5,         //AT+LOCK,x
    EVENT_UNLOCK        =6,         //AT+UNLOCK,x
                                    //user press (o) button event
    EVENT_BUTTON        =7,         //AT+BUTTON                                             (1 byte)
    MAX_CMD_SUPPORTED   =8
} at_cmd_recv_t;


typedef enum {
    LOGIN               =0,          //OK+LOGIN
    CHARGE              =1,          //OK+CHARGE
    BATTERY             =2,          //OK+BATTERY
    LAST_TRIP           =3,          //OK+TRIP
    LOCK                =4,          //OK+LOCK,keystring
    UNLOCK              =5,          //OK+UNLOCK,keystring
    UNKNOWN             =6
} at_cmd_resp_t;

// parse command
extern int tm_atcmd_recv_parser(char* cmd, int len);
extern bool terminate_check(char* msg) ;