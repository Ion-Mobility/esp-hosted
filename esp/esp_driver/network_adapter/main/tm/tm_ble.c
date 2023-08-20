#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "tm_ble_gatts_server.h"
#include "tm_atcmd.h"
#include "tm_atcmd_parser.h"
#include "tm_ble.h"
#include "crypto.h"

#define BLE_TASK_PRIO           3
#define ION_BLE_TAG             "TM_BLE"

QueueHandle_t ble_queue;

static void ble_task(void *arg)
{
    ble_msg_t ble_msg = {0};
    ble_to_tm_msg_t ble_to_tm_msg = {0};

    login_t login = {0};
    charge_t charge = {0};
    battery_t battery = {0};
    trip_t trip = {0};

    uint8_t ciphertext[BLE_MSG_MAX_LEN];
    size_t ciphertext_len;
    uint8_t mac[16];    /* Message authentication code */

    while(1) {
        //wait for message
        if (xQueueReceive(ble_queue, &ble_msg, portMAX_DELAY) != pdTRUE)
            continue;

        //message parsing & processing
        switch (ble_msg.msg_id) {
            case BLE_START_ADVERTISE:
                ESP_LOGI(ION_BLE_TAG, "BLE_START_ADVERTISE");
                tm_ble_gatts_start_advertise();
                break;

            case BLE_DISCONNECT:
                ESP_LOGI(ION_BLE_TAG, "BLE_DISCONNECT");
                tm_ble_gatts_start_advertise();
                break;

            case BLE_CONNECTING:
                ESP_LOGI(ION_BLE_TAG, "BLE_CONNECTING...");
                break;

            case BLE_CONNECTED:
                ESP_LOGI(ION_BLE_TAG, "BLE_CONNECTED");
                ble_to_tm_msg.msg_id = BLE_TM_LOGIN;
                ble_to_tm_msg.len = 0;
                to_tm_login_msg(&ble_to_tm_msg);
                break;

            case BLE_AUTHEN:
                ESP_LOGI(ION_BLE_TAG, "BLE_AUTHEN");
                break;

            case BLE_PAIRING:
                ESP_LOGI(ION_BLE_TAG, "BLE_PAIRING...");
                break;

            case BLE_SESSION:
                ESP_LOGI(ION_BLE_TAG, "BLE_SESSION");
                break;

            case BLE_PHONE_READ:
                ESP_LOGI(ION_BLE_TAG, "BLE_PHONE_READ");
                break;

            case BLE_PHONE_WRITE:
                ESP_LOGI(ION_BLE_TAG, "BLE_PHONE_WRITE");
                esp_log_buffer_hex(ION_BLE_TAG, ble_msg.data, ble_msg.len);
                break;

            case TM_BLE_LOGIN:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_LOGIN");
                memset(&login, 0, sizeof(login_t));
                memcpy(&login, ble_msg.data, sizeof(login_t));
                ESP_LOGI(ION_BLE_TAG, "battery                    %d",login.battery);
                ESP_LOGI(ION_BLE_TAG, "est_range                  %d",login.est_range);
                ESP_LOGI(ION_BLE_TAG, "odo                        %d",login.odo);
                ESP_LOGI(ION_BLE_TAG, "last_trip_distance         %d",login.last_trip_distance);
                ESP_LOGI(ION_BLE_TAG, "last_trip_time             %d",login.last_trip_time);
                ESP_LOGI(ION_BLE_TAG, "last_trip_elec_used        %d",login.last_trip_elec_used);
                ESP_LOGI(ION_BLE_TAG, "last_charge_level          %d",login.last_charge_level);
                ESP_LOGI(ION_BLE_TAG, "distance_since_last_charge %d",login.distance_since_last_charge);
                // todo: encrypt these data before sending to phone
                message_encrypt(ciphertext, &ciphertext_len, mac, ble_msg.data, ble_msg.len);
                //todo: send to phone
                break;

            case TM_BLE_CHARGE:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_CHARGE");
                memset(&charge, 0, sizeof(charge_t));
                memcpy(&charge, ble_msg.data, sizeof(charge_t));
                ESP_LOGI(ION_BLE_TAG, "charge state            %d",charge.state);
                ESP_LOGI(ION_BLE_TAG, "charge vol              %d",charge.vol);
                ESP_LOGI(ION_BLE_TAG, "charge cur              %d",charge.cur);
                ESP_LOGI(ION_BLE_TAG, "charge cycle            %d",charge.cycle);
                ESP_LOGI(ION_BLE_TAG, "charge time_to_full     %d",charge.time_to_full);
                // todo: encrypt these data before sending to phone
                message_encrypt(ciphertext, &ciphertext_len, mac, ble_msg.data, ble_msg.len);
                //todo: send to phone
                break;

            case TM_BLE_BATTERY:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_BATTERY");
                memset(&battery, 0, sizeof(battery_t));
                memcpy(&battery, ble_msg.data, sizeof(battery_t));
                ESP_LOGI(ION_BLE_TAG, "battery level           %d",battery.level);
                ESP_LOGI(ION_BLE_TAG, "estimate range vol      %d",battery.estimate_range);
                // todo: encrypt these data before sending to phone
                message_encrypt(ciphertext, &ciphertext_len, mac, ble_msg.data, ble_msg.len);
                //todo: send to phone
                break;

            case TM_BLE_LAST_TRIP:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_LAST_TRIP");
                memset(&trip, 0, sizeof(trip_t));
                memcpy(&trip, ble_msg.data, sizeof(trip_t));
                // todo: encrypt these data before sending to phone
                ESP_LOGI(ION_BLE_TAG, "last trip distance      %d",trip.distance);
                ESP_LOGI(ION_BLE_TAG, "last trip ride time     %d",trip.ride_time);
                ESP_LOGI(ION_BLE_TAG, "last trip electric used %d",trip.elec_used);
                // todo: encrypt these data before sending to phone
                message_encrypt(ciphertext, &ciphertext_len, mac, ble_msg.data, ble_msg.len);
                //todo: send to phone
                break;

            default:
                continue;
                break;
        }
    }
}

void tm_ble_init(void)
{
    tm_ble_gatts_server_init();
    ble_queue = xQueueCreate(20, sizeof(ble_msg_t));

    //Create and start stats task
    xTaskCreatePinnedToCore(ble_task, "ble_task", 8192, NULL, BLE_TASK_PRIO, NULL, tskNO_AFFINITY);
}

void tm_ble_start_advertise(void)
{
    ble_msg_t ble_msg = {0};
    ble_msg.msg_id = BLE_START_ADVERTISE;
    ble_msg.len = 0;
    xQueueSend(ble_queue, (void*)&ble_msg, (TickType_t)0);
}