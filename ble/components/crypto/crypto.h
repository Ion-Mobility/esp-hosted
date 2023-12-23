#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DEBUG_CRYPTO            (0)
#define DEBUG_BLE               (1)
#define DEBUG_TM                (1)
#define TEST_COMMAND            (0)
#define ENABLE_PAIR_TIMEOUT     (0)
#define IGNORE_PAIRING          (0)

extern esp_err_t crypto_init(void);
extern int session_request(uint8_t* request, size_t req_len, uint8_t* response, size_t *res_len);               //establish session
extern int pairing_request(uint8_t* request, size_t req_len, uint8_t* response, size_t *res_len);               //request to pair
extern void message_encrypt(uint8_t *response, size_t *res_len, uint8_t *plaintext, size_t plaintext_len);      //encrypt message
extern int message_decrypt(uint8_t *plaintext, size_t *plaintext_len, uint8_t *request, size_t request_len);    //decrypt message
extern void client_disconnect(void);
// to verify the unity test framework only, remove later
extern int crypto_add2num(int a, int b);