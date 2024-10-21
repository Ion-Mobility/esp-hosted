#include "heartbeat.h"

static const char OTA_TAG[] = "OTA_SWITCH";

// Create watchdog timer for 118 and 148
static esp_timer_handle_t watchdog_timer_118;
static esp_timer_handle_t watchdog_timer_148;

// Function to reset the watchdog timer for 118
static void reset_watchdog_118() {
    esp_timer_stop(watchdog_timer_118);  // Stop the current timer
    esp_timer_start_once(watchdog_timer_118, WATCHDOG_TIMEOUT_MS * 1000);  // Restart the timer
}

// Function to reset the watchdog timer for 148
static void reset_watchdog_148() {
    esp_timer_stop(watchdog_timer_148);  // Stop the current timer
    esp_timer_start_once(watchdog_timer_148, WATCHDOG_TIMEOUT_MS * 1000);  // Restart the timer
}


void switch_partition(int partition_id) {
    const esp_partition_t *target_partition = NULL;

    // Get the current running partition
    const esp_partition_t *running_partition = esp_ota_get_running_partition();

    ESP_LOGI(OTA_TAG, "Switch to partition: %d",partition_id);

    // Determine the target partition based on the input
    switch (partition_id) {
        case 0: // Factory partition
            target_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
            break;
        case 1: // OTA1 partition
            target_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
            break;
        case 2: // OTA2 partition
            target_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
            break;
        default:
            ESP_LOGE(OTA_TAG, "Invalid partition ID. Use 0 for factory, 1 for OTA1, 2 for OTA2");
            return;
    }

    if (target_partition != NULL) {
        ESP_LOGI(OTA_TAG, "Current partition: type: 0x%x, subtype: 0x%x, address: 0x%x",
                 (unsigned int)running_partition->type,
                 (unsigned int)running_partition->subtype,
                 (unsigned int)running_partition->address);

        ESP_LOGI(OTA_TAG, "Switching to partition: type: 0x%x, subtype: 0x%x, address: 0x%x",
                 (unsigned int)target_partition->type,
                 (unsigned int)target_partition->subtype,
                 (unsigned int)target_partition->address);

        // Set the next boot partition to the new partition
        esp_err_t err = esp_ota_set_boot_partition(target_partition);
        if (err == ESP_OK) {
            ESP_LOGI(OTA_TAG, "Partition switch successful. Restarting...");
            esp_restart();  // Restart the device to boot from the new partition
        } else {
            ESP_LOGE(OTA_TAG, "Failed to switch partition, error: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(OTA_TAG, "No valid partition found for the given ID");
    }
}

// This is the function to be called if the watchdog times out
static void watchdog_timeout_callback(void *arg) {
    ESP_LOGE(OTA_TAG, "No CAN packet received for 10 seconds!");
    // Switch to esp-diag at partition 0
    switch_partition(0);
}

// Task to receive CAN messages
void can_receive_task(void *arg) {
    twai_message_t rx_msg;
	// Create watchdog timer for 118 and 148
    const esp_timer_create_args_t watchdog_timer_args = {
        .callback = &watchdog_timeout_callback,
        .name = "CAN Watchdog"
    };
    esp_timer_create(&watchdog_timer_args, &watchdog_timer_118);
    esp_timer_create(&watchdog_timer_args, &watchdog_timer_148);
	
	// Start the watchdog timer with 10s timeout
    esp_timer_start_once(watchdog_timer_118, WATCHDOG_TIMEOUT_MS * 1000);
    esp_timer_start_once(watchdog_timer_148, WATCHDOG_TIMEOUT_MS * 1000);

    while (1) {
        // Block until a CAN message is received
        if (twai_receive(&rx_msg, portMAX_DELAY) == ESP_OK) {
            if (rx_msg.identifier == CAN_118_OTA_ID || rx_msg.identifier == CAN_118_VERSION_ID) {
                ESP_LOGI(OTA_TAG, "Receive Realtime Heartbeat - ID 0x%lx", rx_msg.identifier);
			    // Reset watchdog on successful reception
                reset_watchdog_118();
            }

            if (rx_msg.identifier == CAN_148_OTA_ID || rx_msg.identifier == CAN_148_VERSION_ID) {
                ESP_LOGI(OTA_TAG, "Receive Telematic Heartbeat - ID 0x%lx", rx_msg.identifier);
                // Reset watchdog on successful reception
                reset_watchdog_148();
            }

            if (rx_msg.identifier == CAN_WIFI_STATUS_ID) {
                ESP_LOGI(OTA_TAG, "Received WIFI_STATUS_PACKET");
                // start partition switching
                switch_partition(rx_msg.data[0]);
            }
        } else {
            ESP_LOGI(OTA_TAG, "Failed to receive CAN message");
        }
    }
}