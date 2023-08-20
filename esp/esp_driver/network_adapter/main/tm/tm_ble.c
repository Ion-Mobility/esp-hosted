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
    while(1) {
        //wait for message
        if (xQueueReceive(ble_queue, &ble_msg, portMAX_DELAY) != pdTRUE)
            continue;

        //message parsing & processing
        switch (ble_msg.msg_id) {
            case BLE_START_ADVERTISE:
                ESP_LOGI(ION_BLE_TAG, "BLE_START_ADVERTISE");
                ble_gatts_start_advertise();
                break;

            case BLE_DISCONNECT:
                ESP_LOGI(ION_BLE_TAG, "BLE_DISCONNECT");
                ble_gatts_start_advertise();
                break;

            case BLE_CONNECTING:
                ESP_LOGI(ION_BLE_TAG, "BLE_CONNECTING...");
                break;

            case BLE_CONNECTED:
                ESP_LOGI(ION_BLE_TAG, "BLE_CONNECTED");
                ble_to_tm_msg.msg_id = LOGIN;
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

            case BLE_TM_MSG:
                ESP_LOGI(ION_BLE_TAG, "BLE_TM_MSG");

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