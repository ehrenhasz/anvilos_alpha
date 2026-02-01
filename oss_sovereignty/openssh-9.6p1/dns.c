 

 

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xmalloc.h"
#include "sshkey.h"
#include "ssherr.h"
#include "dns.h"
#include "log.h"
#include "digest.h"

static const char * const errset_text[] = {
	"success",		 
	"out of memory",	 
	"general failure",	 
	"invalid parameter",	 
	"name does not exist",	 
	"data does not exist",	 
};

static const char *
dns_result_totext(unsigned int res)
{
	switch (res) {
	case ERRSET_SUCCESS:
		return errset_text[ERRSET_SUCCESS];
	case ERRSET_NOMEMORY:
		return errset_text[ERRSET_NOMEMORY];
	case ERRSET_FAIL:
		return errset_text[ERRSET_FAIL];
	case ERRSET_INVAL:
		return errset_text[ERRSET_INVAL];
	case ERRSET_NONAME:
		return errset_text[ERRSET_NONAME];
	case ERRSET_NODATA:
		return errset_text[ERRSET_NODATA];
	default:
		return "unknown error";
	}
}

 
static int
dns_read_key(u_int8_t *algorithm, u_int8_t *digest_type,
    u_char **digest, size_t *digest_len, struct sshkey *key)
{
	int r, success = 0;
	int fp_alg = -1;

	switch (key->type) {
	case KEY_RSA:
		*algorithm = SSHFP_KEY_RSA;
		break;
	case KEY_DSA:
		*algorithm = SSHFP_KEY_DSA;
		break;
	case KEY_ECDSA:
		*algorithm = SSHFP_KEY_ECDSA;
		break;
	case KEY_ED25519:
		*algorithm = SSHFP_KEY_ED25519;
		break;
	case KEY_XMSS:
		*algorithm = SSHFP_KEY_XMSS;
		break;
	default:
		*algorithm = SSHFP_KEY_RESERVED;  
	}

	switch (*digest_type) {
	case SSHFP_HASH_SHA1:
		fp_alg = SSH_DIGEST_SHA1;
		break;
	case SSHFP_HASH_SHA256:
		fp_alg = SSH_DIGEST_SHA256;
		break;
	default:
		*digest_type = SSHFP_HASH_RESERVED;  
	}

	if (*algorithm && *digest_type) {
		if ((r = sshkey_fingerprint_raw(key, fp_alg, digest,
		    digest_len)) != 0)
			fatal_fr(r, "sshkey_fingerprint_raw");
		success = 1;
	} else {
		*digest = NULL;
		*digest_len = 0;
	}

	return success;
}

 
static int
dns_read_rdata(u_int8_t *algorithm, u_int8_t *digest_type,
    u_char **digest, size_t *digest_len, u_char *rdata, int rdata_len)
{
	int success = 0;

	*algorithm = SSHFP_KEY_RESERVED;
	*digest_type = SSHFP_HASH_RESERVED;

	if (rdata_len >= 2) {
		*algorithm = rdata[0];
		*digest_type = rdata[1];
		*digest_len = rdata_len - 2;

		if (*digest_len > 0) {
			*digest = xmalloc(*digest_len);
			memcpy(*digest, rdata + 2, *digest_len);
		} else {
			*digest = (u_char *)xstrdup("");
		}

		success = 1;
	}

	return success;
}

 
static int
is_numeric_hostname(const char *hostname)
{
	struct addrinfo hints, *ai;

	 
	if (hostname == NULL) {
		error("is_numeric_hostname called with NULL hostname");
		return -1;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICHOST;

	if (getaddrinfo(hostname, NULL, &hints, &ai) == 0) {
		freeaddrinfo(ai);
		return -1;
	}

	return 0;
}

 
int
verify_host_key_dns(const char *hostname, struct sockaddr *address,
    struct sshkey *hostkey, int *flags)
{
	u_int counter;
	int result;
	struct rrsetinfo *fingerprints = NULL;

	u_int8_t hostkey_algorithm;
	u_char *hostkey_digest;
	size_t hostkey_digest_len;

	u_int8_t dnskey_algorithm;
	u_int8_t dnskey_digest_type;
	u_char *dnskey_digest;
	size_t dnskey_digest_len;

	*flags = 0;

	debug3("verify_host_key_dns");
	if (hostkey == NULL)
		fatal("No key to look up!");

	if (is_numeric_hostname(hostname)) {
		debug("skipped DNS lookup for numerical hostname");
		return -1;
	}

	result = getrrsetbyname(hostname, DNS_RDATACLASS_IN,
	    DNS_RDATATYPE_SSHFP, 0, &fingerprints);
	if (result) {
		verbose("DNS lookup error: %s", dns_result_totext(result));
		return -1;
	}

	if (fingerprints->rri_flags & RRSET_VALIDATED) {
		*flags |= DNS_VERIFY_SECURE;
		debug("found %d secure fingerprints in DNS",
		    fingerprints->rri_nrdatas);
	} else {
		debug("found %d insecure fingerprints in DNS",
		    fingerprints->rri_nrdatas);
	}

	if (fingerprints->rri_nrdatas)
		*flags |= DNS_VERIFY_FOUND;

	for (counter = 0; counter < fingerprints->rri_nrdatas; counter++) {
		 
		if (!dns_read_rdata(&dnskey_algorithm, &dnskey_digest_type,
		    &dnskey_digest, &dnskey_digest_len,
		    fingerprints->rri_rdatas[counter].rdi_data,
		    fingerprints->rri_rdatas[counter].rdi_length)) {
			verbose("Error parsing fingerprint from DNS.");
			continue;
		}
		debug3_f("checking SSHFP type %d fptype %d", dnskey_algorithm,
		    dnskey_digest_type);

		 
		if (!dns_read_key(&hostkey_algorithm, &dnskey_digest_type,
		    &hostkey_digest, &hostkey_digest_len, hostkey)) {
			error("Error calculating key fingerprint.");
			free(dnskey_digest);
			freerrset(fingerprints);
			return -1;
		}

		 
		if (hostkey_algorithm == dnskey_algorithm &&
		    hostkey_digest_len == dnskey_digest_len) {
			if (timingsafe_bcmp(hostkey_digest, dnskey_digest,
			    hostkey_digest_len) == 0) {
				debug_f("matched SSHFP type %d fptype %d",
				    dnskey_algorithm, dnskey_digest_type);
				*flags |= DNS_VERIFY_MATCH;
			} else {
				debug_f("failed SSHFP type %d fptype %d",
				    dnskey_algorithm, dnskey_digest_type);
				*flags |= DNS_VERIFY_FAILED;
			}
		}
		free(dnskey_digest);
		free(hostkey_digest);  
	}

	freerrset(fingerprints);

	 
	if (*flags & DNS_VERIFY_FAILED)
		*flags &= ~DNS_VERIFY_MATCH;

	if (*flags & DNS_VERIFY_FOUND)
		if (*flags & DNS_VERIFY_MATCH)
			debug("matching host key fingerprint found in DNS");
		else
			debug("mismatching host key fingerprint found in DNS");
	else
		debug("no host key fingerprint found in DNS");

	return 0;
}

 
int
export_dns_rr(const char *hostname, struct sshkey *key, FILE *f, int generic,
    int alg)
{
	u_int8_t rdata_pubkey_algorithm = 0;
	u_int8_t rdata_digest_type = SSHFP_HASH_RESERVED;
	u_int8_t dtype;
	u_char *rdata_digest;
	size_t i, rdata_digest_len;
	int success = 0;

	for (dtype = SSHFP_HASH_SHA1; dtype < SSHFP_HASH_MAX; dtype++) {
		if (alg != -1 && dtype != alg)
			continue;
		rdata_digest_type = dtype;
		if (dns_read_key(&rdata_pubkey_algorithm, &rdata_digest_type,
		    &rdata_digest, &rdata_digest_len, key)) {
			if (generic) {
				fprintf(f, "%s IN TYPE%d \\# %zu %02x %02x ",
				    hostname, DNS_RDATATYPE_SSHFP,
				    2 + rdata_digest_len,
				    rdata_pubkey_algorithm, rdata_digest_type);
			} else {
				fprintf(f, "%s IN SSHFP %d %d ", hostname,
				    rdata_pubkey_algorithm, rdata_digest_type);
			}
			for (i = 0; i < rdata_digest_len; i++)
				fprintf(f, "%02x", rdata_digest[i]);
			fprintf(f, "\n");
			free(rdata_digest);  
			success = 1;
		}
	}

	 
	if (success == 0) {
		error_f("unsupported algorithm and/or digest_type");
	}

	return success;
}
