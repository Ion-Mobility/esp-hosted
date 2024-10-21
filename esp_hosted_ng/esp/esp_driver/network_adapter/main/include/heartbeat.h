#include "driver/twai.h"
#include "esp_ota_ops.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "stats.h"

// Define the CAN bus TX and RX pins and the baudrate
#define CAN_TX_PIN 37
#define CAN_RX_PIN 38
#define CAN_BAUDRATE TWAI_TIMING_CONFIG_250KBITS()  // 250 kbps

// Filter ID and Mask
#define FILTER_ID 0x00000000
#define FILTER_MASK 0x00FF0000

// Accepted IDs for heartbeat
#define CAN_118_VERSION_ID 0x1800F4FA
#define CAN_148_VERSION_ID 0x1800F4F9
#define CAN_WIFI_STATUS_ID 0x1800F2F9
#define CAN_118_OTA_ID 0x118
#define CAN_148_OTA_ID 0x148

#define WATCHDOG_TIMEOUT_MS 10000  // 10 seconds

void can_receive_task(void *arg);