#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// #include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "tm_ble_gatts_server.h"
#include "tm_atcmd.h"
#include "tm_atcmd_parser.h"
#include "tm_ble.h"

#define BLE_TASK_PRIO           3
#define ION_BLE_TAG             "TM_BLE"

static void ble_task(void *arg)
{
    ion_ble_event_group = xEventGroupCreate();
    EventBits_t xEventGroupValue;
    ble_to_tm_msg_t ble_to_tm_msg = {0};

    while(1) {
        // wait for connect & pairing event
        xEventGroupValue = xEventGroupWaitBits(ion_ble_event_group, START_ADVERTISE |CONNECTING | REQUEST_TO_PAIR |
                                                                    CONFIRM_PASSKEY | DISCONNECT| CONNECTED,
                                                                    true,//clear on exit
                                                                    false,//do not wait for all bits to be triggered
                                                                    portMAX_DELAY);
        if ((xEventGroupValue & START_ADVERTISE) !=0) {
            // start advertising
            tm_ble_start_advertising();
            ESP_LOGI(ION_BLE_TAG, "start advertising...");
        }
        if ((xEventGroupValue & CONNECTING) !=0) {
            // user request to connect after scanned
            ESP_LOGI(ION_BLE_TAG, "user request to connect");
            // bike's screen should show "connect to phone"
            // spi_slave_send_ble_connect_event();
        }
        if ((xEventGroupValue & REQUEST_TO_PAIR) !=0) {
            // generated passkey
            ESP_LOGI(ION_BLE_TAG, "passkey generated");
            // spi_slave_display_6digits_pairing_number();
        }
        if ((xEventGroupValue & CONFIRM_PASSKEY) !=0) {
            // this event is received from 148 when user press(o) on bike
            ESP_LOGI(ION_BLE_TAG, "confirm button(o) onbike pressed");
        }
        if ((xEventGroupValue & DISCONNECT) !=0) {
            // BLE disconnect
            ESP_LOGI(ION_BLE_TAG, "BLE disconected");
            // spi_slave_send_ble_disconnect_event();
            tm_ble_start_advertise();
        }
        if ((xEventGroupValue & CONNECTED) !=0) {
            // BLE disconnect
            ESP_LOGI(ION_BLE_TAG, "BLE connected");
            ble_to_tm_msg.msg_id = LOGIN;
            ble_to_tm_msg.len = 0;
            to_tm_login_msg(&ble_to_tm_msg);
        }
    }
}

void tm_ble_init(void)
{
    tm_ble_gatts_server_init();

    //Create and start stats task
    xTaskCreatePinnedToCore(ble_task, "ble_task", 4096, NULL, BLE_TASK_PRIO, NULL, tskNO_AFFINITY);
}

void tm_ble_start_advertise(void)
{
    xEventGroupSetBits(ion_ble_event_group, START_ADVERTISE);
}