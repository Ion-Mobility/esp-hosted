#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RCV_HOST    SPI2_HOST

extern void spi_slave_pull_interupt_high(void);
extern void spi_slave_pull_interupt_low(void);
extern void spi_slave_init(void);
