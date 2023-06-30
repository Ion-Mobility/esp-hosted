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
#include "tm_spi_slave.h"
// #include "ble_gatts_server.h"

#define SPI_SLAVE_TASK_PRIO     3
#define SPI_SLAVE_TICKS         pdMS_TO_TICKS(1000)

QueueHandle_t ble_spi_queue;

static void spi_slave_task(void *arg)
{
    esp_err_t ret = ESP_OK;

    WORD_ALIGNED_ATTR char sendbuf[129]="";
    WORD_ALIGNED_ATTR char recvbuf[129]="";
    memset(recvbuf, 0, 33);
    spi_slave_transaction_t t;
    memset(&t, 0, sizeof(t));

    while(1) {
        //Clear receive buffer, set send buffer to something sane
        memset(recvbuf, 0, sizeof(recvbuf));
        memset(sendbuf, 0 , sizeof(sendbuf));
        // spi_slave_pull_interupt_low();
        snprintf(sendbuf, sizeof("phone request to pair\r\n"), "phone request to pair\r\n");

        //Set up a transaction of 128 bytes to send/receive
        t.length=128*8;
        t.tx_buffer=sendbuf;
        t.rx_buffer=recvbuf;
        /* This call enables the SPI slave interface to send/receive to the sendbuf and recvbuf. The transaction is
        initialized by the SPI master, however, so it will not actually happen until the master starts a hardware transaction
        by pulling CS low and pulsing the clock etc. In this specific example, we use the handshake line, pulled up by the
        .post_setup_cb callback that is called as soon as a transaction is ready, to let the master know it is free to transfer
        data.
        */
        ret=spi_slave_transmit(RCV_HOST, &t, portMAX_DELAY);

        //spi_slave_transmit does not return until the master has done a transmission, so by here we have sent our data and
        //received data from the master. Print it.
        // printf("Received: %s\n", recvbuf);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        // spi_slave_pull_interupt_low();

        memset(recvbuf, 0, sizeof(recvbuf));
        memset(sendbuf, 0, sizeof(sendbuf));
        sprintf(sendbuf, "123456\r\n");

        //Set up a transaction of 128 bytes to send/receive
        t.length=128*8;
        t.tx_buffer=sendbuf;
        t.rx_buffer=recvbuf;
        /* This call enables the SPI slave interface to send/receive to the sendbuf and recvbuf. The transaction is
        initialized by the SPI master, however, so it will not actually happen until the master starts a hardware transaction
        by pulling CS low and pulsing the clock etc. In this specific example, we use the handshake line, pulled up by the
        .post_setup_cb callback that is called as soon as a transaction is ready, to let the master know it is free to transfer
        data.
        */
        ret=spi_slave_transmit(RCV_HOST, &t, portMAX_DELAY);
        
        memset(recvbuf, 0, sizeof(recvbuf));
        memset(sendbuf, 0, sizeof(sendbuf));
        t.length=128*8;
        t.tx_buffer=sendbuf;
        t.rx_buffer=recvbuf;
        ret=spi_slave_transmit(RCV_HOST, &t, portMAX_DELAY);
        printf("Received: %s\n", recvbuf);
        vTaskDelay(500 / portTICK_PERIOD_MS);

    }
}

void spi_slave_task_init(void)
{
    spi_slave_init();

    //Create and start stats task
    xTaskCreatePinnedToCore(spi_slave_task, "spi_slave_task", 4096, NULL, SPI_SLAVE_TASK_PRIO, NULL, tskNO_AFFINITY);

}

void spi_slave_send_ble_connect_event(void){}
void spi_slave_display_6digits_pairing_number(void){}
void spi_slave_send_ble_disconnect_event(void){}