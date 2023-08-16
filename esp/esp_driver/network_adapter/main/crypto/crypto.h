#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern esp_err_t crypto_init(void);
extern int session_request(uint8_t* request, size_t req_len, uint8_t* response, size_t *res_len);       //establish session
extern int pairing_request(uint8_t* request, size_t req_len, uint8_t* response, size_t *res_len);       //request to pair
extern void message_encrypt(uint8_t *ciphertext, size_t *ciphertext_len, uint8_t *mac, uint8_t *plaintext, size_t plaintext_len);
extern void message_decrypt(uint8_t*plaintext, size_t *plaintext_len, uint8_t *mac, uint8_t *ciphertext, size_t ciphertext_len);