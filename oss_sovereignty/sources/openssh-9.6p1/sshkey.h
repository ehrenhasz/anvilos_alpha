


#ifndef SSHKEY_H
#define SSHKEY_H

#include <sys/types.h>

#ifdef WITH_OPENSSL
#include <openssl/rsa.h>
#include <openssl/dsa.h>
# ifdef OPENSSL_HAS_ECC
#  include <openssl/ec.h>
#  include <openssl/ecdsa.h>
# else 
#  define EC_KEY	void
#  define EC_GROUP	void
#  define EC_POINT	void
# endif 
#define SSH_OPENSSL_VERSION OpenSSL_version(OPENSSL_VERSION)
#else 
# define BIGNUM		void
# define RSA		void
# define DSA		void
# define EC_KEY		void
# define EC_GROUP	void
# define EC_POINT	void
#define SSH_OPENSSL_VERSION "without OpenSSL"
#endif 

#define SSH_RSA_MINIMUM_MODULUS_SIZE	1024
#define SSH_KEY_MAX_SIGN_DATA_SIZE	(1 << 20)

struct sshbuf;


enum sshkey_types {
	KEY_RSA,
	KEY_DSA,
	KEY_ECDSA,
	KEY_ED25519,
	KEY_RSA_CERT,
	KEY_DSA_CERT,
	KEY_ECDSA_CERT,
	KEY_ED25519_CERT,
	KEY_XMSS,
	KEY_XMSS_CERT,
	KEY_ECDSA_SK,
	KEY_ECDSA_SK_CERT,
	KEY_ED25519_SK,
	KEY_ED25519_SK_CERT,
	KEY_UNSPEC
};


#define SSH_FP_HASH_DEFAULT	SSH_DIGEST_SHA256


enum sshkey_fp_rep {
	SSH_FP_DEFAULT = 0,
	SSH_FP_HEX,
	SSH_FP_BASE64,
	SSH_FP_BUBBLEBABBLE,
	SSH_FP_RANDOMART
};


enum sshkey_serialize_rep {
	SSHKEY_SERIALIZE_DEFAULT = 0,
	SSHKEY_SERIALIZE_STATE = 1,	
	SSHKEY_SERIALIZE_FULL = 2,	
	SSHKEY_SERIALIZE_SHIELD = 3,	
	SSHKEY_SERIALIZE_INFO = 254,	
};


enum sshkey_private_format {
	SSHKEY_PRIVATE_OPENSSH = 0,
	SSHKEY_PRIVATE_PEM = 1,
	SSHKEY_PRIVATE_PKCS8 = 2,
};


#define SSHKEY_FLAG_EXT		0x0001

#define SSHKEY_CERT_MAX_PRINCIPALS	256

struct sshkey_cert {
	struct sshbuf	*certblob; 
	u_int		 type; 
	u_int64_t	 serial;
	char		*key_id;
	u_int		 nprincipals;
	char		**principals;
	u_int64_t	 valid_after, valid_before;
	struct sshbuf	*critical;
	struct sshbuf	*extensions;
	struct sshkey	*signature_key;
	char		*signature_type;
};


struct sshkey {
	int	 type;
	int	 flags;
	
	RSA	*rsa;
	
	DSA	*dsa;
	
	int	 ecdsa_nid;	
	EC_KEY	*ecdsa;
	
	u_char	*ed25519_sk;
	u_char	*ed25519_pk;
	
	char	*xmss_name;
	char	*xmss_filename;	
	void	*xmss_state;	
	u_char	*xmss_sk;
	u_char	*xmss_pk;
	
	char	*sk_application;
	uint8_t	sk_flags;
	struct sshbuf *sk_key_handle;
	struct sshbuf *sk_reserved;
	
	struct sshkey_cert *cert;
	
	u_char	*shielded_private;
	size_t	shielded_len;
	u_char	*shield_prekey;
	size_t	shield_prekey_len;
};

#define	ED25519_SK_SZ	crypto_sign_ed25519_SECRETKEYBYTES
#define	ED25519_PK_SZ	crypto_sign_ed25519_PUBLICKEYBYTES


struct sshkey_sig_details {
	uint32_t sk_counter;	
	uint8_t sk_flags;	
};

struct sshkey_impl_funcs {
	u_int (*size)(const struct sshkey *);	
	int (*alloc)(struct sshkey *);		
	void (*cleanup)(struct sshkey *);	
	int (*equal)(const struct sshkey *, const struct sshkey *);
	int (*serialize_public)(const struct sshkey *, struct sshbuf *,
	    enum sshkey_serialize_rep);
	int (*deserialize_public)(const char *, struct sshbuf *,
	    struct sshkey *);
	int (*serialize_private)(const struct sshkey *, struct sshbuf *,
	    enum sshkey_serialize_rep);
	int (*deserialize_private)(const char *, struct sshbuf *,
	    struct sshkey *);
	int (*generate)(struct sshkey *, int);	
	int (*copy_public)(const struct sshkey *, struct sshkey *);
	int (*sign)(struct sshkey *, u_char **, size_t *,
	    const u_char *, size_t, const char *,
	    const char *, const char *, u_int); 
	int (*verify)(const struct sshkey *, const u_char *, size_t,
	    const u_char *, size_t, const char *, u_int,
	    struct sshkey_sig_details **);
};

struct sshkey_impl {
	const char *name;
	const char *shortname;
	const char *sigalg;
	int type;
	int nid;
	int cert;
	int sigonly;
	int keybits;
	const struct sshkey_impl_funcs *funcs;
};

struct sshkey	*sshkey_new(int);
void		 sshkey_free(struct sshkey *);
int		 sshkey_equal_public(const struct sshkey *,
    const struct sshkey *);
int		 sshkey_equal(const struct sshkey *, const struct sshkey *);
char		*sshkey_fingerprint(const struct sshkey *,
    int, enum sshkey_fp_rep);
int		 sshkey_fingerprint_raw(const struct sshkey *k,
    int, u_char **retp, size_t *lenp);
const char	*sshkey_type(const struct sshkey *);
const char	*sshkey_cert_type(const struct sshkey *);
int		 sshkey_format_text(const struct sshkey *, struct sshbuf *);
int		 sshkey_write(const struct sshkey *, FILE *);
int		 sshkey_read(struct sshkey *, char **);
u_int		 sshkey_size(const struct sshkey *);

int		 sshkey_generate(int type, u_int bits, struct sshkey **keyp);
int		 sshkey_from_private(const struct sshkey *, struct sshkey **);

int		 sshkey_is_shielded(struct sshkey *);
int		 sshkey_shield_private(struct sshkey *);
int		 sshkey_unshield_private(struct sshkey *);

int	 sshkey_type_from_name(const char *);
int	 sshkey_is_cert(const struct sshkey *);
int	 sshkey_is_sk(const struct sshkey *);
int	 sshkey_type_is_cert(int);
int	 sshkey_type_plain(int);


int	 sshkey_match_keyname_to_sigalgs(const char *, const char *);

int	 sshkey_to_certified(struct sshkey *);
int	 sshkey_drop_cert(struct sshkey *);
int	 sshkey_cert_copy(const struct sshkey *, struct sshkey *);
int	 sshkey_cert_check_authority(const struct sshkey *, int, int, int,
    uint64_t, const char *, const char **);
int	 sshkey_cert_check_authority_now(const struct sshkey *, int, int, int,
    const char *, const char **);
int	 sshkey_cert_check_host(const struct sshkey *, const char *,
    int , const char *, const char **);
size_t	 sshkey_format_cert_validity(const struct sshkey_cert *,
    char *, size_t) __attribute__((__bounded__(__string__, 2, 3)));
int	 sshkey_check_cert_sigtype(const struct sshkey *, const char *);

int	 sshkey_certify(struct sshkey *, struct sshkey *,
    const char *, const char *, const char *);

typedef int sshkey_certify_signer(struct sshkey *, u_char **, size_t *,
    const u_char *, size_t, const char *, const char *, const char *,
    u_int, void *);
int	 sshkey_certify_custom(struct sshkey *, struct sshkey *, const char *,
    const char *, const char *, sshkey_certify_signer *, void *);

int		 sshkey_ecdsa_nid_from_name(const char *);
int		 sshkey_curve_name_to_nid(const char *);
const char *	 sshkey_curve_nid_to_name(int);
u_int		 sshkey_curve_nid_to_bits(int);
int		 sshkey_ecdsa_bits_to_nid(int);
int		 sshkey_ecdsa_key_to_nid(EC_KEY *);
int		 sshkey_ec_nid_to_hash_alg(int nid);
int		 sshkey_ec_validate_public(const EC_GROUP *, const EC_POINT *);
int		 sshkey_ec_validate_private(const EC_KEY *);
const char	*sshkey_ssh_name(const struct sshkey *);
const char	*sshkey_ssh_name_plain(const struct sshkey *);
int		 sshkey_names_valid2(const char *, int, int);
char		*sshkey_alg_list(int, int, int, char);

int	 sshkey_from_blob(const u_char *, size_t, struct sshkey **);
int	 sshkey_fromb(struct sshbuf *, struct sshkey **);
int	 sshkey_froms(struct sshbuf *, struct sshkey **);
int	 sshkey_to_blob(const struct sshkey *, u_char **, size_t *);
int	 sshkey_to_base64(const struct sshkey *, char **);
int	 sshkey_putb(const struct sshkey *, struct sshbuf *);
int	 sshkey_puts(const struct sshkey *, struct sshbuf *);
int	 sshkey_puts_opts(const struct sshkey *, struct sshbuf *,
    enum sshkey_serialize_rep);
int	 sshkey_plain_to_blob(const struct sshkey *, u_char **, size_t *);
int	 sshkey_putb_plain(const struct sshkey *, struct sshbuf *);

int	 sshkey_sign(struct sshkey *, u_char **, size_t *,
    const u_char *, size_t, const char *, const char *, const char *, u_int);
int	 sshkey_verify(const struct sshkey *, const u_char *, size_t,
    const u_char *, size_t, const char *, u_int, struct sshkey_sig_details **);
int	 sshkey_check_sigtype(const u_char *, size_t, const char *);
const char *sshkey_sigalg_by_name(const char *);
int	 sshkey_get_sigtype(const u_char *, size_t, char **);


void	sshkey_dump_ec_point(const EC_GROUP *, const EC_POINT *);
void	sshkey_dump_ec_key(const EC_KEY *);


int	sshkey_private_serialize(struct sshkey *key, struct sshbuf *buf);
int	sshkey_private_serialize_opt(struct sshkey *key, struct sshbuf *buf,
    enum sshkey_serialize_rep);
int	sshkey_private_deserialize(struct sshbuf *buf,  struct sshkey **keyp);


int	sshkey_private_to_fileblob(struct sshkey *key, struct sshbuf *blob,
    const char *passphrase, const char *comment,
    int format, const char *openssh_format_cipher, int openssh_format_rounds);
int	sshkey_parse_private_fileblob(struct sshbuf *buffer,
    const char *passphrase, struct sshkey **keyp, char **commentp);
int	sshkey_parse_private_fileblob_type(struct sshbuf *blob, int type,
    const char *passphrase, struct sshkey **keyp, char **commentp);
int	sshkey_parse_pubkey_from_private_fileblob_type(struct sshbuf *blob,
    int type, struct sshkey **pubkeyp);

int sshkey_check_rsa_length(const struct sshkey *, int);

int ssh_rsa_complete_crt_parameters(struct sshkey *, const BIGNUM *);


int	 sshkey_set_filename(struct sshkey *, const char *);
int	 sshkey_enable_maxsign(struct sshkey *, u_int32_t);
u_int32_t sshkey_signatures_left(const struct sshkey *);
int	 sshkey_forward_state(const struct sshkey *, u_int32_t, int);
int	 sshkey_private_serialize_maxsign(struct sshkey *key,
    struct sshbuf *buf, u_int32_t maxsign, int);

void	 sshkey_sig_details_free(struct sshkey_sig_details *);

#ifdef SSHKEY_INTERNAL
int	sshkey_sk_fields_equal(const struct sshkey *a, const struct sshkey *b);
void	sshkey_sk_cleanup(struct sshkey *k);
int	sshkey_serialize_sk(const struct sshkey *key, struct sshbuf *b);
int	sshkey_copy_public_sk(const struct sshkey *from, struct sshkey *to);
int	sshkey_deserialize_sk(struct sshbuf *b, struct sshkey *key);
int	sshkey_serialize_private_sk(const struct sshkey *key,
    struct sshbuf *buf);
int	sshkey_private_deserialize_sk(struct sshbuf *buf, struct sshkey *k);
#ifdef WITH_OPENSSL
int	check_rsa_length(const RSA *rsa); 
#endif
#endif

#if !defined(WITH_OPENSSL)
# undef RSA
# undef DSA
# undef EC_KEY
# undef EC_GROUP
# undef EC_POINT
#elif !defined(OPENSSL_HAS_ECC)
# undef EC_KEY
# undef EC_GROUP
# undef EC_POINT
#endif

#endif 
