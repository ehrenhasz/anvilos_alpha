
 

#include <crypto/hash.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/scatterlist.h>
#include <net/sctp/sctp.h>
#include <net/sctp/auth.h>

static struct sctp_hmac sctp_hmac_list[SCTP_AUTH_NUM_HMACS] = {
	{
		 
		.hmac_id = SCTP_AUTH_HMAC_ID_RESERVED_0,
	},
	{
		.hmac_id = SCTP_AUTH_HMAC_ID_SHA1,
		.hmac_name = "hmac(sha1)",
		.hmac_len = SCTP_SHA1_SIG_SIZE,
	},
	{
		 
		.hmac_id = SCTP_AUTH_HMAC_ID_RESERVED_2,
	},
#if IS_ENABLED(CONFIG_CRYPTO_SHA256)
	{
		.hmac_id = SCTP_AUTH_HMAC_ID_SHA256,
		.hmac_name = "hmac(sha256)",
		.hmac_len = SCTP_SHA256_SIG_SIZE,
	}
#endif
};


void sctp_auth_key_put(struct sctp_auth_bytes *key)
{
	if (!key)
		return;

	if (refcount_dec_and_test(&key->refcnt)) {
		kfree_sensitive(key);
		SCTP_DBG_OBJCNT_DEC(keys);
	}
}

 
static struct sctp_auth_bytes *sctp_auth_create_key(__u32 key_len, gfp_t gfp)
{
	struct sctp_auth_bytes *key;

	 
	if (key_len > (INT_MAX - sizeof(struct sctp_auth_bytes)))
		return NULL;

	 
	key = kmalloc(sizeof(struct sctp_auth_bytes) + key_len, gfp);
	if (!key)
		return NULL;

	key->len = key_len;
	refcount_set(&key->refcnt, 1);
	SCTP_DBG_OBJCNT_INC(keys);

	return key;
}

 
struct sctp_shared_key *sctp_auth_shkey_create(__u16 key_id, gfp_t gfp)
{
	struct sctp_shared_key *new;

	 
	new = kzalloc(sizeof(struct sctp_shared_key), gfp);
	if (!new)
		return NULL;

	INIT_LIST_HEAD(&new->key_list);
	refcount_set(&new->refcnt, 1);
	new->key_id = key_id;

	return new;
}

 
static void sctp_auth_shkey_destroy(struct sctp_shared_key *sh_key)
{
	BUG_ON(!list_empty(&sh_key->key_list));
	sctp_auth_key_put(sh_key->key);
	sh_key->key = NULL;
	kfree(sh_key);
}

void sctp_auth_shkey_release(struct sctp_shared_key *sh_key)
{
	if (refcount_dec_and_test(&sh_key->refcnt))
		sctp_auth_shkey_destroy(sh_key);
}

void sctp_auth_shkey_hold(struct sctp_shared_key *sh_key)
{
	refcount_inc(&sh_key->refcnt);
}

 
void sctp_auth_destroy_keys(struct list_head *keys)
{
	struct sctp_shared_key *ep_key;
	struct sctp_shared_key *tmp;

	if (list_empty(keys))
		return;

	key_for_each_safe(ep_key, tmp, keys) {
		list_del_init(&ep_key->key_list);
		sctp_auth_shkey_release(ep_key);
	}
}

 
static int sctp_auth_compare_vectors(struct sctp_auth_bytes *vector1,
			      struct sctp_auth_bytes *vector2)
{
	int diff;
	int i;
	const __u8 *longer;

	diff = vector1->len - vector2->len;
	if (diff) {
		longer = (diff > 0) ? vector1->data : vector2->data;

		 
		for (i = 0; i < abs(diff); i++) {
			if (longer[i] != 0)
				return diff;
		}
	}

	 
	return memcmp(vector1->data, vector2->data, vector1->len);
}

 
static struct sctp_auth_bytes *sctp_auth_make_key_vector(
			struct sctp_random_param *random,
			struct sctp_chunks_param *chunks,
			struct sctp_hmac_algo_param *hmacs,
			gfp_t gfp)
{
	struct sctp_auth_bytes *new;
	__u32	len;
	__u32	offset = 0;
	__u16	random_len, hmacs_len, chunks_len = 0;

	random_len = ntohs(random->param_hdr.length);
	hmacs_len = ntohs(hmacs->param_hdr.length);
	if (chunks)
		chunks_len = ntohs(chunks->param_hdr.length);

	len = random_len + hmacs_len + chunks_len;

	new = sctp_auth_create_key(len, gfp);
	if (!new)
		return NULL;

	memcpy(new->data, random, random_len);
	offset += random_len;

	if (chunks) {
		memcpy(new->data + offset, chunks, chunks_len);
		offset += chunks_len;
	}

	memcpy(new->data + offset, hmacs, hmacs_len);

	return new;
}


 
static struct sctp_auth_bytes *sctp_auth_make_local_vector(
				    const struct sctp_association *asoc,
				    gfp_t gfp)
{
	return sctp_auth_make_key_vector(
			(struct sctp_random_param *)asoc->c.auth_random,
			(struct sctp_chunks_param *)asoc->c.auth_chunks,
			(struct sctp_hmac_algo_param *)asoc->c.auth_hmacs, gfp);
}

 
static struct sctp_auth_bytes *sctp_auth_make_peer_vector(
				    const struct sctp_association *asoc,
				    gfp_t gfp)
{
	return sctp_auth_make_key_vector(asoc->peer.peer_random,
					 asoc->peer.peer_chunks,
					 asoc->peer.peer_hmacs,
					 gfp);
}


 
static struct sctp_auth_bytes *sctp_auth_asoc_set_secret(
			struct sctp_shared_key *ep_key,
			struct sctp_auth_bytes *first_vector,
			struct sctp_auth_bytes *last_vector,
			gfp_t gfp)
{
	struct sctp_auth_bytes *secret;
	__u32 offset = 0;
	__u32 auth_len;

	auth_len = first_vector->len + last_vector->len;
	if (ep_key->key)
		auth_len += ep_key->key->len;

	secret = sctp_auth_create_key(auth_len, gfp);
	if (!secret)
		return NULL;

	if (ep_key->key) {
		memcpy(secret->data, ep_key->key->data, ep_key->key->len);
		offset += ep_key->key->len;
	}

	memcpy(secret->data + offset, first_vector->data, first_vector->len);
	offset += first_vector->len;

	memcpy(secret->data + offset, last_vector->data, last_vector->len);

	return secret;
}

 
static struct sctp_auth_bytes *sctp_auth_asoc_create_secret(
				 const struct sctp_association *asoc,
				 struct sctp_shared_key *ep_key,
				 gfp_t gfp)
{
	struct sctp_auth_bytes *local_key_vector;
	struct sctp_auth_bytes *peer_key_vector;
	struct sctp_auth_bytes	*first_vector,
				*last_vector;
	struct sctp_auth_bytes	*secret = NULL;
	int	cmp;


	 

	local_key_vector = sctp_auth_make_local_vector(asoc, gfp);
	peer_key_vector = sctp_auth_make_peer_vector(asoc, gfp);

	if (!peer_key_vector || !local_key_vector)
		goto out;

	 
	cmp = sctp_auth_compare_vectors(local_key_vector,
					peer_key_vector);
	if (cmp < 0) {
		first_vector = local_key_vector;
		last_vector = peer_key_vector;
	} else {
		first_vector = peer_key_vector;
		last_vector = local_key_vector;
	}

	secret = sctp_auth_asoc_set_secret(ep_key, first_vector, last_vector,
					    gfp);
out:
	sctp_auth_key_put(local_key_vector);
	sctp_auth_key_put(peer_key_vector);

	return secret;
}

 
int sctp_auth_asoc_copy_shkeys(const struct sctp_endpoint *ep,
				struct sctp_association *asoc,
				gfp_t gfp)
{
	struct sctp_shared_key *sh_key;
	struct sctp_shared_key *new;

	BUG_ON(!list_empty(&asoc->endpoint_shared_keys));

	key_for_each(sh_key, &ep->endpoint_shared_keys) {
		new = sctp_auth_shkey_create(sh_key->key_id, gfp);
		if (!new)
			goto nomem;

		new->key = sh_key->key;
		sctp_auth_key_hold(new->key);
		list_add(&new->key_list, &asoc->endpoint_shared_keys);
	}

	return 0;

nomem:
	sctp_auth_destroy_keys(&asoc->endpoint_shared_keys);
	return -ENOMEM;
}


 
int sctp_auth_asoc_init_active_key(struct sctp_association *asoc, gfp_t gfp)
{
	struct sctp_auth_bytes	*secret;
	struct sctp_shared_key *ep_key;
	struct sctp_chunk *chunk;

	 
	if (!asoc->peer.auth_capable)
		return 0;

	 
	ep_key = sctp_auth_get_shkey(asoc, asoc->active_key_id);
	BUG_ON(!ep_key);

	secret = sctp_auth_asoc_create_secret(asoc, ep_key, gfp);
	if (!secret)
		return -ENOMEM;

	sctp_auth_key_put(asoc->asoc_shared_key);
	asoc->asoc_shared_key = secret;
	asoc->shkey = ep_key;

	 
	list_for_each_entry(chunk, &asoc->outqueue.out_chunk_list, list) {
		if (sctp_auth_send_cid(chunk->chunk_hdr->type, asoc)) {
			chunk->auth = 1;
			if (!chunk->shkey) {
				chunk->shkey = asoc->shkey;
				sctp_auth_shkey_hold(chunk->shkey);
			}
		}
	}

	return 0;
}


 
struct sctp_shared_key *sctp_auth_get_shkey(
				const struct sctp_association *asoc,
				__u16 key_id)
{
	struct sctp_shared_key *key;

	 
	key_for_each(key, &asoc->endpoint_shared_keys) {
		if (key->key_id == key_id) {
			if (!key->deactivated)
				return key;
			break;
		}
	}

	return NULL;
}

 
int sctp_auth_init_hmacs(struct sctp_endpoint *ep, gfp_t gfp)
{
	struct crypto_shash *tfm = NULL;
	__u16   id;

	 
	if (ep->auth_hmacs)
		return 0;

	 
	ep->auth_hmacs = kcalloc(SCTP_AUTH_NUM_HMACS,
				 sizeof(struct crypto_shash *),
				 gfp);
	if (!ep->auth_hmacs)
		return -ENOMEM;

	for (id = 0; id < SCTP_AUTH_NUM_HMACS; id++) {

		 
		if (!sctp_hmac_list[id].hmac_name)
			continue;

		 
		if (ep->auth_hmacs[id])
			continue;

		 
		tfm = crypto_alloc_shash(sctp_hmac_list[id].hmac_name, 0, 0);
		if (IS_ERR(tfm))
			goto out_err;

		ep->auth_hmacs[id] = tfm;
	}

	return 0;

out_err:
	 
	sctp_auth_destroy_hmacs(ep->auth_hmacs);
	ep->auth_hmacs = NULL;
	return -ENOMEM;
}

 
void sctp_auth_destroy_hmacs(struct crypto_shash *auth_hmacs[])
{
	int i;

	if (!auth_hmacs)
		return;

	for (i = 0; i < SCTP_AUTH_NUM_HMACS; i++) {
		crypto_free_shash(auth_hmacs[i]);
	}
	kfree(auth_hmacs);
}


struct sctp_hmac *sctp_auth_get_hmac(__u16 hmac_id)
{
	return &sctp_hmac_list[hmac_id];
}

 
struct sctp_hmac *sctp_auth_asoc_get_hmac(const struct sctp_association *asoc)
{
	struct sctp_hmac_algo_param *hmacs;
	__u16 n_elt;
	__u16 id = 0;
	int i;

	 
	if (asoc->default_hmac_id)
		return &sctp_hmac_list[asoc->default_hmac_id];

	 
	hmacs = asoc->peer.peer_hmacs;
	if (!hmacs)
		return NULL;

	n_elt = (ntohs(hmacs->param_hdr.length) -
		 sizeof(struct sctp_paramhdr)) >> 1;
	for (i = 0; i < n_elt; i++) {
		id = ntohs(hmacs->hmac_ids[i]);

		 
		if (id > SCTP_AUTH_HMAC_ID_MAX ||
		    !sctp_hmac_list[id].hmac_name) {
			id = 0;
			continue;
		}

		break;
	}

	if (id == 0)
		return NULL;

	return &sctp_hmac_list[id];
}

static int __sctp_auth_find_hmacid(__be16 *hmacs, int n_elts, __be16 hmac_id)
{
	int  found = 0;
	int  i;

	for (i = 0; i < n_elts; i++) {
		if (hmac_id == hmacs[i]) {
			found = 1;
			break;
		}
	}

	return found;
}

 
int sctp_auth_asoc_verify_hmac_id(const struct sctp_association *asoc,
				    __be16 hmac_id)
{
	struct sctp_hmac_algo_param *hmacs;
	__u16 n_elt;

	if (!asoc)
		return 0;

	hmacs = (struct sctp_hmac_algo_param *)asoc->c.auth_hmacs;
	n_elt = (ntohs(hmacs->param_hdr.length) -
		 sizeof(struct sctp_paramhdr)) >> 1;

	return __sctp_auth_find_hmacid(hmacs->hmac_ids, n_elt, hmac_id);
}


 
void sctp_auth_asoc_set_default_hmac(struct sctp_association *asoc,
				     struct sctp_hmac_algo_param *hmacs)
{
	struct sctp_endpoint *ep;
	__u16   id;
	int	i;
	int	n_params;

	 
	if (asoc->default_hmac_id)
		return;

	n_params = (ntohs(hmacs->param_hdr.length) -
		    sizeof(struct sctp_paramhdr)) >> 1;
	ep = asoc->ep;
	for (i = 0; i < n_params; i++) {
		id = ntohs(hmacs->hmac_ids[i]);

		 
		if (id > SCTP_AUTH_HMAC_ID_MAX)
			continue;

		 
		if (ep->auth_hmacs[id]) {
			asoc->default_hmac_id = id;
			break;
		}
	}
}


 
static int __sctp_auth_cid(enum sctp_cid chunk, struct sctp_chunks_param *param)
{
	unsigned short len;
	int found = 0;
	int i;

	if (!param || param->param_hdr.length == 0)
		return 0;

	len = ntohs(param->param_hdr.length) - sizeof(struct sctp_paramhdr);

	 
	for (i = 0; !found && i < len; i++) {
		switch (param->chunks[i]) {
		case SCTP_CID_INIT:
		case SCTP_CID_INIT_ACK:
		case SCTP_CID_SHUTDOWN_COMPLETE:
		case SCTP_CID_AUTH:
			break;

		default:
			if (param->chunks[i] == chunk)
				found = 1;
			break;
		}
	}

	return found;
}

 
int sctp_auth_send_cid(enum sctp_cid chunk, const struct sctp_association *asoc)
{
	if (!asoc)
		return 0;

	if (!asoc->peer.auth_capable)
		return 0;

	return __sctp_auth_cid(chunk, asoc->peer.peer_chunks);
}

 
int sctp_auth_recv_cid(enum sctp_cid chunk, const struct sctp_association *asoc)
{
	if (!asoc)
		return 0;

	if (!asoc->peer.auth_capable)
		return 0;

	return __sctp_auth_cid(chunk,
			      (struct sctp_chunks_param *)asoc->c.auth_chunks);
}

 
void sctp_auth_calculate_hmac(const struct sctp_association *asoc,
			      struct sk_buff *skb, struct sctp_auth_chunk *auth,
			      struct sctp_shared_key *ep_key, gfp_t gfp)
{
	struct sctp_auth_bytes *asoc_key;
	struct crypto_shash *tfm;
	__u16 key_id, hmac_id;
	unsigned char *end;
	int free_key = 0;
	__u8 *digest;

	 
	key_id = ntohs(auth->auth_hdr.shkey_id);
	hmac_id = ntohs(auth->auth_hdr.hmac_id);

	if (key_id == asoc->active_key_id)
		asoc_key = asoc->asoc_shared_key;
	else {
		 
		asoc_key = sctp_auth_asoc_create_secret(asoc, ep_key, gfp);
		if (!asoc_key)
			return;

		free_key = 1;
	}

	 
	end = skb_tail_pointer(skb);

	tfm = asoc->ep->auth_hmacs[hmac_id];

	digest = (u8 *)(&auth->auth_hdr + 1);
	if (crypto_shash_setkey(tfm, &asoc_key->data[0], asoc_key->len))
		goto free;

	crypto_shash_tfm_digest(tfm, (u8 *)auth, end - (unsigned char *)auth,
				digest);

free:
	if (free_key)
		sctp_auth_key_put(asoc_key);
}

 

 
int sctp_auth_ep_add_chunkid(struct sctp_endpoint *ep, __u8 chunk_id)
{
	struct sctp_chunks_param *p = ep->auth_chunk_list;
	__u16 nchunks;
	__u16 param_len;

	 
	if (__sctp_auth_cid(chunk_id, p))
		return 0;

	 
	param_len = ntohs(p->param_hdr.length);
	nchunks = param_len - sizeof(struct sctp_paramhdr);
	if (nchunks == SCTP_NUM_CHUNK_TYPES)
		return -EINVAL;

	p->chunks[nchunks] = chunk_id;
	p->param_hdr.length = htons(param_len + 1);
	return 0;
}

 
int sctp_auth_ep_set_hmacs(struct sctp_endpoint *ep,
			   struct sctp_hmacalgo *hmacs)
{
	int has_sha1 = 0;
	__u16 id;
	int i;

	 
	for (i = 0; i < hmacs->shmac_num_idents; i++) {
		id = hmacs->shmac_idents[i];

		if (id > SCTP_AUTH_HMAC_ID_MAX)
			return -EOPNOTSUPP;

		if (SCTP_AUTH_HMAC_ID_SHA1 == id)
			has_sha1 = 1;

		if (!sctp_hmac_list[id].hmac_name)
			return -EOPNOTSUPP;
	}

	if (!has_sha1)
		return -EINVAL;

	for (i = 0; i < hmacs->shmac_num_idents; i++)
		ep->auth_hmacs_list->hmac_ids[i] =
				htons(hmacs->shmac_idents[i]);
	ep->auth_hmacs_list->param_hdr.length =
			htons(sizeof(struct sctp_paramhdr) +
			hmacs->shmac_num_idents * sizeof(__u16));
	return 0;
}

 
int sctp_auth_set_key(struct sctp_endpoint *ep,
		      struct sctp_association *asoc,
		      struct sctp_authkey *auth_key)
{
	struct sctp_shared_key *cur_key, *shkey;
	struct sctp_auth_bytes *key;
	struct list_head *sh_keys;
	int replace = 0;

	 
	if (asoc) {
		if (!asoc->peer.auth_capable)
			return -EACCES;
		sh_keys = &asoc->endpoint_shared_keys;
	} else {
		if (!ep->auth_enable)
			return -EACCES;
		sh_keys = &ep->endpoint_shared_keys;
	}

	key_for_each(shkey, sh_keys) {
		if (shkey->key_id == auth_key->sca_keynumber) {
			replace = 1;
			break;
		}
	}

	cur_key = sctp_auth_shkey_create(auth_key->sca_keynumber, GFP_KERNEL);
	if (!cur_key)
		return -ENOMEM;

	 
	key = sctp_auth_create_key(auth_key->sca_keylength, GFP_KERNEL);
	if (!key) {
		kfree(cur_key);
		return -ENOMEM;
	}

	memcpy(key->data, &auth_key->sca_key[0], auth_key->sca_keylength);
	cur_key->key = key;

	if (!replace) {
		list_add(&cur_key->key_list, sh_keys);
		return 0;
	}

	list_del_init(&shkey->key_list);
	list_add(&cur_key->key_list, sh_keys);

	if (asoc && asoc->active_key_id == auth_key->sca_keynumber &&
	    sctp_auth_asoc_init_active_key(asoc, GFP_KERNEL)) {
		list_del_init(&cur_key->key_list);
		sctp_auth_shkey_release(cur_key);
		list_add(&shkey->key_list, sh_keys);
		return -ENOMEM;
	}

	sctp_auth_shkey_release(shkey);
	return 0;
}

int sctp_auth_set_active_key(struct sctp_endpoint *ep,
			     struct sctp_association *asoc,
			     __u16  key_id)
{
	struct sctp_shared_key *key;
	struct list_head *sh_keys;
	int found = 0;

	 
	if (asoc) {
		if (!asoc->peer.auth_capable)
			return -EACCES;
		sh_keys = &asoc->endpoint_shared_keys;
	} else {
		if (!ep->auth_enable)
			return -EACCES;
		sh_keys = &ep->endpoint_shared_keys;
	}

	key_for_each(key, sh_keys) {
		if (key->key_id == key_id) {
			found = 1;
			break;
		}
	}

	if (!found || key->deactivated)
		return -EINVAL;

	if (asoc) {
		__u16  active_key_id = asoc->active_key_id;

		asoc->active_key_id = key_id;
		if (sctp_auth_asoc_init_active_key(asoc, GFP_KERNEL)) {
			asoc->active_key_id = active_key_id;
			return -ENOMEM;
		}
	} else
		ep->active_key_id = key_id;

	return 0;
}

int sctp_auth_del_key_id(struct sctp_endpoint *ep,
			 struct sctp_association *asoc,
			 __u16  key_id)
{
	struct sctp_shared_key *key;
	struct list_head *sh_keys;
	int found = 0;

	 
	if (asoc) {
		if (!asoc->peer.auth_capable)
			return -EACCES;
		if (asoc->active_key_id == key_id)
			return -EINVAL;

		sh_keys = &asoc->endpoint_shared_keys;
	} else {
		if (!ep->auth_enable)
			return -EACCES;
		if (ep->active_key_id == key_id)
			return -EINVAL;

		sh_keys = &ep->endpoint_shared_keys;
	}

	key_for_each(key, sh_keys) {
		if (key->key_id == key_id) {
			found = 1;
			break;
		}
	}

	if (!found)
		return -EINVAL;

	 
	list_del_init(&key->key_list);
	sctp_auth_shkey_release(key);

	return 0;
}

int sctp_auth_deact_key_id(struct sctp_endpoint *ep,
			   struct sctp_association *asoc, __u16  key_id)
{
	struct sctp_shared_key *key;
	struct list_head *sh_keys;
	int found = 0;

	 
	if (asoc) {
		if (!asoc->peer.auth_capable)
			return -EACCES;
		if (asoc->active_key_id == key_id)
			return -EINVAL;

		sh_keys = &asoc->endpoint_shared_keys;
	} else {
		if (!ep->auth_enable)
			return -EACCES;
		if (ep->active_key_id == key_id)
			return -EINVAL;

		sh_keys = &ep->endpoint_shared_keys;
	}

	key_for_each(key, sh_keys) {
		if (key->key_id == key_id) {
			found = 1;
			break;
		}
	}

	if (!found)
		return -EINVAL;

	 
	if (asoc && !list_empty(&key->key_list) &&
	    refcount_read(&key->refcnt) == 1) {
		struct sctp_ulpevent *ev;

		ev = sctp_ulpevent_make_authkey(asoc, key->key_id,
						SCTP_AUTH_FREE_KEY, GFP_KERNEL);
		if (ev)
			asoc->stream.si->enqueue_event(&asoc->ulpq, ev);
	}

	key->deactivated = 1;

	return 0;
}

int sctp_auth_init(struct sctp_endpoint *ep, gfp_t gfp)
{
	int err = -ENOMEM;

	 
	if (!ep->auth_hmacs_list) {
		struct sctp_hmac_algo_param *auth_hmacs;

		auth_hmacs = kzalloc(struct_size(auth_hmacs, hmac_ids,
						 SCTP_AUTH_NUM_HMACS), gfp);
		if (!auth_hmacs)
			goto nomem;
		 
		auth_hmacs->param_hdr.type = SCTP_PARAM_HMAC_ALGO;
		auth_hmacs->param_hdr.length =
				htons(sizeof(struct sctp_paramhdr) + 2);
		auth_hmacs->hmac_ids[0] = htons(SCTP_AUTH_HMAC_ID_SHA1);
		ep->auth_hmacs_list = auth_hmacs;
	}

	if (!ep->auth_chunk_list) {
		struct sctp_chunks_param *auth_chunks;

		auth_chunks = kzalloc(sizeof(*auth_chunks) +
				      SCTP_NUM_CHUNK_TYPES, gfp);
		if (!auth_chunks)
			goto nomem;
		 
		auth_chunks->param_hdr.type = SCTP_PARAM_CHUNKS;
		auth_chunks->param_hdr.length =
				htons(sizeof(struct sctp_paramhdr));
		ep->auth_chunk_list = auth_chunks;
	}

	 
	err = sctp_auth_init_hmacs(ep, gfp);
	if (err)
		goto nomem;

	return 0;

nomem:
	 
	kfree(ep->auth_hmacs_list);
	kfree(ep->auth_chunk_list);
	ep->auth_hmacs_list = NULL;
	ep->auth_chunk_list = NULL;
	return err;
}

void sctp_auth_free(struct sctp_endpoint *ep)
{
	kfree(ep->auth_hmacs_list);
	kfree(ep->auth_chunk_list);
	ep->auth_hmacs_list = NULL;
	ep->auth_chunk_list = NULL;
	sctp_auth_destroy_hmacs(ep->auth_hmacs);
	ep->auth_hmacs = NULL;
}
