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

    // battery_t battery = {0};
    // uint8_t cellular = STATE;
    // uint8_t location = STATE;
    // uint8_t ride_tracking = STATE;
    //signature + phone pairing key + bike pairing key
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
                send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
                ble_gatts_start_advertise();
                connection_state = UNPAIRED;
                client_disconnect();
                break;

            case BLE_CONNECT:
                ESP_LOGI(ION_BLE_TAG, "BLE_CONNECT");
                send_to_tm_queue(TM_BLE_GET_TIME, NULL, 0);
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
                    send_to_tm_queue(PHONE_BLE_PAIRING, NULL, 0);
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
                        send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
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
                    send_to_tm_queue(PHONE_BLE_SESSION, NULL, 0);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "phone not pair yet, kill this connection");
                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
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
                        ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_COMMAND:");
                        esp_log_buffer_hex(ION_BLE_TAG, buf, buf_len);
#endif
                        switch (buf[3]) {
                            case PHONE_BLE_BATTERY:
                                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_BATTERY");
                                if (connection_state == SESSION_CREATED) {
#if (IGNORE_PAIRING)
                                    battery.level = 80;
                                    battery.estimate_range = 120;
                                    battery.time_to_full = 123;
                                    memset(buf, 0, sizeof(buf));
                                    msg_id = PHONE_BLE_BATTERY;
                                    buf_len = serialize_data(buf, 0, (uint8_t*)&msg_id, sizeof(msg_id));
                                    buf_len += serialize_data(buf, buf_len, (uint8_t*)&battery.level, sizeof(battery.level));
                                    buf_len += serialize_data(buf, buf_len, (uint8_t*)&battery.estimate_range, sizeof(battery.estimate_range));
                                    buf_len += serialize_data(buf, buf_len, (uint8_t*)&battery.time_to_full, sizeof(battery.time_to_full));
                                    //encrypt data & send to phone
                                    message_encrypt(to_phone_msg.data, (size_t*)&to_phone_msg.len, buf, buf_len);

                                    ESP_LOGI(ION_BLE_TAG, "battery.level           %d",battery.level);
                                    ESP_LOGI(ION_BLE_TAG, "battery.estimate_range  %d",battery.estimate_range);
                                    ESP_LOGI(ION_BLE_TAG, "battery.time_to_full    %d",battery.time_to_full);

                                    if (to_phone_msg.len <= BLE_MSG_MAX_LEN) {
                                        to_phone_msg.msg_id = PHONE_BLE_COMMAND;
                                        send_to_phone(&to_phone_msg);
                                    }
#else
                                    send_to_tm_queue(PHONE_BLE_BATTERY, NULL, 0);
#endif
                                } else {
                                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
                                    tm_ble_gatts_kill_connection();
                                }
                                break;

                            case PHONE_BLE_PING_BIKE:
                                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_PING_BIKE");
                                if (connection_state == SESSION_CREATED) {
                                    send_to_tm_queue(PHONE_BLE_PING_BIKE, NULL, 0);
                                } else {
                                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
                                    tm_ble_gatts_kill_connection();
                                }
                                break;

                            case PHONE_BLE_OPEN_SEAT:
                                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_OPEN_SEAT");
                                if (connection_state == SESSION_CREATED) {
                                    send_to_tm_queue(PHONE_BLE_OPEN_SEAT, NULL, 0);
                                } else {
                                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
                                    tm_ble_gatts_kill_connection();
                                }
                                break;

                            case PHONE_BLE_DIAG:
                                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_DIAG");
                                if (connection_state == SESSION_CREATED) {
                                    send_to_tm_queue(PHONE_BLE_DIAG, NULL, 0);
                                } else {
                                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
                                    tm_ble_gatts_kill_connection();
                                }
                                break;

                            case PHONE_BLE_BIKE_INFO:
                                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_INFO");
                                if (connection_state == SESSION_CREATED) {
                                    send_to_tm_queue(PHONE_BLE_BIKE_INFO, NULL, 0);
                                } else {
                                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
                                    tm_ble_gatts_kill_connection();
                                }
                                break;

                            case PHONE_BLE_POWER_ON:
                                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_POWER_ON");
                                if (connection_state == SESSION_CREATED) {
                                    buf[4] = ON;
                                    send_to_tm_queue(PHONE_BLE_POWER_ON, &buf[4], sizeof(buf[4]));
                                } else {
                                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
                                    tm_ble_gatts_kill_connection();
                                }
                                break;

                            case PHONE_BLE_CELLULAR:
                                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_CELLULAR");
                                if (connection_state == SESSION_CREATED) {
#if (DEBUG_BLE)
                                    switch (buf[4]) {
                                        case ON:
                                            ESP_LOGI(ION_BLE_TAG, "ON");
                                            break;
                                        case OFF:
                                            ESP_LOGI(ION_BLE_TAG, "OFF");
                                            break;
                                        case STATE:
                                            ESP_LOGI(ION_BLE_TAG, "STATE");
                                            break;
                                        default:
                                            break;
                                    }
#endif
                                    send_to_tm_queue(PHONE_BLE_CELLULAR, &buf[4], sizeof(buf[4]));
                                } else {
                                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
                                    tm_ble_gatts_kill_connection();
                                }
                                break;

                            case PHONE_BLE_LOCATION:
                                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_LOCATION");
                                if (connection_state == SESSION_CREATED) {
#if (DEBUG_BLE)
                                    switch (buf[4]) {
                                        case ON:
                                            ESP_LOGI(ION_BLE_TAG, "ON");
                                            break;
                                        case OFF:
                                            ESP_LOGI(ION_BLE_TAG, "OFF");
                                            break;
                                        case STATE:
                                            ESP_LOGI(ION_BLE_TAG, "STATE");
                                            break;
                                        default:
                                            break;
                                    }
#endif
                                    send_to_tm_queue(PHONE_BLE_LOCATION, &buf[4], sizeof(buf[4]));
                                } else {
                                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
                                    tm_ble_gatts_kill_connection();
                                }
                                break;

                            case PHONE_BLE_RIDE_TRACKING:
                                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_RIDE_TRACKING");
                                if (connection_state == SESSION_CREATED) {
#if (DEBUG_BLE)
                                    switch (buf[4]) {
                                        case ON:
                                            ESP_LOGI(ION_BLE_TAG, "ON");
                                            break;
                                        case OFF:
                                            ESP_LOGI(ION_BLE_TAG, "OFF");
                                            break;
                                        case STATE:
                                            ESP_LOGI(ION_BLE_TAG, "STATE");
                                            break;
                                        default:
                                            break;
                                    }
#endif
                                    send_to_tm_queue(PHONE_BLE_RIDE_TRACKING, &buf[4], sizeof(buf[4]));
                                } else {
                                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
                                    tm_ble_gatts_kill_connection();
                                }
                                break;

                            default:
                                continue;
                                break;
                        }
                    } else {
                        //failed to decrypt message
                        ESP_LOGE(ION_BLE_TAG, "failed to decrypt message");
                    }
                } else {
                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                    send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
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
    if (connection_state < PAIRED) {
        ESP_LOGW(ION_BLE_TAG, "pairing state:%d/expected:%d, terminate the connection",connection_state, PAIRED);
        send_to_tm_queue(BLE_DISCONNECT, NULL, 0);
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