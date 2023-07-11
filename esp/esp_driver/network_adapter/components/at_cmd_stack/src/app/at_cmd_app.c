#include "at_cmd_app.h"
#include "at_cmd_stack_types.h"
#include "interface.h"
#include "adapter.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <string.h>
#include <stdbool.h>
#include "sys_common_funcs.h"
#include "at_cmd_app_helper.h"
#include "at_cmd_parse_and_exec.h"
#include "common_helpers.h"
#include "mqtt_service.h"
#include "at_cmd_stack_event.h"

#define AT_TASK_SLEEP_ms				 100

typedef struct {
	char* cmd;
	AT_BUFF_SIZE_T cmd_length;
} at_command_t;

static QueueHandle_t at_cmd_queue = NULL;
static char at_response[MAX_AT_RESP_LENGTH];
static bool is_at_app_initialized = false;

//================================
// Private functions declaration
//================================
/**
 * @brief Perform AT handling in ESP32
 * 
 * @param pvParameters unused argument
 */
static void at_handling_task(void* pvParameters);

static void prepare_at_resp_buff_handle(char* at_resp, 
    AT_BUFF_SIZE_T at_resp_length, interface_buffer_handle_t *buff_handle);



//==============================
// Public functions definition
//==============================
void init_at_cmd_app()
{
    at_cmd_queue = xQueueCreate(3, sizeof(at_command_t));
    assert(xTaskCreate(at_handling_task , "at_handling_task" , 4096 , NULL , 22 , NULL) == pdTRUE);
	

	if (mqtt_service_init() != MQTT_SERVICE_STATUS_OK)
	{
		AT_STACK_LOGE("Something wrong when initialize MQTT service");
		return;
	}
	AT_STACK_LOGI("Done init command app");
	is_at_app_initialized = true;
}



int forward_to_at_cmd_task(uint8_t *at_cmd_buff, uint32_t cmd_buff_size)
{
    if (!is_at_app_initialized) 
	{
		AT_STACK_LOGE("AT command app is not initialized yet!");
		return 1;
	}
	at_command_t at_command_to_forward;
	at_command_to_forward.cmd = sys_mem_malloc(MAX_AT_CMD_LENGTH);
	at_command_to_forward.cmd_length = AT_QuecTelString_To_NormalString(
		(char*)at_cmd_buff, cmd_buff_size, at_command_to_forward.cmd);
	
	AT_STACK_LOGI("Receiving new AT command '%s' len %d\n",
		at_command_to_forward.cmd, cmd_buff_size);

	xQueueSend(at_cmd_queue, &at_command_to_forward, portMAX_DELAY);
    return 0;
}


//===============================
// Private functions definition
//===============================
static void at_handling_task(void* pvParameters)
{
	at_command_t recv_at_cmd_to_handle;
	interface_buffer_handle_t to_host_buff_handle;
	char *sending_at_response = NULL;
	AT_BUFF_SIZE_T at_response_length, sending_at_response_length;
	while (1) {
		if (xQueueReceive(at_cmd_queue, &recv_at_cmd_to_handle, portMAX_DELAY))
		{
			AT_STACK_LOGI(" Get a command '%s'!\n",recv_at_cmd_to_handle.cmd);
			memset(at_response, 0, MAX_AT_RESP_LENGTH);
			at_response_length = parse_and_exec_at_cmd(recv_at_cmd_to_handle.cmd, strlen(recv_at_cmd_to_handle.cmd), at_response);
			sys_mem_free(recv_at_cmd_to_handle.cmd);
			if (!at_response_length)
			{
				AT_STACK_LOGE(
					"Invalid arguments when parse! Do not produce reponse!");
				continue;
			}
			sending_at_response = sys_mem_malloc(MAX_AT_RESP_LENGTH);
			sending_at_response_length = AT_NormalString_To_QuecTelString(
				at_response, at_response_length, sending_at_response);
			if (!sending_at_response_length)
			{
				AT_STACK_LOGE(
					"Something wrong to args when convert normal to Quectel");
				sys_mem_free(sending_at_response);
				continue;
			}
			prepare_at_resp_buff_handle(sending_at_response, 
				sending_at_response_length, &to_host_buff_handle);
			if (send_to_host_queue(&to_host_buff_handle, PRIO_Q_OTHERS) != ESP_OK) 
			{
				// Send to host task will not free this buffer by SPI TX task,
				// so we must free ourselves
				sys_mem_free(sending_at_response);
			}
			else
			{
				AT_STACK_LOGI(" Send response to host successfully!\n");
			}
		}
		sys_task_sleep_us(AT_TASK_SLEEP_ms*1000);
	}
}

static void prepare_at_resp_buff_handle(char* at_resp, AT_BUFF_SIZE_T at_resp_length, interface_buffer_handle_t *buff_handle)
{
	buff_handle->if_type = ESP_AT_IF;
	buff_handle->if_num = 0x2;
	buff_handle->priv_buffer_handle = at_resp;
	buff_handle->free_buf_handle = free;
	buff_handle->payload = (uint8_t*)at_resp;
	buff_handle->payload_len = at_resp_length;
}