#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "tm_atcmd.h"
#include "tm_ble.h"
#include "crypto.h"

#define BLE_TASK_PRIO           3
#define ION_BLE_TAG             "TM_BLE"

#if (ENABLE_PAIR_TIMEOUT)
#define BLE_PAIRING_TIMEOUT     1500    //1500ms
#elif (TEST_COMMAND)
#define TEST_COMMAND_TIMEOUT    5000    //5s
#endif

#define OK                      1
#define FAIL                    0

QueueHandle_t ble_queue;
static uint8_t connection_state = UNPAIRED;
static esp_timer_handle_t oneshot_timer;
static void oneshot_timer_callback(void* arg);
static void start_oneshot_timer(int timeout_ms);
static void ble_task(void *arg)
{
    ble_msg_t to_ble_msg = {0};
    ble_msg_t to_phone_msg = {0};
    ble_to_tm_msg_t ble_to_tm_msg = {0};
    uint8_t buf[BLE_MSG_MAX_LEN];
    size_t buf_len;
    int ret = 0;
    // int msg_id;

    ble_gatts_start_advertise();
    while(1) {
        //wait for message
        if (xQueueReceive(ble_queue, &to_ble_msg, portMAX_DELAY) != pdTRUE)
            continue;

        //message parsing & processing
        switch (to_ble_msg.msg_id) {
            case BLE_START_ADVERTISE:
                ESP_LOGI(ION_BLE_TAG, "BLE_START_ADVERTISE");
                ble_gatts_start_advertise();
                connection_state = UNPAIRED;
                break;

            case BLE_DISCONNECT:
                ESP_LOGI(ION_BLE_TAG, "BLE_DISCONNECT");
                ble_to_tm_msg.msg_id = BLE_DISCONNECT;
                ble_to_tm_msg.len = 0;
                send_to_tm_queue(&ble_to_tm_msg);
                ble_gatts_start_advertise();
                connection_state = UNPAIRED;
                client_disconnect();
                break;

            case BLE_CONNECT:
                ESP_LOGI(ION_BLE_TAG, "BLE_CONNECT");
                ble_to_tm_msg.msg_id = TM_BLE_GET_TIME;
                ble_to_tm_msg.len = 0;
                send_to_tm_queue(&ble_to_tm_msg);
#if (ENABLE_PAIR_TIMEOUT)
                start_oneshot_timer(BLE_PAIRING_TIMEOUT);
#elif (TEST_COMMAND)
                start_oneshot_timer(TEST_COMMAND_TIMEOUT);
#endif
                connection_state = UNPAIRED;
                break;

            case PHONE_BLE_PAIRING:
                if (connection_state == UNPAIRED) {
                    ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_PAIRING");
                    ble_to_tm_msg.msg_id = PHONE_BLE_PAIRING;
                    ble_to_tm_msg.len = 0;
                    send_to_tm_queue(&ble_to_tm_msg);
#if (DEBUG_BLE)
                    esp_log_buffer_hex(ION_BLE_TAG, to_ble_msg.data, to_ble_msg.len);
#endif
                    //verify pairing request
                    int pairing_result = pairing_request(to_ble_msg.data, to_ble_msg.len, buf, &buf_len);
                    if (pairing_result == ESP_OK) {
                        to_phone_msg.msg_id = PHONE_BLE_PAIRING;

                        connection_state = PAIRED;
                        to_phone_msg.len = buf_len;
                        memcpy(to_phone_msg.data, buf, buf_len);

                        send_to_phone(&to_phone_msg);
                        ESP_LOGI(ION_BLE_TAG, "Pair OK");
                    } else {
                        to_phone_msg.msg_id = PHONE_BLE_PAIRING;
                        to_phone_msg.len = 1;
                        to_phone_msg.data[0] = 0;
                        send_to_phone(&to_phone_msg);
                        vTaskDelay(50/portTICK_PERIOD_MS);
                        ble_to_tm_msg.msg_id = BLE_DISCONNECT;
                        ble_to_tm_msg.len = 0;
                        send_to_tm_queue(&ble_to_tm_msg);
                        tm_ble_gatts_kill_connection();
                        ESP_LOGE(ION_BLE_TAG, "Pair fails");
                    }
                }
                break;

            case PHONE_BLE_SESSION:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_SESSION");
                if (connection_state == PAIRED) {
                    int session_result = session_request(to_ble_msg.data, to_ble_msg.len, buf, &buf_len);
                    if (session_result == ESP_OK) {
                        connection_state = SESSION_CREATED;
                        to_phone_msg.msg_id = PHONE_BLE_SESSION;
                        to_phone_msg.len = buf_len;
                        memcpy(to_phone_msg.data, buf, buf_len);
                        send_to_phone(&to_phone_msg);
                        vTaskDelay(50/portTICK_PERIOD_MS);
                        ESP_LOGI(ION_BLE_TAG, "Session created");
                    } else {
                        ESP_LOGE(ION_BLE_TAG, "Failed to create session");
                    }
                    // connection_state = SESSION_CREATED;
                        ble_to_tm_msg.msg_id = PHONE_BLE_SESSION;
                        ble_to_tm_msg.len = 0;
                        send_to_tm_queue(&ble_to_tm_msg);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "phone not pair yet, kill this connection");
                    tm_ble_gatts_kill_connection();
                }
                break;

            case PHONE_BLE_COMMAND:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_COMMAND");
                if (connection_state == SESSION_CREATED) {
                    //decrypt this command
                    ret = message_decrypt(buf, &buf_len, to_ble_msg.data, to_ble_msg.len);
                    if (ret == 0) {
                        //successfully decrypted message, let process this command
#if (DEBUG_BLE)
                        ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_COMMAND len: %d",buf_len);
                        esp_log_buffer_hex(ION_BLE_TAG, buf, buf_len);
#endif
                        if (PHONE_BLE_COMMAND < buf[3]) {
                            ble_to_tm_msg.msg_id = buf[3];
                            ble_to_tm_msg.len = buf_len - 4;
                            if (ble_to_tm_msg.len > 0)
                                memcpy(ble_to_tm_msg.data, &buf[4], ble_to_tm_msg.len);
                            send_to_tm_queue(&ble_to_tm_msg);
                        }
                    } else {
                        //failed to decrypt message
                        ESP_LOGE(ION_BLE_TAG, "failed to decrypt message, kill this connection");
                        ble_to_tm_msg.msg_id = BLE_DISCONNECT;
                        ble_to_tm_msg.len = 0;
                        send_to_tm_queue(&ble_to_tm_msg);
                        tm_ble_gatts_kill_connection();
                    }
                } else {
                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                    ble_to_tm_msg.msg_id = BLE_DISCONNECT;
                    ble_to_tm_msg.len = 0;
                    send_to_tm_queue(&ble_to_tm_msg);
                    tm_ble_gatts_kill_connection();
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
    crypto_init();
    tm_ble_gatts_server_init();
    ble_queue = xQueueCreate(20, sizeof(ble_msg_t));
    init_ble_queue(&ble_queue);

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

void send_to_phone(ble_msg_t *pble_msg) {
    int len = pble_msg->len + sizeof(pble_msg->msg_id) + sizeof(pble_msg->len); 
    tm_ble_gatts_send_to_phone((uint8_t*)pble_msg, len);
}

#if ((ENABLE_PAIR_TIMEOUT) || (TEST_COMMAND))
static void oneshot_timer_callback(void* arg)
{
#if (TEST_COMMAND)
    uint8_t buf[4] = {0};
    ble_msg_t to_ble_msg = {0};
    to_ble_msg.msg_id = PHONE_BLE_COMMAND;
    buf[3] = PHONE_BLE_BIKE_INFO;
    to_ble_msg.len = sizeof(buf);
    message_encrypt((uint8_t*)to_ble_msg.data, (size_t*)&to_ble_msg.len , (uint8_t*)buf, sizeof(buf));
    xQueueSend(ble_queue, (void*)&to_ble_msg, (TickType_t)0);
#elif (ENABLE_PAIR_TIMEOUT)
    ble_to_tm_msg_t ble_to_tm_msg = {0};
    if (connection_state < PAIRED) {
        ESP_LOGW(ION_BLE_TAG, "pairing state:%d/expected:%d, terminate the connection",connection_state, PAIRED);
        ble_to_tm_msg.msg_id = BLE_DISCONNECT;
        ble_to_tm_msg.len = 0;
        send_to_tm_queue(&ble_to_tm_msg);
#if (ENABLE_PAIR_TIMEOUT)
        tm_ble_gatts_kill_connection();
#endif
    }
#endif
}
#endif

#if ((ENABLE_PAIR_TIMEOUT) || (TEST_COMMAND))
static void start_oneshot_timer(int timeout_ms)
{
    esp_timer_create_args_t oneshot_timer_args = {
            .callback = &oneshot_timer_callback,
            /* argument specified here will be passed to timer callback function */
            .arg = NULL,
            .name = "one-shot"
    };
    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, timeout_ms*1000));
    ESP_LOGI(ION_BLE_TAG, "Started timers");
}
#endif