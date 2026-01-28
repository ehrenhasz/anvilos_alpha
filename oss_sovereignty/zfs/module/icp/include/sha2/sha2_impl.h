#ifndef	_SHA2_IMPL_H
#define	_SHA2_IMPL_H
#include <sys/sha2.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*sha256_f)(uint32_t state[8], const void *data, size_t blks);
typedef void (*sha512_f)(uint64_t state[8], const void *data, size_t blks);
typedef boolean_t (*sha2_is_supported_f)(void);
typedef struct {
	const char *name;
	sha256_f transform;
	sha2_is_supported_f is_supported;
} sha256_ops_t;
typedef struct {
	const char *name;
	sha512_f transform;
	sha2_is_supported_f is_supported;
} sha512_ops_t;
extern const sha256_ops_t *sha256_get_ops(void);
extern const sha512_ops_t *sha512_get_ops(void);
typedef enum {
	SHA1_TYPE,
	SHA256_TYPE,
	SHA384_TYPE,
	SHA512_TYPE
} sha2_mech_t;
typedef struct sha2_ctx {
	sha2_mech_type_t	sc_mech_type;	 
	SHA2_CTX		sc_sha2_ctx;	 
} sha2_ctx_t;
typedef struct sha2_hmac_ctx {
	sha2_mech_type_t	hc_mech_type;	 
	uint32_t		hc_digest_len;	 
	SHA2_CTX		hc_icontext;	 
	SHA2_CTX		hc_ocontext;	 
} sha2_hmac_ctx_t;
#ifdef	__cplusplus
}
#endif
#endif  
