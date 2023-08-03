#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "mqtt_service.h"
#include "unity.h"
#include "sdkconfig.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common_test_helpers.h"

#define RECV_BUFF_GROUP_TIMEOUT_ms 2000

static const char *ion_broker_username = "9b5766d8-8766-492c-ace8-a80f191e47e6";
static const char *ion_broker_password = "0caabff1-dd1e-49da-a3a3-9a00d4ff2cb6";
static const char *ion_allowed_topic = "channels/ccb53e96-8628-4ac0-9e3f-95185f38733a/messages";


typedef struct {
    const char *test_name;
    const char *at_cmd;
    const char *expected_response;
} at_cmd_test_scene_entry_t;


static const char *module = "mqtt_test: ";
static void init_and_assert();
static void connect_and_assert(int client_index,
    const char *client_id, const char *hostname, const char *username,
    const char *pass, uint16_t port);
static void disconnect_and_assert(int client_index);

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
static void get_recv_buff_with_timeout_and_assert(
    int client_index, size_t timeout_ms, 
    recv_buffer_t *out_recv_buff);

static void disconnect_and_assert(int client_index);

static void generate_random_string(char *ret_random_string, size_t target_len);

static void basic_action_test(const char* hostname, uint16_t port,
    const char* username, const char* password, int num_of_trials, 
    bool unsub_after_trial);

static void basic_action_test_for_ion_broker(int num_of_trials, 
    bool unsub_after_trial);


void TestCase_Connect_to_WiFi()
{
    test_begin_setup();
}

void TestCase_MQTT_Connect_Disconnect()
{
    test_begin_setup();
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
    test_done_clear_up();
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
    test_begin_setup();
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
    test_done_clear_up();
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
    test_begin_setup();
    init_and_assert();
    TEST_LOG("Test connect and disconnect ION broker\n");
    connect_and_assert(0, "esp32-wifi-test", "mqtts://ion-broker-s.ionmobility.net", 
        ion_broker_username, ion_broker_password, 8883);
    disconnect_and_assert(0);
    mqtt_service_deinit();
    test_done_clear_up();
}

void TestCase_MQTT_ION_Broker_Basic_Actions()
{
    basic_action_test_for_ion_broker(1, true);
}

void TestCase_MQTT_ION_Broker_StressTest()
{
    basic_action_test_for_ion_broker(50, false);
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
    unsigned int num_of_filled_recv_buffs;
    service_status = mqtt_service_get_num_of_filled_recv_buffs(0, &num_of_filled_recv_buffs);
    TEST_ASSERT_EQUAL_UINT_MESSAGE(MQTT_SERVICE_PACKET_STATUS_OK, 
        service_status, "Failed to get num_of_filled_recv_buffs");
    while (num_of_filled_recv_buffs)
    {
        service_status = mqtt_service_clear_current_filled_recv_buff(
            client_index);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(MQTT_SERVICE_PACKET_STATUS_OK, 
            service_status, "Failed to clear curent filled recv buff");
        service_status = mqtt_service_get_num_of_filled_recv_buffs(
            0, &num_of_filled_recv_buffs);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(MQTT_SERVICE_PACKET_STATUS_OK, 
            service_status, "Failed to get num_of_filled_recv_buffs");
    }
}

static void get_recv_buff_with_timeout_and_assert(
    int client_index, size_t timeout_ms, 
    recv_buffer_t *out_recv_buff)
{
    bool is_there_filled_recv_buff = false;
    mqtt_service_status_t service_status;
    uint32_t max_ticks_to_wait = pdMS_TO_TICKS(timeout_ms);
    while (max_ticks_to_wait > 0)
    {
        service_status = mqtt_service_read_current_filled_recv_buff(
            client_index, out_recv_buff);  
        TEST_ASSERT_EQUAL_UINT(MQTT_SERVICE_STATUS_OK, service_status);
        if ((out_recv_buff->topic) && (out_recv_buff->msg))
        {
            is_there_filled_recv_buff = true;
            break;
        }
        max_ticks_to_wait--;
        vTaskDelay(1);
    }
    TEST_ASSERT_TRUE_MESSAGE(is_there_filled_recv_buff, 
        "No filled recv buff found");
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
    recv_buffer_t test_recv_buff;
    test_begin_setup();
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
        get_recv_buff_with_timeout_and_assert(0, 
            RECV_BUFF_GROUP_TIMEOUT_ms, &test_recv_buff);
        if (unsub_after_trial)
            unsubscribe_and_assert(0, random_topic);
        char *recv_topic = test_recv_buff.topic;
        char *recv_msg = test_recv_buff.msg;
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
    test_done_clear_up();
}

static void basic_action_test_for_ion_broker(int num_of_trials, 
    bool unsub_after_trial)
{
    recv_buffer_t test_recv_buff;
    test_begin_setup();
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
        get_recv_buff_with_timeout_and_assert(0, 
            RECV_BUFF_GROUP_TIMEOUT_ms, &test_recv_buff);
        if (unsub_after_trial)
            unsubscribe_and_assert(0, ion_allowed_topic);
        char *recv_topic = test_recv_buff.topic;
        char *recv_msg = test_recv_buff.msg;
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
    test_done_clear_up();
}