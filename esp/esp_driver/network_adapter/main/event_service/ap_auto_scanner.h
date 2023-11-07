#ifndef ESP_HOSTED_EVENT_SERVICE_ap_auto_scanner_H
#define ESP_HOSTED_EVENT_SERVICE_ap_auto_scanner_H
#include <stddef.h>
#include <esp_wifi.h>

typedef struct ap_auto_scanner_t* ap_auto_scanner_handle_t;

typedef struct ap_auto_scanner_config_t {
    size_t scan_interval_ms;
    size_t update_interval_ms;
    size_t num_of_records_per_update;
} ap_auto_scanner_config_t;

typedef struct scan_result_t {
    size_t count;
    wifi_ap_record_t *records;
} scan_result_t;

typedef void (*update_scan_result_cb_t)(const scan_result_t *result,
    void *param);

extern ap_auto_scanner_handle_t ap_auto_scanner_new();

extern void ap_auto_scanner_start(ap_auto_scanner_handle_t scanner,
    ap_auto_scanner_config_t *config, update_scan_result_cb_t cb, void *param);

extern void ap_auto_scanner_stop(ap_auto_scanner_handle_t scanner);

extern void ap_auto_scanner_notify_done_process_scan_result(
    ap_auto_scanner_handle_t scanner);

#endif