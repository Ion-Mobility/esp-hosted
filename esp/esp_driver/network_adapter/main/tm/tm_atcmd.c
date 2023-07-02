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

QueueHandle_t atcmd_to_handler_queue;
QueueHandle_t handler_to_atcmd_queue;
static void tm_atcmd_process_cmd(void *arg);

static void tm_atcmd_task(void *arg)
{
    esp_err_t ret = ESP_OK;

    WORD_ALIGNED_ATTR char sendbuf[129]="";
    WORD_ALIGNED_ATTR char recvbuf[129]="";
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));

    while(1) {
        // waiting for command
        memset(recvbuf, 0, sizeof(recvbuf));
        memset(sendbuf, 0, sizeof(sendbuf));
        t.length=128*8;
        t.tx_buffer=sendbuf;
        t.rx_buffer=recvbuf;
        ret=spi_slave_transmit(RCV_HOST, &t, portMAX_DELAY);
        printf("cmd_len: %d\ncmd_data:%s", t.trans_len, recvbuf);

        tm_atcmd_process_cmd(NULL);

        t.length=128*8;
        t.tx_buffer=sendbuf;
        t.rx_buffer=recvbuf;
        memset(recvbuf, 0, sizeof(recvbuf));
        memset(sendbuf, 0, sizeof(sendbuf));
        snprintf(sendbuf, sizeof(sendbuf), "OK\r\n");
        ret=spi_slave_transmit(RCV_HOST, &t, portMAX_DELAY);
        printf("responsed to host\r\n");
    }
}

void tm_atcmd_tasks_init(void)
{
    spi_slave_init();

    //Create and start stats task
    xTaskCreatePinnedToCore(tm_atcmd_task, "tm_atcmd_task", 4096, NULL, ATCMD_TASK_PRIO, NULL, tskNO_AFFINITY);

}

static void tm_atcmd_process_cmd(void *arg) {
    printf("Processing command...\r\n");
    vTaskDelay(pdMS_TO_TICKS(1000*2));
}