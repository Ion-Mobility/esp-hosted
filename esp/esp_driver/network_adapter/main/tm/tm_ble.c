#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "tm_ble_gatts_server.h"
#include "tm_atcmd.h"
// #include "tm_atcmd_parser.h"
#include "tm_ble.h"
#include "crypto.h"

#define BLE_TASK_PRIO           3
#define ION_BLE_TAG             "TM_BLE"

// ble to phone serialization
#define MSG_ID_BYTE_IDX         0
#define MSG_LEN_BYTE_IDX        1
#define MSG_DATA_BYTE_IDX       5

QueueHandle_t ble_queue;
static uint8_t pair_status = UNPAIRED;
static void send_to_phone(ble_msg_t *pble_msg);

static void ble_task(void *arg)
{
    ble_msg_t to_ble_msg = {0};
    ble_msg_t to_phone_msg = {0};

    general_t general = {0};
    charge_t charge = {0};
    battery_t battery = {0};
    trip_t trip = {0};

    uint8_t mac[16];    /* Message authentication code */

    while(1) {
        //wait for message
        if (xQueueReceive(ble_queue, &to_ble_msg, portMAX_DELAY) != pdTRUE)
            continue;

        //message parsing & processing
        switch (to_ble_msg.msg_id) {
            case BLE_START_ADVERTISE:
                ESP_LOGI(ION_BLE_TAG, "BLE_START_ADVERTISE");
                tm_ble_gatts_start_advertise();
                break;

            case BLE_DISCONNECT:
                ESP_LOGI(ION_BLE_TAG, "BLE_DISCONNECT");
                pair_status = UNPAIRED;
                tm_ble_gatts_start_advertise();
                break;

            case BLE_CONNECTING:
                ESP_LOGI(ION_BLE_TAG, "BLE_CONNECTING...");
                break;

            case BLE_CONNECTED:
                ESP_LOGI(ION_BLE_TAG, "BLE_CONNECTED");
                break;

            case PHONE_BLE_PAIRING:
                if (pair_status == UNPAIRED) {
                    ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_PAIRING");
                    esp_log_buffer_hex(ION_BLE_TAG, to_ble_msg.data, to_ble_msg.len);
                    //todo: verify pairing request

                    //verified
                    pair_status = PAIRED;

                    //todo: response to phone

                }
                break;

            case PHONE_BLE_SESSION:
                if (pair_status == PAIRED) {
                    //todo: verify this msg
                    ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_SESSION");
                    pair_status = SESSION_CREATED;
                }
                break;

            case PHONE_BLE_GENERAL:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_GENERAL");
                if (pair_status == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_GENERAL, NULL, 0);
                }
                break;

            case PHONE_BLE_BATTERY:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_BATTERY");
                if (pair_status == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_BATTERY, NULL, 0);
                }
                break;

            case PHONE_BLE_CHARGE:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_CHARGE");
                if (pair_status == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_CHARGE, NULL, 0);
                }
                break;


            case PHONE_BLE_LAST_TRIP:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_LAST_TRIP");
                if (pair_status == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_LAST_TRIP, NULL, 0);
                }                        
                break;

            case TM_BLE_GENERAL:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_GENERAL");
                if (pair_status == SESSION_CREATED) {
                    //todo: debug only, remove later
                    esp_log_buffer_hex(ION_BLE_TAG, to_ble_msg.data, to_ble_msg.len);
                    memset(&general, 0, sizeof(general_t));
                    memcpy(&general, to_ble_msg.data, sizeof(general_t));
                    ESP_LOGI(ION_BLE_TAG, "battery                    %d",general.battery);
                    ESP_LOGI(ION_BLE_TAG, "est_range                  %d",general.est_range);
                    ESP_LOGI(ION_BLE_TAG, "odo                        %d",general.odo);
                    ESP_LOGI(ION_BLE_TAG, "last_trip_distance         %d",general.last_trip_distance);
                    ESP_LOGI(ION_BLE_TAG, "last_trip_time             %d",general.last_trip_time);
                    ESP_LOGI(ION_BLE_TAG, "last_trip_elec_used        %d",general.last_trip_elec_used);
                    ESP_LOGI(ION_BLE_TAG, "last_charge_level          %d",general.last_charge_level);
                    ESP_LOGI(ION_BLE_TAG, "distance_since_last_charge %d",general.distance_since_last_charge);

                    //encrypt data & send to phone
                    message_encrypt(to_phone_msg.data, (size_t*)&to_phone_msg.len, mac, to_ble_msg.data, to_ble_msg.len);
                    if (to_phone_msg.len <= BLE_MSG_MAX_LEN) {
                        to_phone_msg.msg_id = PHONE_BLE_GENERAL;
                        send_to_phone(&to_phone_msg);
                    }
                }
                break;

            case TM_BLE_CHARGE:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_CHARGE");
                if (pair_status == SESSION_CREATED) {
                    //todo: debug only, remove later
                    memset(&charge, 0, sizeof(charge_t));
                    memcpy(&charge, to_ble_msg.data, sizeof(charge_t));
                    ESP_LOGI(ION_BLE_TAG, "charge state            %d",charge.state);
                    ESP_LOGI(ION_BLE_TAG, "charge vol              %d",charge.vol);
                    ESP_LOGI(ION_BLE_TAG, "charge cur              %d",charge.cur);
                    ESP_LOGI(ION_BLE_TAG, "charge cycle            %d",charge.cycle);
                    ESP_LOGI(ION_BLE_TAG, "charge time_to_full     %d",charge.time_to_full);
                    //encrypt data & send to phone
                    message_encrypt(to_phone_msg.data, (size_t*)&to_phone_msg.len, mac, to_ble_msg.data, to_ble_msg.len);
                    if (to_phone_msg.len <= BLE_MSG_MAX_LEN) {
                        to_phone_msg.msg_id = PHONE_BLE_CHARGE;
                        send_to_phone(&to_phone_msg);
                    }
                }
                break;

            case TM_BLE_BATTERY:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_BATTERY");
                if (pair_status == SESSION_CREATED) {
                    //todo: debug only, remove later
                    memset(&battery, 0, sizeof(battery_t));
                    memcpy(&battery, to_ble_msg.data, sizeof(battery_t));
                    ESP_LOGI(ION_BLE_TAG, "battery level           %d",battery.level);
                    ESP_LOGI(ION_BLE_TAG, "estimate range vol      %d",battery.estimate_range);
                    //encrypt data & send to phone
                    message_encrypt(to_phone_msg.data, (size_t*)&to_phone_msg.len, mac, to_ble_msg.data, to_ble_msg.len);
                    if (to_phone_msg.len <= BLE_MSG_MAX_LEN) {
                        to_phone_msg.msg_id = PHONE_BLE_BATTERY;
                        send_to_phone(&to_phone_msg);
                    }
                }
                break;

            case TM_BLE_LAST_TRIP:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_LAST_TRIP");
                if (pair_status == SESSION_CREATED) {
                    //todo: debug only, remove later
                    memset(&trip, 0, sizeof(trip_t));
                    memcpy(&trip, to_ble_msg.data, sizeof(trip_t));
                    // todo: encrypt these data before sending to phone
                    ESP_LOGI(ION_BLE_TAG, "last trip distance      %d",trip.distance);
                    ESP_LOGI(ION_BLE_TAG, "last trip ride time     %d",trip.ride_time);
                    ESP_LOGI(ION_BLE_TAG, "last trip electric used %d",trip.elec_used);
                    //encrypt data & send to phone
                    message_encrypt(to_phone_msg.data, (size_t*)&to_phone_msg.len, mac, to_ble_msg.data, to_ble_msg.len);
                    if (to_phone_msg.len <= BLE_MSG_MAX_LEN) {
                        to_phone_msg.msg_id = PHONE_BLE_LAST_TRIP;
                        send_to_phone(&to_phone_msg);
                    }
                }
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
    ble_msg_t to_ble_msg = {0};
    to_ble_msg.msg_id = BLE_START_ADVERTISE;
    to_ble_msg.len = 0;
    xQueueSend(ble_queue, (void*)&to_ble_msg, (TickType_t)0);
}

static void send_to_phone(ble_msg_t *pble_msg) {
    int len = pble_msg->len + sizeof(pble_msg->msg_id) + sizeof(pble_msg->len); 
    tm_ble_gattd_send_to_phone((uint8_t*)pble_msg, len);
}