 
 

#ifndef __KEYS_ECRYPTFS_H
#define __KEYS_ECRYPTFS_H

#include <linux/ecryptfs.h>

#define PGP_DIGEST_ALGO_SHA512   10

u8 *ecryptfs_get_auth_tok_key(struct ecryptfs_auth_tok *auth_tok);
void ecryptfs_get_versions(int *major, int *minor, int *file_version);
int ecryptfs_fill_auth_tok(struct ecryptfs_auth_tok *auth_tok,
			   const char *key_desc);

#endif  
