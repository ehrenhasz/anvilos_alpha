#ifndef	_SYS_CRYPTO_SPI_H
#define	_SYS_CRYPTO_SPI_H
#include <sys/zfs_context.h>
#include <sys/crypto/common.h>
#ifdef	__cplusplus
extern "C" {
#endif
#ifdef CONSTIFY_PLUGIN
#define	__no_const __attribute__((no_const))
#else
#define	__no_const
#endif  
typedef void *crypto_spi_ctx_template_t;
typedef struct crypto_ctx {
	void			*cc_provider_private;	 
	void			*cc_framework_private;	 
} crypto_ctx_t;
typedef struct crypto_digest_ops {
	int (*digest_init)(crypto_ctx_t *, crypto_mechanism_t *);
	int (*digest)(crypto_ctx_t *, crypto_data_t *, crypto_data_t *);
	int (*digest_update)(crypto_ctx_t *, crypto_data_t *);
	int (*digest_key)(crypto_ctx_t *, crypto_key_t *);
	int (*digest_final)(crypto_ctx_t *, crypto_data_t *);
	int (*digest_atomic)(crypto_mechanism_t *, crypto_data_t *,
	    crypto_data_t *);
} __no_const crypto_digest_ops_t;
typedef struct crypto_cipher_ops {
	int (*encrypt_init)(crypto_ctx_t *,
	    crypto_mechanism_t *, crypto_key_t *,
	    crypto_spi_ctx_template_t);
	int (*encrypt)(crypto_ctx_t *,
	    crypto_data_t *, crypto_data_t *);
	int (*encrypt_update)(crypto_ctx_t *,
	    crypto_data_t *, crypto_data_t *);
	int (*encrypt_final)(crypto_ctx_t *,
	    crypto_data_t *);
	int (*encrypt_atomic)(crypto_mechanism_t *, crypto_key_t *,
	    crypto_data_t *, crypto_data_t *, crypto_spi_ctx_template_t);
	int (*decrypt_init)(crypto_ctx_t *,
	    crypto_mechanism_t *, crypto_key_t *,
	    crypto_spi_ctx_template_t);
	int (*decrypt)(crypto_ctx_t *,
	    crypto_data_t *, crypto_data_t *);
	int (*decrypt_update)(crypto_ctx_t *,
	    crypto_data_t *, crypto_data_t *);
	int (*decrypt_final)(crypto_ctx_t *,
	    crypto_data_t *);
	int (*decrypt_atomic)(crypto_mechanism_t *, crypto_key_t *,
	    crypto_data_t *, crypto_data_t *, crypto_spi_ctx_template_t);
} __no_const crypto_cipher_ops_t;
typedef struct crypto_mac_ops {
	int (*mac_init)(crypto_ctx_t *,
	    crypto_mechanism_t *, crypto_key_t *,
	    crypto_spi_ctx_template_t);
	int (*mac)(crypto_ctx_t *,
	    crypto_data_t *, crypto_data_t *);
	int (*mac_update)(crypto_ctx_t *,
	    crypto_data_t *);
	int (*mac_final)(crypto_ctx_t *,
	    crypto_data_t *);
	int (*mac_atomic)(crypto_mechanism_t *, crypto_key_t *,
	    crypto_data_t *, crypto_data_t *, crypto_spi_ctx_template_t);
	int (*mac_verify_atomic)(crypto_mechanism_t *, crypto_key_t *,
	    crypto_data_t *, crypto_data_t *, crypto_spi_ctx_template_t);
} __no_const crypto_mac_ops_t;
typedef struct crypto_ctx_ops {
	int (*create_ctx_template)(crypto_mechanism_t *, crypto_key_t *,
	    crypto_spi_ctx_template_t *, size_t *);
	int (*free_context)(crypto_ctx_t *);
} __no_const crypto_ctx_ops_t;
typedef struct crypto_ops {
	const crypto_digest_ops_t			*co_digest_ops;
	const crypto_cipher_ops_t			*co_cipher_ops;
	const crypto_mac_ops_t			*co_mac_ops;
	const crypto_ctx_ops_t			*co_ctx_ops;
} crypto_ops_t;
typedef uint32_t crypto_func_group_t;
#define	CRYPTO_FG_ENCRYPT		0x00000001  
#define	CRYPTO_FG_DECRYPT		0x00000002  
#define	CRYPTO_FG_DIGEST		0x00000004  
#define	CRYPTO_FG_MAC			0x00001000  
#define	CRYPTO_FG_ENCRYPT_ATOMIC	0x00008000  
#define	CRYPTO_FG_DECRYPT_ATOMIC	0x00010000  
#define	CRYPTO_FG_MAC_ATOMIC		0x00020000  
#define	CRYPTO_FG_DIGEST_ATOMIC		0x00040000  
#define	CRYPTO_PROVIDER_DESCR_MAX_LEN	64
typedef struct crypto_mech_info {
	crypto_mech_name_t	cm_mech_name;
	crypto_mech_type_t	cm_mech_number;
	crypto_func_group_t	cm_func_group_mask;
} crypto_mech_info_t;
typedef uint_t crypto_kcf_provider_handle_t;
typedef struct crypto_provider_info {
	const char				*pi_provider_description;
	const crypto_ops_t			*pi_ops_vector;
	uint_t				pi_mech_list_count;
	const crypto_mech_info_t		*pi_mechanisms;
} crypto_provider_info_t;
extern int crypto_register_provider(const crypto_provider_info_t *,
		crypto_kcf_provider_handle_t *);
extern int crypto_unregister_provider(crypto_kcf_provider_handle_t);
#ifdef	__cplusplus
}
#endif
#endif	 
