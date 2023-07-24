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
#define ATCMD_TIMEOUT       2000        //ms
#define ATCMD_SMART_KEY_LEN 16
#define ION_TM_ATCMD_TAG    "TM_ATCMD"

QueueHandle_t ble_to_tm_queue;
QueueHandle_t tm_to_ble_queue;

static esp_err_t tm_atcmd_construct(ble_to_tm_msg_t *msg, char* txbuf);
static esp_err_t tm_atcmd_send(char* msg);
static esp_err_t tm_atcmd_recv(char* cmd, int* len);
static esp_err_t tm_atcmd_process(char* cmd, int cmd_len);
static esp_err_t tm_atcmd_handler(ble_to_tm_msg_t* msg);

static void tm_atcmd_task(void *arg)
{
    ble_to_tm_msg_t ble_to_tm_msg = {0};
    ble_to_tm_queue = xQueueCreate(10, sizeof(ble_to_tm_msg_t));
 
    while(1) {
        if (xQueueReceive(ble_to_tm_queue, &ble_to_tm_msg, portMAX_DELAY) != pdTRUE)
            continue;

        switch (ble_to_tm_msg.msg_id) {
            case LOGIN:
                ESP_LOGI(ION_TM_ATCMD_TAG, "login info");
                break;

            case CHARGE:
                ESP_LOGI(ION_TM_ATCMD_TAG, "charge info");
                break;

            case BATTERY:
                ESP_LOGI(ION_TM_ATCMD_TAG, "battery info");
                break;

            case LAST_TRIP:
                ESP_LOGI(ION_TM_ATCMD_TAG, "last trip info");
                break;

            case LOCK:
                ESP_LOGI(ION_TM_ATCMD_TAG, "lock info");
                break;

            case UNLOCK:
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
                if (ret == ESP_OK) {
                    msg->msg_id = BLE_OK;
                } else {
                    msg->msg_id = BLE_ERR;
                }
                tm_atcmd_construct(msg, txbuf);
                tm_atcmd_send(txbuf);
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
        case BLE_OK:
            snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+OK\r\n");
            break;
        case BLE_ERR:
            snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+ERR\r\n");
            break;
        case LOGIN:
            snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+LOGIN\r\n");
            break;
        case CHARGE:
            snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+CHARGE\r\n");
            break;
        case BATTERY:
            snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+BATTERY\r\n");
            break;
        case LAST_TRIP:
            snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+TRIP\r\n");
            break;
        case LOCK:
            // auto lock, phone send 16 bytes key to lock via ble
            if (msg->len > ATCMD_SMART_KEY_LEN)
                snprintf(txbuf, ATCMD_CMD_LEN+1, "ERR,invalid param\r\n");
            else {
                memcpy(key, msg->data, ATCMD_SMART_KEY_LEN);
                snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+LOCK,%s\r\n",key);
                // snprintf(txbuf, ATCMD_CMD_LEN+1, "BLE+LOCK,1234567890123456\r\n");
            }
            break;
        case UNLOCK:
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
    login_t login = {0};
    charge_t charge = {0};
    battery_t battery = {0};
    trip_t trip = {0};
    bool ret;

    // process each command here, timeout is 2s
    ESP_LOGI(ION_TM_ATCMD_TAG, "cmd_data:%s", cmd);
    int cmd_id = tm_atcmd_recv_parser(cmd, cmd_len);

    switch (cmd_id) {
        case DATA_LOGIN: {
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

            //todo: send these data to phone
            ESP_LOGI(ION_TM_ATCMD_TAG, "battery                    %d",login.battery);
            ESP_LOGI(ION_TM_ATCMD_TAG, "est_range                  %d",login.est_range);
            ESP_LOGI(ION_TM_ATCMD_TAG, "odo                        %d",login.odo);
            ESP_LOGI(ION_TM_ATCMD_TAG, "last_trip_distance         %d",login.last_trip_distance);
            ESP_LOGI(ION_TM_ATCMD_TAG, "last_trip_time             %d",login.last_trip_time);
            ESP_LOGI(ION_TM_ATCMD_TAG, "last_trip_elec_used        %d",login.last_trip_elec_used);
            ESP_LOGI(ION_TM_ATCMD_TAG, "last_charge_level          %d",login.last_charge_level);
            ESP_LOGI(ION_TM_ATCMD_TAG, "distance_since_last_charge %d",login.distance_since_last_charge);
        }
        break;

        case DATA_CHARGE: {
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
            //todo: send these data to phone
            ESP_LOGI(ION_TM_ATCMD_TAG, "charge state            %d",charge.state);
            ESP_LOGI(ION_TM_ATCMD_TAG, "charge vol              %d",charge.vol);
            ESP_LOGI(ION_TM_ATCMD_TAG, "charge cur              %d",charge.cur);
            ESP_LOGI(ION_TM_ATCMD_TAG, "charge cycle            %d",charge.cycle);
            ESP_LOGI(ION_TM_ATCMD_TAG, "charge time_to_full     %d",charge.time_to_full);
        }
        break;

        case DATA_BATTERY: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //AT+BATTERY,xxx,xxxxx
            //level,estimate range
            memset(&battery, 0, sizeof(battery_t));
            battery.level           = atoi(&cmd[11]);
            battery.estimate_range  = atoi(&cmd[15]);

            //todo: send these data to phone
            ESP_LOGI(ION_TM_ATCMD_TAG, "battery level           %d",battery.level);
            ESP_LOGI(ION_TM_ATCMD_TAG, "estimate range vol      %d",battery.estimate_range);
        }
        break;

        case DATA_TRIP: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //AT+TRIP,xxxxx,xxxxx,xxxxx
            //distance, ride_time, elec_used
            memset(&trip, 0, sizeof(trip_t));
            trip.distance       = atoi(&cmd[8]);
            trip.ride_time      = atoi(&cmd[14]);
            trip.elec_used      = atoi(&cmd[20]);
            //todo: send these data to phone
            ESP_LOGI(ION_TM_ATCMD_TAG, "last trip distance      %d",trip.distance);
            ESP_LOGI(ION_TM_ATCMD_TAG, "last trip ride time     %d",trip.ride_time);
            ESP_LOGI(ION_TM_ATCMD_TAG, "last trip electric used %d",trip.elec_used);
        }
        break;

        case EVENT_LOCK: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //AT+LOCKED,x
            ret = atoi(&cmd[10]);
            //todo: send these data to phone
            if (ret)
                ESP_LOGI(ION_TM_ATCMD_TAG, "bike successful locked by phone");
            else
                ESP_LOGI(ION_TM_ATCMD_TAG, "bike failed to lock by phone");
        }
        break;

        case EVENT_UNLOCK: {
            //0         1         2         3         4         5
            //01234567890123456789012345678901234567890123456789012
            //AT+UNLOCKED,x
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

void to_tm_login_msg(ble_to_tm_msg_t* pMsg) {
    ble_to_tm_msg_t ble_to_tm_msg;
    memcpy(&ble_to_tm_msg, pMsg, sizeof(ble_to_tm_msg_t));
    xQueueSend(ble_to_tm_queue, (void*)&ble_to_tm_msg, (TickType_t)0);
}