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

            case TM_BLE_BATTERY:
                ESP_LOGI(ION_TM_ATCMD_TAG, "battery info");
                break;

            case TM_BLE_LAST_TRIP:
                ESP_LOGI(ION_TM_ATCMD_TAG, "last trip info");
                break;

            case TM_BLE_STEERING:
                ESP_LOGI(ION_TM_ATCMD_TAG, "steering");
                break;

            case TM_BLE_PING_BIKE:
                ESP_LOGI(ION_TM_ATCMD_TAG, "ping bike");
                break;

            case TM_BLE_OPEN_SEAT:
                ESP_LOGI(ION_TM_ATCMD_TAG, "open seat");
                break;

            case TM_BLE_DIAG:
                ESP_LOGI(ION_TM_ATCMD_TAG, "diag");
                break;

            case TM_BLE_PAIRING:
                ESP_LOGI(ION_TM_ATCMD_TAG, "ble pairing");
                break;

            case TM_BLE_SESSION:
                ESP_LOGI(ION_TM_ATCMD_TAG, "ble paired");
                break;

            case TM_BLE_DISCONNECT:
                ESP_LOGI(ION_TM_ATCMD_TAG, "ble disconnected");
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
    if (msg->msg_id>=TM_BLE_PAIRING && msg->msg_id<TM_BLE_INVALID) {
        txbuf[0] = (uint8_t)(msg->msg_id & 0xFF);
        switch (txbuf[0]) {
            case TM_BLE_PAIRING:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_PAIRING");
                break;
            case TM_BLE_SESSION:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_SESSION");
                break;
            case TM_BLE_DISCONNECT:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_DISCONNECT");
                break;
            case TM_BLE_BATTERY:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_BATTERY");
                break;
            case TM_BLE_LAST_TRIP:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_LAST_TRIP");
                break;
            case TM_BLE_STEERING:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_STEERING");
                break;
            case TM_BLE_PING_BIKE:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_PING_BIKE");
                break;
            case TM_BLE_OPEN_SEAT:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_OPEN_SEAT");
                break;
            case TM_BLE_DIAG:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_DIAG");
                break;
            case TM_BLE_BIKE_INFO:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_BIKE_INFO");
                break;
            case TM_BLE_POWER_ON:
                ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_construct TM_BLE_POWER_ON");
                break;

            default:
                ESP_LOGE(ION_TM_ATCMD_TAG, "unknown command");
                return ESP_FAIL;
                break;
        }
    } else {
        return ESP_FAIL;
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
    // process each command here, timeout is 2s
    ESP_LOGI(ION_TM_ATCMD_TAG, "cmd_data:%s", cmd);
    int cmd_id = tm_atcmd_recv_parser(cmd, cmd_len);

    switch (cmd_id) {
        case TM_BLE_BATTERY: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+BATTERY,xxx,xxxxx,xxxxx
            //level,estimate range
            memset(&battery, 0, sizeof(battery_t));
            battery.level           = atoi(&cmd[11]);
            battery.estimate_range  = atoi(&cmd[15]);
            battery.time_to_full    = atoi(&cmd[21]);
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

        case TM_BLE_STEERING: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+STEERING
            send_msg_to_ble(TM_BLE_STEERING, NULL, 0);
        }
        break;

        case TM_BLE_PING_BIKE: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+PING_BIKE
            send_msg_to_ble(TM_BLE_PING_BIKE, NULL, 0);
        }
        break;

        case TM_BLE_OPEN_SEAT: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+OPEN_SEAT
            send_msg_to_ble(TM_BLE_OPEN_SEAT, NULL, 0);
        }
        break;

        case TM_BLE_DIAG: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+DIAG
            send_msg_to_ble(TM_BLE_DIAG, NULL, 0);
        }
        break;

        case TM_BLE_PAIRING: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+PAIRING
            ESP_LOGI(ION_TM_ATCMD_TAG, "148 received pairing notification");
        }
        break;

        case TM_BLE_SESSION: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+PAIRED
            ESP_LOGI(ION_TM_ATCMD_TAG, "148 received paired notification");
        }
        break;

        case TM_BLE_DISCONNECT: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //TM+DISCONNECT
            ESP_LOGI(ION_TM_ATCMD_TAG, "148 received disconnect notification");
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
    if ((msg_id >= TM_BLE_PAIRING) && (msg_id < TM_BLE_INVALID)) {
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
    ble_msg.msg_id = msg_id;
    ble_msg.len = len;
    if (len > 0)
        memcpy(ble_msg.data, data, len);

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
        case 'L':
            //TM+LAST_TRIP,...
            if (strncmp("LAST_TRIP",&cmd[CMD_START_CHAR_INDEX], sizeof("LAST_TRIP")-1) == 0) {
                ret = TM_BLE_LAST_TRIP;
            } else {
                ret = -1;
            }
            break;

        case 'B':
            //TM+BATTERY,...
            if (strncmp("BATTERY",&cmd[CMD_START_CHAR_INDEX], sizeof("BATTERY")-1) == 0) {
                ret = TM_BLE_BATTERY;
            } else {
                ret = -1;
            }
            break;

        case 'S':
            //TM+STEERING_LOCK, //TM+STEERING_UNLOCK
            if (strncmp("STEERING",&cmd[CMD_START_CHAR_INDEX], sizeof("STEERING")-1) == 0) {
                ret = TM_BLE_STEERING;
            } else {
                ret = -1;
            }
            break;

        case 'P':
            //TM+PING_BIKE, TM+PAIRING, TM+PAIRED
            if (strncmp("PING_BIKE",&cmd[CMD_START_CHAR_INDEX], sizeof("PING_BIKE")-1) == 0) {
                ret = TM_BLE_PING_BIKE;
            } else if (strncmp("PAIRING",&cmd[CMD_START_CHAR_INDEX], sizeof("PAIRING")-1) == 0) {
                ret = TM_BLE_PAIRING;
            } else if (strncmp("PAIRED",&cmd[CMD_START_CHAR_INDEX], sizeof("PAIRED")-1) == 0) {
                ret = TM_BLE_SESSION;
            } else {
                ret = -1;
            }
            break;

        case 'O':
            //TM+OPEN_SEAT
            if (strncmp("OPEN_SEAT",&cmd[CMD_START_CHAR_INDEX], sizeof("OPEN_SEAT")-1) == 0) {
                ret = TM_BLE_OPEN_SEAT;
            } else {
                ret = -1;
            }
            break;

        case 'D':
            //TM+DIAG
            if (strncmp("DIAG",&cmd[CMD_START_CHAR_INDEX], sizeof("DIAG")-1) == 0) {
                ret = TM_BLE_DIAG;
            } else {
                ret = -1;
            }
            break;

        default:
            ret = -1;
            break;
    }

    return ret;
}