// Copyright 2015-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __SLAVE_CONTROL__H__
#define __SLAVE_CONTROL__H__
#include <esp_err.h>
#define min(X, Y)               (((X) < (Y)) ? (X) : (Y))

#define SSID_LENGTH             32
#define PASSWORD_LENGTH         64
#define BSSID_LENGTH            19
#define TIMEOUT_IN_SEC          (1000 / portTICK_RATE_MS)
#define MAC_LEN                 6
#define VENDOR_OUI_BUF          3

typedef struct {
	uint8_t ssid[SSID_LENGTH];
	uint8_t pwd[PASSWORD_LENGTH];
	uint8_t bssid[BSSID_LENGTH];
	uint8_t chnl;
	uint8_t max_conn;
	int8_t rssi;
	bool ssid_hidden;
	wifi_auth_mode_t ecn;
	uint8_t bw;
	uint16_t count;
} credentials_t;

enum {
	CTRL_ERR_NOT_CONNECTED = 1,
	CTRL_ERR_NO_AP_FOUND,
	CTRL_ERR_INVALID_PASSWORD,
	CTRL_ERR_INVALID_ARGUMENT,
	CTRL_ERR_OUT_OF_RANGE,
	CTRL_ERR_MEMORY_FAILURE,
	CTRL_ERR_UNSUPPORTED_MSG,
	CTRL_ERR_INCORRECT_ARG,
	CTRL_ERR_PROTOBUF_ENCODE,
	CTRL_ERR_PROTOBUF_DECODE,
	CTRL_ERR_SET_ASYNC_CB,
	CTRL_ERR_TRANSPORT_SEND,
	CTRL_ERR_REQUEST_TIMEOUT,
	CTRL_ERR_REQ_IN_PROG,
	OUT_OF_RANGE
};

esp_err_t data_transfer_handler(uint32_t session_id,const uint8_t *inbuf,
		ssize_t inlen,uint8_t **outbuf, ssize_t *outlen, void *priv_data);
esp_err_t ctrl_notify_handler(uint32_t session_id,const uint8_t *inbuf,
		ssize_t inlen, uint8_t **outbuf, ssize_t *outlen, void *priv_data);
void send_event_to_host(int event_id);
void send_event_data_to_host(int event_id, uint8_t *data, int size);
void ctrl_espconnected_set(uint8_t status);
void ctrl_smartconnect_start(void);

#endif /*__SLAVE_CONTROL__H__*/
