
 

#include <linux/export.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/security.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/user_namespace.h>
#include <linux/nsproxy.h>
#include <keys/keyring-type.h>
#include <keys/user-type.h>
#include <linux/assoc_array_priv.h>
#include <linux/uaccess.h>
#include <net/net_namespace.h>
#include "internal.h"

 
#define KEYRING_SEARCH_MAX_DEPTH 6

 
#define KEYRING_PTR_SUBTYPE	0x2UL

static inline bool keyring_ptr_is_keyring(const struct assoc_array_ptr *x)
{
	return (unsigned long)x & KEYRING_PTR_SUBTYPE;
}
static inline struct key *keyring_ptr_to_key(const struct assoc_array_ptr *x)
{
	void *object = assoc_array_ptr_to_leaf(x);
	return (struct key *)((unsigned long)object & ~KEYRING_PTR_SUBTYPE);
}
static inline void *keyring_key_to_ptr(struct key *key)
{
	if (key->type == &key_type_keyring)
		return (void *)((unsigned long)key | KEYRING_PTR_SUBTYPE);
	return key;
}

static DEFINE_RWLOCK(keyring_name_lock);

 
void key_free_user_ns(struct user_namespace *ns)
{
	write_lock(&keyring_name_lock);
	list_del_init(&ns->keyring_name_list);
	write_unlock(&keyring_name_lock);

	key_put(ns->user_keyring_register);
#ifdef CONFIG_PERSISTENT_KEYRINGS
	key_put(ns->persistent_keyring_register);
#endif
}

 
static int keyring_preparse(struct key_preparsed_payload *prep);
static void keyring_free_preparse(struct key_preparsed_payload *prep);
static int keyring_instantiate(struct key *keyring,
			       struct key_preparsed_payload *prep);
static void keyring_revoke(struct key *keyring);
static void keyring_destroy(struct key *keyring);
static void keyring_describe(const struct key *keyring, struct seq_file *m);
static long keyring_read(const struct key *keyring,
			 char *buffer, size_t buflen);

struct key_type key_type_keyring = {
	.name		= "keyring",
	.def_datalen	= 0,
	.preparse	= keyring_preparse,
	.free_preparse	= keyring_free_preparse,
	.instantiate	= keyring_instantiate,
	.revoke		= keyring_revoke,
	.destroy	= keyring_destroy,
	.describe	= keyring_describe,
	.read		= keyring_read,
};
EXPORT_SYMBOL(key_type_keyring);

 
static DEFINE_MUTEX(keyring_serialise_link_lock);

 
static void keyring_publish_name(struct key *keyring)
{
	struct user_namespace *ns = current_user_ns();

	if (keyring->description &&
	    keyring->description[0] &&
	    keyring->description[0] != '.') {
		write_lock(&keyring_name_lock);
		list_add_tail(&keyring->name_link, &ns->keyring_name_list);
		write_unlock(&keyring_name_lock);
	}
}

 
static int keyring_preparse(struct key_preparsed_payload *prep)
{
	return prep->datalen != 0 ? -EINVAL : 0;
}

 
static void keyring_free_preparse(struct key_preparsed_payload *prep)
{
}

 
static int keyring_instantiate(struct key *keyring,
			       struct key_preparsed_payload *prep)
{
	assoc_array_init(&keyring->keys);
	 
	keyring_publish_name(keyring);
	return 0;
}

 
static u64 mult_64x32_and_fold(u64 x, u32 y)
{
	u64 hi = (u64)(u32)(x >> 32) * y;
	u64 lo = (u64)(u32)(x) * y;
	return lo + ((u64)(u32)hi << 32) + (u32)(hi >> 32);
}

 
static void hash_key_type_and_desc(struct keyring_index_key *index_key)
{
	const unsigned level_shift = ASSOC_ARRAY_LEVEL_STEP;
	const unsigned long fan_mask = ASSOC_ARRAY_FAN_MASK;
	const char *description = index_key->description;
	unsigned long hash, type;
	u32 piece;
	u64 acc;
	int n, desc_len = index_key->desc_len;

	type = (unsigned long)index_key->type;
	acc = mult_64x32_and_fold(type, desc_len + 13);
	acc = mult_64x32_and_fold(acc, 9207);
	piece = (unsigned long)index_key->domain_tag;
	acc = mult_64x32_and_fold(acc, piece);
	acc = mult_64x32_and_fold(acc, 9207);

	for (;;) {
		n = desc_len;
		if (n <= 0)
			break;
		if (n > 4)
			n = 4;
		piece = 0;
		memcpy(&piece, description, n);
		description += n;
		desc_len -= n;
		acc = mult_64x32_and_fold(acc, piece);
		acc = mult_64x32_and_fold(acc, 9207);
	}

	 
	hash = acc;
	if (ASSOC_ARRAY_KEY_CHUNK_SIZE == 32)
		hash ^= acc >> 32;

	 
	if (index_key->type != &key_type_keyring && (hash & fan_mask) == 0)
		hash |= (hash >> (ASSOC_ARRAY_KEY_CHUNK_SIZE - level_shift)) | 1;
	else if (index_key->type == &key_type_keyring && (hash & fan_mask) != 0)
		hash = (hash + (hash << level_shift)) & ~fan_mask;
	index_key->hash = hash;
}

 
void key_set_index_key(struct keyring_index_key *index_key)
{
	static struct key_tag default_domain_tag = { .usage = REFCOUNT_INIT(1), };
	size_t n = min_t(size_t, index_key->desc_len, sizeof(index_key->desc));

	memcpy(index_key->desc, index_key->description, n);

	if (!index_key->domain_tag) {
		if (index_key->type->flags & KEY_TYPE_NET_DOMAIN)
			index_key->domain_tag = current->nsproxy->net_ns->key_domain;
		else
			index_key->domain_tag = &default_domain_tag;
	}

	hash_key_type_and_desc(index_key);
}

 
bool key_put_tag(struct key_tag *tag)
{
	if (refcount_dec_and_test(&tag->usage)) {
		kfree_rcu(tag, rcu);
		return true;
	}

	return false;
}

 
void key_remove_domain(struct key_tag *domain_tag)
{
	domain_tag->removed = true;
	if (!key_put_tag(domain_tag))
		key_schedule_gc_links();
}

 
static unsigned long keyring_get_key_chunk(const void *data, int level)
{
	const struct keyring_index_key *index_key = data;
	unsigned long chunk = 0;
	const u8 *d;
	int desc_len = index_key->desc_len, n = sizeof(chunk);

	level /= ASSOC_ARRAY_KEY_CHUNK_SIZE;
	switch (level) {
	case 0:
		return index_key->hash;
	case 1:
		return index_key->x;
	case 2:
		return (unsigned long)index_key->type;
	case 3:
		return (unsigned long)index_key->domain_tag;
	default:
		level -= 4;
		if (desc_len <= sizeof(index_key->desc))
			return 0;

		d = index_key->description + sizeof(index_key->desc);
		d += level * sizeof(long);
		desc_len -= sizeof(index_key->desc);
		if (desc_len > n)
			desc_len = n;
		do {
			chunk <<= 8;
			chunk |= *d++;
		} while (--desc_len > 0);
		return chunk;
	}
}

static unsigned long keyring_get_object_key_chunk(const void *object, int level)
{
	const struct key *key = keyring_ptr_to_key(object);
	return keyring_get_key_chunk(&key->index_key, level);
}

static bool keyring_compare_object(const void *object, const void *data)
{
	const struct keyring_index_key *index_key = data;
	const struct key *key = keyring_ptr_to_key(object);

	return key->index_key.type == index_key->type &&
		key->index_key.domain_tag == index_key->domain_tag &&
		key->index_key.desc_len == index_key->desc_len &&
		memcmp(key->index_key.description, index_key->description,
		       index_key->desc_len) == 0;
}

 
static int keyring_diff_objects(const void *object, const void *data)
{
	const struct key *key_a = keyring_ptr_to_key(object);
	const struct keyring_index_key *a = &key_a->index_key;
	const struct keyring_index_key *b = data;
	unsigned long seg_a, seg_b;
	int level, i;

	level = 0;
	seg_a = a->hash;
	seg_b = b->hash;
	if ((seg_a ^ seg_b) != 0)
		goto differ;
	level += ASSOC_ARRAY_KEY_CHUNK_SIZE / 8;

	 
	seg_a = a->x;
	seg_b = b->x;
	if ((seg_a ^ seg_b) != 0)
		goto differ;
	level += sizeof(unsigned long);

	 
	seg_a = (unsigned long)a->type;
	seg_b = (unsigned long)b->type;
	if ((seg_a ^ seg_b) != 0)
		goto differ;
	level += sizeof(unsigned long);

	seg_a = (unsigned long)a->domain_tag;
	seg_b = (unsigned long)b->domain_tag;
	if ((seg_a ^ seg_b) != 0)
		goto differ;
	level += sizeof(unsigned long);

	i = sizeof(a->desc);
	if (a->desc_len <= i)
		goto same;

	for (; i < a->desc_len; i++) {
		seg_a = *(unsigned char *)(a->description + i);
		seg_b = *(unsigned char *)(b->description + i);
		if ((seg_a ^ seg_b) != 0)
			goto differ_plus_i;
	}

same:
	return -1;

differ_plus_i:
	level += i;
differ:
	i = level * 8 + __ffs(seg_a ^ seg_b);
	return i;
}

 
static void keyring_free_object(void *object)
{
	key_put(keyring_ptr_to_key(object));
}

 
static const struct assoc_array_ops keyring_assoc_array_ops = {
	.get_key_chunk		= keyring_get_key_chunk,
	.get_object_key_chunk	= keyring_get_object_key_chunk,
	.compare_object		= keyring_compare_object,
	.diff_objects		= keyring_diff_objects,
	.free_object		= keyring_free_object,
};

 
static void keyring_destroy(struct key *keyring)
{
	if (keyring->description) {
		write_lock(&keyring_name_lock);

		if (keyring->name_link.next != NULL &&
		    !list_empty(&keyring->name_link))
			list_del(&keyring->name_link);

		write_unlock(&keyring_name_lock);
	}

	if (keyring->restrict_link) {
		struct key_restriction *keyres = keyring->restrict_link;

		key_put(keyres->key);
		kfree(keyres);
	}

	assoc_array_destroy(&keyring->keys, &keyring_assoc_array_ops);
}

 
static void keyring_describe(const struct key *keyring, struct seq_file *m)
{
	if (keyring->description)
		seq_puts(m, keyring->description);
	else
		seq_puts(m, "[anon]");

	if (key_is_positive(keyring)) {
		if (keyring->keys.nr_leaves_on_tree != 0)
			seq_printf(m, ": %lu", keyring->keys.nr_leaves_on_tree);
		else
			seq_puts(m, ": empty");
	}
}

struct keyring_read_iterator_context {
	size_t			buflen;
	size_t			count;
	key_serial_t		*buffer;
};

static int keyring_read_iterator(const void *object, void *data)
{
	struct keyring_read_iterator_context *ctx = data;
	const struct key *key = keyring_ptr_to_key(object);

	kenter("{%s,%d},,{%zu/%zu}",
	       key->type->name, key->serial, ctx->count, ctx->buflen);

	if (ctx->count >= ctx->buflen)
		return 1;

	*ctx->buffer++ = key->serial;
	ctx->count += sizeof(key->serial);
	return 0;
}

 
static long keyring_read(const struct key *keyring,
			 char *buffer, size_t buflen)
{
	struct keyring_read_iterator_context ctx;
	long ret;

	kenter("{%d},,%zu", key_serial(keyring), buflen);

	if (buflen & (sizeof(key_serial_t) - 1))
		return -EINVAL;

	 
	if (buffer && buflen) {
		ctx.buffer = (key_serial_t *)buffer;
		ctx.buflen = buflen;
		ctx.count = 0;
		ret = assoc_array_iterate(&keyring->keys,
					  keyring_read_iterator, &ctx);
		if (ret < 0) {
			kleave(" = %ld [iterate]", ret);
			return ret;
		}
	}

	 
	ret = keyring->keys.nr_leaves_on_tree * sizeof(key_serial_t);
	if (ret <= buflen)
		kleave("= %ld [ok]", ret);
	else
		kleave("= %ld [buffer too small]", ret);
	return ret;
}

 
struct key *keyring_alloc(const char *description, kuid_t uid, kgid_t gid,
			  const struct cred *cred, key_perm_t perm,
			  unsigned long flags,
			  struct key_restriction *restrict_link,
			  struct key *dest)
{
	struct key *keyring;
	int ret;

	keyring = key_alloc(&key_type_keyring, description,
			    uid, gid, cred, perm, flags, restrict_link);
	if (!IS_ERR(keyring)) {
		ret = key_instantiate_and_link(keyring, NULL, 0, dest, NULL);
		if (ret < 0) {
			key_put(keyring);
			keyring = ERR_PTR(ret);
		}
	}

	return keyring;
}
EXPORT_SYMBOL(keyring_alloc);

 
int restrict_link_reject(struct key *keyring,
			 const struct key_type *type,
			 const union key_payload *payload,
			 struct key *restriction_key)
{
	return -EPERM;
}

 
bool key_default_cmp(const struct key *key,
		     const struct key_match_data *match_data)
{
	return strcmp(key->description, match_data->raw_data) == 0;
}

 
static int keyring_search_iterator(const void *object, void *iterator_data)
{
	struct keyring_search_context *ctx = iterator_data;
	const struct key *key = keyring_ptr_to_key(object);
	unsigned long kflags = READ_ONCE(key->flags);
	short state = READ_ONCE(key->state);

	kenter("{%d}", key->serial);

	 
	if (key->type != ctx->index_key.type) {
		kleave(" = 0 [!type]");
		return 0;
	}

	 
	if (ctx->flags & KEYRING_SEARCH_DO_STATE_CHECK) {
		time64_t expiry = READ_ONCE(key->expiry);

		if (kflags & ((1 << KEY_FLAG_INVALIDATED) |
			      (1 << KEY_FLAG_REVOKED))) {
			ctx->result = ERR_PTR(-EKEYREVOKED);
			kleave(" = %d [invrev]", ctx->skipped_ret);
			goto skipped;
		}

		if (expiry && ctx->now >= expiry) {
			if (!(ctx->flags & KEYRING_SEARCH_SKIP_EXPIRED))
				ctx->result = ERR_PTR(-EKEYEXPIRED);
			kleave(" = %d [expire]", ctx->skipped_ret);
			goto skipped;
		}
	}

	 
	if (!ctx->match_data.cmp(key, &ctx->match_data)) {
		kleave(" = 0 [!match]");
		return 0;
	}

	 
	if (!(ctx->flags & KEYRING_SEARCH_NO_CHECK_PERM) &&
	    key_task_permission(make_key_ref(key, ctx->possessed),
				ctx->cred, KEY_NEED_SEARCH) < 0) {
		ctx->result = ERR_PTR(-EACCES);
		kleave(" = %d [!perm]", ctx->skipped_ret);
		goto skipped;
	}

	if (ctx->flags & KEYRING_SEARCH_DO_STATE_CHECK) {
		 
		if (state < 0) {
			ctx->result = ERR_PTR(state);
			kleave(" = %d [neg]", ctx->skipped_ret);
			goto skipped;
		}
	}

	 
	ctx->result = make_key_ref(key, ctx->possessed);
	kleave(" = 1 [found]");
	return 1;

skipped:
	return ctx->skipped_ret;
}

 
static int search_keyring(struct key *keyring, struct keyring_search_context *ctx)
{
	if (ctx->match_data.lookup_type == KEYRING_SEARCH_LOOKUP_DIRECT) {
		const void *object;

		object = assoc_array_find(&keyring->keys,
					  &keyring_assoc_array_ops,
					  &ctx->index_key);
		return object ? ctx->iterator(object, ctx) : 0;
	}
	return assoc_array_iterate(&keyring->keys, ctx->iterator, ctx);
}

 
static bool search_nested_keyrings(struct key *keyring,
				   struct keyring_search_context *ctx)
{
	struct {
		struct key *keyring;
		struct assoc_array_node *node;
		int slot;
	} stack[KEYRING_SEARCH_MAX_DEPTH];

	struct assoc_array_shortcut *shortcut;
	struct assoc_array_node *node;
	struct assoc_array_ptr *ptr;
	struct key *key;
	int sp = 0, slot;

	kenter("{%d},{%s,%s}",
	       keyring->serial,
	       ctx->index_key.type->name,
	       ctx->index_key.description);

#define STATE_CHECKS (KEYRING_SEARCH_NO_STATE_CHECK | KEYRING_SEARCH_DO_STATE_CHECK)
	BUG_ON((ctx->flags & STATE_CHECKS) == 0 ||
	       (ctx->flags & STATE_CHECKS) == STATE_CHECKS);

	if (ctx->index_key.description)
		key_set_index_key(&ctx->index_key);

	 
	if (ctx->match_data.lookup_type == KEYRING_SEARCH_LOOKUP_ITERATE ||
	    keyring_compare_object(keyring, &ctx->index_key)) {
		ctx->skipped_ret = 2;
		switch (ctx->iterator(keyring_key_to_ptr(keyring), ctx)) {
		case 1:
			goto found;
		case 2:
			return false;
		default:
			break;
		}
	}

	ctx->skipped_ret = 0;

	 
descend_to_keyring:
	kdebug("descend to %d", keyring->serial);
	if (keyring->flags & ((1 << KEY_FLAG_INVALIDATED) |
			      (1 << KEY_FLAG_REVOKED)))
		goto not_this_keyring;

	 
	if (search_keyring(keyring, ctx))
		goto found;

	 
	if (!(ctx->flags & KEYRING_SEARCH_RECURSE))
		goto not_this_keyring;

	ptr = READ_ONCE(keyring->keys.root);
	if (!ptr)
		goto not_this_keyring;

	if (assoc_array_ptr_is_shortcut(ptr)) {
		 
		shortcut = assoc_array_ptr_to_shortcut(ptr);
		if ((shortcut->index_key[0] & ASSOC_ARRAY_FAN_MASK) != 0)
			goto not_this_keyring;

		ptr = READ_ONCE(shortcut->next_node);
		node = assoc_array_ptr_to_node(ptr);
		goto begin_node;
	}

	node = assoc_array_ptr_to_node(ptr);
	ptr = node->slots[0];
	if (!assoc_array_ptr_is_meta(ptr))
		goto begin_node;

descend_to_node:
	 
	kdebug("descend");
	if (assoc_array_ptr_is_shortcut(ptr)) {
		shortcut = assoc_array_ptr_to_shortcut(ptr);
		ptr = READ_ONCE(shortcut->next_node);
		BUG_ON(!assoc_array_ptr_is_node(ptr));
	}
	node = assoc_array_ptr_to_node(ptr);

begin_node:
	kdebug("begin_node");
	slot = 0;
ascend_to_node:
	 
	for (; slot < ASSOC_ARRAY_FAN_OUT; slot++) {
		ptr = READ_ONCE(node->slots[slot]);

		if (assoc_array_ptr_is_meta(ptr) && node->back_pointer)
			goto descend_to_node;

		if (!keyring_ptr_is_keyring(ptr))
			continue;

		key = keyring_ptr_to_key(ptr);

		if (sp >= KEYRING_SEARCH_MAX_DEPTH) {
			if (ctx->flags & KEYRING_SEARCH_DETECT_TOO_DEEP) {
				ctx->result = ERR_PTR(-ELOOP);
				return false;
			}
			goto not_this_keyring;
		}

		 
		if (!(ctx->flags & KEYRING_SEARCH_NO_CHECK_PERM) &&
		    key_task_permission(make_key_ref(key, ctx->possessed),
					ctx->cred, KEY_NEED_SEARCH) < 0)
			continue;

		 
		stack[sp].keyring = keyring;
		stack[sp].node = node;
		stack[sp].slot = slot;
		sp++;

		 
		keyring = key;
		goto descend_to_keyring;
	}

	 
	ptr = READ_ONCE(node->back_pointer);
	slot = node->parent_slot;

	if (ptr && assoc_array_ptr_is_shortcut(ptr)) {
		shortcut = assoc_array_ptr_to_shortcut(ptr);
		ptr = READ_ONCE(shortcut->back_pointer);
		slot = shortcut->parent_slot;
	}
	if (!ptr)
		goto not_this_keyring;
	node = assoc_array_ptr_to_node(ptr);
	slot++;

	 
	if (node->back_pointer) {
		kdebug("ascend %d", slot);
		goto ascend_to_node;
	}

	 
not_this_keyring:
	kdebug("not_this_keyring %d", sp);
	if (sp <= 0) {
		kleave(" = false");
		return false;
	}

	 
	sp--;
	keyring = stack[sp].keyring;
	node = stack[sp].node;
	slot = stack[sp].slot + 1;
	kdebug("ascend to %d [%d]", keyring->serial, slot);
	goto ascend_to_node;

	 
found:
	key = key_ref_to_ptr(ctx->result);
	key_check(key);
	if (!(ctx->flags & KEYRING_SEARCH_NO_UPDATE_TIME)) {
		key->last_used_at = ctx->now;
		keyring->last_used_at = ctx->now;
		while (sp > 0)
			stack[--sp].keyring->last_used_at = ctx->now;
	}
	kleave(" = true");
	return true;
}

 
key_ref_t keyring_search_rcu(key_ref_t keyring_ref,
			     struct keyring_search_context *ctx)
{
	struct key *keyring;
	long err;

	ctx->iterator = keyring_search_iterator;
	ctx->possessed = is_key_possessed(keyring_ref);
	ctx->result = ERR_PTR(-EAGAIN);

	keyring = key_ref_to_ptr(keyring_ref);
	key_check(keyring);

	if (keyring->type != &key_type_keyring)
		return ERR_PTR(-ENOTDIR);

	if (!(ctx->flags & KEYRING_SEARCH_NO_CHECK_PERM)) {
		err = key_task_permission(keyring_ref, ctx->cred, KEY_NEED_SEARCH);
		if (err < 0)
			return ERR_PTR(err);
	}

	ctx->now = ktime_get_real_seconds();
	if (search_nested_keyrings(keyring, ctx))
		__key_get(key_ref_to_ptr(ctx->result));
	return ctx->result;
}

 
key_ref_t keyring_search(key_ref_t keyring,
			 struct key_type *type,
			 const char *description,
			 bool recurse)
{
	struct keyring_search_context ctx = {
		.index_key.type		= type,
		.index_key.description	= description,
		.index_key.desc_len	= strlen(description),
		.cred			= current_cred(),
		.match_data.cmp		= key_default_cmp,
		.match_data.raw_data	= description,
		.match_data.lookup_type	= KEYRING_SEARCH_LOOKUP_DIRECT,
		.flags			= KEYRING_SEARCH_DO_STATE_CHECK,
	};
	key_ref_t key;
	int ret;

	if (recurse)
		ctx.flags |= KEYRING_SEARCH_RECURSE;
	if (type->match_preparse) {
		ret = type->match_preparse(&ctx.match_data);
		if (ret < 0)
			return ERR_PTR(ret);
	}

	rcu_read_lock();
	key = keyring_search_rcu(keyring, &ctx);
	rcu_read_unlock();

	if (type->match_free)
		type->match_free(&ctx.match_data);
	return key;
}
EXPORT_SYMBOL(keyring_search);

static struct key_restriction *keyring_restriction_alloc(
	key_restrict_link_func_t check)
{
	struct key_restriction *keyres =
		kzalloc(sizeof(struct key_restriction), GFP_KERNEL);

	if (!keyres)
		return ERR_PTR(-ENOMEM);

	keyres->check = check;

	return keyres;
}

 
static DECLARE_RWSEM(keyring_serialise_restrict_sem);

 
static bool keyring_detect_restriction_cycle(const struct key *dest_keyring,
					     struct key_restriction *keyres)
{
	while (keyres && keyres->key &&
	       keyres->key->type == &key_type_keyring) {
		if (keyres->key == dest_keyring)
			return true;

		keyres = keyres->key->restrict_link;
	}

	return false;
}

 
int keyring_restrict(key_ref_t keyring_ref, const char *type,
		     const char *restriction)
{
	struct key *keyring;
	struct key_type *restrict_type = NULL;
	struct key_restriction *restrict_link;
	int ret = 0;

	keyring = key_ref_to_ptr(keyring_ref);
	key_check(keyring);

	if (keyring->type != &key_type_keyring)
		return -ENOTDIR;

	if (!type) {
		restrict_link = keyring_restriction_alloc(restrict_link_reject);
	} else {
		restrict_type = key_type_lookup(type);

		if (IS_ERR(restrict_type))
			return PTR_ERR(restrict_type);

		if (!restrict_type->lookup_restriction) {
			ret = -ENOENT;
			goto error;
		}

		restrict_link = restrict_type->lookup_restriction(restriction);
	}

	if (IS_ERR(restrict_link)) {
		ret = PTR_ERR(restrict_link);
		goto error;
	}

	down_write(&keyring->sem);
	down_write(&keyring_serialise_restrict_sem);

	if (keyring->restrict_link) {
		ret = -EEXIST;
	} else if (keyring_detect_restriction_cycle(keyring, restrict_link)) {
		ret = -EDEADLK;
	} else {
		keyring->restrict_link = restrict_link;
		notify_key(keyring, NOTIFY_KEY_SETATTR, 0);
	}

	up_write(&keyring_serialise_restrict_sem);
	up_write(&keyring->sem);

	if (ret < 0) {
		key_put(restrict_link->key);
		kfree(restrict_link);
	}

error:
	if (restrict_type)
		key_type_put(restrict_type);

	return ret;
}
EXPORT_SYMBOL(keyring_restrict);

 
key_ref_t find_key_to_update(key_ref_t keyring_ref,
			     const struct keyring_index_key *index_key)
{
	struct key *keyring, *key;
	const void *object;

	keyring = key_ref_to_ptr(keyring_ref);

	kenter("{%d},{%s,%s}",
	       keyring->serial, index_key->type->name, index_key->description);

	object = assoc_array_find(&keyring->keys, &keyring_assoc_array_ops,
				  index_key);

	if (object)
		goto found;

	kleave(" = NULL");
	return NULL;

found:
	key = keyring_ptr_to_key(object);
	if (key->flags & ((1 << KEY_FLAG_INVALIDATED) |
			  (1 << KEY_FLAG_REVOKED))) {
		kleave(" = NULL [x]");
		return NULL;
	}
	__key_get(key);
	kleave(" = {%d}", key->serial);
	return make_key_ref(key, is_key_possessed(keyring_ref));
}

 
struct key *find_keyring_by_name(const char *name, bool uid_keyring)
{
	struct user_namespace *ns = current_user_ns();
	struct key *keyring;

	if (!name)
		return ERR_PTR(-EINVAL);

	read_lock(&keyring_name_lock);

	 
	list_for_each_entry(keyring, &ns->keyring_name_list, name_link) {
		if (!kuid_has_mapping(ns, keyring->user->uid))
			continue;

		if (test_bit(KEY_FLAG_REVOKED, &keyring->flags))
			continue;

		if (strcmp(keyring->description, name) != 0)
			continue;

		if (uid_keyring) {
			if (!test_bit(KEY_FLAG_UID_KEYRING,
				      &keyring->flags))
				continue;
		} else {
			if (key_permission(make_key_ref(keyring, 0),
					   KEY_NEED_SEARCH) < 0)
				continue;
		}

		 
		if (!refcount_inc_not_zero(&keyring->usage))
			continue;
		keyring->last_used_at = ktime_get_real_seconds();
		goto out;
	}

	keyring = ERR_PTR(-ENOKEY);
out:
	read_unlock(&keyring_name_lock);
	return keyring;
}

static int keyring_detect_cycle_iterator(const void *object,
					 void *iterator_data)
{
	struct keyring_search_context *ctx = iterator_data;
	const struct key *key = keyring_ptr_to_key(object);

	kenter("{%d}", key->serial);

	 
	if (key != ctx->match_data.raw_data)
		return 0;

	ctx->result = ERR_PTR(-EDEADLK);
	return 1;
}

 
static int keyring_detect_cycle(struct key *A, struct key *B)
{
	struct keyring_search_context ctx = {
		.index_key		= A->index_key,
		.match_data.raw_data	= A,
		.match_data.lookup_type = KEYRING_SEARCH_LOOKUP_DIRECT,
		.iterator		= keyring_detect_cycle_iterator,
		.flags			= (KEYRING_SEARCH_NO_STATE_CHECK |
					   KEYRING_SEARCH_NO_UPDATE_TIME |
					   KEYRING_SEARCH_NO_CHECK_PERM |
					   KEYRING_SEARCH_DETECT_TOO_DEEP |
					   KEYRING_SEARCH_RECURSE),
	};

	rcu_read_lock();
	search_nested_keyrings(B, &ctx);
	rcu_read_unlock();
	return PTR_ERR(ctx.result) == -EAGAIN ? 0 : PTR_ERR(ctx.result);
}

 
int __key_link_lock(struct key *keyring,
		    const struct keyring_index_key *index_key)
	__acquires(&keyring->sem)
	__acquires(&keyring_serialise_link_lock)
{
	if (keyring->type != &key_type_keyring)
		return -ENOTDIR;

	down_write(&keyring->sem);

	 
	if (index_key->type == &key_type_keyring)
		mutex_lock(&keyring_serialise_link_lock);

	return 0;
}

 
int __key_move_lock(struct key *l_keyring, struct key *u_keyring,
		    const struct keyring_index_key *index_key)
	__acquires(&l_keyring->sem)
	__acquires(&u_keyring->sem)
	__acquires(&keyring_serialise_link_lock)
{
	if (l_keyring->type != &key_type_keyring ||
	    u_keyring->type != &key_type_keyring)
		return -ENOTDIR;

	 
	if (l_keyring < u_keyring) {
		down_write(&l_keyring->sem);
		down_write_nested(&u_keyring->sem, 1);
	} else {
		down_write(&u_keyring->sem);
		down_write_nested(&l_keyring->sem, 1);
	}

	 
	if (index_key->type == &key_type_keyring)
		mutex_lock(&keyring_serialise_link_lock);

	return 0;
}

 
int __key_link_begin(struct key *keyring,
		     const struct keyring_index_key *index_key,
		     struct assoc_array_edit **_edit)
{
	struct assoc_array_edit *edit;
	int ret;

	kenter("%d,%s,%s,",
	       keyring->serial, index_key->type->name, index_key->description);

	BUG_ON(index_key->desc_len == 0);
	BUG_ON(*_edit != NULL);

	*_edit = NULL;

	ret = -EKEYREVOKED;
	if (test_bit(KEY_FLAG_REVOKED, &keyring->flags))
		goto error;

	 
	edit = assoc_array_insert(&keyring->keys,
				  &keyring_assoc_array_ops,
				  index_key,
				  NULL);
	if (IS_ERR(edit)) {
		ret = PTR_ERR(edit);
		goto error;
	}

	 
	if (!edit->dead_leaf) {
		ret = key_payload_reserve(keyring,
					  keyring->datalen + KEYQUOTA_LINK_BYTES);
		if (ret < 0)
			goto error_cancel;
	}

	*_edit = edit;
	kleave(" = 0");
	return 0;

error_cancel:
	assoc_array_cancel_edit(edit);
error:
	kleave(" = %d", ret);
	return ret;
}

 
int __key_link_check_live_key(struct key *keyring, struct key *key)
{
	if (key->type == &key_type_keyring)
		 
		return keyring_detect_cycle(keyring, key);
	return 0;
}

 
void __key_link(struct key *keyring, struct key *key,
		struct assoc_array_edit **_edit)
{
	__key_get(key);
	assoc_array_insert_set_object(*_edit, keyring_key_to_ptr(key));
	assoc_array_apply_edit(*_edit);
	*_edit = NULL;
	notify_key(keyring, NOTIFY_KEY_LINKED, key_serial(key));
}

 
void __key_link_end(struct key *keyring,
		    const struct keyring_index_key *index_key,
		    struct assoc_array_edit *edit)
	__releases(&keyring->sem)
	__releases(&keyring_serialise_link_lock)
{
	BUG_ON(index_key->type == NULL);
	kenter("%d,%s,", keyring->serial, index_key->type->name);

	if (edit) {
		if (!edit->dead_leaf) {
			key_payload_reserve(keyring,
				keyring->datalen - KEYQUOTA_LINK_BYTES);
		}
		assoc_array_cancel_edit(edit);
	}
	up_write(&keyring->sem);

	if (index_key->type == &key_type_keyring)
		mutex_unlock(&keyring_serialise_link_lock);
}

 
static int __key_link_check_restriction(struct key *keyring, struct key *key)
{
	if (!keyring->restrict_link || !keyring->restrict_link->check)
		return 0;
	return keyring->restrict_link->check(keyring, key->type, &key->payload,
					     keyring->restrict_link->key);
}

 
int key_link(struct key *keyring, struct key *key)
{
	struct assoc_array_edit *edit = NULL;
	int ret;

	kenter("{%d,%d}", keyring->serial, refcount_read(&keyring->usage));

	key_check(keyring);
	key_check(key);

	ret = __key_link_lock(keyring, &key->index_key);
	if (ret < 0)
		goto error;

	ret = __key_link_begin(keyring, &key->index_key, &edit);
	if (ret < 0)
		goto error_end;

	kdebug("begun {%d,%d}", keyring->serial, refcount_read(&keyring->usage));
	ret = __key_link_check_restriction(keyring, key);
	if (ret == 0)
		ret = __key_link_check_live_key(keyring, key);
	if (ret == 0)
		__key_link(keyring, key, &edit);

error_end:
	__key_link_end(keyring, &key->index_key, edit);
error:
	kleave(" = %d {%d,%d}", ret, keyring->serial, refcount_read(&keyring->usage));
	return ret;
}
EXPORT_SYMBOL(key_link);

 
static int __key_unlink_lock(struct key *keyring)
	__acquires(&keyring->sem)
{
	if (keyring->type != &key_type_keyring)
		return -ENOTDIR;

	down_write(&keyring->sem);
	return 0;
}

 
static int __key_unlink_begin(struct key *keyring, struct key *key,
			      struct assoc_array_edit **_edit)
{
	struct assoc_array_edit *edit;

	BUG_ON(*_edit != NULL);

	edit = assoc_array_delete(&keyring->keys, &keyring_assoc_array_ops,
				  &key->index_key);
	if (IS_ERR(edit))
		return PTR_ERR(edit);

	if (!edit)
		return -ENOENT;

	*_edit = edit;
	return 0;
}

 
static void __key_unlink(struct key *keyring, struct key *key,
			 struct assoc_array_edit **_edit)
{
	assoc_array_apply_edit(*_edit);
	notify_key(keyring, NOTIFY_KEY_UNLINKED, key_serial(key));
	*_edit = NULL;
	key_payload_reserve(keyring, keyring->datalen - KEYQUOTA_LINK_BYTES);
}

 
static void __key_unlink_end(struct key *keyring,
			     struct key *key,
			     struct assoc_array_edit *edit)
	__releases(&keyring->sem)
{
	if (edit)
		assoc_array_cancel_edit(edit);
	up_write(&keyring->sem);
}

 
int key_unlink(struct key *keyring, struct key *key)
{
	struct assoc_array_edit *edit = NULL;
	int ret;

	key_check(keyring);
	key_check(key);

	ret = __key_unlink_lock(keyring);
	if (ret < 0)
		return ret;

	ret = __key_unlink_begin(keyring, key, &edit);
	if (ret == 0)
		__key_unlink(keyring, key, &edit);
	__key_unlink_end(keyring, key, edit);
	return ret;
}
EXPORT_SYMBOL(key_unlink);

 
int key_move(struct key *key,
	     struct key *from_keyring,
	     struct key *to_keyring,
	     unsigned int flags)
{
	struct assoc_array_edit *from_edit = NULL, *to_edit = NULL;
	int ret;

	kenter("%d,%d,%d", key->serial, from_keyring->serial, to_keyring->serial);

	if (from_keyring == to_keyring)
		return 0;

	key_check(key);
	key_check(from_keyring);
	key_check(to_keyring);

	ret = __key_move_lock(from_keyring, to_keyring, &key->index_key);
	if (ret < 0)
		goto out;
	ret = __key_unlink_begin(from_keyring, key, &from_edit);
	if (ret < 0)
		goto error;
	ret = __key_link_begin(to_keyring, &key->index_key, &to_edit);
	if (ret < 0)
		goto error;

	ret = -EEXIST;
	if (to_edit->dead_leaf && (flags & KEYCTL_MOVE_EXCL))
		goto error;

	ret = __key_link_check_restriction(to_keyring, key);
	if (ret < 0)
		goto error;
	ret = __key_link_check_live_key(to_keyring, key);
	if (ret < 0)
		goto error;

	__key_unlink(from_keyring, key, &from_edit);
	__key_link(to_keyring, key, &to_edit);
error:
	__key_link_end(to_keyring, &key->index_key, to_edit);
	__key_unlink_end(from_keyring, key, from_edit);
out:
	kleave(" = %d", ret);
	return ret;
}
EXPORT_SYMBOL(key_move);

 
int keyring_clear(struct key *keyring)
{
	struct assoc_array_edit *edit;
	int ret;

	if (keyring->type != &key_type_keyring)
		return -ENOTDIR;

	down_write(&keyring->sem);

	edit = assoc_array_clear(&keyring->keys, &keyring_assoc_array_ops);
	if (IS_ERR(edit)) {
		ret = PTR_ERR(edit);
	} else {
		if (edit)
			assoc_array_apply_edit(edit);
		notify_key(keyring, NOTIFY_KEY_CLEARED, 0);
		key_payload_reserve(keyring, 0);
		ret = 0;
	}

	up_write(&keyring->sem);
	return ret;
}
EXPORT_SYMBOL(keyring_clear);

 
static void keyring_revoke(struct key *keyring)
{
	struct assoc_array_edit *edit;

	edit = assoc_array_clear(&keyring->keys, &keyring_assoc_array_ops);
	if (!IS_ERR(edit)) {
		if (edit)
			assoc_array_apply_edit(edit);
		key_payload_reserve(keyring, 0);
	}
}

static bool keyring_gc_select_iterator(void *object, void *iterator_data)
{
	struct key *key = keyring_ptr_to_key(object);
	time64_t *limit = iterator_data;

	if (key_is_dead(key, *limit))
		return false;
	key_get(key);
	return true;
}

static int keyring_gc_check_iterator(const void *object, void *iterator_data)
{
	const struct key *key = keyring_ptr_to_key(object);
	time64_t *limit = iterator_data;

	key_check(key);
	return key_is_dead(key, *limit);
}

 
void keyring_gc(struct key *keyring, time64_t limit)
{
	int result;

	kenter("%x{%s}", keyring->serial, keyring->description ?: "");

	if (keyring->flags & ((1 << KEY_FLAG_INVALIDATED) |
			      (1 << KEY_FLAG_REVOKED)))
		goto dont_gc;

	 
	rcu_read_lock();
	result = assoc_array_iterate(&keyring->keys,
				     keyring_gc_check_iterator, &limit);
	rcu_read_unlock();
	if (result == true)
		goto do_gc;

dont_gc:
	kleave(" [no gc]");
	return;

do_gc:
	down_write(&keyring->sem);
	assoc_array_gc(&keyring->keys, &keyring_assoc_array_ops,
		       keyring_gc_select_iterator, &limit);
	up_write(&keyring->sem);
	kleave(" [gc]");
}

 
void keyring_restriction_gc(struct key *keyring, struct key_type *dead_type)
{
	struct key_restriction *keyres;

	kenter("%x{%s}", keyring->serial, keyring->description ?: "");

	 
	if (!dead_type || !keyring->restrict_link ||
	    keyring->restrict_link->keytype != dead_type) {
		kleave(" [no restriction gc]");
		return;
	}

	 
	down_write(&keyring->sem);

	keyres = keyring->restrict_link;

	keyres->check = restrict_link_reject;

	key_put(keyres->key);
	keyres->key = NULL;
	keyres->keytype = NULL;

	up_write(&keyring->sem);

	kleave(" [restriction gc]");
}
