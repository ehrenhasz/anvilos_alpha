 
 

#ifndef _SK_API_H
#define _SK_API_H 1

#include <stddef.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

 
#define SSH_SK_USER_PRESENCE_REQD	0x01
#define SSH_SK_USER_VERIFICATION_REQD	0x04
#define SSH_SK_FORCE_OPERATION		0x10
#define SSH_SK_RESIDENT_KEY		0x20

 
#define SSH_SK_ECDSA			0x00
#define SSH_SK_ED25519			0x01

 
#define SSH_SK_ERR_GENERAL		-1
#define SSH_SK_ERR_UNSUPPORTED		-2
#define SSH_SK_ERR_PIN_REQUIRED		-3
#define SSH_SK_ERR_DEVICE_NOT_FOUND	-4
#define SSH_SK_ERR_CREDENTIAL_EXISTS	-5

struct sk_enroll_response {
	uint8_t flags;
	uint8_t *public_key;
	size_t public_key_len;
	uint8_t *key_handle;
	size_t key_handle_len;
	uint8_t *signature;
	size_t signature_len;
	uint8_t *attestation_cert;
	size_t attestation_cert_len;
	uint8_t *authdata;
	size_t authdata_len;
};

struct sk_sign_response {
	uint8_t flags;
	uint32_t counter;
	uint8_t *sig_r;
	size_t sig_r_len;
	uint8_t *sig_s;
	size_t sig_s_len;
};

struct sk_resident_key {
	uint32_t alg;
	size_t slot;
	char *application;
	struct sk_enroll_response key;
	uint8_t flags;
	uint8_t *user_id;
	size_t user_id_len;
};

struct sk_option {
	char *name;
	char *value;
	uint8_t required;
};

#define SSH_SK_VERSION_MAJOR		0x000a0000  
#define SSH_SK_VERSION_MAJOR_MASK	0xffff0000

 
uint32_t sk_api_version(void);

 
int sk_enroll(uint32_t alg, const uint8_t *challenge, size_t challenge_len,
    const char *application, uint8_t flags, const char *pin,
    struct sk_option **options, struct sk_enroll_response **enroll_response);

 
int sk_sign(uint32_t alg, const uint8_t *data, size_t data_len,
    const char *application, const uint8_t *key_handle, size_t key_handle_len,
    uint8_t flags, const char *pin, struct sk_option **options,
    struct sk_sign_response **sign_response);

 
int sk_load_resident_keys(const char *pin, struct sk_option **options,
    struct sk_resident_key ***rks, size_t *nrks);

#endif  
