#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int signature_eddsa_verification(uint8_t *signature,
                                        uint8_t *public_key,
                                        uint8_t *message,
                                        size_t message_size);

extern int xed25519_verify(uint8_t signature[64],
                           uint8_t public_key[32],
                           uint8_t *message,
                           size_t message_size);

extern void xed25519_sign(uint8_t signature[64],
                          uint8_t secret_key[32],
                          uint8_t random[64],
                          uint8_t *message,
                          size_t message_size);

extern int bike_session(uint8_t *session_id,
                        uint8_t *new_signature,
                        uint8_t *session_aead_secretkey,
                        uint8_t *session_response,
                        uint8_t signature[64],
                        uint8_t session_request[128],
                        int session_request_len,
                        uint8_t secret_key[32]);