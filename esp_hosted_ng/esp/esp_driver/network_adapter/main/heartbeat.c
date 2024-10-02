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


static void switch_partition() {
    // Get the currently running partition
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    
    // Get the next OTA partition to switch to (the inactive one)
    const esp_partition_t *next_partition = esp_ota_get_next_update_partition(NULL);

    if (next_partition != NULL) {
        ESP_LOGI(OTA_TAG, "Current partition: type: 0x%x, subtype: 0x%x, address: 0x%x",
                 (unsigned int)running_partition->type,
				 (unsigned int)running_partition->subtype,
				 (unsigned int)running_partition->address);
        ESP_LOGI(OTA_TAG, "Switching to partition: type: 0x%x, subtype: 0x%x, address: 0x%x",
                 (unsigned int)next_partition->type,
				 (unsigned int)next_partition->subtype,
				 (unsigned int)next_partition->address);

        // Set the next boot partition to the new partition
        esp_err_t err = esp_ota_set_boot_partition(next_partition);
        if (err == ESP_OK) {
            ESP_LOGI(OTA_TAG, "Partition switch successful. Restarting...");
            esp_restart();  // Restart the device to boot from the new partition
        } else {
            ESP_LOGE(OTA_TAG, "Failed to switch partition, error: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(OTA_TAG, "No valid OTA partition found for update");
    }
}

// This is the function to be called if the watchdog times out
static void watchdog_timeout_callback(void *arg) {
    ESP_LOGE(OTA_TAG, "No CAN packet received for 10 seconds!");
    // Switch to esp-diag
    switch_partition();
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
            ESP_LOGI(OTA_TAG, "CAN message received:");
            printf("ID: 0x%lx\n", rx_msg.identifier);
            if (rx_msg.identifier == 0x118) {
                ESP_LOGI(OTA_TAG, "Receive Realtime Heartbeat");
			    // Reset watchdog on successful reception
                reset_watchdog_118();
            }

            if (rx_msg.identifier == 0x148) {
                ESP_LOGI(OTA_TAG, "Receive Telematic Heartbeat");
                // Reset watchdog on successful reception
                reset_watchdog_148();
            }

        } else {
            ESP_LOGI(OTA_TAG, "Failed to receive CAN message");
        }
    }
}