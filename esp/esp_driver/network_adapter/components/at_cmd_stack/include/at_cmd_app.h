#ifndef _AT_CMD_APP_H
#define _AT_CMD_APP_H

#include <stdint.h>

/**
 * @brief Initialize AT command handling apps
 * 
 */
extern void init_at_cmd_app();

/**
 * @brief This function forward AT command received from SPI Slave interface to
 * AT command handling task
 * 
 * @param at_cmd_buff buffer to AT command receive from HOST encoded in Quectel format [in]
 * @param cmd_buff_size size of AT command buffer, including <LF><CR> [in]
 */
extern int forward_to_at_cmd_task(uint8_t *at_cmd_buff, uint32_t cmd_buff_size);

#endif