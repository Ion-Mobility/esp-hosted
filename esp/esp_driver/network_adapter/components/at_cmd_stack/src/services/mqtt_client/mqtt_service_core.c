#include "mqtt_service.h"
#include "mqtt_client.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "common_helpers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "sys_common_funcs.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "app_and_service.h"

#define RECEIVE_BUFF_QUEUE_WAIT_TIME_ms 5

#define MUST_GET_STRING_IN_NVS_OR_RETURN_ERR(str, str_size, handle, key, err_msg) \
    do { \
        str = nvs_load_value_if_exist(handle, key, &str_size); \
        if (str == NULL) \
        { \
            AT_STACK_LOGE(err_msg); \
            nvs_close(handle); \
            return -1; \
        } \
    } while (0)


#define CLEAR_RECV_BUFF_GROUP(buff_group) do {\
    memset(buff_group.buff, 0, sizeof(recv_buffer_t) * MAX_NUM_OF_RECV_BUFFER); \
    buff_group.current_empty_buff_index = 0; \
} while (0)

typedef struct {
    esp_mqtt_client_handle_t esp_client_handle;
    EventGroupHandle_t connect_req_event_group;
    EventGroupHandle_t sub_req_event_group;
    EventGroupHandle_t unsub_req_event_group;
    EventGroupHandle_t pub_req_event_group;
    EventGroupHandle_t connect_status_event_group;
    QueueHandle_t recv_queue_handle;
} mqtt_service_client_t;

static bool is_mqtt_service_initialized = false;

#define DEFAULT_PACKET_TIMEOUT_s 15

#define FROM_SEC_TO_MSEC(sec_value) sec_value * 1000
#define MQTT_CONNECT_REQUEST_SUCCESS_BIT BIT0
#define MQTT_CONNECT_REQUEST_FAILED_PROTOCOL_BIT BIT1
#define MQTT_CONNECT_REQUEST_FAILED_IDENTIFIER_BIT BIT2
#define MQTT_CONNECT_REQUEST_FAILED_SERVER_BIT BIT3
#define MQTT_CONNECT_REQUEST_FAILED_USERNAME_PASSWORD_BIT BIT4
#define MQTT_CONNECT_REQUEST_FAILED_AUTHORIZE_BIT BIT5

#define MQTT_CONNECT_STATUS_CONNECTED_BIT BIT0
#define MQTT_CONNECT_STATUS_NOT_CONNECTED_BIT BIT1

#define MQTT_SUBSCRIBE_REQUEST_SUCCESS_BIT BIT0
#define MQTT_UNSUBSCRIBE_REQUEST_SUCCESS_BIT BIT0
#define MQTT_PUBLISH_REQUEST_SUCCESS_BIT BIT0

static const ip_addr_t default_dns_server[] =
    {
        IPADDR4_INIT_BYTES(1, 1, 1, 1), // Cloudflare DNS server
        IPADDR4_INIT_BYTES(8, 8, 8, 8), // Google DNS server
        IPADDR4_INIT_BYTES(8, 8, 4, 4), // Google DNS server
    };

static mqtt_service_client_t mqtt_service_clients_table[MAX_NUM_OF_MQTT_CLIENT] = {0};

//==============================
// Events Handlers declaration
//==============================

/**
 * @brief Event handler registered to receive MQTT events
 * 
 * @note This function is called by MQTT task. So REMEMBER TO use
 * synchronization mechanism to protected shared resources of MQTT service
 * 
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
    int32_t event_id, void *event_data);

//================================
// Private functions declaration
//================================

static uint32_t wait_for_connect_req_status(int client_index, uint32_t wait_ms);
static uint32_t wait_for_subscribe_req_status(int client_index, uint32_t wait_ms);
static uint32_t wait_for_unsubscribe_req_status(int client_index, uint32_t wait_ms);
static uint32_t wait_for_publish_req_status(int client_index, uint32_t wait_ms);

static uint32_t map_connect_ret_code_to_event_bits(
        esp_mqtt_connect_return_code_t connect_ret_code);
static esp_mqtt_connect_return_code_t map_event_bit_to_connect_ret_code(
        uint32_t event_bit);
static void announce_connect_request_success(int client_index);
static void announce_connect_request_fail(int client_index,
    esp_mqtt_connect_return_code_t connect_req_ret_code);
static void announce_subscribe_request_success(int client_index);
static void announce_unsubscribe_request_success(int client_index);
static void announce_publish_request_success(int client_index);
static void change_connection_status(int client_index, 
    mqtt_client_connection_status_t status_to_change);

static int push_new_sub_data_to_recv_buff_queue(int client_idx, 
    const char *topic, uint16_t topic_len, const char *msg, uint32_t msg_offset,
    uint32_t msg_len, uint32_t msg_total_len);

static bool is_ion_broker(const char *hostname);

static void mqtt_client_setup_before_connect(int client_index);

static int mqtt_client_configure_and_request_connect(int client_index,
    const char *client_id, const char *hostname, const char *username,
    const char *pass, uint16_t port, const char* ca_cert, 
    const char* client_cert, const char* client_priv_key);

static mqtt_service_pkt_status_t mqtt_client_get_connect_req_status_timeout(
    int client_index, uint32_t timeout_ms,
    esp_mqtt_connect_return_code_t *connect_ret_code);

static int get_crts_from_nvs(char** get_cacert, char** get_client_cert,
    char** get_client_key);

static char * nvs_load_value_if_exist(nvs_handle_t handle, const char* key,
    size_t *value_size);

/**
 * @brief Find the client index of givent MQTT client handle
 * 
 * @param client 
 * @retval index of client
 * @retval -1 if index not found
 */
static int find_index_from_client_handle(esp_mqtt_client_handle_t client);

static esp_mqtt_protocol_ver_t get_protocol_vsn_from_mqtt_service_config(int client_index);
static int get_keepalive_from_mqtt_service_config(int client_index);
static bool get_disable_clean_session_from_mqtt_service_config(int client_index);

static void mqtt_service_deinit_internal();


//===============================
// Public functions definition
//===============================
mqtt_service_status_t mqtt_service_init()
{
    AT_STACK_LOGI("Init MQTT service");
    for (int index = 0; 
        index < sizeof(default_dns_server) / sizeof(default_dns_server[0]);
        index++)
    {
        dns_setserver(index, &default_dns_server[index]);
    }

    for (int index = 0; index < MAX_NUM_OF_MQTT_CLIENT; index++)
    {
        AT_STACK_LOGI("Init client #%d", index);
        // Dummy config only for initialization. Later on MQTT client will
        // be configured again when perform connecting, publishing,
        // subscribing
        esp_mqtt_client_config_t dummy_config ={0};
        mqtt_service_client_t *service_client_handle = 
            &mqtt_service_clients_table[index];
        service_client_handle->esp_client_handle = 
            esp_mqtt_client_init(&dummy_config);
        if (service_client_handle->esp_client_handle == NULL)
        {
            AT_STACK_LOGE("ESP client handle is NULL!!");
            goto init_err;
        }
        AT_STACK_LOGD("Successfully got an ESP client handle for client!");
        esp_err_t event_register_status =  esp_mqtt_client_register_event(
            service_client_handle->esp_client_handle,
            ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        switch (event_register_status)
        {
        case ESP_ERR_INVALID_ARG:
            AT_STACK_LOGE("Wrong initialization of ESP client");
            goto init_err;
            break;
        case ESP_ERR_NO_MEM:
            AT_STACK_LOGE("No more memory for register client event");
            goto init_err;
            break;
        default:
            AT_STACK_LOGD("Register event for client successfully!");
        }
        service_client_handle->connect_req_event_group = xEventGroupCreate();
        if (service_client_handle->connect_req_event_group == NULL)
        {
            AT_STACK_LOGE("No more memory for creating connect request event group");
            goto init_err;
        }
        service_client_handle->sub_req_event_group = xEventGroupCreate();
        if (service_client_handle->sub_req_event_group == NULL)
        {
            AT_STACK_LOGE("No more memory for creating subscribe request event group");
            goto init_err;
        }
        service_client_handle->unsub_req_event_group = xEventGroupCreate();
        if (service_client_handle->unsub_req_event_group == NULL)
        {
            AT_STACK_LOGE("No more memory for creating unsubcribe request event group");
            goto init_err;
        }
        service_client_handle->pub_req_event_group = xEventGroupCreate();
        if (service_client_handle->pub_req_event_group == NULL)
        {
            AT_STACK_LOGE("No more memory for creating publish request event group");
            goto init_err;
        }
        AT_STACK_LOGD("Create event group for connect request successfully");
        service_client_handle->connect_status_event_group = xEventGroupCreate();
        if (service_client_handle->connect_status_event_group == NULL)
        {
            AT_STACK_LOGE("No more memory for creating connect status event group");
            goto init_err;
        }
        AT_STACK_LOGD("Create event group for connect status successfully");
        // must initialize connection status to not connected
        change_connection_status(index, MQTT_CONNECTION_STATUS_NOT_CONNECTED);

        // Create client's receive queue
        service_client_handle->recv_queue_handle = 
            xQueueCreate(MAX_NUM_OF_RECV_BUFFER, sizeof(recv_buffer_t));
    }
    /*  Initialize NVS. This is necessary because we don't know if NVS flash
        is initialized before by caller of this fuction. Moreover, if NVS flash
        is initialized before, subsequent call nvs_flash_init simply 
        returns ESP_OK */
	esp_err_t ret = nvs_flash_init();

	switch (ret)
    {
    case ESP_ERR_NVS_NO_FREE_PAGES:
        AT_STACK_LOGE("Init Non-volatile Storage FAILED due to no free pages!");
        goto init_err;
        break;
    case ESP_ERR_NVS_NEW_VERSION_FOUND:
        AT_STACK_LOGE("Init Non-volatile Storage FAILED due to no new version!");
        goto init_err;
        break;
    }

    AT_STACK_LOGD("Initialize Non-volatile Storage SUCCESS");
    AT_STACK_LOGI("MQTT service initializes successfully");
    is_mqtt_service_initialized = true;
    return MQTT_SERVICE_STATUS_OK;

init_err:
    AT_STACK_LOGI("Deinit MQTT service due to failing MQTT service init");
    mqtt_service_deinit_internal();
    return MQTT_SERVICE_STATUS_ERROR;
}

mqtt_service_status_t mqtt_service_deinit()
{
    if (!is_mqtt_service_initialized)
    {
        AT_STACK_LOGE("MQTT service is not initialized yet");
        return MQTT_SERVICE_STATUS_ERROR;
    }

    mqtt_service_deinit_internal();
    AT_STACK_LOGI("MQTT service is deinitialized SUCCESS");
    return MQTT_SERVICE_STATUS_OK;
}

mqtt_client_connection_status_t mqtt_service_get_connection_status(
    int client_index)
{
    uint32_t connect_status_bit = xEventGroupGetBits(
        mqtt_service_clients_table[client_index].connect_status_event_group
    );
    if (connect_status_bit == MQTT_CONNECT_STATUS_CONNECTED_BIT)
    {
        AT_STACK_LOGI("Get MQTT service: client #%d is connected", client_index);
        return MQTT_CONNECTION_STATUS_CONNECTED;
    }
    AT_STACK_LOGI("Get MQTT service: client #%d is not connected", client_index);
    return MQTT_CONNECTION_STATUS_NOT_CONNECTED;

}

mqtt_service_pkt_status_t mqtt_service_connect(int client_index,
    const char *client_id, const char *hostname, const char *username,
    const char *pass, uint16_t port,
    esp_mqtt_connect_return_code_t *connect_ret_code)
{
    mqtt_service_pkt_status_t ret_status;
    char *get_ca_crt = NULL, *get_client_crt = NULL, 
        *get_client_priv_key = NULL;
    if (is_ion_broker(hostname))
    {
        AT_STACK_LOGD("Hostname is an ION broker");
        get_crts_from_nvs(&get_ca_crt, &get_client_crt, &get_client_priv_key);
    }
    mqtt_client_setup_before_connect(client_index);
    if (mqtt_client_configure_and_request_connect(client_index, client_id, 
        hostname, username, pass, port, 
        get_ca_crt, get_client_crt, get_client_priv_key))
    {
        AT_STACK_LOGE("Something wrong with MQTT configuration and connect");
        ret_status = MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND;
        *connect_ret_code = MQTT_CONNECTION_REFUSE_PROTOCOL;
        goto connect_done;
    }
    ret_status = mqtt_client_get_connect_req_status_timeout(
        client_index, DEFAULT_PACKET_TIMEOUT_s, connect_ret_code);
    AT_STACK_LOGI("Client #%d try to connect to '%s', port %u. Connect return code %u, packet status %u",
        client_index, hostname, port, *connect_ret_code, ret_status);

connect_done:
    sys_mem_free(get_ca_crt);
    sys_mem_free(get_client_crt);
    sys_mem_free(get_client_priv_key);
    return ret_status;
}

mqtt_service_pkt_status_t mqtt_service_subscribe(int client_index,
    const char *topic, mqtt_qos_t qos)
{
    uint32_t connect_status_bit = xEventGroupGetBits(
        mqtt_service_clients_table[client_index].connect_status_event_group);
    bool is_broker_connected = 
        (connect_status_bit == MQTT_CONNECT_STATUS_CONNECTED_BIT);
    MUST_BE_CORRECT_OR_EXIT(is_broker_connected, 
        MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND);
    esp_mqtt_client_handle_t client = 
        mqtt_service_clients_table[client_index].esp_client_handle;

    int msg_id = esp_mqtt_client_subscribe(client, topic, (int) qos);
    if (msg_id == -1)
        goto failed_sub;
    AT_STACK_LOGD("Client #%d subscribe MQTT packet has msgid=%d", client_index,
        msg_id);

    uint32_t req_status = wait_for_subscribe_req_status(client_index, 
        DEFAULT_PACKET_TIMEOUT_s * 1000);
    if (req_status != MQTT_SUBSCRIBE_REQUEST_SUCCESS_BIT)
        goto failed_sub;

    AT_STACK_LOGI("Client #%d subscribe to topic '%s', qos %u SUCCESS",
        client_index, topic, qos);
    return MQTT_SERVICE_PACKET_STATUS_OK;
failed_sub:
        AT_STACK_LOGE("Client #%d subscribe to topic '%s', qos %u FAILED!",
            client_index, topic, qos);
        return MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND; 
}

mqtt_service_pkt_status_t mqtt_service_unsubscribe(int client_index,
    const char *topic)
{
    uint32_t connect_status_bit = xEventGroupGetBits(
        mqtt_service_clients_table[client_index].connect_status_event_group);
    bool is_broker_connected = 
        (connect_status_bit == MQTT_CONNECT_STATUS_CONNECTED_BIT);
    MUST_BE_CORRECT_OR_EXIT(is_broker_connected, 
        MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND);
    esp_mqtt_client_handle_t client = 
        mqtt_service_clients_table[client_index].esp_client_handle;

    int msg_id = esp_mqtt_client_unsubscribe(client, topic);
    if (msg_id == -1)
        goto failed_unsub;
    AT_STACK_LOGD("Client #%d unsubscribe MQTT packet has msgid=%d", client_index,
        msg_id);

    uint32_t req_status = wait_for_unsubscribe_req_status(client_index, 
        DEFAULT_PACKET_TIMEOUT_s * 1000);
    if (req_status != MQTT_UNSUBSCRIBE_REQUEST_SUCCESS_BIT)
        goto failed_unsub;

    AT_STACK_LOGI("Client #%d unsubscribe from topic '%s'SUCCESS",
        client_index, topic);
    return MQTT_SERVICE_PACKET_STATUS_OK;
failed_unsub:
        AT_STACK_LOGE("Client #%d unsubscribe from topic '%s' FAILED!",
            client_index, topic);
        return MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND; 
}

mqtt_service_pkt_status_t mqtt_service_publish(int client_index,
    const char *topic, const char* msg, mqtt_qos_t qos,
    bool is_retain)
{
    uint32_t connect_status_bit = xEventGroupGetBits(
        mqtt_service_clients_table[client_index].connect_status_event_group);
    bool is_broker_connected = 
        (connect_status_bit == MQTT_CONNECT_STATUS_CONNECTED_BIT);
    MUST_BE_CORRECT_OR_EXIT(is_broker_connected, 
        MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND);

    esp_mqtt_client_handle_t client = 
        mqtt_service_clients_table[client_index].esp_client_handle;

    int msg_id = esp_mqtt_client_publish(client, topic, msg, strlen(msg), qos, 
        is_retain);
    if (msg_id == -1)
        goto failed_pub;

    AT_STACK_LOGD("Client #%d publish MQTT packet has msgid=%d", client_index,
        msg_id);
    uint32_t req_status = wait_for_publish_req_status(client_index, 
        DEFAULT_PACKET_TIMEOUT_s * 1000);
    if (req_status != MQTT_PUBLISH_REQUEST_SUCCESS_BIT)
        goto failed_pub;

    AT_STACK_LOGI("Client #%d publish to topic '%s', msg '%s', qos %u, retain '%d' SUCCESS", 
        client_index, topic, msg, qos, 
        is_retain);
    return MQTT_SERVICE_PACKET_STATUS_OK;
failed_pub:
    AT_STACK_LOGE("Client #%d publish to topic '%s', msg '%s', qos %u, retain '%d' FAILED", 
            client_index, topic, msg, qos, 
            is_retain);
    return MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND; 
}

mqtt_service_status_t mqtt_service_disconnect(int client_index)
{
    mqtt_service_client_t *service_client_handle = 
            &mqtt_service_clients_table[client_index];
    MUST_BE_CORRECT_OR_EXIT(is_mqtt_service_initialized, 
        MQTT_SERVICE_STATUS_ERROR);

    mqtt_client_connection_status_t connection_status = 
        mqtt_service_get_connection_status(client_index);
    bool is_client_connected = 
        (connection_status == MQTT_CONNECTION_STATUS_CONNECTED);
    if (!is_client_connected)
        return MQTT_SERVICE_STATUS_OK;

    esp_mqtt_client_handle_t client = service_client_handle->esp_client_handle;
    if (esp_mqtt_client_disconnect(client))
    {
        AT_STACK_LOGE("Client #%d disconnect FAILED", client_index);
        return MQTT_SERVICE_STATUS_ERROR;
    }
    AT_STACK_LOGI("Client #%d disconnect SUCCESS", client_index);
    return MQTT_SERVICE_STATUS_OK;
}

mqtt_service_status_t mqtt_service_get_num_of_filled_recv_buffs(
    int client_index, unsigned int *num_of_filled_recv_buffs)
{
    mqtt_service_client_t *service_client_handle = 
            &mqtt_service_clients_table[client_index];
    MUST_BE_CORRECT_OR_EXIT(is_mqtt_service_initialized, 
        MQTT_SERVICE_STATUS_ERROR);
    mqtt_client_connection_status_t connection_status = 
        mqtt_service_get_connection_status(client_index);
    bool is_client_connected = 
        (connection_status == MQTT_CONNECTION_STATUS_CONNECTED);
    MUST_BE_CORRECT_OR_EXIT(is_client_connected, 
        MQTT_SERVICE_STATUS_ERROR);

    *num_of_filled_recv_buffs = MAX_NUM_OF_RECV_BUFFER 
        - uxQueueSpacesAvailable(service_client_handle->recv_queue_handle);
    AT_STACK_LOGI("Get number of filled recv buffers of client #%d, num_of_filled_recv_buffs=%u", 
        client_index, *num_of_filled_recv_buffs);

    return MQTT_SERVICE_PACKET_STATUS_OK;
}

mqtt_service_status_t mqtt_service_read_current_filled_recv_buff(
    int client_index, recv_buffer_t *out_recv_buff)
{
    mqtt_service_client_t *service_client_handle = 
            &mqtt_service_clients_table[client_index];
    MUST_BE_CORRECT_OR_EXIT(is_mqtt_service_initialized, 
        MQTT_SERVICE_STATUS_ERROR);
    mqtt_client_connection_status_t connection_status = 
        mqtt_service_get_connection_status(client_index);
    bool is_client_connected = 
        (connection_status == MQTT_CONNECTION_STATUS_CONNECTED);
    MUST_BE_CORRECT_OR_EXIT(is_client_connected, 
        MQTT_SERVICE_STATUS_ERROR);

    // Simply copy local buffer group to output buffer group
    // NOTE: caller is responsible for free allocated buffer of topic and message
    if (xQueuePeek(service_client_handle->recv_queue_handle,
        out_recv_buff, pdMS_TO_TICKS(RECEIVE_BUFF_QUEUE_WAIT_TIME_ms))
        != pdTRUE)
    {
        AT_STACK_LOGI("Read recv buff of client #%d FAILED",
            client_index);
        memset(out_recv_buff, 0, sizeof(recv_buffer_t));
    }
    else
    {
        AT_STACK_LOGI("Read recv buff of client #%d SUCCESS", 
            client_index);
    }

    return MQTT_SERVICE_PACKET_STATUS_OK;
}

mqtt_service_status_t mqtt_service_clear_current_filled_recv_buff(
    int client_index)
{
    mqtt_service_client_t *service_client_handle = 
            &mqtt_service_clients_table[client_index];
    MUST_BE_CORRECT_OR_EXIT(is_mqtt_service_initialized, 
        MQTT_SERVICE_STATUS_ERROR);
    mqtt_client_connection_status_t connection_status = 
        mqtt_service_get_connection_status(client_index);
    bool is_client_connected = 
        (connection_status == MQTT_CONNECTION_STATUS_CONNECTED);
    MUST_BE_CORRECT_OR_EXIT(is_client_connected, 
        MQTT_SERVICE_STATUS_ERROR);

    // Simply copy local buffer group to output buffer group
    recv_buffer_t dummy_recv_buff;
    if (xQueueReceive(service_client_handle->recv_queue_handle,
        &dummy_recv_buff, pdMS_TO_TICKS(RECEIVE_BUFF_QUEUE_WAIT_TIME_ms))
        != pdTRUE)
    {
        AT_STACK_LOGI("Clear recv buff of client #%d FAILED",
            client_index);
    }
    else
    {
        AT_STACK_LOGI("Clear recv buff of client #%d SUCCESS", 
            client_index);
    }

    return MQTT_SERVICE_PACKET_STATUS_OK;
}

//============================
// Events Handler definition
//============================

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int client_idx = find_index_from_client_handle(client);
    mqtt_service_client_t *service_client_handle = 
            &mqtt_service_clients_table[client_idx];
    int result;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        AT_STACK_LOGI("Client %d connected to broker!", client_idx);
        announce_connect_request_success(client_idx);
        break;

    case MQTT_EVENT_DISCONNECTED:
        AT_STACK_LOGI("Client %d disconnected to broker!", client_idx);
        change_connection_status(client_idx, 
            MQTT_CONNECTION_STATUS_NOT_CONNECTED);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        AT_STACK_LOGI("Client %d has subcribed to a topic with msgid=%d!", client_idx
            , event->msg_id);
        announce_subscribe_request_success(client_idx);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        AT_STACK_LOGI("Client %d has unsubcribed!", client_idx);
        announce_unsubscribe_request_success(client_idx);
        break;
    case MQTT_EVENT_PUBLISHED:
        AT_STACK_LOGI("Client %d has published a data, msgid=%d", client_idx,
            event->msg_id);
        announce_publish_request_success(client_idx);
        break;

    case MQTT_EVENT_DATA:
        AT_STACK_LOGI("Client %d got a data, QoS=%d, retain=%d", client_idx, 
            event->qos, event->retain);
        result = push_new_sub_data_to_recv_buff_queue(client_idx, 
            event->topic, (uint16_t) event->topic_len, event->data, 
            (uint32_t) event->current_data_offset, (uint32_t)event->data_len,
            (uint32_t)event->total_data_len);
        if (result)
        {
            AT_STACK_LOGI("Push new subscribed data FAILED for some reason!");
        }
        else
        {
            AT_STACK_LOGI("Push new subscribed data SUCCESS!");
        }
        break;

    case MQTT_EVENT_ERROR:
        AT_STACK_LOGI("Client %d have error!", client_idx);
        if (event->error_handle->error_type == 
            MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            announce_connect_request_fail(client_idx, 
                event->error_handle->connect_return_code);
            AT_STACK_LOGI("MQTT connection refuse, ret code = %d!",
                event->error_handle->connect_return_code);
        }
        break;

    default:
        AT_STACK_LOGI("Other event id:%d", event->event_id);
        break;
    }
}


//===============================
// Private functions definition
//===============================

static void mqtt_client_setup_before_connect(int client_index)
{
    esp_mqtt_client_handle_t client_handle = 
        mqtt_service_clients_table[client_index].esp_client_handle;
    esp_mqtt_client_stop(client_handle);
    change_connection_status(client_index, 
        MQTT_CONNECT_STATUS_NOT_CONNECTED_BIT);
}

static uint32_t wait_for_connect_req_status(int client_index, uint32_t wait_ms)
{
    uint32_t ticks_to_wait = pdMS_TO_TICKS(wait_ms);
    uint32_t total_bits_to_wait = MQTT_CONNECT_REQUEST_SUCCESS_BIT || 
        MQTT_CONNECT_REQUEST_FAILED_PROTOCOL_BIT || 
        MQTT_CONNECT_REQUEST_FAILED_IDENTIFIER_BIT || 
        MQTT_CONNECT_REQUEST_FAILED_SERVER_BIT ||
        MQTT_CONNECT_REQUEST_FAILED_USERNAME_PASSWORD_BIT ||
        MQTT_CONNECT_REQUEST_FAILED_AUTHORIZE_BIT;
    
    mqtt_service_client_t *service_client_handle = 
        &mqtt_service_clients_table[client_index];
    xEventGroupClearBits(service_client_handle->connect_req_event_group, 
        total_bits_to_wait);
    xEventGroupClearBits(service_client_handle->connect_status_event_group, 
        MQTT_CONNECT_STATUS_CONNECTED_BIT);
    xEventGroupSetBits(service_client_handle->connect_status_event_group, 
        MQTT_CONNECT_STATUS_NOT_CONNECTED_BIT);
    return xEventGroupWaitBits(service_client_handle->connect_req_event_group,
        total_bits_to_wait,
        pdFALSE,
        pdFALSE,
        ticks_to_wait
        );
}

static uint32_t wait_for_subscribe_req_status(int client_index, uint32_t wait_ms)
{
    uint32_t ticks_to_wait = pdMS_TO_TICKS(wait_ms);
    uint32_t total_bits_to_wait = MQTT_SUBSCRIBE_REQUEST_SUCCESS_BIT;
    
    mqtt_service_client_t *service_client_handle = 
        &mqtt_service_clients_table[client_index];
    xEventGroupClearBits(service_client_handle->sub_req_event_group, 
        total_bits_to_wait);
    return xEventGroupWaitBits(service_client_handle->sub_req_event_group,
        total_bits_to_wait,
        pdFALSE,
        pdFALSE,
        ticks_to_wait
        );
}

static uint32_t wait_for_unsubscribe_req_status(int client_index, uint32_t wait_ms)
{
    uint32_t ticks_to_wait = pdMS_TO_TICKS(wait_ms);
    uint32_t total_bits_to_wait = MQTT_UNSUBSCRIBE_REQUEST_SUCCESS_BIT;
    
    mqtt_service_client_t *service_client_handle = 
        &mqtt_service_clients_table[client_index];
    xEventGroupClearBits(service_client_handle->unsub_req_event_group, 
        total_bits_to_wait);
    return xEventGroupWaitBits(service_client_handle->unsub_req_event_group,
        total_bits_to_wait,
        pdFALSE,
        pdFALSE,
        ticks_to_wait
        );
}

static uint32_t wait_for_publish_req_status(int client_index, uint32_t wait_ms)
{
    uint32_t ticks_to_wait = pdMS_TO_TICKS(wait_ms);
    uint32_t total_bits_to_wait = MQTT_PUBLISH_REQUEST_SUCCESS_BIT;
    
    mqtt_service_client_t *service_client_handle = 
        &mqtt_service_clients_table[client_index];
    xEventGroupClearBits(service_client_handle->pub_req_event_group, 
        total_bits_to_wait);
    return xEventGroupWaitBits(service_client_handle->pub_req_event_group,
        total_bits_to_wait,
        pdFALSE,
        pdFALSE,
        ticks_to_wait
        );
}

static uint32_t map_connect_ret_code_to_event_bits(
        esp_mqtt_connect_return_code_t connect_ret_code)
{
    uint32_t ret_val = MQTT_CONNECT_REQUEST_SUCCESS_BIT;
    switch (connect_ret_code)
    {
        case MQTT_CONNECTION_ACCEPTED: // do this to avoid compiler error
            break;
        case MQTT_CONNECTION_REFUSE_PROTOCOL:
            ret_val = MQTT_CONNECT_REQUEST_FAILED_PROTOCOL_BIT;
            break;
        case MQTT_CONNECTION_REFUSE_ID_REJECTED:
            ret_val = MQTT_CONNECT_REQUEST_FAILED_IDENTIFIER_BIT;
            break;
        case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE:
            ret_val = MQTT_CONNECT_REQUEST_FAILED_SERVER_BIT;
            break;
        case MQTT_CONNECTION_REFUSE_BAD_USERNAME:
            ret_val = MQTT_CONNECT_REQUEST_FAILED_USERNAME_PASSWORD_BIT;
            break;
        case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:
            ret_val = MQTT_CONNECT_REQUEST_FAILED_AUTHORIZE_BIT;
            break;
    }
    return ret_val;
}

static esp_mqtt_connect_return_code_t map_event_bit_to_connect_ret_code(
        uint32_t event_bit)
{
    esp_mqtt_connect_return_code_t ret_val = MQTT_CONNECTION_ACCEPTED;
    switch (event_bit)
    {
        case MQTT_CONNECT_REQUEST_FAILED_PROTOCOL_BIT:
            ret_val = MQTT_CONNECTION_REFUSE_PROTOCOL;
            break;
        case MQTT_CONNECT_REQUEST_FAILED_IDENTIFIER_BIT:
            ret_val = MQTT_CONNECTION_REFUSE_ID_REJECTED;
            break;
        case MQTT_CONNECT_REQUEST_FAILED_SERVER_BIT:
            ret_val = MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE;
            break;
        case MQTT_CONNECT_REQUEST_FAILED_USERNAME_PASSWORD_BIT:
            ret_val = MQTT_CONNECTION_REFUSE_BAD_USERNAME;
            break;
        case MQTT_CONNECT_REQUEST_FAILED_AUTHORIZE_BIT:
            ret_val = MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED;
            break;
    }
    return ret_val;
}

static void announce_connect_request_success(int client_index)
{
    mqtt_service_client_t *service_client_handle = 
        &mqtt_service_clients_table[client_index];
    xEventGroupSetBits(service_client_handle->connect_req_event_group, 
        MQTT_CONNECT_REQUEST_SUCCESS_BIT);
    change_connection_status(client_index, MQTT_CONNECTION_STATUS_CONNECTED);
}

static void announce_connect_request_fail(int client_index, 
    esp_mqtt_connect_return_code_t connect_req_ret_code)
{
    mqtt_service_client_t *service_client_handle = 
        &mqtt_service_clients_table[client_index];
    uint32_t connect_req_bit = map_connect_ret_code_to_event_bits(
        connect_req_ret_code);
    xEventGroupSetBits(service_client_handle->connect_req_event_group, 
        connect_req_bit);
    change_connection_status(client_index, 
        MQTT_CONNECTION_STATUS_NOT_CONNECTED);
}

static void announce_subscribe_request_success(int client_index)
{
    mqtt_service_client_t *service_client_handle = 
        &mqtt_service_clients_table[client_index];
    xEventGroupSetBits(service_client_handle->sub_req_event_group, 
        MQTT_SUBSCRIBE_REQUEST_SUCCESS_BIT);
}

static void announce_unsubscribe_request_success(int client_index)
{
    mqtt_service_client_t *service_client_handle = 
        &mqtt_service_clients_table[client_index];
    xEventGroupSetBits(service_client_handle->unsub_req_event_group, 
        MQTT_UNSUBSCRIBE_REQUEST_SUCCESS_BIT);
}

static void announce_publish_request_success(int client_index)
{
    mqtt_service_client_t *service_client_handle = 
        &mqtt_service_clients_table[client_index];
    xEventGroupSetBits(service_client_handle->pub_req_event_group, 
        MQTT_PUBLISH_REQUEST_SUCCESS_BIT);
}

static int find_index_from_client_handle(esp_mqtt_client_handle_t client)
{
    for (int index = 0; index < MAX_NUM_OF_MQTT_CLIENT; index++)
    {
        if (client == mqtt_service_clients_table[index].esp_client_handle)
            return index;
    }
    return -1;
}

static void change_connection_status(int client_index, 
    mqtt_client_connection_status_t status_to_change)
{
    mqtt_service_client_t *service_client_handle = 
        &mqtt_service_clients_table[client_index];
    switch (status_to_change)
    {
    case MQTT_CONNECTION_STATUS_CONNECTED:
        xEventGroupClearBits(service_client_handle->connect_status_event_group, 
            MQTT_CONNECT_STATUS_NOT_CONNECTED_BIT);
        xEventGroupSetBits(service_client_handle->connect_status_event_group, 
            MQTT_CONNECT_STATUS_CONNECTED_BIT); 
        break;
    
    case MQTT_CONNECTION_STATUS_NOT_CONNECTED:
        xEventGroupClearBits(service_client_handle->connect_status_event_group, 
            MQTT_CONNECT_STATUS_CONNECTED_BIT);
        xEventGroupSetBits(service_client_handle->connect_status_event_group, 
            MQTT_CONNECT_STATUS_NOT_CONNECTED_BIT); 
        break;
    }
}

static int push_new_sub_data_to_recv_buff_queue(int client_idx, 
    const char *topic, uint16_t topic_len, const char *msg, uint32_t msg_offset,
    uint32_t msg_len, uint32_t msg_total_len)
{
    // Since esp_mqtt_task will try to forward all fragments of the same 
    // messages at once, it's safe to define only one static recv_buff_to_push to store
    // just one pending message then push to local queue
    static recv_buffer_t *recv_buff_to_push = NULL;
    if ((msg_offset > 0) && (NULL == recv_buff_to_push))
    {
        AT_STACK_LOGE("Not found first data fragment due to NULL receive buff to push but offset is larger than 0!");
        return -1;
    }
    if ((recv_buff_to_push) && (msg_offset != recv_buff_to_push->msg_len))
    {
        AT_STACK_LOGE("Data offset (offset=%u) does not match the current message length (len=%u)",
            msg_offset, recv_buff_to_push->msg_len);
        sys_mem_free(recv_buff_to_push);
        return -1;
    }

    if (msg_offset > 0)
    {
        AT_STACK_LOGD("Receive new data fragment, append to existing fragments! current_topic='%s', current msg='%s', fragment len=%u, offset=%u", 
            recv_buff_to_push->topic, recv_buff_to_push->msg, msg_len, msg_offset);
        memcpy(recv_buff_to_push->msg + msg_offset, msg, msg_len);
        recv_buff_to_push->msg_len += msg_len;
    }
    mqtt_service_client_t *service_client_handle = 
    &mqtt_service_clients_table[client_idx];

    if (msg_offset == 0)
    {
        uint8_t client_num_of_empty_recv_buff = 
            uxQueueSpacesAvailable(service_client_handle->recv_queue_handle);
        if (!client_num_of_empty_recv_buff)
        {
            AT_STACK_LOGE("No more empty recv buffer. Discard this data!");
            return -1;
        }
        recv_buff_to_push = sys_mem_calloc(sizeof(recv_buffer_t), 1);
        recv_buff_to_push->topic = sys_mem_calloc(topic_len + 1, 1);
        memcpy(recv_buff_to_push->topic, topic, topic_len);
        recv_buff_to_push->topic_len = topic_len;

        recv_buff_to_push->msg = sys_mem_calloc(msg_total_len + 1, 1);
        memcpy(recv_buff_to_push->msg, msg, msg_len);
        recv_buff_to_push->msg_len = msg_len;
        AT_STACK_LOGD("Receive first data fragment! topic='%s', msg='%s', msg_len=%u, msg_total_len=%u", 
            recv_buff_to_push->topic, recv_buff_to_push->msg,
            recv_buff_to_push->msg_len, msg_total_len);
    }

    if (recv_buff_to_push->msg_len > msg_total_len)
    {
        AT_STACK_LOGE("Size of assembled message (len=%u) is larger than total length receiving message (total_len=%u)! Not good!",
            recv_buff_to_push->msg_len, msg_total_len);
        sys_mem_free(recv_buff_to_push);
        return -1;
    }

    if (recv_buff_to_push->msg_len == msg_total_len)
    {
        AT_STACK_LOGD("Finish assembling all fragments of message (total_len=%u bytes). Send to MQTT service receive buffer now...", 
            recv_buff_to_push->msg_len);
        AT_STACK_LOGD("Sending: topic='%s', msg='%s", recv_buff_to_push->topic, recv_buff_to_push->msg);

        if (!can_app_accept_incoming_recv_buff(client_idx, recv_buff_to_push))
        {
            AT_STACK_LOGE("Cannot push to client recv buff queue due to upper application will not accept it. Discard this data!");
            sys_mem_free(recv_buff_to_push);
            return -1;
        }

        if (xQueueSend(service_client_handle->recv_queue_handle, recv_buff_to_push,
            pdMS_TO_TICKS(RECEIVE_BUFF_QUEUE_WAIT_TIME_ms)) != pdTRUE)
        {
            AT_STACK_LOGE("Cannot push to client recv buff queue. Discard this data!");
            sys_mem_free(recv_buff_to_push);
            return -1;
        }
        sys_mem_free(recv_buff_to_push);
    }
    return 0;
}

static int  mqtt_client_configure_and_request_connect(int client_index,
    const char *client_id, const char *hostname, const char *username,
    const char *pass, uint16_t port, const char* ca_cert, 
    const char* client_cert, const char* client_priv_key)
{
    mqtt_service_client_t *service_client_handle = 
            &mqtt_service_clients_table[client_index];
    esp_mqtt_client_config_t client_config = 
    {
        .uri = hostname,
        .client_id = client_id,
        .port = port,
        .username = username,
        .password = pass,
        .protocol_ver = get_protocol_vsn_from_mqtt_service_config(client_index),
        .keepalive = get_keepalive_from_mqtt_service_config(client_index),
        .disable_clean_session = 
            get_disable_clean_session_from_mqtt_service_config(client_index),
        .disable_auto_reconnect = true
    };
#if CONFIG_SKIP_VERIFYING_BROKER
    AT_STACK_LOGD("This variant of firmware WILL ALLOW UNVERIFIED BROKER");
    client_config.cert_pem = NULL;
    client_config.crt_bundle_attach = NULL;
    client_config.psk_hint_key = NULL;
    client_config.use_global_ca_store = false;
#else
    if ((ca_cert == NULL))
    {
        AT_STACK_LOGD("Used bundled crts due to all input certs are NULL");
        client_config.crt_bundle_attach = esp_crt_bundle_attach;
    }
    else
    {
        AT_STACK_LOGD("Used input crts");
        client_config.cert_pem = ca_cert;
    }
#endif
    client_config.client_cert_pem = client_cert;
    client_config.client_key_pem = client_priv_key;

    esp_mqtt_client_handle_t client = 
        service_client_handle->esp_client_handle;
    esp_err_t err_from_esp_mqtt;
    err_from_esp_mqtt = esp_mqtt_set_config(client, &client_config);
    if (err_from_esp_mqtt != ESP_OK) 
    {
        AT_STACK_LOGE("Cannot set configuration for MQTT client before connecting");
        return -1;
    }
    
    err_from_esp_mqtt = esp_mqtt_client_start(client);
    if (err_from_esp_mqtt != ESP_OK) 
    {
        AT_STACK_LOGE("Cannot start MQTT");
        return -1;
    }
    AT_STACK_LOGD("Configure and connect done!");
    return 0;
}

static mqtt_service_pkt_status_t mqtt_client_get_connect_req_status_timeout(
    int client_index, uint32_t timeout_ms,
    esp_mqtt_connect_return_code_t *connect_ret_code)
{
    // It's safe to not lock client, since below codes only deal with 
    // event group, which by itself have mechanism to prevent race condition
    // (assume ESP-IDF doing it right :v )
    mqtt_service_pkt_status_t ret_status;

    uint32_t get_event_bit = wait_for_connect_req_status(client_index, 
        timeout_ms * 1000);

    switch (get_event_bit)
    {
        case MQTT_CONNECT_REQUEST_SUCCESS_BIT:
            ret_status = MQTT_SERVICE_PACKET_STATUS_OK;
            break;
        default:
            ret_status = MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND;
    }

    *connect_ret_code = 
        map_event_bit_to_connect_ret_code(get_event_bit);
    
    return ret_status;
}

static bool is_ion_broker(const char *hostname)
{
    if (memcmp("mqtts", hostname, strlen("mqtts")))
        return false;

    if (strstr(hostname, "ionmobility.net") == NULL)
        return false;

    return true;
}


static int get_crts_from_nvs(char** get_cacert, char** get_client_cert,
    char** get_client_key)
    
{
    size_t get_cacert_size, get_client_key_size, get_client_cert_size;
    nvs_handle_t handle;
    AT_STACK_LOGD("Get crts in namespace 'ssl_crts' in NVS");
    if (nvs_open("ssl_crts", NVS_READONLY, &handle) != ESP_OK)
    {
        AT_STACK_LOGE("Failed to open name space 'ssl_crts' in NVS");
        return -1;
    }
    MUST_GET_STRING_IN_NVS_OR_RETURN_ERR(*get_cacert, get_cacert_size, 
        handle, "root_ca_cert", "Failed to get ION CA certificate in NVS");
    MUST_GET_STRING_IN_NVS_OR_RETURN_ERR(*get_client_key, get_client_key_size, 
        handle, "esp_priv_key", "Failed to get client key in NVS");
    MUST_GET_STRING_IN_NVS_OR_RETURN_ERR(*get_client_cert, get_client_cert_size, 
        handle, "esp_cert", "Failed to get client certificate in NVS");

    nvs_close(handle);
    return 0;
}

static char * nvs_load_value_if_exist(nvs_handle_t handle, const char* key,
    size_t *value_size)
{
    // Try to get the size of the item
    if(nvs_get_str(handle, key, NULL, value_size) != ESP_OK){
        AT_STACK_LOGE("Failed to get size of NVS key '%s'", key);
        return NULL;
    }

    char* value = sys_mem_malloc(*value_size);
    if(nvs_get_str(handle, key, value, value_size) != ESP_OK){
        AT_STACK_LOGE("Failed to load value of NVS key '%s'", key);
        return NULL;
    }

    return value;
}

static esp_mqtt_protocol_ver_t get_protocol_vsn_from_mqtt_service_config(int client_index)
{
    mqtt_service_client_cfg_t target_mqtt_cfg = 
        mqtt_service_client_cfg_table[client_index];
    if (target_mqtt_cfg.mqtt_version == MQTT_VERSION_V3_1)
        return MQTT_PROTOCOL_V_3_1;

    return MQTT_PROTOCOL_V_3_1_1;
}

static int get_keepalive_from_mqtt_service_config(int client_index)
{
    return (int) mqtt_service_client_cfg_table[client_index].keepalive_time_s;
}

static bool get_disable_clean_session_from_mqtt_service_config(int client_index)
{
    mqtt_service_client_cfg_t target_mqtt_cfg = 
        mqtt_service_client_cfg_table[client_index];
    if (target_mqtt_cfg.session_type == SESSION_TYPE_STORE_AFTER_DISCONNECT)
        return true;

    return false;
}

static void mqtt_service_deinit_internal()
{
    for (int index = 0; index < MAX_NUM_OF_MQTT_CLIENT; index++)
    {
        mqtt_service_client_t *service_client_handle = 
            &mqtt_service_clients_table[index];
        if (service_client_handle->esp_client_handle)
            esp_mqtt_client_destroy(service_client_handle->esp_client_handle);
        if (service_client_handle->connect_req_event_group)
            vEventGroupDelete(service_client_handle->connect_req_event_group);
        if (service_client_handle->sub_req_event_group)
            vEventGroupDelete(service_client_handle->sub_req_event_group);
        if (service_client_handle->unsub_req_event_group)
            vEventGroupDelete(service_client_handle->unsub_req_event_group);
        if (service_client_handle->pub_req_event_group)
            vEventGroupDelete(service_client_handle->pub_req_event_group);
        if (service_client_handle->connect_status_event_group)
            vEventGroupDelete(service_client_handle->connect_status_event_group);
        if (service_client_handle->recv_queue_handle)
            vQueueDelete(service_client_handle->recv_queue_handle);
    }
}
