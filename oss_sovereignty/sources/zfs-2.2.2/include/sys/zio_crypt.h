



#ifndef	_SYS_ZIO_CRYPT_H
#define	_SYS_ZIO_CRYPT_H

#include <sys/dmu.h>
#include <sys/zfs_refcount.h>
#if defined(__FreeBSD__) && defined(_KERNEL)
#include <sys/freebsd_crypto.h>
#else
#include <sys/crypto/api.h>
#endif 
#include <sys/nvpair.h>
#include <sys/avl.h>
#include <sys/zio.h>


struct zbookmark_phys;

#define	WRAPPING_KEY_LEN	32
#define	WRAPPING_IV_LEN		ZIO_DATA_IV_LEN
#define	WRAPPING_MAC_LEN	ZIO_DATA_MAC_LEN
#define	MASTER_KEY_MAX_LEN	32
#define	SHA512_HMAC_KEYLEN	64

#define	ZIO_CRYPT_KEY_CURRENT_VERSION	1ULL

typedef enum zio_crypt_type {
	ZC_TYPE_NONE = 0,
	ZC_TYPE_CCM,
	ZC_TYPE_GCM
} zio_crypt_type_t;


typedef struct zio_crypt_info {
	
#if defined(__FreeBSD__) && defined(_KERNEL)
	
	const char	*ci_algname;
#else
	crypto_mech_name_t ci_mechname;
#endif
	
	zio_crypt_type_t ci_crypt_type;

	
	size_t ci_keylen;

	
	const char *ci_name;
} zio_crypt_info_t;

extern const zio_crypt_info_t zio_crypt_table[ZIO_CRYPT_FUNCTIONS];


typedef struct zio_crypt_key {
	
	uint64_t zk_crypt;

	
	uint64_t zk_version;

	
	uint64_t zk_guid;

	
	uint8_t zk_master_keydata[MASTER_KEY_MAX_LEN];

	
	uint8_t zk_hmac_keydata[SHA512_HMAC_KEYLEN];

	
	uint8_t zk_current_keydata[MASTER_KEY_MAX_LEN];

	
	uint8_t zk_salt[ZIO_DATA_SALT_LEN];

	
	uint64_t zk_salt_count;

	
	crypto_key_t zk_current_key;

#if defined(__FreeBSD__) && defined(_KERNEL)
	
	freebsd_crypt_session_t	zk_session;
#else
	
	crypto_ctx_template_t zk_current_tmpl;
#endif

	
	crypto_key_t zk_hmac_key;

	
	crypto_ctx_template_t zk_hmac_tmpl;

	
	krwlock_t zk_salt_lock;
} zio_crypt_key_t;

void zio_crypt_key_destroy(zio_crypt_key_t *key);
int zio_crypt_key_init(uint64_t crypt, zio_crypt_key_t *key);
int zio_crypt_key_get_salt(zio_crypt_key_t *key, uint8_t *salt_out);

int zio_crypt_key_wrap(crypto_key_t *cwkey, zio_crypt_key_t *key, uint8_t *iv,
    uint8_t *mac, uint8_t *keydata_out, uint8_t *hmac_keydata_out);
int zio_crypt_key_unwrap(crypto_key_t *cwkey, uint64_t crypt, uint64_t version,
    uint64_t guid, uint8_t *keydata, uint8_t *hmac_keydata, uint8_t *iv,
    uint8_t *mac, zio_crypt_key_t *key);
int zio_crypt_generate_iv(uint8_t *ivbuf);
int zio_crypt_generate_iv_salt_dedup(zio_crypt_key_t *key, uint8_t *data,
    uint_t datalen, uint8_t *ivbuf, uint8_t *salt);

void zio_crypt_encode_params_bp(blkptr_t *bp, uint8_t *salt, uint8_t *iv);
void zio_crypt_decode_params_bp(const blkptr_t *bp, uint8_t *salt, uint8_t *iv);
void zio_crypt_encode_mac_bp(blkptr_t *bp, uint8_t *mac);
void zio_crypt_decode_mac_bp(const blkptr_t *bp, uint8_t *mac);
void zio_crypt_encode_mac_zil(void *data, uint8_t *mac);
void zio_crypt_decode_mac_zil(const void *data, uint8_t *mac);
void zio_crypt_copy_dnode_bonus(abd_t *src_abd, uint8_t *dst, uint_t datalen);

int zio_crypt_do_indirect_mac_checksum(boolean_t generate, void *buf,
    uint_t datalen, boolean_t byteswap, uint8_t *cksum);
int zio_crypt_do_indirect_mac_checksum_abd(boolean_t generate, abd_t *abd,
    uint_t datalen, boolean_t byteswap, uint8_t *cksum);
int zio_crypt_do_hmac(zio_crypt_key_t *key, uint8_t *data, uint_t datalen,
    uint8_t *digestbuf, uint_t digestlen);
int zio_crypt_do_objset_hmacs(zio_crypt_key_t *key, void *data, uint_t datalen,
    boolean_t byteswap, uint8_t *portable_mac, uint8_t *local_mac);
int zio_do_crypt_data(boolean_t encrypt, zio_crypt_key_t *key,
    dmu_object_type_t ot, boolean_t byteswap, uint8_t *salt, uint8_t *iv,
    uint8_t *mac, uint_t datalen, uint8_t *plainbuf, uint8_t *cipherbuf,
    boolean_t *no_crypt);
int zio_do_crypt_abd(boolean_t encrypt, zio_crypt_key_t *key,
    dmu_object_type_t ot, boolean_t byteswap, uint8_t *salt, uint8_t *iv,
    uint8_t *mac, uint_t datalen, abd_t *pabd, abd_t *cabd,
    boolean_t *no_crypt);

#endif
