#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"

#include "tm_spi_slave_driver.h"
#include "tm_atcmd.h"
#include "tm_atcmd_parser.h"

#define ATCMD_TASK_PRIO     3
#define ATCMD_CMD_LEN       128
#define ION_TM_ATCMD_TAG    "TM_ATCMD"

QueueHandle_t atcmd_to_handler_queue;
QueueHandle_t handler_to_atcmd_queue;

static esp_err_t tm_atcmd_recv(char* cmd, int* len);
static esp_err_t tm_atcmd_process(char* cmd, int cmd_len, char* res, int* res_len);
static esp_err_t tm_atcmd_resp(char* res, int len);

static void tm_atcmd_task(void *arg)
{
    esp_err_t ret = ESP_OK;
    WORD_ALIGNED_ATTR char recvbuf[ATCMD_CMD_LEN+1]="";
    WORD_ALIGNED_ATTR char respbuf[ATCMD_CMD_LEN+1]="";
    int recv_len=0;
    int resp_len=0;
    while(1) {
        memset(recvbuf, 0, sizeof(recvbuf));
        ret = tm_atcmd_recv(recvbuf, &recv_len);
        if (ret != ESP_OK)
            continue;

        // process command
        memset(respbuf, 0, sizeof(respbuf));
        ret = tm_atcmd_process(recvbuf, recv_len, respbuf, &resp_len);
        if (ret != ESP_OK)
            ESP_LOGE(ION_TM_ATCMD_TAG, "failed to process command");

        // response with ok or error
        ret = tm_atcmd_resp(respbuf, resp_len);
        if (ret != ESP_OK)
            ESP_LOGE(ION_TM_ATCMD_TAG, "failed to response to 148");
    }
}

void tm_atcmd_tasks_init(void)
{
    spi_slave_init();

    //Create and start stats task
    xTaskCreatePinnedToCore(tm_atcmd_task, "tm_atcmd_task", 4096, NULL, ATCMD_TASK_PRIO, NULL, tskNO_AFFINITY);

}

static esp_err_t tm_atcmd_recv(char* cmd, int* len) {
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=ATCMD_CMD_LEN*8;
    t.tx_buffer=NULL;
    t.rx_buffer=cmd;
    esp_err_t ret = spi_slave_transmit(RCV_HOST, &t, portMAX_DELAY);
    *len = t.trans_len/8;
    return ret;
}

static esp_err_t tm_atcmd_process(char* cmd, int cmd_len, char* res, int* res_len) {
    ESP_LOGI(ION_TM_ATCMD_TAG, "cmd_data:%s", cmd);
    // process each command here, timeout is 2s
    int cmd_id = tm_atcmd_recv_parser(cmd, cmd_len);
    login_t login = {0};
    charge_t charge = {0};

    switch (cmd_id) {
        case EVENT_START:
            snprintf(res, ATCMD_CMD_LEN+1, "OK+CHARGE\r\n");
            *res_len = strlen(res);
        break;
        case DATA_LOGIN:
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //AT+LOGIN,xxx,xxxxx,xxxxxx,xxxxx,xxxxx,xxxxx,xxx,xxxxx
            //bat,est_ran,odo,last_trip,last_trip_time,last_trip_elec,lastchargelevel,distancesincelastcharge
     
            login.battery                       = atoi(&cmd[9]);
            login.est_range                     = atoi(&cmd[13]);
            login.odo                           = atoi(&cmd[19]);
            login.last_trip_distance            = atoi(&cmd[26]);
            login.last_trip_time                = atoi(&cmd[32]);
            login.last_trip_elec_used           = atoi(&cmd[38]);
            login.last_charge_level             = atoi(&cmd[44]);
            login.distance_since_last_charge    = atoi(&cmd[48]);

            ESP_LOGI(ION_TM_ATCMD_TAG, "battery                    %d",login.battery);
            ESP_LOGI(ION_TM_ATCMD_TAG, "est_range                  %d",login.est_range);
            ESP_LOGI(ION_TM_ATCMD_TAG, "odo                        %d",login.odo);
            ESP_LOGI(ION_TM_ATCMD_TAG, "last_trip_distance         %d",login.last_trip_distance);
            ESP_LOGI(ION_TM_ATCMD_TAG, "last_trip_time             %d",login.last_trip_time);
            ESP_LOGI(ION_TM_ATCMD_TAG, "last_trip_elec_used        %d",login.last_trip_elec_used);
            ESP_LOGI(ION_TM_ATCMD_TAG, "last_charge_level          %d",login.last_charge_level);
            ESP_LOGI(ION_TM_ATCMD_TAG, "distance_since_last_charge %d",login.distance_since_last_charge);

            snprintf(res, ATCMD_CMD_LEN+1, "OK\r\n");
            *res_len = strlen(res);
        break;

        case DATA_CHARGE:
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //AT+CHARGE,x,xxxxx,xxxxx,xxxxx,xxxxx
            //on/off,charge_vol,charge_cur,charge_cycle,charge_time_to_full
            memset(&charge, 0, sizeof(charge_t));
            charge.state                = atoi(&cmd[10]);
            if (charge.state) {
                charge.vol              = atoi(&cmd[12]);
                charge.cur              = atoi(&cmd[18]);
                charge.cycle            = atoi(&cmd[24]);
                charge.time_to_full     = atoi(&cmd[30]);
            }
            ESP_LOGI(ION_TM_ATCMD_TAG, "charge state            %d",charge.state);
            ESP_LOGI(ION_TM_ATCMD_TAG, "charge vol              %d",charge.vol);
            ESP_LOGI(ION_TM_ATCMD_TAG, "charge cur              %d",charge.cur);
            ESP_LOGI(ION_TM_ATCMD_TAG, "charge cycle            %d",charge.cycle);
            ESP_LOGI(ION_TM_ATCMD_TAG, "charge time_to_full     %d",charge.time_to_full);

            snprintf(res, ATCMD_CMD_LEN+1, "OK\r\n");
            *res_len = strlen(res);
        break;

        default:
            ESP_LOGI(ION_TM_ATCMD_TAG, "CMD NOT FOUND");
            snprintf(res, ATCMD_CMD_LEN+1, "ERR\r\n");
            *res_len = strlen(res);
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    return ESP_OK;
}

static esp_err_t tm_atcmd_resp(char* res, int len) {
    spi_slave_transaction_t t;
    WORD_ALIGNED_ATTR char sendbuf[ATCMD_CMD_LEN+1]="";
    memset(&t, 0, sizeof(t));
    t.length=ATCMD_CMD_LEN*8;
    t.tx_buffer=sendbuf;
    t.rx_buffer=NULL;
    memcpy(sendbuf, res, len);
    return spi_slave_transmit(RCV_HOST, &t, portMAX_DELAY);
}

// construct response command
int tm_atcmd_resp_construct(char* cmd, int len) {
    if (len < CMD_MIN_LEN || len > CMD_MAX_LEN)
        return -1;

    if (strncmp(CMD_PREFIX, cmd, CMD_MIN_LEN) < 0)
        return -1;

    int ret = -1;

    // command below is parse with alphabet order, when adding new command, make sure it's in right order
    switch (cmd[CMD_START_CHAR_INDEX]) {
        case '?':
            //AT+?
            if (terminate_check(&cmd[CMD_START_CHAR_INDEX+1]))
                ret = EVENT_START;
            else
                ret = -1;
        break;

        case 'L':
            //AT+LOGIN,...
            if (strncmp("LOGIN",&cmd[CMD_START_CHAR_INDEX+1], sizeof("LOGIN") == 0)) {
                ret = DATA_LOGIN;
                break;
            }
            ret = -1;
        break;

        case 'C':
            //AT+CHARGE,...
            if (strncmp("CHARGE",&cmd[CMD_START_CHAR_INDEX+1], sizeof("CHARGE") != 0)) {
                ret = DATA_CHARGE;
                break;
            }
            ret = -1;
        break;

        case 'B':
            //AT+BATTERY,...
            if (strncmp("BATTERY",&cmd[CMD_START_CHAR_INDEX+1], sizeof("BATTERY") != 0)) {
                ret = DATA_BATTERY;
                break;
            }
            //AT+BUTTON,...
            if (strncmp("BUTTON",&cmd[CMD_START_CHAR_INDEX+1], sizeof("BUTTON") != 0)) {
                ret = EVENT_BUTTON;
                break;
            }
            ret = -1;
        break;

        case 'T':
            //AT+TRIP,...
            if (strncmp("TRIP",&cmd[CMD_START_CHAR_INDEX+1], sizeof("TRIP") != 0)) {
                ret = DATA_TRIP;
                break;
            }
            ret = -1;
        break;

        case 'S':
            //AT+SMART_KEY,...
            if (strncmp("SMART_KEY",&cmd[CMD_START_CHAR_INDEX+1], sizeof("SMART_KEY") != 0)) {
                ret = DATA_SMART_KEY;
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