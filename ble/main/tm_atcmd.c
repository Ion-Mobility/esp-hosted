#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"

#include "tm_spi_slave_driver.h"
#include "tm_atcmd.h"
#include "tm_ble.h"
#include "crypto.h"

#define ATCMD_TASK_PRIO         3
#define ATCMD_CMD_LEN           32
#define ATCMD_TIMEOUT           2000        //ms
#define ION_TM_ATCMD_TAG        "TM_ATCMD"

QueueHandle_t ble_to_tm_queue;

static esp_err_t tm_atcmd_construct(ble_to_tm_msg_t *msg, char* txbuf);
static IRAM_ATTR esp_err_t tm_atcmd_transmit(char* tx, char* rx);
static esp_err_t tm_atcmd_process(char* cmd);
static esp_err_t tm_atcmd_handler(ble_to_tm_msg_t* msg);

battery_t battery = {0};
uint8_t power = STATE;
uint8_t cellular = STATE;
uint8_t location = STATE;
uint8_t ride_tracking = STATE;
struct tm timeinfo;

static void tm_atcmd_task(void *arg)
{
    ble_to_tm_msg_t ble_to_tm_msg = {0};
    ble_to_tm_queue = xQueueCreate(10, sizeof(ble_to_tm_msg_t));
 
    while(1) {
        if (xQueueReceive(ble_to_tm_queue, &ble_to_tm_msg, portMAX_DELAY) != pdTRUE)
            continue;
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
    txbuf[0] = (uint8_t)(msg->msg_id & 0xFF);
    txbuf[1] = (uint8_t)(msg->len & 0xFF);
    memcpy(&txbuf[2], &msg->data[0], msg->len);
    return ESP_OK;
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
    uint8_t buf[BLE_MSG_MAX_LEN];
    ble_msg_t to_phone_msg = {0};
    size_t len;
    int msg_id;
    size_t buf_len;

#if (DEBUG_TM)
    ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process:");
    esp_log_buffer_hex(ION_TM_ATCMD_TAG, cmd, 16);
#endif

    switch (cmd[0]) {
        case PHONE_BLE_PAIRING: {
            ESP_LOGI(ION_TM_ATCMD_TAG, "148 received pairing notification");
        }
        break;

        case PHONE_BLE_SESSION: {
            ESP_LOGI(ION_TM_ATCMD_TAG, "148 received paired notification");
        }
        break;

        case BLE_DISCONNECT: {
            ESP_LOGI(ION_TM_ATCMD_TAG, "148 received disconnect notification");
        }
        break;

        case TM_BLE_GET_TIME: {
            timeinfo.tm_sec     = cmd[2];
            timeinfo.tm_min     = cmd[3];
            timeinfo.tm_hour    = cmd[4];
            timeinfo.tm_mday    = cmd[5];
            timeinfo.tm_mon     = cmd[6];
            timeinfo.tm_year    = cmd[7]+1900;
            timeinfo.tm_wday    = cmd[8];
#if (DEBUG_TM)
            char strftime_buf[64];
            time_t now;
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process: sec:  %d",timeinfo.tm_sec);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process: min:  %d",timeinfo.tm_min);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process: hour: %d",timeinfo.tm_hour);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process: mday: %d",timeinfo.tm_mday);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process: mon:  %d",timeinfo.tm_mon);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process: year: %d",timeinfo.tm_year);
            ESP_LOGI(ION_TM_ATCMD_TAG, "tm_atcmd_process: wday: %d",timeinfo.tm_wday);
            esp_log_buffer_hex(ION_TM_ATCMD_TAG, &cmd[1], sizeof(timeinfo));
            strftime(strftime_buf, sizeof(strftime_buf), "%x - %I:%M%p", &timeinfo);
            now = mktime(&timeinfo);
            ESP_LOGI(ION_TM_ATCMD_TAG, "The current date/time is: %s", strftime_buf);
            ESP_LOGI(ION_TM_ATCMD_TAG, "The current timestamp is: %ld", now);
            // update system time
            // time(&now);
#endif
        }
        break;

        default:
            msg_id = cmd[0];
            len = cmd[1];
            buf_len = serialize_data(buf, 0, (uint8_t*)&msg_id, sizeof(msg_id));
            memcpy(&buf[sizeof(msg_id)], &cmd[2], len);
            buf_len += len;
            message_encrypt(to_phone_msg.data, (size_t*)&to_phone_msg.len, buf, buf_len);
            if (to_phone_msg.len <= BLE_MSG_MAX_LEN) {
                to_phone_msg.msg_id = PHONE_BLE_COMMAND;
                send_to_phone(&to_phone_msg);
            }
#if (DEBUG_BLE)
            ESP_LOGI(ION_TM_ATCMD_TAG, "send_to_phone msg_id: %d",to_phone_msg.msg_id);
            esp_log_buffer_hex(ION_TM_ATCMD_TAG, to_phone_msg.data, to_phone_msg.len);
#endif
        break;
    }
    return ESP_OK;
}

void send_to_tm_queue(ble_to_tm_msg_t *msg)
{
    xQueueSend(ble_to_tm_queue, (void*)msg, (TickType_t)0);
}