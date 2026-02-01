
 

#include <linux/export.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <keys/user-type.h>
#include <linux/uaccess.h>
#include "internal.h"

static int logon_vet_description(const char *desc);

 
struct key_type key_type_user = {
	.name			= "user",
	.preparse		= user_preparse,
	.free_preparse		= user_free_preparse,
	.instantiate		= generic_key_instantiate,
	.update			= user_update,
	.revoke			= user_revoke,
	.destroy		= user_destroy,
	.describe		= user_describe,
	.read			= user_read,
};

EXPORT_SYMBOL_GPL(key_type_user);

 
struct key_type key_type_logon = {
	.name			= "logon",
	.preparse		= user_preparse,
	.free_preparse		= user_free_preparse,
	.instantiate		= generic_key_instantiate,
	.update			= user_update,
	.revoke			= user_revoke,
	.destroy		= user_destroy,
	.describe		= user_describe,
	.vet_description	= logon_vet_description,
};
EXPORT_SYMBOL_GPL(key_type_logon);

 
int user_preparse(struct key_preparsed_payload *prep)
{
	struct user_key_payload *upayload;
	size_t datalen = prep->datalen;

	if (datalen <= 0 || datalen > 32767 || !prep->data)
		return -EINVAL;

	upayload = kmalloc(sizeof(*upayload) + datalen, GFP_KERNEL);
	if (!upayload)
		return -ENOMEM;

	 
	prep->quotalen = datalen;
	prep->payload.data[0] = upayload;
	upayload->datalen = datalen;
	memcpy(upayload->data, prep->data, datalen);
	return 0;
}
EXPORT_SYMBOL_GPL(user_preparse);

 
void user_free_preparse(struct key_preparsed_payload *prep)
{
	kfree_sensitive(prep->payload.data[0]);
}
EXPORT_SYMBOL_GPL(user_free_preparse);

static void user_free_payload_rcu(struct rcu_head *head)
{
	struct user_key_payload *payload;

	payload = container_of(head, struct user_key_payload, rcu);
	kfree_sensitive(payload);
}

 
int user_update(struct key *key, struct key_preparsed_payload *prep)
{
	struct user_key_payload *zap = NULL;
	int ret;

	 
	ret = key_payload_reserve(key, prep->datalen);
	if (ret < 0)
		return ret;

	 
	key->expiry = prep->expiry;
	if (key_is_positive(key))
		zap = dereference_key_locked(key);
	rcu_assign_keypointer(key, prep->payload.data[0]);
	prep->payload.data[0] = NULL;

	if (zap)
		call_rcu(&zap->rcu, user_free_payload_rcu);
	return ret;
}
EXPORT_SYMBOL_GPL(user_update);

 
void user_revoke(struct key *key)
{
	struct user_key_payload *upayload = user_key_payload_locked(key);

	 
	key_payload_reserve(key, 0);

	if (upayload) {
		rcu_assign_keypointer(key, NULL);
		call_rcu(&upayload->rcu, user_free_payload_rcu);
	}
}

EXPORT_SYMBOL(user_revoke);

 
void user_destroy(struct key *key)
{
	struct user_key_payload *upayload = key->payload.data[0];

	kfree_sensitive(upayload);
}

EXPORT_SYMBOL_GPL(user_destroy);

 
void user_describe(const struct key *key, struct seq_file *m)
{
	seq_puts(m, key->description);
	if (key_is_positive(key))
		seq_printf(m, ": %u", key->datalen);
}

EXPORT_SYMBOL_GPL(user_describe);

 
long user_read(const struct key *key, char *buffer, size_t buflen)
{
	const struct user_key_payload *upayload;
	long ret;

	upayload = user_key_payload_locked(key);
	ret = upayload->datalen;

	 
	if (buffer && buflen > 0) {
		if (buflen > upayload->datalen)
			buflen = upayload->datalen;

		memcpy(buffer, upayload->data, buflen);
	}

	return ret;
}

EXPORT_SYMBOL_GPL(user_read);

 
static int logon_vet_description(const char *desc)
{
	char *p;

	 
	p = strchr(desc, ':');
	if (!p)
		return -EINVAL;

	 
	if (p == desc)
		return -EINVAL;

	return 0;
}
