#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"
#include "crypto.h"

#define CRYPTO_TAG              "CRYPTO"
#define KEY_LEN                 (32)
#define SESSION_ID_LEN          (32)
#define SIGNATURE_LEN           (64)
#define PAIRING_RESPONSE_LEN    (64)
#define MAX_PAIRED_PHONE        (3)
#define MAC_LEN                 (16)    // Message Authentication Code
#define NONCE_LEN               (24)    // Use only once per key

#define STORAGE_NAMESPACE       "ble_keys"

// key pair
typedef struct {
    uint8_t sk[KEY_LEN];
    uint8_t pk[KEY_LEN];
} keypair_t;

typedef struct{
    uint8_t value[KEY_LEN];
    time_t start_time;
    time_t expiry_time;
} phone_pairing_key_t;

// phone
typedef struct {
    uint8_t identity_pk[KEY_LEN];               // long-term, one per device
    phone_pairing_key_t pairing_pk;             // medium-term, pairing key (1 week expired)
    uint8_t derivation_key[KEY_LEN];            // short-term, one per session
    uint8_t session_id[SESSION_ID_LEN];         // session id
} phone_t;

// bike
typedef struct {
    keypair_t identity_key;                                 // long-term, one per device
    keypair_t pairing_key;                                 // long-term, one per device
} bike_t;

// server
typedef struct {
    uint8_t identity_pk[KEY_LEN];
} server_t;

// pair request
typedef struct {
    uint8_t bike_identity_pk[KEY_LEN];
    uint8_t phone_identity_pk[KEY_LEN];
    uint8_t phone_pairing_pk[KEY_LEN];
    time_t start_time;
    time_t expiry_time;
} pair_request_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];   //pair request must be signed by server
    pair_request_content_t contents;
} pair_request_t;

// pair response
typedef struct {
    uint8_t phone_pairing_pk[KEY_LEN];
    uint8_t bike_pairing_pk[KEY_LEN];
} pair_response_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];   //pair response must be signed by bike secret identity key
    pair_response_content_t contents;
} pair_response_t;

// session response
typedef struct {
    uint8_t ephemeral_pk[KEY_LEN];      //phone ephemeral key
    uint8_t session_id[SESSION_ID_LEN];
} session_response_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];   //session response must be signed by bike secret identity key
    session_response_content_t contents;
} session_response_t;

// session request
typedef struct {
    uint8_t phone_pairing_pk[KEY_LEN];
    uint8_t bike_pairing_pk[KEY_LEN];
    uint8_t ephemeral_pk[KEY_LEN];      //phone ephemeral key
} session_request_content_t;

typedef struct {
    uint8_t signature[SIGNATURE_LEN];
    session_request_content_t contents;
} session_request_t;

static void xed25519_sign(uint8_t signature[SIGNATURE_LEN], uint8_t secret_key[KEY_LEN], uint8_t *message, size_t message_size);
static int xed25519_verify(uint8_t signature[SIGNATURE_LEN], uint8_t public_key[KEY_LEN], uint8_t *message, size_t message_size);
static void random_generator(uint8_t *out, size_t len);
static void message_lock(uint8_t *ciphertext, size_t *res_len, uint8_t mac[MAC_LEN], uint8_t nonce[NONCE_LEN], uint8_t *plaintext, size_t plaintext_len);
static int message_unlock(uint8_t *plaintext, size_t *plaintext_len, uint8_t mac[MAC_LEN], uint8_t nonce[NONCE_LEN], uint8_t *ciphertext, size_t ciphertext_len);
static int pair(pair_request_t *pair_request, pair_response_t *pair_response);
static int session(session_request_t *session_request, session_response_t *session_response);

bike_t bike                 = {0};
server_t server             = {0};
phone_t phone               = {0};
uint8_t empty_key[KEY_LEN]  = {0};

#if (IGNORE_PAIRING)
uint8_t derivation_key[KEY_LEN] = {0x85,0xd8,0x7a,0xe1,0x3e,0xdc,0x8d,0x61,0x34,0x06,0x63,0xa4,0xc9,0x27,0x9f,0x98,0xc3,0x3b,0xf4,0xb5,0x9f,0x45,0x01,0x6b,0x82,0x8d,0xc3,0x0e,0x0b,0x06,0x60,0x39};
uint8_t session_id[SESSION_ID_LEN] = {0x52,0x04,0x2a,0x89,0x2b,0xe5,0x28,0x61,0x89,0x9c,0xb5,0xb0,0x52,0x21,0x4f,0xf8,0x40,0xd4,0x5b,0x70,0x91,0x18,0xc7,0x12,0xd3,0xdb,0x5e,0x8f,0xc9,0x44,0xe1,0x1b};
#endif

esp_err_t crypto_init(void) {
    nvs_handle_t my_handle;
    esp_err_t err = ESP_OK;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(CRYPTO_TAG, "Failed to open NVS storage");
        goto init_exit;
    }

    // read bike identity key from NVS
    {
        size_t required_size = 0;  // value will default to 0, if not set yet in NVS
        err = nvs_get_blob(my_handle, "bike", NULL, &required_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(CRYPTO_TAG, "No bike's identity key founded in flash or data is corrupted");
            goto init_exit;
        }
        // Read previously saved blob if available
        if (required_size == sizeof(keypair_t)) {
            err = nvs_get_blob(my_handle, "bike", &bike.identity_key, &required_size);
            if (err != ESP_OK) {
                ESP_LOGE(CRYPTO_TAG, "Failed to read bike's identity key from flash...");
                goto init_exit;
            } else {
                ESP_LOGI(CRYPTO_TAG, "Bike Identity SK:");
                esp_log_buffer_hex(CRYPTO_TAG, bike.identity_key.sk, KEY_LEN);
                ESP_LOGI(CRYPTO_TAG, "Bike Identity PK:");
                esp_log_buffer_hex(CRYPTO_TAG, bike.identity_key.pk, KEY_LEN);
            }
        } else {
            ESP_LOGE(CRYPTO_TAG, "bike's identity key size is not correct, the stored key is corrupted");
            goto init_exit;
        }
    }

    // read server key from NVS
    {
        size_t required_size = 0;  // value will default to 0, if not set yet in NVS
        err = nvs_get_blob(my_handle, "server", NULL, &required_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(CRYPTO_TAG, "No server's identity key founded in flash or data is corrupted");
            goto init_exit;
        }
        // Read previously saved blob if available
        if (required_size == KEY_LEN) {
            err = nvs_get_blob(my_handle, "server", server.identity_pk, &required_size);
            if (err != ESP_OK) {
                ESP_LOGE(CRYPTO_TAG, "Failed to read server's identity key from flash...");
                goto init_exit;
            } else {
                ESP_LOGI(CRYPTO_TAG, "Server Identity PK:");
                esp_log_buffer_hex(CRYPTO_TAG, server.identity_pk, KEY_LEN);
            }
        } else {
            ESP_LOGE(CRYPTO_TAG, "server's identity key size is not correct, the stored key is corrupted");
            goto init_exit;
        }
    }

    // init random number generator module
    srand(time(NULL));

init_exit:
    // Close
    nvs_close(my_handle);

    return err;
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
    if (req_len != sizeof(pair_request_t)) {
        ESP_LOGE(CRYPTO_TAG, "pairing_request req_len %d",req_len);
        return -1;
    }

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

void client_disconnect(void)
{
    memset(&phone, 0, sizeof(phone));
    memset(&bike.pairing_key, 0, sizeof(bike.pairing_key));
}

static void message_lock(uint8_t *ciphertext, size_t *res_len, uint8_t mac[MAC_LEN], uint8_t nonce[NONCE_LEN], uint8_t *plaintext, size_t plaintext_len)
{
    crypto_aead_lock(ciphertext, mac,                                               //cipher_text, mac,
                     phone.derivation_key, nonce,                                   //key, nonce
                     phone.session_id, sizeof(phone.session_id),                    //ad, ad_size
                     plaintext, plaintext_len);                                     //plain_text, sizeof(plain_text)

    *res_len = MAC_LEN + NONCE_LEN + plaintext_len;
}

static int message_unlock(uint8_t *plaintext, size_t *plaintext_len, uint8_t mac[MAC_LEN], uint8_t nonce[NONCE_LEN], uint8_t *ciphertext, size_t ciphertext_len)
{
    int ret = crypto_aead_unlock(plaintext, mac,                                    //plain_text, mac,
                                phone.derivation_key, nonce,                        //key, nonce
                                phone.session_id, sizeof(phone.session_id),         //ad, ad_size
                                ciphertext, ciphertext_len);                        //cipher_text, sizeof(cipher_text)
#if (DEBUG_CRYPTO)
    ESP_LOGE(CRYPTO_TAG, "mac:");
    esp_log_buffer_hex(CRYPTO_TAG, mac, MAC_LEN);
    ESP_LOGE(CRYPTO_TAG, "phone.derivation_key:");
    esp_log_buffer_hex(CRYPTO_TAG, phone.derivation_key, KEY_LEN);
    ESP_LOGE(CRYPTO_TAG, "nonce:");
    esp_log_buffer_hex(CRYPTO_TAG, nonce, NONCE_LEN);
    ESP_LOGE(CRYPTO_TAG, "phone.session_id,:");
    esp_log_buffer_hex(CRYPTO_TAG, phone.session_id,SESSION_ID_LEN);
    ESP_LOGE(CRYPTO_TAG, "ciphertext_len: %zu", ciphertext_len);
    ESP_LOGE(CRYPTO_TAG, "ciphertext:");
    esp_log_buffer_hex(CRYPTO_TAG, ciphertext, ciphertext_len);
#endif

    if (ret != 0) {
        ESP_LOGE(CRYPTO_TAG, "The message is corrupted");
    } else {
        ESP_LOGI(CRYPTO_TAG, "decrypted mes: ");
        *plaintext_len = ciphertext_len;
        esp_log_buffer_hex(CRYPTO_TAG, plaintext, *plaintext_len);
        // ESP_LOG_BUFFER_CHAR(CRYPTO_TAG, plaintext, *plaintext_len);
    }
    return ret;
}

void message_encrypt(uint8_t *response, size_t *res_len, uint8_t *plaintext, size_t plaintext_len)
{
    uint8_t *mac = &response[0];
    uint8_t *nonce = &response[MAC_LEN];
    random_generator(nonce, NONCE_LEN);
    uint8_t *ciphertext = &response[MAC_LEN + NONCE_LEN];
    message_lock(ciphertext, res_len, mac, nonce, plaintext, plaintext_len);
}

int message_decrypt(uint8_t *plaintext, size_t *plaintext_len, uint8_t *request, size_t request_len)
{
    uint8_t *mac = &request[0];
    uint8_t *nonce = &request[MAC_LEN];
    uint8_t *ciphertext = &request[MAC_LEN + NONCE_LEN];
    size_t ciphertext_len = request_len - MAC_LEN - NONCE_LEN;    
    return message_unlock(plaintext, plaintext_len, mac, nonce, ciphertext, ciphertext_len);
}

static void random_generator(uint8_t *out, size_t len) {
    for (int i=0; i<len; i++) {
        out[i] = rand();
    }
}

static int pair(pair_request_t *pair_request, pair_response_t *pair_response) {
    // Verify that the message was actually signed by the server
    // int verify_server_signature = crypto_eddsa_check(pair_request->signature, server.identity_pk, (uint8_t*)&pair_request->contents, sizeof(pair_request_content_t));
#if (DEBUG_CRYPTO)
    ESP_LOGE(CRYPTO_TAG, "signature:");
    esp_log_buffer_hex(CRYPTO_TAG, pair_request->signature, SIGNATURE_LEN);
    ESP_LOGE(CRYPTO_TAG, "server.identity_pk:");
    esp_log_buffer_hex(CRYPTO_TAG, server.identity_pk, KEY_LEN);
    ESP_LOGE(CRYPTO_TAG, "conntent:");
    esp_log_buffer_hex(CRYPTO_TAG, (uint8_t*)&pair_request->contents, sizeof(pair_request_content_t));
#endif

    if (memcmp(bike.identity_key.pk, pair_request->contents.bike_identity_pk, sizeof(bike.identity_key.pk)) != 0) {
        ESP_LOGE(CRYPTO_TAG, "invalid bike_identity_pk!!!");
        return -1;
    }

    int verify_server_signature = crypto_ed25519_check(pair_request->signature, server.identity_pk, (uint8_t*)&pair_request->contents, sizeof(pair_request_content_t));
    if (verify_server_signature != 0) {
        ESP_LOGE(CRYPTO_TAG, "failed to verify server signature");
        return verify_server_signature;
    }

    memcpy(phone.identity_pk, pair_request->contents.phone_identity_pk, sizeof(phone.identity_pk));
    memcpy(phone.pairing_pk.value, pair_request->contents.phone_pairing_pk, sizeof(phone.pairing_pk.value));
    phone.pairing_pk.start_time = pair_request->contents.start_time;
    phone.pairing_pk.expiry_time = pair_request->contents.expiry_time;

#if (DEBUG_CRYPTO)
    ESP_LOGI(CRYPTO_TAG, "phone_identity_pk:");
    esp_log_buffer_hex(CRYPTO_TAG, phone.identity_pk, KEY_LEN);
    ESP_LOGI(CRYPTO_TAG, "phone.pairing_pk.value:");
    esp_log_buffer_hex(CRYPTO_TAG, phone.pairing_pk.value, KEY_LEN);
    ESP_LOGI(CRYPTO_TAG, "phone.pairing_pk.start_time: %ld", phone.pairing_pk.start_time);
    ESP_LOGI(CRYPTO_TAG, "phone.pairing_pk.expiry_time: %ld", phone.pairing_pk.expiry_time);
#endif

    //generate bike pairing key for this connection
    random_generator(bike.pairing_key.sk, KEY_LEN);
    crypto_x25519_public_key(bike.pairing_key.pk, bike.pairing_key.sk);

#if (DEBUG_CRYPTO)
    ESP_LOGI(CRYPTO_TAG, "bike.pairing_key.sk:");
    esp_log_buffer_hex(CRYPTO_TAG, bike.pairing_key.sk, KEY_LEN);
    ESP_LOGI(CRYPTO_TAG, "bike.pairing_key.pk:");
    esp_log_buffer_hex(CRYPTO_TAG, bike.pairing_key.pk, KEY_LEN);
#endif

    // Create the response and sign it
    memcpy(pair_response->contents.phone_pairing_pk, phone.pairing_pk.value, KEY_LEN);
    memcpy(pair_response->contents.bike_pairing_pk, bike.pairing_key.pk, KEY_LEN);
    xed25519_sign(pair_response->signature, bike.identity_key.sk, (uint8_t*)&pair_response->contents, sizeof(pair_response_content_t));


#if (DEBUG_CRYPTO)
    ESP_LOGE(CRYPTO_TAG, "pair signature:");
    esp_log_buffer_hex(CRYPTO_TAG, (uint8_t*)pair_response->signature, SIGNATURE_LEN);
    ESP_LOGE(CRYPTO_TAG, "content:");
    esp_log_buffer_hex(CRYPTO_TAG, (uint8_t*)&pair_response->contents, sizeof(pair_response_content_t));
#endif

    return 0;
}

static int session(session_request_t *session_request, session_response_t *session_response) {
#if (IGNORE_PAIRING != 1)
    if (memcmp(bike.pairing_key.pk, session_request->contents.bike_pairing_pk, KEY_LEN) != 0) {
        ESP_LOGE(CRYPTO_TAG, "invalid pairing key...");
#if (DEBUG_CRYPTO)
        ESP_LOGE(CRYPTO_TAG, "session Bike pairing key...");
        esp_log_buffer_hex(CRYPTO_TAG, bike.pairing_key.pk, KEY_LEN);
        ESP_LOGE(CRYPTO_TAG, "session Received Bike pairing key...");
        esp_log_buffer_hex(CRYPTO_TAG, session_request->contents.bike_pairing_pk, KEY_LEN);
#endif
        return -1;
    }

    int verify = xed25519_verify(session_request->signature, phone.identity_pk, (uint8_t*)&session_request->contents, sizeof(session_request_content_t));
    if (!verify) {
        ESP_LOGI(CRYPTO_TAG, "session signature is correct");
    } else {
        ESP_LOGE(CRYPTO_TAG, "session signature is incorrect: %d...exit...:(", verify);
        return -1;
    }

    uint8_t dh1[KEY_LEN];
    crypto_x25519(dh1, bike.identity_key.sk, phone.identity_pk);
#if (DEBUG_CRYPTO)
    ESP_LOGE(CRYPTO_TAG, "session bike.identity.sk...");
    esp_log_buffer_hex(CRYPTO_TAG, bike.identity_key.sk, KEY_LEN);
    ESP_LOGE(CRYPTO_TAG, "session phone_identity_pk...");
    esp_log_buffer_hex(CRYPTO_TAG, phone.identity_pk, KEY_LEN);
#endif
    uint8_t dh2[KEY_LEN];
    crypto_x25519(dh2, bike.pairing_key.sk, phone.pairing_pk.value);
#if (DEBUG_CRYPTO)
    ESP_LOGE(CRYPTO_TAG, "session bike_pairing_key.sk...");
    esp_log_buffer_hex(CRYPTO_TAG, bike.pairing_key.sk, KEY_LEN);
    ESP_LOGE(CRYPTO_TAG, "session bike_pairing_key.pk...");
    esp_log_buffer_hex(CRYPTO_TAG, bike.pairing_key.pk, KEY_LEN);
    ESP_LOGE(CRYPTO_TAG, "session phone.pairing_pk.value...");
    esp_log_buffer_hex(CRYPTO_TAG, phone.pairing_pk.value, KEY_LEN);
#endif
    uint8_t dh3[KEY_LEN];
    crypto_x25519(dh3, bike.identity_key.sk, session_request->contents.ephemeral_pk);
#if (DEBUG_CRYPTO)
    ESP_LOGE(CRYPTO_TAG, "ephemeral_pk...");
    esp_log_buffer_hex(CRYPTO_TAG, session_request->contents.ephemeral_pk, KEY_LEN);
#endif
    uint8_t dh4[KEY_LEN];
    crypto_x25519(dh4, bike.pairing_key.sk, session_request->contents.ephemeral_pk);

    uint8_t sk[KEY_LEN*2] = {0};
    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, KEY_LEN);
    crypto_blake2b_update(&ctx, dh1, KEY_LEN);
    crypto_blake2b_update(&ctx, dh2, KEY_LEN);
    crypto_blake2b_update(&ctx, dh3, KEY_LEN);
    crypto_blake2b_update(&ctx, dh4, KEY_LEN);
    crypto_blake2b_final(&ctx, sk);
    memcpy(phone.derivation_key, sk, KEY_LEN);

    // generate random session id
    random_generator(phone.session_id, KEY_LEN);
    memcpy(session_response->contents.session_id, phone.session_id, KEY_LEN);
#else
    memcpy(phone.derivation_key, derivation_key, KEY_LEN);
    memcpy(phone.session_id, session_id, SESSION_ID_LEN);
#endif

#if (DEBUG_CRYPTO)
    ESP_LOGI(CRYPTO_TAG, "session derivation_key");
    esp_log_buffer_hex(CRYPTO_TAG, phone.derivation_key, KEY_LEN);
    ESP_LOGI(CRYPTO_TAG, "session id");
    esp_log_buffer_hex(CRYPTO_TAG, phone.session_id, SESSION_ID_LEN);
#endif

    memcpy(session_response->contents.ephemeral_pk, session_request->contents.ephemeral_pk, KEY_LEN);
    uint8_t random[SIGNATURE_LEN];
    random_generator(random, SIGNATURE_LEN);
    xed25519_sign(session_response->signature,
                   bike.identity_key.sk,
                   (uint8_t*)&session_response->contents, sizeof(session_response_content_t));
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
#if (DEBUG_CRYPTO)
    ESP_LOGI(CRYPTO_TAG, "xed25519_sign()-secret_key:");
    esp_log_buffer_hex(CRYPTO_TAG, secret_key, KEY_LEN);
    ESP_LOGI(CRYPTO_TAG, "xed25519_sign()-message:");
    esp_log_buffer_hex(CRYPTO_TAG, message, message_size);
#endif
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
#if (DEBUG_CRYPTO)
    ESP_LOGI(CRYPTO_TAG, "xed25519_sign()-signature:");
    esp_log_buffer_hex(CRYPTO_TAG, signature, SIGNATURE_LEN);
#endif
}

static int xed25519_verify(uint8_t signature[SIGNATURE_LEN],
                    uint8_t public_key[KEY_LEN],
                    uint8_t *message,
                    size_t message_size)
{
#if (DEBUG_CRYPTO)
    ESP_LOGI(CRYPTO_TAG, "xed25519_verify()-signature:");
    esp_log_buffer_hex(CRYPTO_TAG, signature, SIGNATURE_LEN);
    ESP_LOGI(CRYPTO_TAG, "xed25519_verify()-public_key:");
    esp_log_buffer_hex(CRYPTO_TAG, public_key, KEY_LEN);
    ESP_LOGI(CRYPTO_TAG, "xed25519_verify()-message:");
    esp_log_buffer_hex(CRYPTO_TAG, message, message_size);
#endif

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

// to verify the unity test framework only, remove later
int crypto_add2num(int a, int b)
{
    return (a+b);
}