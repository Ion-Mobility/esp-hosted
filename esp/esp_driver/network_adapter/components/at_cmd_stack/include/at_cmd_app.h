#ifndef _AT_CMD_APP_H
#define _AT_CMD_APP_H

#include <stdint.h>

/**
 * @brief The callback when AT command stack finish handling command and want 
 * to send response
 * 
 * @param sending_resp pointer to AT response to send. Caller is responsible
 * to free this pointer, either in the callback or after callback [in,out]
 * @param resp_len length of AT reponse to send [in]
 * 
 * @retval 0 if send response SUCCESS
 * @retval -1 if send response FAILED
 */
typedef int (*resp_send_callback_t) (void *sending_resp, 
    uint32_t resp_len);

/**
 * @brief Initialize AT command handling apps
 * 
 */
extern void init_at_cmd_app();

/**
 * @brief Make a request to process and handle an AT command
 * 
 * @param at_cmd the NULL-terminated AT command to be processed [in]
 * @param resp_send_cb Callback for sending AT response after done 
 * processing [in]
 * 
 * @retval 0 if make request SUCCESS
 * @retval -1 if otherwise
 */
extern int make_request_to_handle_at_cmd(const char *at_cmd, 
    resp_send_callback_t resp_send_cb);

#endif