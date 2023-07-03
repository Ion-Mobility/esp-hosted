#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"

#include "tm_spi_slave_driver.h"
#include "tm_atcmd.h"

#define ATCMD_TASK_PRIO     3
#define ATCMD_CMD_LEN       128
#define ION_TM_ATCMD_TAG    "TM_ATCMD"

QueueHandle_t atcmd_to_handler_queue;
QueueHandle_t handler_to_atcmd_queue;

static esp_err_t tm_atcmd_recv(char* cmd);
static esp_err_t tm_atcmd_process(char* cmd, char* res);
static esp_err_t tm_atcmd_resp(esp_err_t status, char* res);

static void tm_atcmd_task(void *arg)
{
    esp_err_t ret = ESP_OK;
    WORD_ALIGNED_ATTR char recvbuf[ATCMD_CMD_LEN+1]="";
    WORD_ALIGNED_ATTR char respbuf[ATCMD_CMD_LEN+1]="";

    while(1) {
        memset(recvbuf, 0, sizeof(recvbuf));
        ret = tm_atcmd_recv(recvbuf);
        if (ret != ESP_OK)
            continue;

        memset(respbuf, 0, sizeof(respbuf));
        ret = tm_atcmd_process(recvbuf, respbuf);
        if (ret != ESP_OK)
            continue;

        ret = tm_atcmd_resp(ret, respbuf);
        if (ret != ESP_OK)
            continue;
    }
}

void tm_atcmd_tasks_init(void)
{
    spi_slave_init();

    //Create and start stats task
    xTaskCreatePinnedToCore(tm_atcmd_task, "tm_atcmd_task", 4096, NULL, ATCMD_TASK_PRIO, NULL, tskNO_AFFINITY);

}

static esp_err_t tm_atcmd_recv(char* cmd) {
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=ATCMD_CMD_LEN*8;
    t.tx_buffer=NULL;
    t.rx_buffer=cmd;
    return spi_slave_transmit(RCV_HOST, &t, portMAX_DELAY);
}

static esp_err_t tm_atcmd_process(char* cmd, char* res) {
    ESP_LOGI(ION_TM_ATCMD_TAG, "cmd_data:%s", cmd);
    // process each command here, timeout is 2s
    vTaskDelay(pdMS_TO_TICKS(1000*2));
    snprintf(res, ATCMD_CMD_LEN+1, "ion");
    return ESP_OK;
}

static esp_err_t tm_atcmd_resp(esp_err_t status, char* res) {
    spi_slave_transaction_t t;
    WORD_ALIGNED_ATTR char sendbuf[ATCMD_CMD_LEN+1]="";
    memset(&t, 0, sizeof(t));
    t.length=ATCMD_CMD_LEN*8;
    t.tx_buffer=sendbuf;
    t.rx_buffer=NULL;
    memset(sendbuf, 0, sizeof(sendbuf));

    if (status == ESP_OK) {
        snprintf(sendbuf, sizeof(sendbuf), "OK+%s\r\n",res);
    } else {
        snprintf(sendbuf, sizeof(sendbuf), "ERROR+%s\r\n",res);
    }

    return spi_slave_transmit(RCV_HOST, &t, portMAX_DELAY);
}