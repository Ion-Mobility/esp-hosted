#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// #include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "ble_gatts_server.h"
#include "ion_spi_slave.h"
#include "ion_ble.h"

#define BLE_TASK_PRIO           3
#define ION_BLE_TAG             "ION_BLE"

static void ble_task(void *arg)
{
    ion_ble_event_group = xEventGroupCreate();
    EventBits_t xEventGroupValue;
    while(1) {
        xEventGroupValue = xEventGroupWaitBits(ion_ble_event_group, START_ADVERTISE, true, true, portMAX_DELAY);
        if ((xEventGroupValue & START_ADVERTISE) !=0) {
            // start advertising
            tm_ble_start_advertising();
            ESP_LOGI(ION_BLE_TAG, "start advertising...");

            while(1) {
                // wait for connect & pairing event
                xEventGroupValue = xEventGroupWaitBits(ion_ble_event_group, CONNECT | REQUEST_TO_PAIR |
                                                                            CONFIRM_PASSKEY | DISCONNECT,
                                                                            true,//clear on exit
                                                                            false,//do not wait for all bits to be triggered
                                                                            portMAX_DELAY);
                if ((xEventGroupValue & CONNECT) !=0) {
                    // user request to connect after scanned
                    ESP_LOGI(ION_BLE_TAG, "user request to connect");
                    // bike's screen should show "connect to phone"
                    spi_slave_send_ble_connect_event();
                }
                if ((xEventGroupValue & REQUEST_TO_PAIR) !=0) {
                    // generated passkey
                    ESP_LOGI(ION_BLE_TAG, "passkey generated");
                    spi_slave_display_6digits_pairing_number();
                }
                if ((xEventGroupValue & CONFIRM_PASSKEY) !=0) {
                    // this event is received from 148 when user press(o) on bike
                    ESP_LOGI(ION_BLE_TAG, "confirm button(o) onbike pressed");
                }
                if ((xEventGroupValue & DISCONNECT) !=0) {
                    // BLE disconnect
                    ESP_LOGI(ION_BLE_TAG, "BLE disconected");
                    spi_slave_send_ble_disconnect_event();
                    tm_ble_start_advertise();
                    break;
                }
            }
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