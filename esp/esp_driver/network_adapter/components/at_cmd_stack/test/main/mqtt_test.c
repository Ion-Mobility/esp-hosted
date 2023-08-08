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
#include "sys_common_funcs.h"

#define MAX_RANDOM_STRING_LENGTH 10
#define RECV_BUFF_GROUP_TIMEOUT_ms 2000
#define CHECK_AND_ASSIGN_WHICH_TO_USE(dst_buff, src_buff, random_buff) do { \
    if (src_buff == NULL) \
    { \
        dst_buff = random_buff; \
        memset(random_buff, 0, MAX_RANDOM_STRING_LENGTH + 1); \
        generate_random_string(random_buff, MAX_RANDOM_STRING_LENGTH); \
    } \
    else \
    { \
        dst_buff = sys_mem_calloc(1, strlen(src_buff) + 1); \
        memcpy(dst_buff, src_buff, strlen(src_buff)); \
    } \
} while (0)

#define CLEANUP_SENDING_DATA(data, random_buff) do { \
    if (data != random_buff) \
    { \
        sys_mem_free(data); \
    } \
} while (0)

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

static void mqtt_basic_actions_test(int client_index, const char* hostname, uint16_t port,
    const char* username, const char* password, const char* topic, 
    const char *msg, int num_of_trials, bool is_test_data, bool unsub_after_trial, 
    bool deinit_after_trial, bool disc_after_trial);

static void perform_data_actions_and_assert(int client_index, const char* topic,
    const char* msg, bool unsub_after_trial);


void TestCase_Connect_to_WiFi()
{
    test_begin_setup();
}

void TestCase_MQTT_Connect_Disconnect()
{
    test_begin_setup();
    mqtt_basic_actions_test(0, "mqtt://broker.hivemq.com", 1883, NULL, NULL, 
        NULL, NULL, 1, false, false, false, false);
    mqtt_basic_actions_test(0, "mqtt://mqtt.eclipseprojects.io", 1883, NULL, NULL, 
        NULL, NULL, 1, false, false, false, false);
    test_done_clear_up();
}

void TestCase_MQTT_Basic_Actions()
{
    test_begin_setup();
    mqtt_basic_actions_test(0, "mqtt://broker.hivemq.com", 1883, NULL, NULL, 
        NULL, NULL, 1, true, true, false, false);
    test_done_clear_up();
}

void TestCase_MQTT_StressTest()
{
    test_begin_setup();
    mqtt_basic_actions_test(0, "mqtt://broker.hivemq.com", 1883, NULL, NULL,
        NULL, NULL, 50, true, false, false, false);
    test_done_clear_up();
}

void TestCase_MQTT_TLS_Connect_Disconnect()
{
    test_begin_setup();
    mqtt_basic_actions_test(0, "mqtts://broker.hivemq.com", 8883, NULL, NULL,
        NULL, NULL, 1, false, false, false, false);
    mqtt_basic_actions_test(0, "mqtts://mqtt.eclipseprojects.io", 8883, NULL, NULL,
        NULL, NULL, 1, false, false, false, false);
    test_done_clear_up();
}

void TestCase_MQTT_TLS_Basic_Actions()
{
    test_begin_setup();
    mqtt_basic_actions_test(0, "mqtts://broker.hivemq.com", 8883, NULL, NULL,
        NULL, NULL, 1, true, true, false, false);
    test_done_clear_up();
}

void TestCase_MQTT_TLS_StressTest()
{
    test_begin_setup();
    mqtt_basic_actions_test(0, "mqtts://broker.hivemq.com", 8883, NULL, NULL,
        NULL, NULL, 50, true, false, false, false);
    test_done_clear_up();
}

void TestCase_MQTT_ION_Broker_Connect_Disconnect()
{
    test_begin_setup();
    mqtt_basic_actions_test(0, "mqtts://ion-broker-s.ionmobility.net", 8883, 
        ion_broker_username, ion_broker_password, NULL, NULL, 1,
        false, false, false, false);
    test_done_clear_up();
}

void TestCase_MQTT_ION_Broker_Basic_Actions()
{
    test_begin_setup();
    mqtt_basic_actions_test(0, "mqtts://ion-broker-s.ionmobility.net", 8883,
        ion_broker_username, ion_broker_password, ion_allowed_topic, NULL, 1,
        true, true, false, false);
    test_done_clear_up();
}

void TestCase_MQTT_ION_Broker_StressTest()
{
    test_begin_setup();
    mqtt_basic_actions_test(0, "mqtts://ion-broker-s.ionmobility.net", 8883,
        ion_broker_username, ion_broker_password, ion_allowed_topic, NULL,
        50, true, false, false, false);
    test_done_clear_up();
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

static void mqtt_basic_actions_test(int client_index, const char* hostname, uint16_t port,
    const char* username, const char* password, const char* topic, 
    const char *msg, int num_of_trials, bool is_test_data, bool unsub_after_trial, 
    bool deinit_after_trial, bool disc_after_trial)
{
    if ((!is_test_data) && (unsub_after_trial))
    {
        TEST_LOG("Don't test data but misconfigure unsubcribe after trial. Exiting...");
        return;
    }
    if (!deinit_after_trial)
        init_and_assert();
    if (!disc_after_trial)
        connect_and_assert(0, "esp32-wifi-test", 
            hostname, username, 
            password, port);
    
    
    for (int count = 0; count < num_of_trials; count++)
    {
        TEST_LOG("Trial #%d\n", count);
        if (deinit_after_trial)
            init_and_assert();
        if (disc_after_trial)
            connect_and_assert(0, "esp32-wifi-test", 
                hostname, username, 
                password, port);
        if (is_test_data)
        {
            perform_data_actions_and_assert(0, topic, msg, 
                unsub_after_trial);
        }
        if (disc_after_trial)
            disconnect_and_assert(0);
        if (deinit_after_trial)
            mqtt_service_deinit();
    }
    if (!disc_after_trial)
        disconnect_and_assert(0);
    if (!deinit_after_trial)
        mqtt_service_deinit();
}

static void perform_data_actions_and_assert(int client_index, const char* topic,
    const char* msg, bool unsub_after_trial)
{
    recv_buffer_t test_recv_buff;
    char random_msg[MAX_RANDOM_STRING_LENGTH + 1],
        random_topic[MAX_RANDOM_STRING_LENGTH + 1];
    char *sending_topic, *sending_msg;
    CHECK_AND_ASSIGN_WHICH_TO_USE(sending_topic, topic, random_topic);
    CHECK_AND_ASSIGN_WHICH_TO_USE(sending_msg, msg, random_msg);
    TEST_LOG("Publish topic '%s' and msg '%s'. Subscribe to the same topic to see if publishing is good\n",
        ion_allowed_topic, random_msg);

    clear_recv_buff_group(0); 
    subscribe_and_assert(0, sending_topic, MQTT_QOS_EXACTLY_ONCE);
    publish_and_assert(0, sending_topic, sending_msg, MQTT_QOS_EXACTLY_ONCE);
    get_recv_buff_with_timeout_and_assert(0, 
        RECV_BUFF_GROUP_TIMEOUT_ms, &test_recv_buff);
    if (unsub_after_trial)
        unsubscribe_and_assert(0, sending_topic);
    char *recv_topic = test_recv_buff.topic;
    char *recv_msg = test_recv_buff.msg;
    TEST_LOG("Receive data: topic '%s' and msg '%s'.\n",
        recv_topic, recv_msg);
    TEST_ASSERT_EQUAL_UINT(strlen(sending_topic), strlen(recv_topic));
    TEST_ASSERT_EQUAL_STRING(sending_topic, sending_topic);
    TEST_ASSERT_EQUAL_UINT(strlen(sending_msg), strlen(sending_msg));
    TEST_ASSERT_EQUAL_STRING(sending_msg, sending_msg);
    clear_recv_buff_group(0);
    
    CLEANUP_SENDING_DATA(sending_topic, random_topic);
    CLEANUP_SENDING_DATA(sending_msg, random_msg);
}
