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
#include "tm_ble.h"

#define ATCMD_TASK_PRIO         3
#define ATCMD_CMD_LEN           128
#define ATCMD_TIMEOUT           2000        //ms
#define ATCMD_SMART_KEY_LEN     16
#define ION_TM_ATCMD_TAG        "TM_ATCMD"
#define CMD_PREFIX              "TM+"
#define CMD_MIN_LEN             strlen(CMD_PREFIX)
#define CMD_MAX_LEN             ATCMD_CMD_LEN
#define CMD_START_CHAR_INDEX    CMD_MIN_LEN

QueueHandle_t ble_to_tm_queue;

static esp_err_t tm_atcmd_construct(ble_to_tm_msg_t *msg, char* txbuf);
static esp_err_t tm_atcmd_send(char* msg);
static esp_err_t tm_atcmd_recv(char* cmd, int* len);
static esp_err_t tm_atcmd_process(char* cmd, int cmd_len);
static esp_err_t tm_atcmd_handler(ble_to_tm_msg_t* msg);
static esp_err_t send_msg_to_ble(int msg_id, uint8_t *data, int len);
static int tm_atcmd_recv_parser(char* cmd, int len);
general_t general = {0};
charge_t charge = {0};
battery_t battery = {0};
trip_t trip = {0};

static void tm_atcmd_task(void *arg)
{
    ble_to_tm_msg_t ble_to_tm_msg = {0};
    ble_to_tm_queue = xQueueCreate(10, sizeof(ble_to_tm_msg_t));
 
    while(1) {
        if (xQueueReceive(ble_to_tm_queue, &ble_to_tm_msg, portMAX_DELAY) != pdTRUE)
            continue;

        switch (ble_to_tm_msg.msg_id) {
            case TM_BLE_GENERAL:
                ESP_LOGI(ION_TM_ATCMD_TAG, "general info");
                break;

            case TM_BLE_CHARGE:
                ESP_LOGI(ION_TM_ATCMD_TAG, "charge info");
                break;

            case TM_BLE_BATTERY:
                ESP_LOGI(ION_TM_ATCMD_TAG, "battery info");
                break;

            case TM_BLE_LAST_TRIP:
                ESP_LOGI(ION_TM_ATCMD_TAG, "last trip info");
                break;

            case TM_BLE_LOCK:
                ESP_LOGI(ION_TM_ATCMD_TAG, "lock info");
                break;

            case TM_BLE_UNLOCK:
                ESP_LOGI(ION_TM_ATCMD_TAG, "unlock info");
                break;

            default:
                continue;
                break;
        }

        tm_atcmd_handler(&ble_to_tm_msg);
    }
}

void tm_atcmd_tasks_init(void)
{
    spi_slave_init();

    //Create and start stats task
    xTaskCreatePinnedToCore(tm_atcmd_task, "tm_atcmd_task", 4096, NULL, ATCMD_TASK_PRIO, NULL, tskNO_AFFINITY);

}

static esp_err_t tm_atcmd_handler(ble_to_tm_msg_t *msg) {

    esp_err_t ret = ESP_OK;
    WORD_ALIGNED_ATTR char txbuf[ATCMD_CMD_LEN+1] = {0};
    WORD_ALIGNED_ATTR char rxbuf[ATCMD_CMD_LEN+1] = {0};
    int len;

    // construct message
    ret = tm_atcmd_construct(msg, txbuf);
    if (ret == ESP_OK) {
        // send msg to 148
        ret = tm_atcmd_send(txbuf);
        if (ret == ESP_OK) {
            // get response from 148
            ret = tm_atcmd_recv(rxbuf, &len);
            if (ret == ESP_OK) {
                ret = tm_atcmd_process(rxbuf, len);
            } else {
                ESP_LOGE(ION_TM_ATCMD_TAG, "Failed to get data from 148: %d",ret);
            }
        } else {
            ESP_LOGE(ION_TM_ATCMD_TAG, "Failed to send message: %d",ret);
        }
    } else {
        ESP_LOGE(ION_TM_ATCMD_TAG, "Failed to construct message: %d",ret);
    }

    return ret;
}

static esp_err_t tm_atcmd_construct(ble_to_tm_msg_t *msg, char* txbuf) {
    uint8_t key[ATCMD_SMART_KEY_LEN];
    switch (msg->msg_id) {
        case TM_BLE_GENERAL:
            snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+GENERAL\r\n");
            break;
        case TM_BLE_CHARGE:
            snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+CHARGE\r\n");
            break;
        case TM_BLE_BATTERY:
            snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+BATTERY\r\n");
            break;
        case TM_BLE_LAST_TRIP:
            snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+LAST_TRIP\r\n");
            break;
        case TM_BLE_LOCK:
            // auto lock, phone send 16 bytes key to lock via ble
            if (msg->len > ATCMD_SMART_KEY_LEN)
                snprintf(txbuf, ATCMD_CMD_LEN+1, "ERR,invalid param\r\n");
            else {
                memcpy(key, msg->data, ATCMD_SMART_KEY_LEN);
                snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+LOCK,%s\r\n",key);
                // snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+LOCK,1234567890123456\r\n");
            }
            break;
        case TM_BLE_UNLOCK:
            // auto unlock, phone send 16 bytes key to unlock via ble
            if (msg->len > ATCMD_SMART_KEY_LEN)
                snprintf(txbuf, ATCMD_CMD_LEN+1, "ERR,invalid param\r\n");
            else {
                memcpy(key, msg->data, ATCMD_SMART_KEY_LEN);
                snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+UNLOCK,%s\r\n",key);
                // snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+UNLOCK,1234567890123456\r\n");
            }
            break;
        default:
            ESP_LOGE(ION_TM_ATCMD_TAG, "unknown command");
            return ESP_FAIL;
            break;
    }

    return ESP_OK;
}

static esp_err_t tm_atcmd_send(char* msg) {
    if (strlen(msg) > ATCMD_CMD_LEN)
        return ESP_ERR_INVALID_SIZE;

    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=ATCMD_CMD_LEN*8;   // send 128 bytes each time for alignment
    t.tx_buffer=msg;
    t.rx_buffer=NULL;
    // spi_slave_pull_interupt_high();
    esp_err_t status=spi_slave_transmit(RCV_HOST, &t, pdMS_TO_TICKS(ATCMD_TIMEOUT));
    spi_slave_pull_interupt_low();
    return status;
}

static esp_err_t tm_atcmd_recv(char* cmd, int* len) {
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=ATCMD_CMD_LEN*8;
    t.tx_buffer=NULL;
    t.rx_buffer=cmd;
    esp_err_t ret = spi_slave_transmit(RCV_HOST, &t, pdMS_TO_TICKS(ATCMD_TIMEOUT));
    *len = t.trans_len/8;
    spi_slave_pull_interupt_low();
    return ret;
}

static esp_err_t tm_atcmd_process(char* cmd, int cmd_len) {
    bool ret;

    // process each command here, timeout is 2s
    ESP_LOGI(ION_TM_ATCMD_TAG, "cmd_data:%s", cmd);
    int cmd_id = tm_atcmd_recv_parser(cmd, cmd_len);

    switch (cmd_id) {
        case TM_BLE_GENERAL: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+GENERAL,xxx,xxxxx,xxxxxx,xxxxx,xxxxx,xxxxx,xxx,xxxxx
            //bat,est_ran,odo,last_trip,last_trip_time,last_trip_elec,lastchargelevel,distancesincelastcharge
            memset(&general, 0, sizeof(general_t));
            general.battery                       = atoi(&cmd[11]);
            general.est_range                     = atoi(&cmd[15]);
            general.odo                           = atoi(&cmd[21]);
            general.last_trip_distance            = atoi(&cmd[28]);
            general.last_trip_time                = atoi(&cmd[34]);
            general.last_trip_elec_used           = atoi(&cmd[40]);
            general.last_charge_level             = atoi(&cmd[46]);
            general.distance_since_last_charge    = atoi(&cmd[50]);
            send_msg_to_ble(TM_BLE_GENERAL, (uint8_t*)&general, sizeof(general_t));
        }
        break;

        case TM_BLE_CHARGE: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+CHARGE,x,xxxxx,xxxxx,xxxxx,xxxxx
            //on/off,charge_vol,charge_cur,charge_cycle,charge_time_to_full
            memset(&charge, 0, sizeof(charge_t));
            charge.state                = atoi(&cmd[10]);
            if (charge.state) {
                charge.vol              = atoi(&cmd[12]);
                charge.cur              = atoi(&cmd[18]);
                charge.cycle            = atoi(&cmd[24]);
                charge.time_to_full     = atoi(&cmd[30]);
            }
            send_msg_to_ble(TM_BLE_CHARGE, (uint8_t*)&charge, sizeof(charge_t));
        }
        break;

        case TM_BLE_BATTERY: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+BATTERY,xxx,xxxxx
            //level,estimate range
            memset(&battery, 0, sizeof(battery_t));
            battery.level           = atoi(&cmd[11]);
            battery.estimate_range  = atoi(&cmd[15]);
            send_msg_to_ble(TM_BLE_BATTERY, (uint8_t*)&battery, sizeof(battery_t));
        }
        break;

        case TM_BLE_LAST_TRIP: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+LAST_TRIP,xxxxx,xxxxx,xxxxx
            //distance, ride_time, elec_used
            memset(&trip, 0, sizeof(trip_t));
            trip.distance       = atoi(&cmd[13]);
            trip.ride_time      = atoi(&cmd[19]);
            trip.elec_used      = atoi(&cmd[25]);
            send_msg_to_ble(TM_BLE_LAST_TRIP, (uint8_t*)&trip, sizeof(trip_t));
        }
        break;

        case TM_BLE_LOCK: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+LOCKED,x
            ret = atoi(&cmd[10]);
            //todo: send these data to phone
            if (ret)
                ESP_LOGI(ION_TM_ATCMD_TAG, "bike successful locked by phone");
            else
                ESP_LOGI(ION_TM_ATCMD_TAG, "bike failed to lock by phone");
        }
        break;

        case TM_BLE_UNLOCK: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+UNLOCKED,x
            ret = atoi(&cmd[12]);
            //todo: send these data to phone
            if (ret)
                ESP_LOGI(ION_TM_ATCMD_TAG, "bike successful unlocked by phone");
            else
                ESP_LOGI(ION_TM_ATCMD_TAG, "bike failed to unlock by phone");
        }
        break;

        default: {
            ESP_LOGI(ION_TM_ATCMD_TAG, "CMD NOT FOUND");
            return ESP_FAIL;
        }
        break;
    }

    return ESP_OK;
}

void send_to_tm_queue(int msg_id, uint8_t *data, int len)
{
    if ((msg_id >= TM_BLE_GENERAL) && (msg_id <= TM_BLE_UNLOCK)) {
        ble_to_tm_msg_t ble_to_tm_msg = {0};
        ble_to_tm_msg.msg_id = msg_id;
        if (data != NULL) {
            ble_to_tm_msg.len = len;
            memcpy(ble_to_tm_msg.data, data, ble_to_tm_msg.len);
        }
        xQueueSend(ble_to_tm_queue, (void*)&ble_to_tm_msg, (TickType_t)0);
    }
}

static esp_err_t send_msg_to_ble(int msg_id, uint8_t *data, int len) {
    if (len > BLE_MSG_MAX_LEN) {
        ESP_LOGE(ION_TM_ATCMD_TAG, "send_msg_to_ble: invalid data len: %d",len);
        return ESP_ERR_INVALID_SIZE;
    }

    ble_msg_t ble_msg;
    switch (msg_id) {
        case TM_BLE_GENERAL: {
            ble_msg.msg_id = TM_BLE_GENERAL;
            break;
        }
        case TM_BLE_CHARGE: {
            ble_msg.msg_id = TM_BLE_CHARGE;
            break;
        }
        case TM_BLE_BATTERY: {
            ble_msg.msg_id = TM_BLE_BATTERY;
            break;
        }
        case TM_BLE_LAST_TRIP: {
            ble_msg.msg_id = TM_BLE_LAST_TRIP;
            break;
        }
        default: {
            ESP_LOGE(ION_TM_ATCMD_TAG, "send_msg_to_ble invalid msg_id: %d",msg_id);
            return ESP_ERR_INVALID_SIZE;
            break;
        }
    }

    ble_msg.len = len;
    memcpy(ble_msg.data, data, sizeof(ble_to_tm_msg_t));
    xQueueSend(ble_queue, (void*)&ble_msg, (TickType_t)0);
    return ESP_OK;
}

int tm_atcmd_recv_parser(char* cmd, int len) {
    if (len < CMD_MIN_LEN || len > CMD_MAX_LEN)
        return -1;

    if (strncmp(CMD_PREFIX, cmd, CMD_MIN_LEN) < 0)
        return -1;

    int ret = -1;

    // command below is parse with alphabet order, when adding new command, make sure it's in right order
    switch (cmd[CMD_START_CHAR_INDEX]) {
        case 'G':
            //TM+GENERAL,...
            if (strncmp("GENERAL",&cmd[CMD_START_CHAR_INDEX], sizeof("GENERAL")-1) == 0) {
                ret = TM_BLE_GENERAL;
                break;
            }
            break;
        case 'L':
            //TM+LAST_TRIP,...
            if (strncmp("LAST_TRIP",&cmd[CMD_START_CHAR_INDEX], sizeof("LAST_TRIP")-1) == 0) {
                ret = TM_BLE_LAST_TRIP;
                break;
            } else if (strncmp("LOCK",&cmd[CMD_START_CHAR_INDEX], sizeof("LOCK")-1) == 0) {
                //TM+LOCK,...
                ret = TM_BLE_LOCK;
                break;
            }
            ret = -1;
            break;

        case 'C':
            //TM+CHARGE,...
            if (strncmp("CHARGE",&cmd[CMD_START_CHAR_INDEX], sizeof("CHARGE")-1) == 0) {
                ret = TM_BLE_CHARGE;
                break;
            }
            ret = -1;
            break;

        case 'B':
            //TM+BATTERY,...
            if (strncmp("BATTERY",&cmd[CMD_START_CHAR_INDEX], sizeof("BATTERY")-1) == 0) {
                ret = TM_BLE_BATTERY;
                break;
            }
            ret = -1;
            break;

        case 'U':
            //TM+UNLOCK,x
            if (strncmp("UNLOCK",&cmd[CMD_START_CHAR_INDEX], sizeof("UNLOCK")-1) != 0) {
                ret = TM_BLE_UNLOCK;
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