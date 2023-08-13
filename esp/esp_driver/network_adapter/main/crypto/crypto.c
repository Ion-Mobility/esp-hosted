#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include "monocypher.h"
#include "monocypher-ed25519.h"
#include "crypto.h"

#define CRYPTO_TAG              "CRYPTO"
#define PUBLIC_KEY_LEN          (32)
#define SIGNATURE_LEN           (64)

int signature_eddsa_verification(uint8_t *signature, uint8_t *public_key, uint8_t *message, size_t message_size) {
    return (crypto_eddsa_check(signature, public_key, message, message_size));
}

int xed25519_verify(uint8_t signature[64],
                    uint8_t public_key[32],
                    uint8_t *message,
                    size_t message_size)
{
	/* Convert X25519 key to EdDSA */
	uint8_t A[32];
	crypto_x25519_to_eddsa(A, public_key);

	/* hash(R || A || M) */
	uint8_t H[64];
	crypto_sha512_ctx ctx;
	crypto_sha512_init  (&ctx);
	crypto_sha512_update(&ctx, signature, 32);
	crypto_sha512_update(&ctx, A        , 32);
	crypto_sha512_update(&ctx, message  , message_size);
	crypto_sha512_final (&ctx, H);
	crypto_eddsa_reduce(H, H);

	/* Check signature */
	return crypto_eddsa_check_equation(signature, A, H);
}

// session_request = phone_pairing_pk + bike_pairing_pk + phone_identity_pk + ephemeral_pk
int bike_session(uint8_t *session_id,
                 uint8_t *new_signature,
                 uint8_t *session_aead_secretkey,
                 uint8_t *session_response,
                 uint8_t signature[64],
                 uint8_t session_request[128],
                 int session_request_len,
                 uint8_t secret_key[32])
{
    uint8_t phone_identity_pk[32];
    memcpy(phone_identity_pk, &session_request[64], sizeof(phone_identity_pk)); 
    int verify = xed25519_verify(signature, phone_identity_pk, session_request, session_request_len);
    if (!verify) {
        printf("session signature is correct: %d\n",verify);
    } else {
        printf("session signature fails: %d\n",verify);
        return -1;
    }

    uint8_t bike_identity_sk[32];
    memcpy(bike_identity_sk, secret_key, 32);
    uint8_t ephemeral_pk[32];
    memcpy(ephemeral_pk, &session_request[96], 32);
    uint8_t bike_pairing_sk[32];
    memcpy(bike_pairing_sk, &session_request[32], 32);
    uint8_t phone_pairing_pk[32];
    memcpy(phone_pairing_pk, &session_request[0], 32);

    uint8_t dh1[32];
    crypto_x25519(dh1, bike_identity_sk, phone_identity_pk);

    uint8_t dh2[32];
    crypto_x25519(dh2, bike_pairing_sk, phone_pairing_pk);

    uint8_t dh3[32];
    crypto_x25519(dh3, bike_identity_sk, ephemeral_pk);

    uint8_t dh4[32];
    crypto_x25519(dh4, bike_pairing_sk, ephemeral_pk);

    uint8_t sk[32];
    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, 32);
    crypto_blake2b_update(&ctx, dh1, 32);
    crypto_blake2b_update(&ctx, dh2, 32);
    crypto_blake2b_update(&ctx, dh3, 32);
    crypto_blake2b_update(&ctx, dh4, 32);
    crypto_blake2b_final(&ctx, sk);
    memcpy(session_aead_secretkey, sk, sizeof(sk));

    srand(time(NULL));
    // uint8_t ran_session[32] ={[0 ... 31] = 9};
    // memcpy(session_id, ran_session, 32);
    for (int i=0; i<32; i++) {
        session_id[i] = rand();
    }
    memcpy(session_response, ephemeral_pk, 32);
    memcpy(&session_response[32], session_id, 32);

    uint8_t random[64] ={[0 ... 31] = 10};
    xed25519_sign(new_signature,
                   bike_identity_sk,
                   random,
                   session_response, 64);
    return 0;
}
// void crypto_x25519_sign(uint8_t signature[64], uint8_t secret_key[32], uint8_t *message, size_t message_size) {}
void xed25519_sign(uint8_t signature[64],
                   uint8_t secret_key[32],
                   uint8_t random[64],
                   uint8_t *message,
                   size_t message_size)
{
	static const uint8_t zero   [32] = {0};
	static const uint8_t minus_1[32] = {
		0xec, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
		0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
	};
	static const uint8_t prefix[32] = {
		0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	};

	/* Key pair (a, A) */
	uint8_t A[32];  /* XEdDSA public key  */
	uint8_t a[32];  /* XEdDSA private key */
	crypto_eddsa_trim_scalar(a, secret_key);
	crypto_eddsa_scalarbase(A, a);
	int is_negative = A[31] & 0x80; /* Retrieve sign bit */
	A[31] &= 0x7f;                  /* Clear sign bit    */
	if (is_negative) {
		/* a = -a */
		crypto_eddsa_mul_add(a, a, minus_1, zero);
	}

	/* Secret nonce r */
	uint8_t r[64];
	crypto_sha512_ctx ctx;
	crypto_sha512_init  (&ctx);
	crypto_sha512_update(&ctx, prefix , 32);
	crypto_sha512_update(&ctx, a      , 32);
	crypto_sha512_update(&ctx, message, message_size);
	crypto_sha512_update(&ctx, random , 64);
	crypto_sha512_final (&ctx, r);
	crypto_eddsa_reduce(r, r);

	/* First half of the signature R */
	uint8_t R[32];
	crypto_eddsa_scalarbase(R, r);

	/* hash(R || A || M) */
	uint8_t H[64];
	crypto_sha512_init  (&ctx);
	crypto_sha512_update(&ctx, R      , 32);
	crypto_sha512_update(&ctx, A      , 32);
	crypto_sha512_update(&ctx, message, message_size);
	crypto_sha512_final (&ctx, H);
	crypto_eddsa_reduce(H, H);

	/* Signature */
	memcpy(signature, R, 32);
	crypto_eddsa_mul_add(signature + 32, a, H, r);

	/* Wipe secrets (A, R, and H are not secret) */
	crypto_wipe(a, 32);
	crypto_wipe(r, 32);
}