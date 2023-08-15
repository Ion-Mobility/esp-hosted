#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"
#include "crypto.h"

#define CRYPTO_TAG              "CRYPTO"
#define KEY_LEN                 (32)
#define SESSION_ID_LEN          (32)
#define SIGNATURE_LEN           (64)
#define PAIRING_RESPONSE_LEN    (64)
#define MSG_AUTHN_CODE_LEN      (16)
#define MAX_PHONE               (3)

// key pair
typedef struct {
    uint8_t sk[KEY_LEN];
    uint8_t pk[KEY_LEN];
} keypair_t;

// phone
typedef struct {
    uint8_t identity_pk[KEY_LEN];           // long-term, one per device
    uint8_t pairing_pk[KEY_LEN];            // medium-term, pairing key (1 week expired)
    uint8_t ephemeral_pk[KEY_LEN];          // short-term, session key
    uint8_t derived_session_key[KEY_LEN];   // derived session key
} phone_t;

// bike
typedef struct {
    keypair_t identity;                 // long-term, one per device
    keypair_t pairing;                  // medium-term, pairing key (1 week expired)
} bike_t;

// server
typedef struct {
    uint8_t identity_pk[KEY_LEN];
} server_t;

// pair request
typedef struct {
    uint8_t phone_identity_pk[KEY_LEN];
    uint8_t phone_pairing_pk[KEY_LEN];
} pair_request_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];
    pair_request_content_t contents;
} pair_request_t;

// pair response
typedef struct {
    uint8_t phone_pairing_pk[KEY_LEN];
    uint8_t bike_pairing_pk[KEY_LEN];
} pair_response_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];
    pair_response_content_t contents;
} pair_response_t;

// session response
typedef struct {
    uint8_t ephemeral_pk[KEY_LEN];
    uint8_t session_id[SESSION_ID_LEN];
} session_response_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];
    uint8_t phone_derived_session_key[KEY_LEN];
    session_response_content_t contents;
} session_response_t;

// session request
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

// message lock
typedef struct {
    uint8_t *session_id;
    uint8_t *mac;
    uint8_t *aead_sk;           //Phone derived sesion key
    uint8_t *plaintext;
    size_t  plaintext_len;
    uint8_t *ciphertext;
    size_t  *ciphertext_len;
} msg_lock_t;

// message unlock
typedef struct {
    uint8_t *session_id;
    uint8_t *mac;
    uint8_t *aead_sk;           //Phone derived sesion key
    uint8_t *plaintext;
    size_t  *plaintext_len;
    uint8_t *ciphertext;
    size_t  ciphertext_len;
} msg_unlock_t;

static void xed25519_sign(uint8_t signature[SIGNATURE_LEN], uint8_t secret_key[KEY_LEN], uint8_t *message, size_t message_size);
static int xed25519_verify(uint8_t signature[SIGNATURE_LEN], uint8_t public_key[KEY_LEN], uint8_t *message, size_t message_size);
static void random_generator(uint8_t *out, size_t len);
static void generate_bike_pairing_key(void);
static void message_lock(msg_lock_t *msg_lock);
static void message_unlock(msg_unlock_t *msg_unlock);
static int pair(pair_request_t *pair_request, pair_response_t *pair_response);
static int session(session_request_t *session_request, session_response_t *session_response);

phone_t phone = {0};
bike_t bike = {0};      //todo: read from NVS
server_t server = {0};  //todo: read from NVS

void crypto_init(void) {
    // todo:
    // read bike info from NVS
    // nvs_read(bike, nvs_store, sizeof(bike_t);

    // todo:
    // read server info from NVS
    // nvs_read(server, nvs_store, sizeof(server_t);

    srand(time(NULL));
}

int session_request(uint8_t* request, size_t req_len, uint8_t* response, size_t *res_len)
{
    if (req_len != sizeof(session_request_t))
        return -1;

    session_request_t *session_request = (session_request_t*)request;
    int session_result = session(session_request, (session_response_t*)response);
    if (session_result != 0) {
        ESP_LOGE(CRYPTO_TAG, "Failed to establish a session"); 
    } else {
        ESP_LOGI(CRYPTO_TAG, "Successed to establish a session"); 
        *res_len = sizeof(session_response_t);
    }

    return session_result;

}

int pairing_request(uint8_t* request, size_t req_len, uint8_t* response, size_t *res_len)
{
    if (req_len != sizeof(pair_request_t))
        return -1;

    pair_request_t *pair_request = (pair_request_t*)request;
    int pair_result = pair(pair_request, (pair_response_t*)response);
    if (pair_result != 0) {
        ESP_LOGE(CRYPTO_TAG, "Failed to pair"); 
    } else {
        ESP_LOGI(CRYPTO_TAG, "Successed to pair"); 
        *res_len = sizeof(pair_response_t);
    }

    return pair_result;
}

static void message_lock(msg_lock_t *msg_lock)
{
    crypto_aead_lock(msg_lock->ciphertext, msg_lock->mac,
                 msg_lock->aead_sk, msg_lock->session_id,
                 NULL, 0,
                 msg_lock->plaintext, msg_lock->plaintext_len);
    *(msg_lock->ciphertext_len) = msg_lock->plaintext_len;
}

static void message_unlock(msg_unlock_t *msg_unlock)
{
    if (crypto_aead_unlock(msg_unlock->plaintext, msg_unlock->mac,
                        msg_unlock->aead_sk, msg_unlock->session_id,
                        NULL, 0,
                        msg_unlock->ciphertext, msg_unlock->ciphertext_len)) {
        ESP_LOGE(CRYPTO_TAG, "The message is corrupted");
    } else {
        ESP_LOGI(CRYPTO_TAG, "decrypted mes: ");
        *(msg_unlock->plaintext_len) = msg_unlock->ciphertext_len;
        ESP_LOG_BUFFER_CHAR(CRYPTO_TAG, msg_unlock->plaintext, *(msg_unlock->plaintext_len));
    }
}

void message_encrypt(uint8_t *ciphertext, size_t *ciphertext_len, uint8_t *mac, uint8_t *plaintext, size_t plaintext_len)
{
    msg_lock_t msg_lock;
    msg_lock.plaintext = plaintext;
    msg_lock.plaintext_len = plaintext_len;
    msg_lock.ciphertext = ciphertext;
    msg_lock.ciphertext_len = ciphertext_len;
    msg_lock.mac = mac;
    msg_lock.aead_sk = phone.derived_session_key;
    msg_lock.session_id = phone.ephemeral_pk;

    message_lock(&msg_lock);
}

void message_decrypt(uint8_t*plaintext, size_t *plaintext_len, uint8_t *mac, uint8_t *ciphertext, size_t ciphertext_len)
{
    msg_unlock_t msg_unlock;
    msg_unlock.plaintext = plaintext;
    msg_unlock.plaintext_len = plaintext_len;
    msg_unlock.ciphertext = ciphertext;
    msg_unlock.ciphertext_len = ciphertext_len;
    msg_unlock.mac = mac;
    msg_unlock.aead_sk = phone.derived_session_key;
    msg_unlock.session_id = phone.ephemeral_pk;

    message_unlock(&msg_unlock);
}

static void random_generator(uint8_t *out, size_t len) {
    for (int i=0; i<len; i++) {
        out[i] = rand();
    }
}

static void generate_bike_pairing_key(void) {
    // bike pairing key pair, generate for new phone pairing
    random_generator(bike.pairing.sk, KEY_LEN);
    crypto_x25519_public_key(bike.pairing.pk, bike.pairing.sk);
}

static int pair(pair_request_t *pair_request, pair_response_t *pair_response) {
    // Verify that the message was actually signed by the server
    int verify_server_signature = crypto_eddsa_check(pair_request->signature, server.identity_pk, (uint8_t*)&pair_request->contents, sizeof(pair_request_content_t));
    if (verify_server_signature != 0) {
        ESP_LOGE(CRYPTO_TAG, "failed to verify server signature");
        return verify_server_signature;
    }

    // Generate a new bike pairing key for this phone pairing key and
    generate_bike_pairing_key();

    // todo:add it to the list of phone paring keys.
    // store to NVS pairing list

    // Create the response and sign it
    memcpy(pair_response->contents.phone_pairing_pk, phone.pairing_pk, KEY_LEN);
    memcpy(pair_response->contents.bike_pairing_pk, bike.pairing.pk, KEY_LEN);
    xed25519_sign(pair_response->signature, bike.pairing.sk, (uint8_t*)&pair_response->contents, sizeof(pair_request_content_t));

    return 0;
}

static int session(session_request_t *session_request, session_response_t *session_response) {
    int verify = xed25519_verify(session_request->signature, phone.identity_pk, (uint8_t*)&session_request->contents, sizeof(session_request_content_t));
    if (!verify) {
        ESP_LOGI(CRYPTO_TAG, "session signature is correct");
    } else {
        ESP_LOGE(CRYPTO_TAG, "session signature is incorrect: %d...exit...:(", verify);
        return -1;
    }

    uint8_t dh1[KEY_LEN];
    crypto_x25519(dh1, bike.identity.sk, phone.identity_pk);

    uint8_t dh2[KEY_LEN];
    crypto_x25519(dh2, bike.identity.sk, phone.pairing_pk);

    uint8_t dh3[KEY_LEN];
    crypto_x25519(dh3, bike.identity.sk, phone.ephemeral_pk);

    uint8_t dh4[KEY_LEN];
    crypto_x25519(dh4, bike.pairing.sk, phone.ephemeral_pk);

    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, KEY_LEN);
    crypto_blake2b_update(&ctx, dh1, KEY_LEN);
    crypto_blake2b_update(&ctx, dh2, KEY_LEN);
    crypto_blake2b_update(&ctx, dh3, KEY_LEN);
    crypto_blake2b_update(&ctx, dh4, KEY_LEN);
    crypto_blake2b_final(&ctx, session_response->phone_derived_session_key);

    random_generator(session_response->contents.session_id, KEY_LEN);
    memcpy(session_response->contents.ephemeral_pk, phone.ephemeral_pk, KEY_LEN);

    uint8_t random[SIGNATURE_LEN];
    random_generator(random, SIGNATURE_LEN);
    xed25519_sign(session_response->signature,
                   bike.identity.sk,
                   (uint8_t*)&session_response->contents, sizeof(session_response_content_t));
    memcpy(phone.derived_session_key, session_response->phone_derived_session_key, KEY_LEN);
    return 0;
}

static void xed25519_sign(uint8_t signature[SIGNATURE_LEN],
                   uint8_t secret_key[KEY_LEN],
                   uint8_t *message,
                   size_t message_size)
{

    static const uint8_t zero   [KEY_LEN] = {0};
	static const uint8_t minus_1[KEY_LEN] = {
		0xec, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
		0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
	};
	static const uint8_t prefix[KEY_LEN] = {
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	};

	/* Key pair (a, A) */
	uint8_t A[KEY_LEN];  /* XEdDSA public key  */
	uint8_t a[KEY_LEN];  /* XEdDSA private key */
	crypto_eddsa_trim_scalar(a, secret_key);
	crypto_eddsa_scalarbase(A, a);
	int is_negative = A[31] & 0x80; /* Retrieve sign bit */
	A[31] &= 0x7f;                  /* Clear sign bit    */
	if (is_negative) {
		/* a = -a */
		crypto_eddsa_mul_add(a, a, minus_1, zero);
	}

	/* Secret nonce r */
    uint8_t random[SIGNATURE_LEN];
    random_generator(random, SIGNATURE_LEN);
	uint8_t r[SIGNATURE_LEN];
	crypto_sha512_ctx ctx;
	crypto_sha512_init  (&ctx);
	crypto_sha512_update(&ctx, prefix , KEY_LEN);
	crypto_sha512_update(&ctx, a      , KEY_LEN);
	crypto_sha512_update(&ctx, message, message_size);
	crypto_sha512_update(&ctx, random , SIGNATURE_LEN);
	crypto_sha512_final (&ctx, r);
	crypto_eddsa_reduce(r, r);

	/* First half of the signature R */
	uint8_t R[KEY_LEN];
	crypto_eddsa_scalarbase(R, r);

	/* hash(R || A || M) */
	uint8_t H[SIGNATURE_LEN];
	crypto_sha512_init  (&ctx);
	crypto_sha512_update(&ctx, R      , KEY_LEN);
	crypto_sha512_update(&ctx, A      , KEY_LEN);
	crypto_sha512_update(&ctx, message, message_size);
	crypto_sha512_final (&ctx, H);
	crypto_eddsa_reduce(H, H);

	/* Signature */
	memcpy(signature, R, KEY_LEN);
	crypto_eddsa_mul_add(signature + KEY_LEN, a, H, r);

	/* Wipe secrets (A, R, and H are not secret) */
	crypto_wipe(a, KEY_LEN);
	crypto_wipe(r, KEY_LEN);
}

static int xed25519_verify(uint8_t signature[SIGNATURE_LEN],
                    uint8_t public_key[KEY_LEN],
                    uint8_t *message,
                    size_t message_size)
{
	/* Convert X25519 key to EdDSA */
	uint8_t A[KEY_LEN];
	crypto_x25519_to_eddsa(A, public_key);

	/* hash(R || A || M) */
	uint8_t H[SIGNATURE_LEN];
	crypto_sha512_ctx ctx;
	crypto_sha512_init  (&ctx);
	crypto_sha512_update(&ctx, signature, KEY_LEN);
	crypto_sha512_update(&ctx, A        , KEY_LEN);
	crypto_sha512_update(&ctx, message  , message_size);
	crypto_sha512_final (&ctx, H);
	crypto_eddsa_reduce(H, H);

	/* Check signature */
	return crypto_eddsa_check_equation(signature, A, H);
}