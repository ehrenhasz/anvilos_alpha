 
 

#define SSHBUF_INTERNAL
#include "includes.h"

#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef WITH_OPENSSL
#include <openssl/bn.h>
#ifdef OPENSSL_HAS_ECC
# include <openssl/ec.h>
#endif  

#include "ssherr.h"
#include "sshbuf.h"

int
sshbuf_get_bignum2(struct sshbuf *buf, BIGNUM **valp)
{
	BIGNUM *v;
	const u_char *d;
	size_t len;
	int r;

	if (valp != NULL)
		*valp = NULL;
	if ((r = sshbuf_get_bignum2_bytes_direct(buf, &d, &len)) != 0)
		return r;
	if (valp != NULL) {
		if ((v = BN_new()) == NULL ||
		    BN_bin2bn(d, len, v) == NULL) {
			BN_clear_free(v);
			return SSH_ERR_ALLOC_FAIL;
		}
		*valp = v;
	}
	return 0;
}

#ifdef OPENSSL_HAS_ECC
static int
get_ec(const u_char *d, size_t len, EC_POINT *v, const EC_GROUP *g)
{
	 
	if (len == 0 || len > SSHBUF_MAX_ECPOINT)
		return SSH_ERR_ECPOINT_TOO_LARGE;
	 
	if (*d != POINT_CONVERSION_UNCOMPRESSED)
		return SSH_ERR_INVALID_FORMAT;
	if (v != NULL && EC_POINT_oct2point(g, v, d, len, NULL) != 1)
		return SSH_ERR_INVALID_FORMAT;  
	return 0;
}

int
sshbuf_get_ec(struct sshbuf *buf, EC_POINT *v, const EC_GROUP *g)
{
	const u_char *d;
	size_t len;
	int r;

	if ((r = sshbuf_peek_string_direct(buf, &d, &len)) < 0)
		return r;
	if ((r = get_ec(d, len, v, g)) != 0)
		return r;
	 
	if (sshbuf_get_string_direct(buf, NULL, NULL) != 0) {
		 
		SSHBUF_DBG(("SSH_ERR_INTERNAL_ERROR"));
		SSHBUF_ABORT();
		return SSH_ERR_INTERNAL_ERROR;
	}
	return 0;
}

int
sshbuf_get_eckey(struct sshbuf *buf, EC_KEY *v)
{
	EC_POINT *pt = EC_POINT_new(EC_KEY_get0_group(v));
	int r;
	const u_char *d;
	size_t len;

	if (pt == NULL) {
		SSHBUF_DBG(("SSH_ERR_ALLOC_FAIL"));
		return SSH_ERR_ALLOC_FAIL;
	}
	if ((r = sshbuf_peek_string_direct(buf, &d, &len)) < 0) {
		EC_POINT_free(pt);
		return r;
	}
	if ((r = get_ec(d, len, pt, EC_KEY_get0_group(v))) != 0) {
		EC_POINT_free(pt);
		return r;
	}
	if (EC_KEY_set_public_key(v, pt) != 1) {
		EC_POINT_free(pt);
		return SSH_ERR_ALLOC_FAIL;  
	}
	EC_POINT_free(pt);
	 
	if (sshbuf_get_string_direct(buf, NULL, NULL) != 0) {
		 
		SSHBUF_DBG(("SSH_ERR_INTERNAL_ERROR"));
		SSHBUF_ABORT();
		return SSH_ERR_INTERNAL_ERROR;
	}
	return 0;	
}
#endif  

int
sshbuf_put_bignum2(struct sshbuf *buf, const BIGNUM *v)
{
	u_char d[SSHBUF_MAX_BIGNUM + 1];
	int len = BN_num_bytes(v), prepend = 0, r;

	if (len < 0 || len > SSHBUF_MAX_BIGNUM)
		return SSH_ERR_INVALID_ARGUMENT;
	*d = '\0';
	if (BN_bn2bin(v, d + 1) != len)
		return SSH_ERR_INTERNAL_ERROR;  
	 
	if (len > 0 && (d[1] & 0x80) != 0)
		prepend = 1;
	if ((r = sshbuf_put_string(buf, d + 1 - prepend, len + prepend)) < 0) {
		explicit_bzero(d, sizeof(d));
		return r;
	}
	explicit_bzero(d, sizeof(d));
	return 0;
}

#ifdef OPENSSL_HAS_ECC
int
sshbuf_put_ec(struct sshbuf *buf, const EC_POINT *v, const EC_GROUP *g)
{
	u_char d[SSHBUF_MAX_ECPOINT];
	size_t len;
	int ret;

	if ((len = EC_POINT_point2oct(g, v, POINT_CONVERSION_UNCOMPRESSED,
	    NULL, 0, NULL)) > SSHBUF_MAX_ECPOINT) {
		return SSH_ERR_INVALID_ARGUMENT;
	}
	if (EC_POINT_point2oct(g, v, POINT_CONVERSION_UNCOMPRESSED,
	    d, len, NULL) != len) {
		return SSH_ERR_INTERNAL_ERROR;  
	}
	ret = sshbuf_put_string(buf, d, len);
	explicit_bzero(d, len);
	return ret;
}

int
sshbuf_put_eckey(struct sshbuf *buf, const EC_KEY *v)
{
	return sshbuf_put_ec(buf, EC_KEY_get0_public_key(v),
	    EC_KEY_get0_group(v));
}
#endif  
#endif  
