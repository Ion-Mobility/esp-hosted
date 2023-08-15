#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_LEN                 (32)
#define SESSION_ID_LEN          (32)
#define SIGNATURE_LEN           (64)
#define PAIRING_RESPONSE_LEN    (64)
#define MSG_AUTHN_CODE_LEN      (16)
#define MAX_PHONE               (3)

//------------------pair request---------------------
typedef struct {
    uint8_t phone_identity_pk[KEY_LEN];
    uint8_t phone_pairing_pk[KEY_LEN];
} pair_request_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];
    pair_request_content_t contents;
} pair_request_t;
// --------------------------------------------------


//------------------pair response--------------------
typedef struct {
    uint8_t phone_pairing_pk[KEY_LEN];
    uint8_t bike_pairing_pk[KEY_LEN];
} pair_response_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];
    pair_response_content_t contents;
} pair_response_t;
// --------------------------------------------------


//------------------session response-----------------
typedef struct {
    uint8_t ephemeral_pk[KEY_LEN];
    uint8_t session_id[SESSION_ID_LEN];
} session_response_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];
    uint8_t phone_derived_session_key[KEY_LEN];
    session_response_content_t contents;
} session_response_t;
// --------------------------------------------------


//------------------session request------------------
typedef struct {
    uint8_t phone_pairing_pk[KEY_LEN];
    uint8_t bike_pairing_pk[KEY_LEN];
    uint8_t phone_identity_pk[KEY_LEN];
    uint8_t ephemeral_pk[KEY_LEN];
} session_request_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];
    session_request_content_t contents;
} session_request_t;
// --------------------------------------------------


//------------------message lock/unlock--------------
typedef struct {
    uint8_t session_id[SESSION_ID_LEN];
    uint8_t mac[MSG_AUTHN_CODE_LEN];
    uint8_t aead_sk[KEY_LEN];           //Phone derived sesion key
    uint8_t *plaintext;
    size_t  plaintext_len;
    uint8_t *ciphertext;
    size_t  *ciphertext_len;
} msg_lock_t;

typedef struct {
    uint8_t session_id[SESSION_ID_LEN];
    uint8_t mac[MSG_AUTHN_CODE_LEN];
    uint8_t aead_sk[KEY_LEN];           //Phone derived sesion key
    uint8_t *plaintext;
    size_t  *plaintext_len;
    uint8_t *ciphertext;
    size_t  ciphertext_len;
} msg_unlock_t;
// --------------------------------------------------

int pair(pair_request_t *pair_request, pair_response_t *pair_response);
int session(session_request_t *session_request, session_response_t *session_response);
void message_lock(msg_lock_t *msg_lock);
void message_unlock(msg_unlock_t *msg_unlock);