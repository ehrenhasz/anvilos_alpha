 
 

#include <linux/time.h>
#include <crypto/public_key.h>
#include <keys/asymmetric-type.h>

struct x509_certificate {
	struct x509_certificate *next;
	struct x509_certificate *signer;	 
	struct public_key *pub;			 
	struct public_key_signature *sig;	 
	char		*issuer;		 
	char		*subject;		 
	struct asymmetric_key_id *id;		 
	struct asymmetric_key_id *skid;		 
	time64_t	valid_from;
	time64_t	valid_to;
	const void	*tbs;			 
	unsigned	tbs_size;		 
	unsigned	raw_sig_size;		 
	const void	*raw_sig;		 
	const void	*raw_serial;		 
	unsigned	raw_serial_size;
	unsigned	raw_issuer_size;
	const void	*raw_issuer;		 
	const void	*raw_subject;		 
	unsigned	raw_subject_size;
	unsigned	raw_skid_size;
	const void	*raw_skid;		 
	unsigned	index;
	bool		seen;			 
	bool		verified;
	bool		self_signed;		 
	bool		unsupported_sig;	 
	bool		blacklisted;
};

 
extern void x509_free_certificate(struct x509_certificate *cert);
extern struct x509_certificate *x509_cert_parse(const void *data, size_t datalen);
extern int x509_decode_time(time64_t *_t,  size_t hdrlen,
			    unsigned char tag,
			    const unsigned char *value, size_t vlen);

 
extern int x509_get_sig_params(struct x509_certificate *cert);
extern int x509_check_for_self_signed(struct x509_certificate *cert);
