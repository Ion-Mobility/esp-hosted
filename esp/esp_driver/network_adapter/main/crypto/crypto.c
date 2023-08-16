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
#define MSG_AUTHN_CODE_LEN      (16)
#define MAX_PAIRED_PHONE        (3)

#define STORAGE_NAMESPACE       "ble_keys"

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
    keypair_t pairing;
    uint8_t identity_pk[KEY_LEN];
} paired_phone_t;

typedef struct {
    keypair_t identity;                         // long-term, one per device
    paired_phone_t paired_phone[MAX_PAIRED_PHONE];    // medium-term, pairing key (1 week expired)
    uint8_t num_paired_phone;
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
static uint8_t generate_bike_pairing_key(uint8_t *phone_identity_pk);
static void message_lock(msg_lock_t *msg_lock);
static void message_unlock(msg_unlock_t *msg_unlock);
static int pair(pair_request_t *pair_request, pair_response_t *pair_response);
static int session(session_request_t *session_request, session_response_t *session_response);
static void cleanup_paired_storage(nvs_handle_t *my_handle);

phone_t phone               = {0};
bike_t bike                 = {0};
server_t server             = {0};
uint8_t empty_key[KEY_LEN]  = {0};

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
        err = nvs_get_blob(my_handle, "bike_identity", NULL, &required_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(CRYPTO_TAG, "No bike's identity key founded in flash or data is corrupted");
            goto init_exit;
        }
        // Read previously saved blob if available
        if (required_size == sizeof(keypair_t)) {
            err = nvs_get_blob(my_handle, "bike_identity", &bike.identity, &required_size);
            if (err != ESP_OK) {
                ESP_LOGE(CRYPTO_TAG, "Failed to read bike's identity key from flash...");
                goto init_exit;
            }
        } else {
            ESP_LOGE(CRYPTO_TAG, "bike's identity key size is not correct, the stored key is corrupted");
            goto init_exit;
        }
    }

    // read server key from NVS
    {
        size_t required_size = 0;  // value will default to 0, if not set yet in NVS
        err = nvs_get_blob(my_handle, "server_identity", NULL, &required_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(CRYPTO_TAG, "No server's identity key founded in flash or data is corrupted");
            goto init_exit;
        }
        // Read previously saved blob if available
        if (required_size == KEY_LEN) {
            err = nvs_get_blob(my_handle, "server_identity", server.identity_pk, &required_size);
            if (err != ESP_OK) {
                ESP_LOGE(CRYPTO_TAG, "Failed to read server's identity key from flash...");
                goto init_exit;
            }
        } else {
            ESP_LOGE(CRYPTO_TAG, "server's identity key size is not correct, the stored key is corrupted");
            goto init_exit;
        }
    }

    // reas list of paired phone if any
    {
        bike.num_paired_phone = 0;
        err = nvs_get_u8(my_handle, "num_paired_phone", &bike.num_paired_phone);
        if (err == ESP_OK) {
            if (bike.num_paired_phone > MAX_PAIRED_PHONE) {
                ESP_LOGE(CRYPTO_TAG, "number of paired phone data is corrupted: %d",bike.num_paired_phone);
                cleanup_paired_storage(&my_handle);
            } else {
                ESP_LOGI(CRYPTO_TAG, "number of paired phone: %d",bike.num_paired_phone);
                // read server key from NVS
                size_t required_size = 0;  // value will default to 0, if not set yet in NVS
                err = nvs_get_blob(my_handle, "paired_phones", NULL, &required_size);
                if (err != ESP_OK) {
                    ESP_LOGE(CRYPTO_TAG, "paired phone key size in flash is corrupted");
                    cleanup_paired_storage(&my_handle);
                } else {
                    // Read previously saved phones
                    if (required_size == sizeof(paired_phone_t) * MAX_PAIRED_PHONE) {
                        err = nvs_get_blob(my_handle, "paired_phones", bike.paired_phone, &required_size);
                        if (err != ESP_OK) {
                            ESP_LOGE(CRYPTO_TAG, "Failed to read paired_phones...");
                            cleanup_paired_storage(&my_handle);
                        }
                    } else {
                        ESP_LOGE(CRYPTO_TAG, "paired phones identity keys is corrupted");
                        cleanup_paired_storage(&my_handle);
                    }
                }
            }
        } else {
            cleanup_paired_storage(&my_handle);
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

static void cleanup_paired_storage(nvs_handle_t *my_handle) {
    bike.num_paired_phone = 0;
    nvs_set_u8(*my_handle, "num_paired_phone", bike.num_paired_phone);
    memset(bike.paired_phone, 0, sizeof(bike.paired_phone));
    nvs_set_blob(*my_handle, "paired_phones", bike.paired_phone, sizeof(paired_phone_t) * MAX_PAIRED_PHONE);
    nvs_commit(*my_handle);
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

static uint8_t generate_bike_pairing_key(uint8_t *phone_identity_pk) {
    // bike pairing key for each phone
    // if this phone already existed, just update pairing key
    // else add new phone
    int index = 0;
    for (index = 0; index < MAX_PAIRED_PHONE; index++) {
        if (!memcmp(bike.paired_phone[index].identity_pk, phone_identity_pk, KEY_LEN))
            break;
    }

    if (index >= MAX_PAIRED_PHONE) {
        // found no phone, find a free slot to add this new phone
        for (int i = 0; i < MAX_PAIRED_PHONE; i++)
            if (!memcmp(bike.paired_phone[i].identity_pk, empty_key, KEY_LEN)) {
                index = i;
                break;
            }
    }

    memset(&bike.paired_phone[index].pairing, 0, sizeof(keypair_t));
    random_generator(bike.paired_phone[index].pairing.sk, KEY_LEN);
    crypto_x25519_public_key(bike.paired_phone[index].pairing.pk, bike.paired_phone[index].pairing.sk);
    return index;
}

static int pair(pair_request_t *pair_request, pair_response_t *pair_response) {
    if (bike.num_paired_phone >= MAX_PAIRED_PHONE) {
        ESP_LOGE(CRYPTO_TAG, "number of paired phones reaches limitation: %d", MAX_PAIRED_PHONE);
        return -2;
    }

    // Verify that the message was actually signed by the server
    int verify_server_signature = crypto_eddsa_check(pair_request->signature, server.identity_pk, (uint8_t*)&pair_request->contents, sizeof(pair_request_content_t));
    if (verify_server_signature != 0) {
        ESP_LOGE(CRYPTO_TAG, "failed to verify server signature");
        return verify_server_signature;
    }

    // Generate a new bike pairing key for this phone pairing key and
    int index = generate_bike_pairing_key(pair_request->contents.phone_identity_pk);

    // Create the response and sign it
    memcpy(pair_response->contents.phone_pairing_pk, phone.pairing_pk, KEY_LEN);
    memcpy(pair_response->contents.bike_pairing_pk, bike.paired_phone[index].pairing.pk, KEY_LEN);
    xed25519_sign(pair_response->signature, bike.paired_phone[index].pairing.sk, (uint8_t*)&pair_response->contents, sizeof(pair_request_content_t));

    return 0;
}

static int session(session_request_t *session_request, session_response_t *session_response) {
    // Ensure that we're paired with this phone
    int index = 0;
    for (index = 0; index < MAX_PAIRED_PHONE; index++) {
        if (!memcmp(bike.paired_phone[index].pairing.pk, session_request->contents.bike_pairing_pk, KEY_LEN))
            break;
    }

    if (index >= MAX_PAIRED_PHONE) {
        ESP_LOGE(CRYPTO_TAG, "phone not paired yet...");
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
    crypto_x25519(dh1, bike.identity.sk, phone.identity_pk);

    uint8_t dh2[KEY_LEN];
    crypto_x25519(dh2, bike.identity.sk, phone.pairing_pk);

    uint8_t dh3[KEY_LEN];
    crypto_x25519(dh3, bike.identity.sk, phone.ephemeral_pk);

    uint8_t dh4[KEY_LEN];
    crypto_x25519(dh4, bike.paired_phone[index].pairing.sk, phone.ephemeral_pk);

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