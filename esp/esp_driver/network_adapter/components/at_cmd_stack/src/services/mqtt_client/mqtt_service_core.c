#include "mqtt_service.h"
#include "mqtt_client.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "esp_log.h"
#include "common_helpers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "sys_common_funcs.h"

#define CLEAR_RECV_BUFF_GROUP(buff_group_ptr) do {\
    memset((*buff_group_ptr).buff, 0, sizeof(recv_buffer_t) * MAX_NUM_OF_RECV_BUFFER); \
    (*buff_group_ptr).current_empty_buff_index = 0; \
} while (0)

static EventGroupHandle_t mqtt_connect_req_eventgroup[MAX_NUM_OF_MQTT_CLIENT], 
    mqtt_connect_status_eventgroup[MAX_NUM_OF_MQTT_CLIENT];


static recv_buffer_group_t local_recv_buffer_group[MAX_NUM_OF_MQTT_CLIENT] =
{
    { // Initialize local buffer group for client #0 and buff[0] only to 
      // make variable `local_recv_buffer_group` locate in 
      // .data section. Must initialize later!
        .buff[0] = 
        {
            .is_unread = true,
            .topic = NULL,
            .topic_len = 0,
            .msg = NULL,
            .msg_len = 0,
        },
        .current_empty_buff_index = 0,
    },
};

static bool is_mqtt_service_initialized = false;

#define DEFAULT_PACKET_TIMEOUT_s 10

#define FROM_SEC_TO_MSEC(sec_value) sec_value * 1000
#define MQTT_CONNECT_REQUEST_SUCCESS_BIT BIT0
#define MQTT_CONNECT_REQUEST_FAILED_PROTOCOL_BIT BIT1
#define MQTT_CONNECT_REQUEST_FAILED_IDENTIFIER_BIT BIT2
#define MQTT_CONNECT_REQUEST_FAILED_SERVER_BIT BIT3
#define MQTT_CONNECT_REQUEST_FAILED_USERNAME_PASSWORD_BIT BIT4
#define MQTT_CONNECT_REQUEST_FAILED_AUTHORIZE_BIT BIT5

#define MQTT_CONNECT_STATUS_CONNECTED_BIT BIT0
#define MQTT_CONNECT_STATUS_NOT_CONNECTED_BIT BIT1

static const ip_addr_t default_dns_server[] =
    {
        IPADDR4_INIT_BYTES(1, 1, 1, 1), // Cloudflare DNS server
        IPADDR4_INIT_BYTES(8, 8, 8, 8), // Google DNS server
        IPADDR4_INIT_BYTES(8, 8, 4, 4), // Google DNS server
    };

static esp_mqtt_client_handle_t mqtt_service_clients_handle_table
    [MAX_NUM_OF_MQTT_CLIENT];

//================================
// Private functions declaration
//================================

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
    int32_t event_id, void *event_data);

static esp_err_t mqtt_client_restart(esp_mqtt_client_handle_t client, 
    const esp_mqtt_client_config_t *client_config);

static uint32_t wait_for_connect_req_status(int client_index, uint32_t wait_ms);

static uint32_t map_connect_ret_code_to_event_bits(
        esp_mqtt_connect_return_code_t connect_ret_code);
static esp_mqtt_connect_return_code_t map_event_bit_to_connect_ret_code(
        uint32_t event_bit);
static void announce_connect_request_success(int client_index);
static void announce_connect_request_fail(int client_index,
    esp_mqtt_connect_return_code_t connect_req_ret_code);
static void change_connection_status(int client_index, 
    mqtt_client_connection_status_t status_to_change);

static int copy_new_sub_data_to_local_recv_buff(int client_idx, 
    const char *topic, uint16_t topic_len, const char *msg, uint32_t msg_len);

static void print_out_unread_buffer(int client_idx);

/**
 * @brief Find the client index of givent MQTT client handle
 * 
 * @param client 
 * @retval index of client
 * @retval -1 if index not found
 */
static int find_index_from_client_handle(esp_mqtt_client_handle_t client);


//===============================
// Public functions definition
//===============================
mqtt_service_status_t mqtt_service_init()
{
    for (int index = 0; 
        index < sizeof(default_dns_server) / sizeof(default_dns_server[0]);
        index++)
    {
        dns_setserver(index, &default_dns_server[index]);
    }

    for (int index = 0; index < MAX_NUM_OF_MQTT_CLIENT; index++)
    {
        // Dummy config only for initialization. Later on MQTT client will
        // be configured again when perform connecting, publishing,
        // subscribing
        esp_mqtt_client_config_t dummy_config ={0};
        mqtt_service_clients_handle_table[index] = 
            esp_mqtt_client_init(&dummy_config);
        esp_mqtt_client_register_event(mqtt_service_clients_handle_table[index],
            ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        mqtt_connect_req_eventgroup[index] = xEventGroupCreate();
        mqtt_connect_status_eventgroup[index] = xEventGroupCreate();
        
        // must initialize connection status to not connected
        change_connection_status(index, MQTT_CONNECTION_STATUS_NOT_CONNECTED);
        CLEAR_RECV_BUFF_GROUP(&local_recv_buffer_group[index]);
    }

    is_mqtt_service_initialized = true;
    return MQTT_SERVICE_STATUS_OK;
}

mqtt_client_connection_status_t mqtt_service_get_connection_status(
    int client_index)
{
    uint32_t connect_status_bit = xEventGroupGetBits(
        mqtt_connect_status_eventgroup[client_index]
    );
    if (connect_status_bit == MQTT_CONNECT_STATUS_CONNECTED_BIT)
    {
        return MQTT_CONNECTION_STATUS_CONNECTED;
    }
    return MQTT_CONNECTION_STATUS_NOT_CONNECTED;

}

mqtt_service_pkt_status_t mqtt_service_connect(int client_index,
    const char *client_id, const char *hostname, const char *username,
    const char *pass, uint16_t port,
    esp_mqtt_connect_return_code_t *connect_ret_code)
{
    mqtt_service_pkt_status_t ret_status;
    esp_mqtt_client_config_t client_config = 
    {
        .uri = hostname,
        .client_id = client_id,
        .port = port,
        .username = username,
        .password = pass
    };

    esp_mqtt_client_handle_t client = 
        mqtt_service_clients_handle_table[client_index];
    
    mqtt_client_restart(client, &client_config);
    change_connection_status(client_index, 
        MQTT_CONNECTION_STATUS_NOT_CONNECTED);

    uint32_t get_event_bit = wait_for_connect_req_status(client_index, 
        DEFAULT_PACKET_TIMEOUT_s * 1000);

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

mqtt_service_pkt_status_t mqtt_service_subscribe(int client_index,
    const char *topic, mqtt_qos_t qos)
{
    uint32_t connect_status_bit = xEventGroupGetBits(
        mqtt_connect_status_eventgroup[client_index]);
    bool is_broker_connected = 
        (connect_status_bit == MQTT_CONNECT_STATUS_CONNECTED_BIT);
    MUST_BE_CORRECT_OR_EXIT(is_broker_connected, 
        MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND);

    esp_mqtt_client_handle_t client = 
        mqtt_service_clients_handle_table[client_index];
    if (esp_mqtt_client_subscribe(client, topic, (int) qos) == -1)
        return MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND;
    
    return MQTT_SERVICE_PACKET_STATUS_OK;
}

mqtt_service_pkt_status_t mqtt_service_publish(int client_index,
    const char *topic, const char* msg, mqtt_qos_t qos,
    bool is_retain)
{
    uint32_t connect_status_bit = xEventGroupGetBits(
        mqtt_connect_status_eventgroup[client_index]);
    bool is_broker_connected = 
        (connect_status_bit == MQTT_CONNECT_STATUS_CONNECTED_BIT);
    MUST_BE_CORRECT_OR_EXIT(is_broker_connected, 
        MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND);

    esp_mqtt_client_handle_t client = 
        mqtt_service_clients_handle_table[client_index];
    if (esp_mqtt_client_publish(client, topic, msg, strlen(msg), qos, 
        is_retain) == -1)
        return MQTT_SERVICE_PACKET_STATUS_FAILED_TO_SEND;
    
    return MQTT_SERVICE_PACKET_STATUS_OK;
}

mqtt_service_status_t mqtt_service_get_recv_buff_group_status(
    int client_index, recv_buffer_group_t *out_recv_buff_group)
{
    MUST_BE_CORRECT_OR_EXIT(is_mqtt_service_initialized, 
        MQTT_SERVICE_STATUS_ERROR);
    mqtt_client_connection_status_t connection_status = 
        mqtt_service_get_connection_status(client_index);
    bool is_client_connected = 
        (connection_status == MQTT_CONNECTION_STATUS_CONNECTED);
    MUST_BE_CORRECT_OR_EXIT(is_client_connected, 
        MQTT_SERVICE_STATUS_ERROR);

    for (int buff_index = 0; buff_index < MAX_NUM_OF_RECV_BUFFER; buff_index++)
    {
        out_recv_buff_group->buff[buff_index].is_unread = 
            local_recv_buffer_group[client_index].buff[buff_index].is_unread;
    }

    return MQTT_SERVICE_PACKET_STATUS_OK;
}

mqtt_service_status_t mqtt_service_get_recv_buff_group(
    int client_index, recv_buffer_group_t *out_recv_buff_group)
{
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
    memcpy(out_recv_buff_group, &local_recv_buffer_group[client_index], 
        sizeof(recv_buffer_group_t));

    return MQTT_SERVICE_PACKET_STATUS_OK;
}

mqtt_service_status_t mqtt_service_clear_recv_buff_group(
    int client_index)
{
    MUST_BE_CORRECT_OR_EXIT(is_mqtt_service_initialized, 
        MQTT_SERVICE_STATUS_ERROR);
    mqtt_client_connection_status_t connection_status = 
        mqtt_service_get_connection_status(client_index);
    bool is_client_connected = 
        (connection_status == MQTT_CONNECTION_STATUS_CONNECTED);
    MUST_BE_CORRECT_OR_EXIT(is_client_connected, 
        MQTT_SERVICE_STATUS_ERROR);

    DEEP_DEBUG("Before clear buffer group\n");
    print_out_unread_buffer(client_index);
    CLEAR_RECV_BUFF_GROUP(&local_recv_buffer_group[client_index]);
    DEEP_DEBUG("Before clear buffer group\n");
    print_out_unread_buffer(client_index);

    return MQTT_SERVICE_PACKET_STATUS_OK;
}

//===============================
// Private functions definition
//===============================
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int client_idx = find_index_from_client_handle(client);
    int result;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        DEEP_DEBUG("Client %d connected to broker!\n", client_idx);
        announce_connect_request_success(client_idx);
        break;
    case MQTT_EVENT_PUBLISHED:
        DEEP_DEBUG("Client %d published a data\n", client_idx);
        break;

    case MQTT_EVENT_DATA:
        DEEP_DEBUG("Client %d got a data\n", client_idx);
        result = copy_new_sub_data_to_local_recv_buff(client_idx, 
            event->topic, (uint16_t) event->topic_len, event->data, 
            (uint32_t)event->data_len);
        if (result)
        {
            DEEP_DEBUG("Recv buffer is full! Discard...\n");
        }
        break;

    case MQTT_EVENT_ERROR:
        DEEP_DEBUG("Client %d have error!\n", client_idx);
        if (event->error_handle->error_type == 
            MQTT_ERROR_TYPE_CONNECTION_REFUSED)
        {
            announce_connect_request_fail(client_idx, 
                event->error_handle->connect_return_code);
            DEEP_DEBUG("MQTT connection refuse, ret code = %d! \n",
                event->error_handle->connect_return_code);
        }
        break;

    default:
        DEEP_DEBUG("Other event id:%d\n", event->event_id);
        break;
    }
}

static esp_err_t mqtt_client_restart(esp_mqtt_client_handle_t client, 
    const esp_mqtt_client_config_t *client_config)
{
    esp_err_t ret_status;
    esp_mqtt_client_stop(client);

    ret_status = esp_mqtt_set_config(client, client_config);
    if (ret_status != ESP_OK) return ret_status;
    
    ret_status = esp_mqtt_client_start(client);
    return ret_status;
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
    xEventGroupClearBits(mqtt_connect_req_eventgroup[client_index], 
        total_bits_to_wait);
    xEventGroupClearBits(mqtt_connect_status_eventgroup[client_index], 
        MQTT_CONNECT_STATUS_CONNECTED_BIT);
    xEventGroupSetBits(mqtt_connect_status_eventgroup[client_index], 
        MQTT_CONNECT_STATUS_NOT_CONNECTED_BIT);
    return xEventGroupWaitBits(mqtt_connect_req_eventgroup[client_index],
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
    xEventGroupSetBits(mqtt_connect_req_eventgroup[client_index], 
        MQTT_CONNECT_REQUEST_SUCCESS_BIT);
    change_connection_status(client_index, MQTT_CONNECTION_STATUS_CONNECTED);
}

static void announce_connect_request_fail(int client_index, 
    esp_mqtt_connect_return_code_t connect_req_ret_code)
{
    uint32_t connect_req_bit = map_connect_ret_code_to_event_bits(
        connect_req_ret_code);
    xEventGroupSetBits(mqtt_connect_req_eventgroup[client_index], 
        connect_req_bit);
    change_connection_status(client_index, 
        MQTT_CONNECTION_STATUS_NOT_CONNECTED);
}

static int find_index_from_client_handle(esp_mqtt_client_handle_t client)
{
    for (int index = 0; index < MAX_NUM_OF_MQTT_CLIENT; index++)
    {
        if (client == mqtt_service_clients_handle_table[index])
            return index;
    }
    return -1;
}

static void change_connection_status(int client_index, 
    mqtt_client_connection_status_t status_to_change)
{
    switch (status_to_change)
    {
    case MQTT_CONNECTION_STATUS_CONNECTED:
        xEventGroupClearBits(mqtt_connect_status_eventgroup[client_index], 
            MQTT_CONNECT_STATUS_NOT_CONNECTED_BIT);
        xEventGroupSetBits(mqtt_connect_status_eventgroup[client_index], 
            MQTT_CONNECT_STATUS_CONNECTED_BIT); 
        break;
    
    case MQTT_CONNECTION_STATUS_NOT_CONNECTED:
        xEventGroupClearBits(mqtt_connect_status_eventgroup[client_index], 
            MQTT_CONNECT_STATUS_CONNECTED_BIT);
        xEventGroupSetBits(mqtt_connect_status_eventgroup[client_index], 
            MQTT_CONNECT_STATUS_NOT_CONNECTED_BIT); 
        break;
    }
}

static int copy_new_sub_data_to_local_recv_buff(int client_idx, 
    const char *topic, uint16_t topic_len, const char *msg, uint32_t msg_len)
{
    uint8_t *client_current_empty_recv_buff_index = 
        &local_recv_buffer_group[client_idx].current_empty_buff_index;
    if (*client_current_empty_recv_buff_index >= MAX_NUM_OF_RECV_BUFFER)
    {
        DEEP_DEBUG("client %d's current empty index = %d, mean recv buffer is full!\n", 
            client_idx, *client_current_empty_recv_buff_index);
        return -1;
    }
    recv_buffer_t *copy_recv_buff = 
        &local_recv_buffer_group[client_idx].
        buff[*client_current_empty_recv_buff_index];
    
    copy_recv_buff->topic = sys_mem_calloc(topic_len + 1, 1);
    memcpy(copy_recv_buff->topic, topic, topic_len);
    copy_recv_buff->topic_len = topic_len;

    copy_recv_buff->msg = sys_mem_calloc(msg_len + 1, 1);
    memcpy(copy_recv_buff->msg, msg, msg_len);
    copy_recv_buff->msg_len = msg_len;

    copy_recv_buff->is_unread = true;
    (*client_current_empty_recv_buff_index)++;
    return 0;
}

static void print_out_unread_buffer(int client_idx)
{
    recv_buffer_group_t *printing_recv_buff_group = 
        &local_recv_buffer_group[client_idx];
    for (int buff_index = 0; buff_index < MAX_NUM_OF_RECV_BUFFER; buff_index++)
    {
        recv_buffer_t printing_recv_buff = 
            printing_recv_buff_group->buff[buff_index];
        if (printing_recv_buff.is_unread)
        {
            DEEP_DEBUG("print_unread: client %d, buff index %d, topic is '%s', data is '%s'\n",
                client_idx, buff_index, 
                printing_recv_buff.topic, 
                printing_recv_buff.msg);
        }
        else
        {
            break;
        }
    }
    DEEP_DEBUG("print_unread: Done!\n");
}