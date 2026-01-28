#ifndef _SYS_CRYPTO_COMMON_H
#define	_SYS_CRYPTO_COMMON_H
#ifdef __cplusplus
extern "C" {
#endif
#include <sys/zfs_context.h>
#define	CRYPTO_MAX_MECH_NAME 32
typedef char crypto_mech_name_t[CRYPTO_MAX_MECH_NAME];
typedef uint64_t crypto_mech_type_t;
typedef struct crypto_mechanism {
	crypto_mech_type_t	cm_type;	 
	caddr_t			cm_param;	 
	size_t			cm_param_len;	 
} crypto_mechanism_t;
typedef struct CK_AES_CTR_PARAMS {
	ulong_t	ulCounterBits;
	uint8_t cb[16];
} CK_AES_CTR_PARAMS;
typedef struct CK_AES_CCM_PARAMS {
	ulong_t ulMACSize;
	ulong_t ulNonceSize;
	ulong_t ulAuthDataSize;
	ulong_t ulDataSize;  
	uchar_t *nonce;
	uchar_t *authData;
} CK_AES_CCM_PARAMS;
typedef struct CK_AES_GCM_PARAMS {
	uchar_t *pIv;
	ulong_t ulIvLen;
	ulong_t ulIvBits;
	uchar_t *pAAD;
	ulong_t ulAADLen;
	ulong_t ulTagBits;
} CK_AES_GCM_PARAMS;
typedef struct CK_AES_GMAC_PARAMS {
	uchar_t *pIv;
	uchar_t *pAAD;
	ulong_t ulAADLen;
} CK_AES_GMAC_PARAMS;
typedef uint32_t crypto_keysize_unit_t;
#define	SUN_CKM_SHA256			"CKM_SHA256"
#define	SUN_CKM_SHA256_HMAC		"CKM_SHA256_HMAC"
#define	SUN_CKM_SHA256_HMAC_GENERAL	"CKM_SHA256_HMAC_GENERAL"
#define	SUN_CKM_SHA384			"CKM_SHA384"
#define	SUN_CKM_SHA384_HMAC		"CKM_SHA384_HMAC"
#define	SUN_CKM_SHA384_HMAC_GENERAL	"CKM_SHA384_HMAC_GENERAL"
#define	SUN_CKM_SHA512			"CKM_SHA512"
#define	SUN_CKM_SHA512_HMAC		"CKM_SHA512_HMAC"
#define	SUN_CKM_SHA512_HMAC_GENERAL	"CKM_SHA512_HMAC_GENERAL"
#define	SUN_CKM_SHA512_224		"CKM_SHA512_224"
#define	SUN_CKM_SHA512_256		"CKM_SHA512_256"
#define	SUN_CKM_AES_CBC			"CKM_AES_CBC"
#define	SUN_CKM_AES_ECB			"CKM_AES_ECB"
#define	SUN_CKM_AES_CTR			"CKM_AES_CTR"
#define	SUN_CKM_AES_CCM			"CKM_AES_CCM"
#define	SUN_CKM_AES_GCM			"CKM_AES_GCM"
#define	SUN_CKM_AES_GMAC		"CKM_AES_GMAC"
typedef enum crypto_data_format {
	CRYPTO_DATA_RAW = 1,
	CRYPTO_DATA_UIO,
} crypto_data_format_t;
typedef struct crypto_data {
	crypto_data_format_t	cd_format;	 
	off_t			cd_offset;	 
	size_t			cd_length;	 
	union {
		iovec_t cd_raw;		 
		zfs_uio_t	*cd_uio;
	};	 
} crypto_data_t;
typedef struct {
	uint_t	ck_length;	 
	void	*ck_data;	 
} crypto_key_t;
#define	CRYPTO_BITS2BYTES(n) ((n) == 0 ? 0 : (((n) - 1) >> 3) + 1)
#define	CRYPTO_BYTES2BITS(n) ((n) << 3)
typedef uint32_t 	crypto_provider_id_t;
#define	KCF_PROVID_INVALID	((uint32_t)-1)
typedef void *crypto_session_t;
#define	PROVIDER_OWNS_KEY_SCHEDULE	0x00000001
#define	CRYPTO_SUCCESS				0x00000000
#define	CRYPTO_HOST_MEMORY			0x00000002
#define	CRYPTO_FAILED				0x00000004
#define	CRYPTO_ARGUMENTS_BAD			0x00000005
#define	CRYPTO_DATA_LEN_RANGE			0x0000000C
#define	CRYPTO_ENCRYPTED_DATA_LEN_RANGE		0x00000011
#define	CRYPTO_KEY_SIZE_RANGE			0x00000013
#define	CRYPTO_KEY_TYPE_INCONSISTENT		0x00000014
#define	CRYPTO_MECHANISM_INVALID		0x0000001C
#define	CRYPTO_MECHANISM_PARAM_INVALID		0x0000001D
#define	CRYPTO_SIGNATURE_INVALID		0x0000002D
#define	CRYPTO_BUFFER_TOO_SMALL			0x00000042
#define	CRYPTO_NOT_SUPPORTED			0x00000044
#define	CRYPTO_INVALID_CONTEXT			0x00000047
#define	CRYPTO_INVALID_MAC			0x00000048
#define	CRYPTO_MECH_NOT_SUPPORTED		0x00000049
#define	CRYPTO_INVALID_PROVIDER_ID		0x0000004C
#define	CRYPTO_BUSY				0x0000004E
#define	CRYPTO_UNKNOWN_PROVIDER			0x0000004F
#ifdef __cplusplus
}
#endif
#endif  
