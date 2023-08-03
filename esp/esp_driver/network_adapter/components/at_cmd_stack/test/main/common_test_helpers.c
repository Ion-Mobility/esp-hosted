#include "common_test_helpers.h"
#include "protocol_examples_common.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include <stdbool.h>


static bool is_connected_to_wifi = false;

void test_begin_setup()
{
    if (is_connected_to_wifi)
        return;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    is_connected_to_wifi = true;
}

void test_done_clear_up()
{
    if (!is_connected_to_wifi)
        return;

    ESP_ERROR_CHECK(example_disconnect());
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    ESP_ERROR_CHECK(nvs_flash_deinit());
    is_connected_to_wifi = false;
}