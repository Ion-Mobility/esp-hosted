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
#include "at_cmd_quectel_helpers.h"
#include "at_cmd_parse_and_exec.h"
#include "common_helpers.h"
#include "mqtt_service.h"

#define AT_TASK_SLEEP_ms				 100

typedef struct {
	char* cmd;
	AT_BUFF_SIZE_T cmd_length;
	resp_send_callback_t resp_send_cb;
} at_cmd_handling_req_t;

static QueueHandle_t at_cmd_queue = NULL;
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


//==============================
// Public functions definition
//==============================
void init_at_cmd_app()
{
    at_cmd_queue = xQueueCreate(3, sizeof(at_cmd_handling_req_t));
    assert(xTaskCreate(at_handling_task , "at_handling_task" , 4096 , NULL , 23 , NULL) == pdTRUE);
	

	if (mqtt_service_init() != MQTT_SERVICE_STATUS_OK)
	{
		AT_STACK_LOGE("Something wrong when initialize MQTT service");
		return;
	}
	AT_STACK_LOGI("Done init command app");
	is_at_app_initialized = true;
}


int make_request_to_handle_at_cmd(const char *at_cmd, 
    resp_send_callback_t resp_send_cb)
{
    if (!is_at_app_initialized) 
	{
		AT_STACK_LOGE("AT command app is not initialized yet!");
		return 1;
	}
	if (resp_send_cb == NULL)
	{
		AT_STACK_LOGE("There is no callback for sending AT response! Cannot handle this AT command!");
		return 1;
	}
	AT_STACK_LOGI("New request to handle AT command '%s'",
		at_cmd);
	
	at_cmd_handling_req_t new_at_cmd_handling_req;
	new_at_cmd_handling_req.cmd = sys_mem_calloc(1, MAX_AT_CMD_LENGTH);
	memcpy(new_at_cmd_handling_req.cmd, at_cmd, strlen(at_cmd));
	new_at_cmd_handling_req.cmd_length = strlen(at_cmd);
	new_at_cmd_handling_req.resp_send_cb = resp_send_cb;

	xQueueSend(at_cmd_queue, &new_at_cmd_handling_req, portMAX_DELAY);
    return 0;
}

//===============================
// Private functions definition
//===============================
static void at_handling_task(void* pvParameters)
{
	while (1) {
		at_cmd_handling_req_t recv_at_cmd_handling_req;
		if (xQueueReceive(at_cmd_queue, &recv_at_cmd_handling_req, portMAX_DELAY))
		{
			AT_BUFF_SIZE_T sending_at_response_length;
			AT_STACK_LOGI(" Get a command '%s'!\n",recv_at_cmd_handling_req.cmd);
			if (recv_at_cmd_handling_req.resp_send_cb == NULL)
			{
				AT_STACK_LOGE(
					"No response send handler. AT command stack don't know how to send response!");
				goto done_handling_req;
			}
			char *sending_at_response = sys_mem_calloc(1, MAX_AT_RESP_LENGTH);
			sending_at_response_length = parse_and_exec_at_cmd(
				recv_at_cmd_handling_req.cmd, 
				strlen(recv_at_cmd_handling_req.cmd), sending_at_response);
			if (!sending_at_response_length)
			{
				AT_STACK_LOGE(
					"Invalid arguments when parse! Do not produce reponse!");
				goto discard_send_resp;
			}
			
			// If callback return 0, which means send SUCCESS, jumps to 
			// 'done_handling_req' immediately to avoid double-freeing 
			// response buffer because freeing response buffer is 
			// caller's responsibility now
			AT_STACK_LOGI("Got response '%s' that will be sent through callback!", 
				sending_at_response);
			if (!recv_at_cmd_handling_req.resp_send_cb(
				sending_at_response, sending_at_response_length))
			{
				goto done_handling_req;
			}
discard_send_resp:
			AT_STACK_LOGI("Caller cannot free response, so AT command app free itself");
			sys_mem_free(sending_at_response);
done_handling_req:
			AT_STACK_LOGI("Done handling AT command!");
			sys_mem_free(recv_at_cmd_handling_req.cmd);
		}
		sys_task_sleep_us(AT_TASK_SLEEP_ms*1000);
	}
}
