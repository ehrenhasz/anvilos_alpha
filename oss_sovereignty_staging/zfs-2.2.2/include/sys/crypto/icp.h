 
 

#ifndef	_SYS_CRYPTO_ALGS_H
#define	_SYS_CRYPTO_ALGS_H

int aes_mod_init(void);
int aes_mod_fini(void);

int sha2_mod_init(void);
int sha2_mod_fini(void);

int skein_mod_init(void);
int skein_mod_fini(void);

int icp_init(void);
void icp_fini(void);

int aes_impl_set(const char *);
int gcm_impl_set(const char *);

#endif  
