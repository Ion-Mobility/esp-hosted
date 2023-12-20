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
#include "tm_ble_gatts_server.h"
#include "crypto.h"

#define ATCMD_TASK_PRIO         3
#define ATCMD_CMD_LEN           32
#define ATCMD_TIMEOUT           2000        //ms
#define ION_TM_ATCMD_TAG        "TM_ATCMD"
#define CMD_PREFIX              "TM+"
#define CMD_MIN_LEN             strlen(CMD_PREFIX)
#define CMD_MAX_LEN             ATCMD_CMD_LEN
#define CMD_START_CHAR_INDEX    CMD_MIN_LEN

QueueHandle_t ble_to_tm_queue;

static esp_err_t tm_atcmd_construct(ble_to_tm_msg_t *msg, char* txbuf);
static IRAM_ATTR esp_err_t tm_atcmd_transmit(char* tx, char* rx);
static esp_err_t tm_atcmd_process(char* cmd);
static esp_err_t tm_atcmd_handler(ble_to_tm_msg_t* msg);
static esp_err_t send_msg_to_ble(int msg_id, uint8_t *data, int len);

battery_t battery = {0};
trip_t trip = {0};
on_off_state_t steering = STATE;
on_off_state_t cellular = STATE;
on_off_state_t location = STATE;
on_off_state_t ride_tracking = STATE;

static void tm_atcmd_task(void *arg)
{
    ble_to_tm_msg_t ble_to_tm_msg = {0};
    ble_to_tm_queue = xQueueCreate(10, sizeof(ble_to_tm_msg_t));
 
    while(1) {
        if (xQueueReceive(ble_to_tm_queue, &ble_to_tm_msg, portMAX_DELAY) != pdTRUE)
            continue;

        switch (ble_to_tm_msg.msg_id) {
            case TM_BLE_PAIRING:
                ESP_LOGI(ION_TM_ATCMD_TAG, "ble pairing");
                break;

            case TM_BLE_SESSION:
                ESP_LOGI(ION_TM_ATCMD_TAG, "ble paired");
                break;

            case TM_BLE_DISCONNECT:
                ESP_LOGI(ION_TM_ATCMD_TAG, "ble disconnected");
                break;

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

            case TM_BLE_BIKE_INFO:
                ESP_LOGI(ION_TM_ATCMD_TAG, "bike info");
                break;

            case TM_BLE_POWER_ON:
                ESP_LOGI(ION_TM_ATCMD_TAG, "power on");
                break;

            case TM_BLE_CELLULAR:
                ESP_LOGI(ION_TM_ATCMD_TAG, "cellular");
                break;

            case TM_BLE_LOCATION:
                ESP_LOGI(ION_TM_ATCMD_TAG, "location");
                break;

            case TM_BLE_RIDE_TRACKING:
                ESP_LOGI(ION_TM_ATCMD_TAG, "ride tracking");
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

    // construct message
    ret = tm_atcmd_construct(msg, txbuf);
    if (ret == ESP_OK) {
        // send msg to 148
        ret = tm_atcmd_transmit(txbuf, rxbuf);
        if (ret == ESP_OK) {
            // get response from 148
            txbuf[0] = TM_BLE_OK;
            ret = tm_atcmd_transmit(txbuf, rxbuf);
            if (ret == ESP_OK) {
                ret = tm_atcmd_process(rxbuf);
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
    if (msg->msg_id>=TM_BLE_PAIRING && msg->msg_id<=TM_BLE_OK) {
        txbuf[0] = (uint8_t)(msg->msg_id & 0xFF);
        if (msg->msg_id == TM_BLE_STEERING ||
            msg->msg_id == TM_BLE_CELLULAR ||
            msg->msg_id == TM_BLE_LOCATION ||
            msg->msg_id == TM_BLE_RIDE_TRACKING)
            txbuf[1] = (uint8_t)(msg->data[0]);

        return ESP_OK;
    }
    return ESP_FAIL;
}

static IRAM_ATTR esp_err_t tm_atcmd_transmit(char* tx, char* rx) {
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=ATCMD_CMD_LEN*8;   // send 128 bytes each time for alignment
    t.tx_buffer=tx;
    t.rx_buffer=rx;
    esp_err_t status=spi_slave_transmit(RCV_HOST, &t, pdMS_TO_TICKS(ATCMD_TIMEOUT));
    spi_slave_pull_interupt_low();
#if (DEBUG_TM)
    ESP_LOGI(ION_TM_ATCMD_TAG, "trans_len: %d",t.trans_len);
    ESP_LOGI(ION_TM_ATCMD_TAG, "spi_slave_transmit:");
    esp_log_buffer_hex(ION_TM_ATCMD_TAG, t.tx_buffer, 16);
    esp_log_buffer_hex(ION_TM_ATCMD_TAG, t.rx_buffer, 16);
#endif

    return status;
}


static esp_err_t tm_atcmd_process(char* cmd) {
    // process each command here, timeout is 2s
#if (DEBUG_TM)
    ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process:");
    esp_log_buffer_hex(ION_TM_ATCMD_TAG, cmd, 16);
#endif

    switch (cmd[0]) {
        case TM_BLE_PAIRING: {
            ESP_LOGI(ION_TM_ATCMD_TAG, "148 received pairing notification");
        }
        break;

        case TM_BLE_SESSION: {
            ESP_LOGI(ION_TM_ATCMD_TAG, "148 received paired notification");
        }
        break;

        case TM_BLE_DISCONNECT: {
            ESP_LOGI(ION_TM_ATCMD_TAG, "148 received disconnect notification");
        }
        break;

        case TM_BLE_BATTERY: {
#if (DEBUG_TM)
            memset(&battery, 0, sizeof(battery_t));
            memcpy(&battery.level, &cmd[1], sizeof(cmd[1]));
            memcpy(&battery.estimate_range, &cmd[2], sizeof(battery.estimate_range));
            memcpy(&battery.time_to_full, &cmd[4], sizeof(battery.time_to_full));
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process battery.level: %d",battery.level);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process battery.estimate_range: %d",battery.estimate_range);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process battery.time_to_full: %d",battery.time_to_full);
            send_msg_to_ble(TM_BLE_BATTERY, (uint8_t*)&battery, sizeof(battery_t));
#else
            send_msg_to_ble(TM_BLE_BATTERY, (uint8_t*)&cmd[1], sizeof(battery_t));
#endif
        }
        break;

        case TM_BLE_LAST_TRIP: {
#if (DEBUG_TM)
            memset(&trip, 0, sizeof(trip_t));
            memcpy(&trip.distance, &cmd[1], sizeof(trip.distance));
            memcpy(&trip.ride_time, &cmd[3], sizeof(trip.ride_time));
            memcpy(&trip.elec_used, &cmd[5], sizeof(trip.elec_used));
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process trip.distance: %d",trip.distance);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process trip.ride_time: %d",trip.ride_time);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process trip.elec_used: %d",trip.elec_used);
            send_msg_to_ble(TM_BLE_LAST_TRIP, (uint8_t*)&trip, sizeof(trip_t));
#else
            send_msg_to_ble(TM_BLE_LAST_TRIP, (uint8_t*)&cmd[1], sizeof(trip_t));
#endif
        }
        break;

        case TM_BLE_STEERING: {
            steering = cmd[1];
            send_msg_to_ble(TM_BLE_STEERING, (uint8_t*)&steering, sizeof(steering));
        }
        break;

        case TM_BLE_PING_BIKE: {
            send_msg_to_ble(TM_BLE_PING_BIKE, NULL, 0);
        }
        break;

        case TM_BLE_OPEN_SEAT: {
            send_msg_to_ble(TM_BLE_OPEN_SEAT, NULL, 0);
        }
        break;

        case TM_BLE_DIAG: {
            send_msg_to_ble(TM_BLE_DIAG, NULL, 0);
        }
        break;

        case TM_BLE_BIKE_INFO: {
#if (DEBUG_TM)
            memset(&battery, 0, sizeof(battery_t));
            memset(&trip, 0, sizeof(trip_t));

            memcpy(&battery.level, &cmd[1], sizeof(cmd[1]));
            memcpy(&battery.estimate_range, &cmd[2], sizeof(battery.estimate_range));
            memcpy(&battery.time_to_full, &cmd[4], sizeof(battery.time_to_full));
            memcpy(&trip.distance, &cmd[6], sizeof(trip.distance));
            memcpy(&trip.ride_time, &cmd[8], sizeof(trip.ride_time));
            memcpy(&trip.elec_used, &cmd[10], sizeof(trip.elec_used));
            memcpy(&steering, &cmd[11], sizeof(steering));
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process battery.level: %d",battery.level);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process battery.estimate_range: %d",battery.estimate_range);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process battery.time_to_full: %d",battery.time_to_full);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process trip.distance: %d",trip.distance);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process trip.ride_time: %d",trip.ride_time);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process trip.elec_used: %d",trip.elec_used);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process steering state: %d",steering);
#endif
            send_msg_to_ble(TM_BLE_BIKE_INFO, (uint8_t*)&cmd[1], sizeof(battery_t) + sizeof(trip_t) + sizeof(on_off_state_t));
        }
        break;

        case TM_BLE_POWER_ON: {
            send_msg_to_ble(TM_BLE_POWER_ON, NULL, 0);
        }
        break;

        case TM_BLE_CELLULAR: {
            cellular = cmd[1];
            send_msg_to_ble(TM_BLE_CELLULAR, (uint8_t*)&cellular, sizeof(cellular));
        }
        break;

        case TM_BLE_LOCATION: {
            location = cmd[1];
            send_msg_to_ble(TM_BLE_LOCATION, (uint8_t*)&location, sizeof(location));
        }
        break;

        case TM_BLE_RIDE_TRACKING: {
            ride_tracking = cmd[1];
            send_msg_to_ble(TM_BLE_RIDE_TRACKING, (uint8_t*)&ride_tracking, sizeof(ride_tracking));
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
    if ((msg_id >= TM_BLE_PAIRING) && (msg_id <= TM_BLE_OK)) {
        ble_to_tm_msg_t ble_to_tm_msg = {0};
        ble_to_tm_msg.msg_id = msg_id;
        if (data != NULL) {
            ble_to_tm_msg.len = len;
            memcpy(ble_to_tm_msg.data, data, ble_to_tm_msg.len);
        }
        xQueueSend(ble_to_tm_queue, (void*)&ble_to_tm_msg, (TickType_t)0);
    }
}

// static esp_err_t send_msg_to_ble(int msg_id, uint8_t *data, int len) {
esp_err_t send_msg_to_ble(int msg_id, uint8_t *data, int len) {
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