#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "tm_atcmd_parser.h"
#include "esp_log.h"

#define ATCMD_PARSER_TAG        "TM_ATCMD_PARSER"
#define CMD_PREFIX              "TM+"
#define CMD_MIN_LEN             strlen(CMD_PREFIX)
#define CMD_MAX_LEN             255
#define CMD_START_CHAR_INDEX    CMD_MIN_LEN

// esp32-ble is self-hosted, communication between esp32 & tm148 is mainly for data/event base exchange
// esp32-ble will request system info (battery, user button press, last trip, last charge...) and handle data transfering to phone

// check "\r\n" terminate characters
bool terminate_check(char* msg) {
    return (strncmp("\r\n", msg, sizeof("\r\n")) == 0);
}

// parse command
int tm_atcmd_recv_parser(char* cmd, int len) {
    if (len < CMD_MIN_LEN || len > CMD_MAX_LEN)
        return -1;

    if (strncmp(CMD_PREFIX, cmd, CMD_MIN_LEN) < 0)
        return -1;

    int ret = -1;

    // command below is parse with alphabet order, when adding new command, make sure it's in right order
    switch (cmd[CMD_START_CHAR_INDEX]) {
        case 'L':
            //TM+LOGIN,...
            if (strncmp("LOGIN",&cmd[CMD_START_CHAR_INDEX], sizeof("LOGIN")-1) == 0) {
                ret = DATA_LOGIN;
                break;
            }
            if (strncmp("LOCK",&cmd[CMD_START_CHAR_INDEX], sizeof("LOCK")-1) == 0) {
                ret = EVENT_LOCK;
                break;
            }
            ret = -1;
        break;

        case 'C':
            //TM+CHARGE,...
            if (strncmp("CHARGE",&cmd[CMD_START_CHAR_INDEX], sizeof("CHARGE")-1) == 0) {
                ret = DATA_CHARGE;
                break;
            }
            ret = -1;
        break;

        case 'B':
            //TM+BATTERY,...
            if (strncmp("BATTERY",&cmd[CMD_START_CHAR_INDEX], sizeof("BATTERY")-1) == 0) {
                ret = DATA_BATTERY;
                break;
            }
            //TM+BUTTON,...
            if (strncmp("BUTTON",&cmd[CMD_START_CHAR_INDEX], sizeof("BUTTON")-1) == 0) {
                ret = EVENT_BUTTON;
                break;
            }
            ret = -1;
        break;

        case 'T':
            //TM+TRIP,...
            if (strncmp("TRIP",&cmd[CMD_START_CHAR_INDEX], sizeof("TRIP")-1) == 0) {
                ret = DATA_LAST_TRIP;
                break;
            }
            ret = -1;
        break;

        case 'U':
            //TM+UNLOCK,x
            if (strncmp("UNLOCK",&cmd[CMD_START_CHAR_INDEX], sizeof("UNLOCK")-1) != 0) {
                ret = EVENT_UNLOCK;
                break;
            }
            ret = -1;
        break;

        default:
            ret = -1;
        break;
    }

    return ret;
}

