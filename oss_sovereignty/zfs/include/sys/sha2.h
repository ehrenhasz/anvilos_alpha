#ifndef	_SYS_SHA2_H
#define	_SYS_SHA2_H
#ifdef  _KERNEL
#include <sys/types.h>
#else
#include <stdint.h>
#include <stdlib.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define	SHA224_BLOCK_LENGTH		64
#define	SHA256_BLOCK_LENGTH		64
#define	SHA384_BLOCK_LENGTH		128
#define	SHA512_BLOCK_LENGTH		128
#define	SHA224_DIGEST_LENGTH		28
#define	SHA256_DIGEST_LENGTH		32
#define	SHA384_DIGEST_LENGTH		48
#define	SHA512_DIGEST_LENGTH		64
#define	SHA512_224_DIGEST_LENGTH	28
#define	SHA512_256_DIGEST_LENGTH	32
#define	SHA256_HMAC_BLOCK_SIZE		64
#define	SHA512_HMAC_BLOCK_SIZE		128
typedef struct {
	uint32_t state[8];
	uint64_t count[2];
	uint8_t wbuf[64];
	const void *ops;
} sha256_ctx;
typedef struct {
	uint64_t state[8];
	uint64_t count[2];
	uint8_t wbuf[128];
	const void *ops;
} sha512_ctx;
typedef struct {
	union {
		sha256_ctx sha256;
		sha512_ctx sha512;
	};
	int algotype;
} SHA2_CTX;
typedef enum sha2_mech_type {
	SHA256_MECH_INFO_TYPE,		 
	SHA256_HMAC_MECH_INFO_TYPE,	 
	SHA256_HMAC_GEN_MECH_INFO_TYPE,	 
	SHA384_MECH_INFO_TYPE,		 
	SHA384_HMAC_MECH_INFO_TYPE,	 
	SHA384_HMAC_GEN_MECH_INFO_TYPE,	 
	SHA512_MECH_INFO_TYPE,		 
	SHA512_HMAC_MECH_INFO_TYPE,	 
	SHA512_HMAC_GEN_MECH_INFO_TYPE,	 
	SHA512_224_MECH_INFO_TYPE,	 
	SHA512_256_MECH_INFO_TYPE	 
} sha2_mech_type_t;
#define	SHA256			0
#define	SHA256_HMAC		1
#define	SHA256_HMAC_GEN		2
#define	SHA384			3
#define	SHA384_HMAC		4
#define	SHA384_HMAC_GEN		5
#define	SHA512			6
#define	SHA512_HMAC		7
#define	SHA512_HMAC_GEN		8
#define	SHA512_224		9
#define	SHA512_256		10
extern void SHA2Init(int algotype, SHA2_CTX *ctx);
extern void SHA2Update(SHA2_CTX *ctx, const void *data, size_t len);
extern void SHA2Final(void *digest, SHA2_CTX *ctx);
#ifdef __cplusplus
}
#endif
#endif	 
