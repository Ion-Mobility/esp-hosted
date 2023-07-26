#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "mqtt_service.h"
#include "nvs_flash.h"
#include "unity.h"
#include "sdkconfig.h"
#include "protocol_examples_common.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RECV_BUFF_GROUP_TIMEOUT_ms 2000

static const char *ion_broker_username = "9b5766d8-8766-492c-ace8-a80f191e47e6";
static const char *ion_broker_password = "0caabff1-dd1e-49da-a3a3-9a00d4ff2cb6";
static const char *ion_allowed_topic = "channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages";
static bool is_connected_to_wifi = false;


typedef struct {
    const char *test_name;
    const char *at_cmd;
    const char *expected_response;
} at_cmd_test_scene_entry_t;

#define TEST_LOG(format, ...) printf("%s" format, module, ##__VA_ARGS__) 

static const char *module = "mqtt_test: ";
static void connect_to_wifi();
static void init_and_assert();
static void connect_and_assert(int client_index,
    const char *client_id, const char *hostname, const char *username,
    const char *pass, uint16_t port);
static void disconnect_and_assert(int client_index);
static void connect_to_wifi();

static void connect_and_assert(int client_index,
    const char *client_id, const char *hostname, const char *username,
    const char *pass, uint16_t port);
static void publish_and_assert(int client_index,
    const char *topic, const char* msg, mqtt_qos_t qos);
static void subscribe_and_assert(int client_index,
    const char *topic, mqtt_qos_t qos);
static void unsubscribe_and_assert(int client_index,
    const char *topic);
static void clear_recv_buff_group(int client_index);
static void get_recv_buff_group_with_timeout_and_assert(
    int client_index, size_t timeout_ms, 
    recv_buffer_group_t *out_recv_buff_group);

static void disconnect_and_assert(int client_index);

static void generate_random_string(char *ret_random_string, size_t target_len);

static void basic_action_test(const char* hostname, uint16_t port,
    const char* username, const char* password, int num_of_trials, 
    bool unsub_after_trial);

static void basic_action_test_for_ion_broker(int num_of_trials, 
    bool unsub_after_trial);

static void test_done_deinit();

void TestCase_Connect_to_WiFi()
{
    connect_to_wifi();
}

void TestCase_MQTT_Connect_Disconnect()
{
    connect_to_wifi();
    init_and_assert();
    TEST_LOG("Test connect and disconnect TCP-only from broker.hivemq.com\n");
    connect_and_assert(0, "esp32-wifi-test", "mqtt://broker.hivemq.com", 
        NULL, NULL, 1883);
    disconnect_and_assert(0);
    TEST_LOG("Test connect and disconnect TCP-only from mqtt.eclipseprojects.io\n");
    connect_and_assert(0, "esp32-wifi-test", "mqtt://mqtt.eclipseprojects.io",
        NULL, NULL, 1883);
    disconnect_and_assert(0);
    mqtt_service_deinit();
    test_done_deinit();
}

void TestCase_MQTT_Basic_Actions()
{
    basic_action_test("mqtt://broker.hivemq.com", 1883, NULL, NULL, 1, true);
}

void TestCase_MQTT_StressTest()
{
    basic_action_test("mqtt://broker.hivemq.com", 1883, NULL, NULL, 50, false);
}

void TestCase_MQTT_TLS_Connect_Disconnect()
{
    connect_to_wifi();
    init_and_assert();
    TEST_LOG("Test connect and disconnect TLS from broker.hivemq.com\n");
    connect_and_assert(0, "esp32-wifi-test", "mqtts://broker.hivemq.com", 
        NULL, NULL, 8883);
    disconnect_and_assert(0);
    TEST_LOG("Test connect and disconnect TLS from mqtt.eclipseprojects.io\n");
    connect_and_assert(0, "esp32-wifi-test", "mqtts://mqtt.eclipseprojects.io",
        NULL, NULL, 8883);
    disconnect_and_assert(0);
    mqtt_service_deinit();
    test_done_deinit();
}

void TestCase_MQTT_TLS_Basic_Actions()
{
    basic_action_test("mqtts://broker.hivemq.com", 8883, NULL, NULL, 1, true);
}

void TestCase_MQTT_TLS_StressTest()
{
    basic_action_test("mqtts://broker.hivemq.com", 8883, NULL, NULL, 50, false);
}

void TestCase_MQTT_ION_Broker_Connect_Disconnect()
{
    connect_to_wifi();
    init_and_assert();
    TEST_LOG("Test connect and disconnect ION broker\n");
    connect_and_assert(0, "esp32-wifi-test", "mqtts://ion-broker-s.ionmobility.net", 
        ion_broker_username, ion_broker_password, 8883);
    disconnect_and_assert(0);
    mqtt_service_deinit();
    test_done_deinit();
}

void TestCase_MQTT_ION_Broker_Basic_Actions()
{
    basic_action_test_for_ion_broker(1, true);
}

void TestCase_MQTT_ION_Broker_StressTest()
{
    basic_action_test_for_ion_broker(50, false);
}

static void connect_to_wifi()
{
    if (is_connected_to_wifi)
        return;

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
    is_connected_to_wifi = true;
}

static void test_done_deinit()
{
    if (!is_connected_to_wifi)
        return;

    ESP_ERROR_CHECK(example_disconnect());
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    ESP_ERROR_CHECK(nvs_flash_deinit());
    is_connected_to_wifi = false;
}

static void init_and_assert()
{
    mqtt_service_status_t service_status;
    service_status = mqtt_service_init();
    TEST_ASSERT_EQUAL_INT(MQTT_SERVICE_STATUS_OK, service_status);
}

static void connect_and_assert(int client_index,
    const char *client_id, const char *hostname, const char *username,
    const char *pass, uint16_t port)
{
    mqtt_service_pkt_status_t pkt_status;
    esp_mqtt_connect_return_code_t conn_ret_code;
    pkt_status = mqtt_service_connect(client_index, client_id, 
        hostname, username, pass, port, &conn_ret_code);
    TEST_ASSERT_EQUAL_UINT(MQTT_SERVICE_PACKET_STATUS_OK, pkt_status);
    TEST_ASSERT_EQUAL_UINT(MQTT_CONNECTION_ACCEPTED, conn_ret_code);
}

static void disconnect_and_assert(int client_index)
{
    mqtt_service_status_t service_status;
    service_status = mqtt_service_disconnect(client_index);
    TEST_ASSERT_EQUAL_UINT(MQTT_SERVICE_STATUS_OK, service_status);
}

static void subscribe_and_assert(int client_index,
    const char *topic, mqtt_qos_t qos)
{
    mqtt_service_pkt_status_t pkt_status;
    pkt_status = mqtt_service_subscribe(client_index, topic, qos);
    TEST_ASSERT_EQUAL_UINT(MQTT_SERVICE_PACKET_STATUS_OK, pkt_status);
}

static void unsubscribe_and_assert(int client_index,
    const char *topic)
{
    mqtt_service_pkt_status_t pkt_status;
    pkt_status = mqtt_service_unsubscribe(client_index, topic);
    TEST_ASSERT_EQUAL_UINT(MQTT_SERVICE_PACKET_STATUS_OK, pkt_status);
}

static void publish_and_assert(int client_index,
    const char *topic, const char* msg, mqtt_qos_t qos)
{
    mqtt_service_pkt_status_t pkt_status;
    pkt_status = mqtt_service_publish(client_index, topic, msg, qos, false);
    TEST_ASSERT_EQUAL_UINT(MQTT_SERVICE_PACKET_STATUS_OK, pkt_status);
}


static void clear_recv_buff_group(int client_index)
{
    mqtt_service_status_t service_status;
    service_status = mqtt_service_clear_recv_buff_group(client_index);
    TEST_ASSERT_EQUAL_UINT(MQTT_SERVICE_STATUS_OK, service_status);
}

static void get_recv_buff_group_with_timeout_and_assert(
    int client_index, size_t timeout_ms, 
    recv_buffer_group_t *out_recv_buff_group)
{
    mqtt_service_status_t service_status;
    uint32_t max_ticks_to_wait = pdMS_TO_TICKS(timeout_ms);
    while (max_ticks_to_wait > 0)
    {
        service_status = mqtt_service_get_recv_buff_group(client_index, 
            out_recv_buff_group);  
        TEST_ASSERT_EQUAL_UINT(MQTT_SERVICE_STATUS_OK, service_status);
        if (out_recv_buff_group->current_empty_buff_index > 0)
            break;
        max_ticks_to_wait--;
        vTaskDelay(1);
    }
    TEST_ASSERT_GREATER_THAN_UINT(0, 
        out_recv_buff_group->current_empty_buff_index);
}

static void generate_random_string(char *ret_random_string, size_t target_len)
{
    for (size_t pos = 0; pos < target_len; pos++)
    {
        uint32_t raw_result = esp_random() % 52;
        if (raw_result < 26)
        {
            ret_random_string[pos] = 65 + raw_result;
        }
        else
        {
            ret_random_string[pos] = 97 + raw_result - 26;
        }
    }
}

static void basic_action_test(const char* hostname, uint16_t port,
    const char* username, const char* password, int num_of_trials, 
    bool unsub_after_trial)
{
    recv_buffer_group_t test_recv_buff_group;
    connect_to_wifi();
    init_and_assert();
    connect_and_assert(0, "esp32-wifi-test", hostname, 
        username, password, port);
    
    char random_topic[11];
    char random_msg[11];
    for (int count = 0; count < num_of_trials; count++)
    {
        TEST_LOG("Publish & subscribe trial #%d\n", count);
        memset(random_topic, 0, 11);
        memset(random_msg, 0, 11);
        generate_random_string(random_topic, 10);
        generate_random_string(random_msg, 10);
        TEST_LOG("Publish topic '%s' and msg '%s'. Subscribe to the same topic to see if publishing is good\n",
            random_topic, random_msg);

        clear_recv_buff_group(0); 
        subscribe_and_assert(0, random_topic, MQTT_QOS_EXACTLY_ONCE);
        publish_and_assert(0, random_topic, random_msg, MQTT_QOS_EXACTLY_ONCE);
        get_recv_buff_group_with_timeout_and_assert(0, 
            RECV_BUFF_GROUP_TIMEOUT_ms, &test_recv_buff_group);
        if (unsub_after_trial)
            unsubscribe_and_assert(0, random_topic);
        char *recv_topic = test_recv_buff_group.buff[0].topic;
        char *recv_msg = test_recv_buff_group.buff[0].msg;
        TEST_LOG("Receive data: topic '%s' and msg '%s'.\n",
            recv_topic, recv_msg);
        TEST_ASSERT_EQUAL_UINT(strlen(random_topic), strlen(recv_topic));
        TEST_ASSERT_EQUAL_STRING(random_topic, recv_topic);
        TEST_ASSERT_EQUAL_UINT(strlen(random_msg), strlen(recv_msg));
        TEST_ASSERT_EQUAL_STRING(random_msg, recv_msg);
        clear_recv_buff_group(0);
    }
    disconnect_and_assert(0);
    mqtt_service_deinit();
    test_done_deinit();
}

static void basic_action_test_for_ion_broker(int num_of_trials, 
    bool unsub_after_trial)
{
    recv_buffer_group_t test_recv_buff_group;
    connect_to_wifi();
    init_and_assert();
    connect_and_assert(0, "esp32-wifi-test", "mqtts://ion-broker-s.ionmobility.net", 
        ion_broker_username, ion_broker_password, 8883);
    
    char random_msg[11];
    for (int count = 0; count < num_of_trials; count++)
    {
        TEST_LOG("Publish & subscribe trial #%d\n", count);
        memset(random_msg, 0, 11);
        generate_random_string(random_msg, 10);
        TEST_LOG("Publish topic '%s' and msg '%s'. Subscribe to the same topic to see if publishing is good\n",
            ion_allowed_topic, random_msg);

        clear_recv_buff_group(0); 
        subscribe_and_assert(0, ion_allowed_topic, MQTT_QOS_EXACTLY_ONCE);
        publish_and_assert(0, ion_allowed_topic, random_msg, MQTT_QOS_EXACTLY_ONCE);
        get_recv_buff_group_with_timeout_and_assert(0, 
            RECV_BUFF_GROUP_TIMEOUT_ms, &test_recv_buff_group);
        if (unsub_after_trial)
            unsubscribe_and_assert(0, ion_allowed_topic);
        char *recv_topic = test_recv_buff_group.buff[0].topic;
        char *recv_msg = test_recv_buff_group.buff[0].msg;
        TEST_LOG("Receive data: topic '%s' and msg '%s'.\n",
            recv_topic, recv_msg);
        TEST_ASSERT_EQUAL_UINT(strlen(ion_allowed_topic), strlen(recv_topic));
        TEST_ASSERT_EQUAL_STRING(ion_allowed_topic, recv_topic);
        TEST_ASSERT_EQUAL_UINT(strlen(random_msg), strlen(recv_msg));
        TEST_ASSERT_EQUAL_STRING(random_msg, recv_msg);
        clear_recv_buff_group(0);
    }
    disconnect_and_assert(0);
    mqtt_service_deinit();
    test_done_deinit();
}