 
 

#include <linux/oid_registry.h>
#include <crypto/pkcs7.h>
#include "x509_parser.h"

#define kenter(FMT, ...) \
	pr_devel("==> %s("FMT")\n", __func__, ##__VA_ARGS__)
#define kleave(FMT, ...) \
	pr_devel("<== %s()"FMT"\n", __func__, ##__VA_ARGS__)

struct pkcs7_signed_info {
	struct pkcs7_signed_info *next;
	struct x509_certificate *signer;  
	unsigned	index;
	bool		unsupported_crypto;	 
	bool		blacklisted;

	 
	const void	*msgdigest;
	unsigned	msgdigest_len;

	 
	unsigned	authattrs_len;
	const void	*authattrs;
	unsigned long	aa_set;
#define	sinfo_has_content_type		0
#define	sinfo_has_signing_time		1
#define	sinfo_has_message_digest	2
#define sinfo_has_smime_caps		3
#define	sinfo_has_ms_opus_info		4
#define	sinfo_has_ms_statement_type	5
	time64_t	signing_time;

	 
	struct public_key_signature *sig;
};

struct pkcs7_message {
	struct x509_certificate *certs;	 
	struct x509_certificate *crl;	 
	struct pkcs7_signed_info *signed_infos;
	u8		version;	 
	bool		have_authattrs;	 

	 
	enum OID	data_type;	 
	size_t		data_len;	 
	size_t		data_hdrlen;	 
	const void	*data;		 
};
