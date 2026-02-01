 
 

#ifndef	_SYS_CRYPTO_API_H
#define	_SYS_CRYPTO_API_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/zfs_context.h>
#include <sys/crypto/common.h>

typedef void *crypto_context_t;
typedef void *crypto_ctx_template_t;

 
#define	CRYPTO_MECH_INVALID	((uint64_t)-1)
extern crypto_mech_type_t crypto_mech2id(const char *name);

 
extern int crypto_create_ctx_template(crypto_mechanism_t *mech,
    crypto_key_t *key, crypto_ctx_template_t *tmpl);
extern void crypto_destroy_ctx_template(crypto_ctx_template_t tmpl);

 
extern int crypto_mac(crypto_mechanism_t *mech, crypto_data_t *data,
    crypto_key_t *key, crypto_ctx_template_t tmpl, crypto_data_t *mac);
extern int crypto_mac_init(crypto_mechanism_t *mech, crypto_key_t *key,
    crypto_ctx_template_t tmpl, crypto_context_t *ctxp);
extern int crypto_mac_update(crypto_context_t ctx, crypto_data_t *data);
extern int crypto_mac_final(crypto_context_t ctx, crypto_data_t *data);

 
extern int crypto_encrypt(crypto_mechanism_t *mech, crypto_data_t *plaintext,
    crypto_key_t *key, crypto_ctx_template_t tmpl, crypto_data_t *ciphertext);
extern int crypto_decrypt(crypto_mechanism_t *mech, crypto_data_t *ciphertext,
    crypto_key_t *key, crypto_ctx_template_t tmpl, crypto_data_t *plaintext);

#ifdef	__cplusplus
}
#endif

#endif	 
