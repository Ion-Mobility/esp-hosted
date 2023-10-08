#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "tm_ble_gatts_server.h"
#include "tm_atcmd.h"
#include "tm_ble.h"
#include "crypto.h"

#define BLE_TASK_PRIO           3
#define ION_BLE_TAG             "TM_BLE"
#define BLE_PAIRING_TIMEOUT     10  //10s

#define FOR_IMOS                1

QueueHandle_t ble_queue;
static uint8_t connection_state = UNPAIRED;
static esp_timer_handle_t oneshot_timer;
static void send_to_phone(ble_msg_t *pble_msg);
static void oneshot_timer_callback(void* arg);
static void start_oneshot_timer(int timeout_s);
static void stop_oneshot_timer(void);

static void ble_task(void *arg)
{
    ble_msg_t to_ble_msg = {0};
    ble_msg_t to_phone_msg = {0};

    charge_t charge = {0};
    battery_t battery = {0};
    trip_t trip = {0};
    //signature + phone pairing key + bike pairing key
    uint8_t response[32+32+64];
    size_t res_len;

    uint8_t mac[16];    /* Message authentication code */

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
                send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                ble_gatts_start_advertise();
                connection_state = UNPAIRED;
                client_disconnect();
                break;

            case BLE_CONNECT:
                ESP_LOGI(ION_BLE_TAG, "BLE_CONNECT");
                // start_oneshot_timer(BLE_PAIRING_TIMEOUT);
                connection_state = UNPAIRED;
                break;

            case PHONE_BLE_PAIRING:
                if (connection_state == UNPAIRED) {
                    ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_PAIRING");
                    send_to_tm_queue(TM_BLE_PAIRING, NULL, 0);
                    esp_log_buffer_hex(ION_BLE_TAG, to_ble_msg.data, to_ble_msg.len);
                    //verify pairing request
                    int pairing_result = pairing_request(to_ble_msg.data, to_ble_msg.len, response, &res_len);
                    if (pairing_result == ESP_OK) {
                        to_phone_msg.msg_id = PHONE_BLE_PAIRING;
#if (FOR_IMOS)
                        connection_state = SESSION_CREATED;
                        to_phone_msg.len = 1;
                        to_phone_msg.data[0] = 1;
                        // connection_state = SESSION_CREATED;
                        send_to_tm_queue(TM_BLE_PAIRED, NULL, 0);
#else
                        connection_state = PAIRED;
                        to_phone_msg.len = res_len;
                        memcpy(to_phone_msg.data, response, res_len);
#endif
                        send_to_phone(&to_phone_msg);
                        ESP_LOGI(ION_BLE_TAG, "Pair OK");
                    } else {
                        send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                        tm_ble_gatts_kill_connection();
                        ESP_LOGE(ION_BLE_TAG, "Pair fails");
                    }
                    // stop_oneshot_timer();
                }
                break;

            case PHONE_BLE_SESSION:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_SESSION");
                if (connection_state == PAIRED) {
                    int session_result = session_request(to_ble_msg.data, to_ble_msg.len, response, &res_len);
                    if (session_result == ESP_OK) {
                        connection_state = SESSION_CREATED;
                        to_phone_msg.msg_id = PHONE_BLE_SESSION;
                        to_phone_msg.len = res_len;
                        memcpy(to_phone_msg.data, response, res_len);
                        send_to_phone(&to_phone_msg);
                        ESP_LOGI(ION_BLE_TAG, "Session created");
                    } else {
                        ESP_LOGE(ION_BLE_TAG, "Failed to create session");
                    }
                    // connection_state = SESSION_CREATED;
                    send_to_tm_queue(TM_BLE_PAIRED, NULL, 0);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "phone not pair yet, kill this connection");
                    send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                    tm_ble_gatts_kill_connection();
                }
                break;

            case PHONE_BLE_BATTERY:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_BATTERY");
                if (connection_state == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_BATTERY, NULL, 0);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                    send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                    tm_ble_gatts_kill_connection();
                }
                break;

            case PHONE_BLE_CHARGE:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_CHARGE");
                if (connection_state == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_CHARGE, NULL, 0);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                    send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                    tm_ble_gatts_kill_connection();
                }
                break;


            case PHONE_BLE_LAST_TRIP:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_LAST_TRIP");
                if (connection_state == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_LAST_TRIP, NULL, 0);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                    send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                    tm_ble_gatts_kill_connection();
                }
                break;

            case PHONE_BLE_STEERING_LOCK:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_STEERING_LOCK");
                if (connection_state == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_STEERING_LOCK, NULL, 0);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                    send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                    tm_ble_gatts_kill_connection();
                }
                break;

            case PHONE_BLE_STEERING_UNLOCK:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_STEERING_UNLOCK");
                if (connection_state == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_STEERING_UNLOCK, NULL, 0);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                    send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                    tm_ble_gatts_kill_connection();
                }
                break;

            case PHONE_BLE_PING_BIKE:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_PING_BIKE");
                if (connection_state == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_PING_BIKE, NULL, 0);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                    send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                    tm_ble_gatts_kill_connection();
                }
                break;

            case PHONE_BLE_OPEN_SEAT:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_OPEN_SEAT");
                if (connection_state == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_OPEN_SEAT, NULL, 0);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                    send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                    tm_ble_gatts_kill_connection();
                }
                break;

            case PHONE_BLE_DIAG:
                ESP_LOGI(ION_BLE_TAG, "PHONE_BLE_DIAG");
                if (connection_state == SESSION_CREATED) {
                    send_to_tm_queue(TM_BLE_DIAG, NULL, 0);
                } else {
                    ESP_LOGW(ION_BLE_TAG, "session not created yet, kill this connection");
                    send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
                    tm_ble_gatts_kill_connection();
                }
                break;

            case TM_BLE_CHARGE:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_CHARGE");
                if (connection_state == SESSION_CREATED) {
                    //todo: debug only, remove later
                    memset(&charge, 0, sizeof(charge_t));
                    memcpy(&charge, to_ble_msg.data, sizeof(charge_t));
#if (FOR_IMOS)
                    charge.state = 0;
                    charge.vol = 500;
                    charge.cur = 700;
                    charge.cycle = 400;
                    charge.time_to_full = 360;
                    to_phone_msg.len = sizeof(charge.state) + sizeof(charge.vol) +
                                       sizeof(charge.cur) + sizeof(charge.cycle) +
                                       sizeof(charge.time_to_full);
                    memcpy(&to_phone_msg.data[0], &charge.state, sizeof(charge.state));
                    memcpy(&to_phone_msg.data[0+sizeof(charge.state)], &charge.vol, sizeof(charge.vol));
                    memcpy(&to_phone_msg.data[0+sizeof(charge.state)+sizeof(charge.vol)], &charge.cur, sizeof(charge.cur));
                    memcpy(&to_phone_msg.data[0+sizeof(charge.state)+sizeof(charge.vol)+sizeof(charge.cur)], &charge.cycle, sizeof(charge.cycle));
                    memcpy(&to_phone_msg.data[0+sizeof(charge.state)+sizeof(charge.vol)+sizeof(charge.cur)+sizeof(charge.cycle)], &charge.time_to_full, sizeof(charge.time_to_full));
                    esp_log_buffer_hex(ION_BLE_TAG, to_phone_msg.data, to_phone_msg.len);
#else
                    //encrypt data & send to phone
                    message_encrypt(to_phone_msg.data, (size_t*)&to_phone_msg.len, mac, to_ble_msg.data, to_ble_msg.len);
#endif
                    ESP_LOGI(ION_BLE_TAG, "charge state            %d",charge.state);
                    ESP_LOGI(ION_BLE_TAG, "charge vol              %d",charge.vol);
                    ESP_LOGI(ION_BLE_TAG, "charge cur              %d",charge.cur);
                    ESP_LOGI(ION_BLE_TAG, "charge cycle            %d",charge.cycle);
                    ESP_LOGI(ION_BLE_TAG, "charge time_to_full     %d",charge.time_to_full);

                    if (to_phone_msg.len <= BLE_MSG_MAX_LEN) {
                        to_phone_msg.msg_id = PHONE_BLE_CHARGE;
                        send_to_phone(&to_phone_msg);
                    }
                }
                break;

            case TM_BLE_BATTERY:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_BATTERY");
                if (connection_state == SESSION_CREATED) {
                    //todo: debug only, remove later
                    memset(&battery, 0, sizeof(battery_t));
                    memcpy(&battery, to_ble_msg.data, sizeof(battery_t));
#if (FOR_IMOS)
                    battery.level = 80;
                    battery.estimate_range = 145;
                    to_phone_msg.len = sizeof(battery.level) + sizeof(battery.estimate_range);
                    to_phone_msg.data[0] = battery.level;
                    memcpy(&to_phone_msg.data[1], &battery.estimate_range, sizeof(battery.estimate_range));
                    esp_log_buffer_hex(ION_BLE_TAG, to_phone_msg.data, to_phone_msg.len);
#else
                    //encrypt data & send to phone
                    message_encrypt(to_phone_msg.data, (size_t*)&to_phone_msg.len, mac, to_ble_msg.data, to_ble_msg.len);
#endif
                    ESP_LOGI(ION_BLE_TAG, "battery level           %d",battery.level);
                    ESP_LOGI(ION_BLE_TAG, "estimate range vol      %d",battery.estimate_range);

                    if (to_phone_msg.len <= BLE_MSG_MAX_LEN) {
                        to_phone_msg.msg_id = PHONE_BLE_BATTERY;
                        send_to_phone(&to_phone_msg);
                    }
                }
                break;

            case TM_BLE_LAST_TRIP:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_LAST_TRIP");
                if (connection_state == SESSION_CREATED) {
                    //todo: debug only, remove later
                    memset(&trip, 0, sizeof(trip_t));
                    memcpy(&trip, to_ble_msg.data, sizeof(trip_t));
#if (FOR_IMOS)
                    trip.distance = 40;
                    trip.ride_time = 35;
                    trip.elec_used = 45;
                    to_phone_msg.len = sizeof(trip.distance) + sizeof(trip.ride_time) + sizeof(trip.elec_used);
                    memcpy(&to_phone_msg.data[0], &trip.distance, sizeof(trip.distance));
                    memcpy(&to_phone_msg.data[0+sizeof(trip.distance)], &trip.ride_time, sizeof(trip.ride_time));
                    memcpy(&to_phone_msg.data[0+sizeof(trip.distance)+sizeof(trip.ride_time)], &trip.elec_used, sizeof(trip.elec_used));
                    esp_log_buffer_hex(ION_BLE_TAG, to_phone_msg.data, to_phone_msg.len);
#else
                    //encrypt data & send to phone
                    message_encrypt(to_phone_msg.data, (size_t*)&to_phone_msg.len, mac, to_ble_msg.data, to_ble_msg.len);
#endif
                    ESP_LOGI(ION_BLE_TAG, "last trip distance      %d",trip.distance);
                    ESP_LOGI(ION_BLE_TAG, "last trip ride time     %d",trip.ride_time);
                    ESP_LOGI(ION_BLE_TAG, "last trip electric used %d",trip.elec_used);

                    if (to_phone_msg.len <= BLE_MSG_MAX_LEN) {
                        to_phone_msg.msg_id = PHONE_BLE_LAST_TRIP;
                        send_to_phone(&to_phone_msg);
                    }
                }
                break;

            case TM_BLE_STEERING_LOCK:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_STEERING_LOCK");
                if (connection_state == SESSION_CREATED) {
                    to_phone_msg.len = 0;
                    to_phone_msg.msg_id = PHONE_BLE_STEERING_LOCK;
                    send_to_phone(&to_phone_msg);
                }
                break;

            case TM_BLE_STEERING_UNLOCK:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_STEERING_UNLOCK");
                if (connection_state == SESSION_CREATED) {
                    to_phone_msg.len = 0;
                    to_phone_msg.msg_id = PHONE_BLE_STEERING_UNLOCK;
                    send_to_phone(&to_phone_msg);
                }
                break;

            case TM_BLE_PING_BIKE:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_PING_BIKE");
#if (FOR_IMOS)
                ESP_LOGI(ION_BLE_TAG, "148-ble ping bike");
#else
                if (connection_state == SESSION_CREATED) {
                    to_phone_msg.len = 0;
                    to_phone_msg.msg_id = PHONE_BLE_PING_BIKE;
                    send_to_phone(&to_phone_msg);
                }
#endif
                break;

            case TM_BLE_OPEN_SEAT:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_OPEN_SEAT");
#if (FOR_IMOS)
                ESP_LOGI(ION_BLE_TAG, "148-ble open seat");
#else
                if (connection_state == SESSION_CREATED) {
                    to_phone_msg.len = 0;
                    to_phone_msg.msg_id = PHONE_BLE_OPEN_SEAT;
                    send_to_phone(&to_phone_msg);
                }
#endif
                break;

            case TM_BLE_DIAG:
                ESP_LOGI(ION_BLE_TAG, "TM_BLE_DIAG");
                if (connection_state == SESSION_CREATED) {
                    to_phone_msg.len = 0;
                    to_phone_msg.msg_id = PHONE_BLE_DIAG;
                    send_to_phone(&to_phone_msg);
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

static void send_to_phone(ble_msg_t *pble_msg) {
    int len = pble_msg->len + sizeof(pble_msg->msg_id) + sizeof(pble_msg->len); 
    tm_ble_gatts_send_to_phone((uint8_t*)pble_msg, len);
}

static void oneshot_timer_callback(void* arg)
{
    if (connection_state < PAIRED) {
        ESP_LOGW(ION_BLE_TAG, "pairing timeout, terminate the connection");
        send_to_tm_queue(TM_BLE_DISCONNECT, NULL, 0);
        tm_ble_gatts_kill_connection();
    }
}

static void start_oneshot_timer(int timeout_s)
{
    esp_timer_create_args_t oneshot_timer_args = {
            .callback = &oneshot_timer_callback,
            /* argument specified here will be passed to timer callback function */
            .arg = NULL,
            .name = "one-shot"
    };
    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, timeout_s*1000000));
    ESP_LOGI(ION_BLE_TAG, "Started timers");
}
static void stop_oneshot_timer(void)
{
    ESP_ERROR_CHECK(esp_timer_stop(oneshot_timer));
    ESP_ERROR_CHECK(esp_timer_delete(oneshot_timer));
}