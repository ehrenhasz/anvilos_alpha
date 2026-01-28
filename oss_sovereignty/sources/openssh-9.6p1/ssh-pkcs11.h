



#define	SSH_PKCS11_ERR_GENERIC			1
#define	SSH_PKCS11_ERR_LOGIN_FAIL		2
#define	SSH_PKCS11_ERR_NO_SLOTS			3
#define	SSH_PKCS11_ERR_PIN_REQUIRED		4
#define	SSH_PKCS11_ERR_PIN_LOCKED		5

int	pkcs11_init(int);
void	pkcs11_terminate(void);
int	pkcs11_add_provider(char *, char *, struct sshkey ***, char ***);
int	pkcs11_del_provider(char *);
#ifdef WITH_PKCS11_KEYGEN
struct sshkey *
	pkcs11_gakp(char *, char *, unsigned int, char *, unsigned int,
	    unsigned int, unsigned char, u_int32_t *);
struct sshkey *
	pkcs11_destroy_keypair(char *, char *, unsigned long, unsigned char,
	    u_int32_t *);
#endif


int pkcs11_make_cert(const struct sshkey *,
    const struct sshkey *, struct sshkey **);
#if !defined(WITH_OPENSSL) && defined(ENABLE_PKCS11)
#undef ENABLE_PKCS11
#endif
