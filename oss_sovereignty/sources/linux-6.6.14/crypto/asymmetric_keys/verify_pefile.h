


#include <crypto/pkcs7.h>
#include <crypto/hash_info.h>

struct pefile_context {
	unsigned	header_size;
	unsigned	image_checksum_offset;
	unsigned	cert_dirent_offset;
	unsigned	n_data_dirents;
	unsigned	n_sections;
	unsigned	certs_size;
	unsigned	sig_offset;
	unsigned	sig_len;
	const struct section_header *secs;

	
	const void	*digest;		
	unsigned	digest_len;		
	const char	*digest_algo;		
};

#define kenter(FMT, ...)					\
	pr_devel("==> %s("FMT")\n", __func__, ##__VA_ARGS__)
#define kleave(FMT, ...) \
	pr_devel("<== %s()"FMT"\n", __func__, ##__VA_ARGS__)


extern int mscode_parse(void *_ctx, const void *content_data, size_t data_len,
			size_t asn1hdrlen);
