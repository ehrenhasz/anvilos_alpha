
 

#define pr_fmt(fmt) "ASYM: "fmt
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <crypto/public_key.h>
#include "asymmetric_keys.h"

static bool use_builtin_keys;
static struct asymmetric_key_id *ca_keyid;

#ifndef MODULE
static struct {
	struct asymmetric_key_id id;
	unsigned char data[10];
} cakey;

static int __init ca_keys_setup(char *str)
{
	if (!str)		 
		return 1;

	if (strncmp(str, "id:", 3) == 0) {
		struct asymmetric_key_id *p = &cakey.id;
		size_t hexlen = (strlen(str) - 3) / 2;
		int ret;

		if (hexlen == 0 || hexlen > sizeof(cakey.data)) {
			pr_err("Missing or invalid ca_keys id\n");
			return 1;
		}

		ret = __asymmetric_key_hex_to_key_id(str + 3, p, hexlen);
		if (ret < 0)
			pr_err("Unparsable ca_keys id hex string\n");
		else
			ca_keyid = p;	 
	} else if (strcmp(str, "builtin") == 0) {
		use_builtin_keys = true;
	}

	return 1;
}
__setup("ca_keys=", ca_keys_setup);
#endif

 
int restrict_link_by_signature(struct key *dest_keyring,
			       const struct key_type *type,
			       const union key_payload *payload,
			       struct key *trust_keyring)
{
	const struct public_key_signature *sig;
	struct key *key;
	int ret;

	pr_devel("==>%s()\n", __func__);

	if (!trust_keyring)
		return -ENOKEY;

	if (type != &key_type_asymmetric)
		return -EOPNOTSUPP;

	sig = payload->data[asym_auth];
	if (!sig)
		return -ENOPKG;
	if (!sig->auth_ids[0] && !sig->auth_ids[1] && !sig->auth_ids[2])
		return -ENOKEY;

	if (ca_keyid && !asymmetric_key_id_partial(sig->auth_ids[1], ca_keyid))
		return -EPERM;

	 
	key = find_asymmetric_key(trust_keyring,
				  sig->auth_ids[0], sig->auth_ids[1],
				  sig->auth_ids[2], false);
	if (IS_ERR(key))
		return -ENOKEY;

	if (use_builtin_keys && !test_bit(KEY_FLAG_BUILTIN, &key->flags))
		ret = -ENOKEY;
	else
		ret = verify_signature(key, sig);
	key_put(key);
	return ret;
}

 
int restrict_link_by_ca(struct key *dest_keyring,
			const struct key_type *type,
			const union key_payload *payload,
			struct key *trust_keyring)
{
	const struct public_key *pkey;

	if (type != &key_type_asymmetric)
		return -EOPNOTSUPP;

	pkey = payload->data[asym_crypto];
	if (!pkey)
		return -ENOPKG;
	if (!test_bit(KEY_EFLAG_CA, &pkey->key_eflags))
		return -ENOKEY;
	if (!test_bit(KEY_EFLAG_KEYCERTSIGN, &pkey->key_eflags))
		return -ENOKEY;
	if (!IS_ENABLED(CONFIG_INTEGRITY_CA_MACHINE_KEYRING_MAX))
		return 0;
	if (test_bit(KEY_EFLAG_DIGITALSIG, &pkey->key_eflags))
		return -ENOKEY;

	return 0;
}

 
int restrict_link_by_digsig(struct key *dest_keyring,
			    const struct key_type *type,
			    const union key_payload *payload,
			    struct key *trust_keyring)
{
	const struct public_key *pkey;

	if (type != &key_type_asymmetric)
		return -EOPNOTSUPP;

	pkey = payload->data[asym_crypto];

	if (!pkey)
		return -ENOPKG;

	if (!test_bit(KEY_EFLAG_DIGITALSIG, &pkey->key_eflags))
		return -ENOKEY;

	if (test_bit(KEY_EFLAG_CA, &pkey->key_eflags))
		return -ENOKEY;

	if (test_bit(KEY_EFLAG_KEYCERTSIGN, &pkey->key_eflags))
		return -ENOKEY;

	return restrict_link_by_signature(dest_keyring, type, payload,
					  trust_keyring);
}

static bool match_either_id(const struct asymmetric_key_id **pair,
			    const struct asymmetric_key_id *single)
{
	return (asymmetric_key_id_same(pair[0], single) ||
		asymmetric_key_id_same(pair[1], single));
}

static int key_or_keyring_common(struct key *dest_keyring,
				 const struct key_type *type,
				 const union key_payload *payload,
				 struct key *trusted, bool check_dest)
{
	const struct public_key_signature *sig;
	struct key *key = NULL;
	int ret;

	pr_devel("==>%s()\n", __func__);

	if (!dest_keyring)
		return -ENOKEY;
	else if (dest_keyring->type != &key_type_keyring)
		return -EOPNOTSUPP;

	if (!trusted && !check_dest)
		return -ENOKEY;

	if (type != &key_type_asymmetric)
		return -EOPNOTSUPP;

	sig = payload->data[asym_auth];
	if (!sig)
		return -ENOPKG;
	if (!sig->auth_ids[0] && !sig->auth_ids[1] && !sig->auth_ids[2])
		return -ENOKEY;

	if (trusted) {
		if (trusted->type == &key_type_keyring) {
			 
			key = find_asymmetric_key(trusted, sig->auth_ids[0],
						  sig->auth_ids[1],
						  sig->auth_ids[2], false);
			if (IS_ERR(key))
				key = NULL;
		} else if (trusted->type == &key_type_asymmetric) {
			const struct asymmetric_key_id **signer_ids;

			signer_ids = (const struct asymmetric_key_id **)
				asymmetric_key_ids(trusted)->id;

			 
			if (!sig->auth_ids[0] && !sig->auth_ids[1]) {
				if (asymmetric_key_id_same(signer_ids[2],
							   sig->auth_ids[2]))
					key = __key_get(trusted);

			} else if (!sig->auth_ids[0] || !sig->auth_ids[1]) {
				const struct asymmetric_key_id *auth_id;

				auth_id = sig->auth_ids[0] ?: sig->auth_ids[1];
				if (match_either_id(signer_ids, auth_id))
					key = __key_get(trusted);

			} else if (asymmetric_key_id_same(signer_ids[1],
							  sig->auth_ids[1]) &&
				   match_either_id(signer_ids,
						   sig->auth_ids[0])) {
				key = __key_get(trusted);
			}
		} else {
			return -EOPNOTSUPP;
		}
	}

	if (check_dest && !key) {
		 
		key = find_asymmetric_key(dest_keyring, sig->auth_ids[0],
					  sig->auth_ids[1], sig->auth_ids[2],
					  false);
		if (IS_ERR(key))
			key = NULL;
	}

	if (!key)
		return -ENOKEY;

	ret = key_validate(key);
	if (ret == 0)
		ret = verify_signature(key, sig);

	key_put(key);
	return ret;
}

 
int restrict_link_by_key_or_keyring(struct key *dest_keyring,
				    const struct key_type *type,
				    const union key_payload *payload,
				    struct key *trusted)
{
	return key_or_keyring_common(dest_keyring, type, payload, trusted,
				     false);
}

 
int restrict_link_by_key_or_keyring_chain(struct key *dest_keyring,
					  const struct key_type *type,
					  const union key_payload *payload,
					  struct key *trusted)
{
	return key_or_keyring_common(dest_keyring, type, payload, trusted,
				     true);
}
