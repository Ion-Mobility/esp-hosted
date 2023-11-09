#include "ap_auto_scanner.h"
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_log.h>

#define EVENT_DONE_SCANNING_BIT (1 << 0)
#define EVENT_DONE_PROCESSING_SCAN_RESULT_BIT (1 << 1)

#define mem_free(x)                 \
        {                           \
            if (x) {                \
                free(x);            \
                x = NULL;           \
            }                       \
        }

typedef struct ap_auto_scanner_t {
    int is_started;
    ap_auto_scanner_config_t config;
    TaskHandle_t scan_ap_task_handle;
    scan_result_t update_aps_list;
    EventGroupHandle_t event_group;
    update_scan_result_cb_t update_cb;
    void * update_cb_param;
} ap_auto_scanner_t;

static const char* TAG = "event_service_ap_auto_scanner";

//================================
// Private functions declaration
//================================
static void ap_auto_scanner_task(void * pvParameters);
static int perform_scan(ap_auto_scanner_handle_t scanner);
static int store_scan_data(ap_auto_scanner_handle_t scanner);
static void done_ap_scan_event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data);


//==============================
// Public functions definition
//==============================
ap_auto_scanner_handle_t ap_auto_scanner_new() {
    static ap_auto_scanner_t s_available_ins[1];
    static size_t s_current_available_ins_indx = 0;
    if (s_current_available_ins_indx >= 1)
        goto err;
    ap_auto_scanner_t *ret =
        &s_available_ins[s_current_available_ins_indx];
    ret->is_started = 0;
    esp_event_handler_register(WIFI_EVENT,
        WIFI_EVENT_SCAN_DONE, &done_ap_scan_event_handler, ret);
    s_current_available_ins_indx++;
    ESP_LOGI(TAG, "Create new AP scanner instance SUCCESS!");
    return ret;
err:
    ESP_LOGE(TAG, "Create new AP scanner instance FAILED!");
    return NULL;
}

void ap_auto_scanner_start(ap_auto_scanner_handle_t scanner,
    ap_auto_scanner_config_t *config, update_scan_result_cb_t cb, void *param) {
    if (scanner->is_started) {
        ap_auto_scanner_stop(scanner);
    }
    wifi_mode_t current_mode;
    esp_err_t err = esp_wifi_get_mode(&current_mode);
    switch (current_mode)
    {
    case WIFI_MODE_NULL:
    case WIFI_MODE_AP:
        ESP_LOGE(TAG, "AP auto scanner cannot start with wifi mode NULL or AP!");
        goto err;
    default:
        break;
    }
    scanner->config = *config;
    esp_err_t status = xTaskCreate(ap_auto_scanner_task , "ap_auto_scanner" , 3096 , scanner , 20,
        &scanner->scan_ap_task_handle);
    ESP_LOGD(TAG, "Task create with status %d", status);
	if (pdPASS != status) {
        ESP_LOGE(TAG, "Create scan AP task FAILED");
        goto err;
    }
    ESP_LOGD(TAG, "Create scan AP task SUCCESS");

    scanner->event_group = xEventGroupCreate();
    if (NULL == scanner->event_group) {
        ESP_LOGE(TAG, "Create internal events group FAILED");
        goto err;
    }
    ESP_LOGD(TAG, "Create internal events group SUCCESS");
    scanner->update_cb = cb;
    scanner->update_cb_param = param;
	ESP_LOGI(TAG, "Start auto-scan AP SUCCESS:\nScan interval %ums\nUpdate interval %ums\n%u records per event update\n",
        scanner->config.scan_interval_ms,
        scanner->config.update_interval_ms,
        scanner->config.num_of_records_per_update);
	return ESP_OK;

err:
    ESP_LOGE(TAG, "Start auto-scan AP FAILED");
    return ESP_FAIL;
}

void ap_auto_scanner_stop(ap_auto_scanner_handle_t scanner) {
    if (!scanner->is_started) {
        ESP_LOGW(TAG, "Cannot stop AP scanner event service because it did not start");
        return;
    }
    if (scanner->scan_ap_task_handle) {
        vTaskDelete(scanner->scan_ap_task_handle);
	}
}

void ap_auto_scanner_notify_done_process_scan_result(
    ap_auto_scanner_handle_t scanner) {
    xEventGroupSetBits(scanner->event_group,
        EVENT_DONE_PROCESSING_SCAN_RESULT_BIT);
    ESP_LOGI(TAG, "Scan result is done processing!");

}

//===============================
// Private functions definition
//===============================
static void ap_auto_scanner_task(void * pvParameters) {
    ap_auto_scanner_t *scanner =
        (ap_auto_scanner_t*) pvParameters;
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(scanner->config.scan_interval_ms));
        if (perform_scan(scanner)) {
            continue;
        }
        if (store_scan_data(scanner)) {
            continue;
        }
        size_t current_index = 0;
        ESP_LOGD(TAG, "Scan and get %u results", scanner->update_aps_list.count);
        for (int i = 0; i < scanner->update_aps_list.count; i++) {
            ESP_LOGD(TAG, "BSSID: " MACSTR, MAC2STR(scanner->update_aps_list.records[i].bssid));
        }
        while (1) {
            size_t next_index = 
                current_index + scanner->config.num_of_records_per_update > 
                scanner->update_aps_list.count?
                scanner->update_aps_list.count:
                current_index + scanner->config.num_of_records_per_update;

            size_t num_of_records_to_send = next_index - current_index;
            scan_result_t update_list = {
                .count = num_of_records_to_send,
                .records = &scanner->update_aps_list.records[current_index]
            };
            if (scanner->update_cb) {
                scanner->update_cb(&update_list, scanner->update_cb_param);
            } else {
                ap_auto_scanner_notify_done_process_scan_result(scanner);
                ESP_LOGW(TAG, "No update result callback is provided!");
            }
            xEventGroupWaitBits(scanner->event_group, EVENT_DONE_PROCESSING_SCAN_RESULT_BIT, pdTRUE,
                pdTRUE, portMAX_DELAY);
            ESP_LOGD(TAG, "Done sending %d records", num_of_records_to_send);
            if (next_index >= scanner->update_aps_list.count) {
                ESP_LOGD(TAG, "Done sending all records");
                mem_free(scanner->update_aps_list.records);
                scanner->update_aps_list.count = 0;
                break;
            }
            current_index = next_index;
            vTaskDelay(pdMS_TO_TICKS(scanner->config.update_interval_ms));
        }
    }
}

static int perform_scan(ap_auto_scanner_handle_t scanner) {
    esp_err_t err;
    wifi_scan_config_t scanConf = {
        .show_hidden = false
    };

    err = esp_wifi_scan_start(&scanConf, true);
    if (err) {
        ESP_LOGE(TAG,"Failed to start scan start command. Err %d", err);
        goto err;
    }
    xEventGroupWaitBits(scanner->event_group, EVENT_DONE_SCANNING_BIT, pdTRUE,
        pdTRUE, portMAX_DELAY);

    ESP_LOGD(TAG, "Scan nearby AP SUCCESS!");
    return 0;
err:
    ESP_LOGE(TAG, "Scan nearby AP FAILED!");
    return -1;
}

static int store_scan_data(ap_auto_scanner_handle_t scanner) {
    esp_err_t err;
    err = esp_wifi_scan_get_ap_num(&scanner->update_aps_list.count);
    if (err) {
        ESP_LOGE(TAG,"Failed to get scan AP number");
        goto err;
    }

    scanner->update_aps_list.records = (wifi_ap_record_t *)
        calloc(scanner->update_aps_list.count, sizeof(wifi_ap_record_t));
    if (!scanner->update_aps_list.records) {
        ESP_LOGE(TAG,"Failed to allocate memory");
        goto err;
    }

    err = esp_wifi_scan_get_ap_records(&scanner->update_aps_list.count,
        scanner->update_aps_list.records);
    if (err) {
        ESP_LOGE(TAG,"Failed to get scan ap records");
        goto err;
    }
    ESP_LOGD(TAG,"Store scan data SUCCESS");
    return 0;
err:
    ESP_LOGE(TAG,"Store scan data FAILED");
    return -1;

}

static void done_ap_scan_event_handler(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data) {
    ap_auto_scanner_t *scanner =
        (ap_auto_scanner_t*) arg;
	if ((event_base == WIFI_EVENT) && (event_id == WIFI_EVENT_SCAN_DONE)) {
		xEventGroupSetBits(scanner->event_group, EVENT_DONE_SCANNING_BIT);
	}
}
