



#ifndef DNS_H
#define DNS_H

enum sshfp_types {
	SSHFP_KEY_RESERVED = 0,
	SSHFP_KEY_RSA = 1,
	SSHFP_KEY_DSA = 2,
	SSHFP_KEY_ECDSA = 3,
	SSHFP_KEY_ED25519 = 4,
	SSHFP_KEY_XMSS = 5
};

enum sshfp_hashes {
	SSHFP_HASH_RESERVED = 0,
	SSHFP_HASH_SHA1 = 1,
	SSHFP_HASH_SHA256 = 2,
	SSHFP_HASH_MAX = 3
};

#define DNS_RDATACLASS_IN	1
#define DNS_RDATATYPE_SSHFP	44

#define DNS_VERIFY_FOUND	0x00000001
#define DNS_VERIFY_MATCH	0x00000002
#define DNS_VERIFY_SECURE	0x00000004
#define DNS_VERIFY_FAILED	0x00000008

int	verify_host_key_dns(const char *, struct sockaddr *,
    struct sshkey *, int *);
int	export_dns_rr(const char *, struct sshkey *, FILE *, int, int);

#endif 
