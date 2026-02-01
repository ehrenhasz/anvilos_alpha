
 

#define pr_fmt(fmt) "MPTCP: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/sock.h>
#include <net/inet_common.h>
#include <net/protocol.h>
#include <net/mptcp.h>
#include "protocol.h"

#define TOKEN_MAX_CHAIN_LEN	4

struct token_bucket {
	spinlock_t		lock;
	int			chain_len;
	struct hlist_nulls_head	req_chain;
	struct hlist_nulls_head	msk_chain;
};

static struct token_bucket *token_hash __read_mostly;
static unsigned int token_mask __read_mostly;

static struct token_bucket *token_bucket(u32 token)
{
	return &token_hash[token & token_mask];
}

 
static struct mptcp_subflow_request_sock *
__token_lookup_req(struct token_bucket *t, u32 token)
{
	struct mptcp_subflow_request_sock *req;
	struct hlist_nulls_node *pos;

	hlist_nulls_for_each_entry_rcu(req, pos, &t->req_chain, token_node)
		if (req->token == token)
			return req;
	return NULL;
}

 
static struct mptcp_sock *
__token_lookup_msk(struct token_bucket *t, u32 token)
{
	struct hlist_nulls_node *pos;
	struct sock *sk;

	sk_nulls_for_each_rcu(sk, pos, &t->msk_chain)
		if (mptcp_sk(sk)->token == token)
			return mptcp_sk(sk);
	return NULL;
}

static bool __token_bucket_busy(struct token_bucket *t, u32 token)
{
	return !token || t->chain_len >= TOKEN_MAX_CHAIN_LEN ||
	       __token_lookup_req(t, token) || __token_lookup_msk(t, token);
}

static void mptcp_crypto_key_gen_sha(u64 *key, u32 *token, u64 *idsn)
{
	 
	get_random_bytes(key, sizeof(u64));
	mptcp_crypto_key_sha(*key, token, idsn);
}

 
int mptcp_token_new_request(struct request_sock *req)
{
	struct mptcp_subflow_request_sock *subflow_req = mptcp_subflow_rsk(req);
	struct token_bucket *bucket;
	u32 token;

	mptcp_crypto_key_sha(subflow_req->local_key,
			     &subflow_req->token,
			     &subflow_req->idsn);
	pr_debug("req=%p local_key=%llu, token=%u, idsn=%llu\n",
		 req, subflow_req->local_key, subflow_req->token,
		 subflow_req->idsn);

	token = subflow_req->token;
	bucket = token_bucket(token);
	spin_lock_bh(&bucket->lock);
	if (__token_bucket_busy(bucket, token)) {
		spin_unlock_bh(&bucket->lock);
		return -EBUSY;
	}

	hlist_nulls_add_head_rcu(&subflow_req->token_node, &bucket->req_chain);
	bucket->chain_len++;
	spin_unlock_bh(&bucket->lock);
	return 0;
}

 
int mptcp_token_new_connect(struct sock *ssk)
{
	struct mptcp_subflow_context *subflow = mptcp_subflow_ctx(ssk);
	struct mptcp_sock *msk = mptcp_sk(subflow->conn);
	int retries = MPTCP_TOKEN_MAX_RETRIES;
	struct sock *sk = subflow->conn;
	struct token_bucket *bucket;

again:
	mptcp_crypto_key_gen_sha(&subflow->local_key, &subflow->token,
				 &subflow->idsn);

	bucket = token_bucket(subflow->token);
	spin_lock_bh(&bucket->lock);
	if (__token_bucket_busy(bucket, subflow->token)) {
		spin_unlock_bh(&bucket->lock);
		if (!--retries)
			return -EBUSY;
		goto again;
	}

	pr_debug("ssk=%p, local_key=%llu, token=%u, idsn=%llu\n",
		 ssk, subflow->local_key, subflow->token, subflow->idsn);

	WRITE_ONCE(msk->token, subflow->token);
	__sk_nulls_add_node_rcu((struct sock *)msk, &bucket->msk_chain);
	bucket->chain_len++;
	spin_unlock_bh(&bucket->lock);
	sock_prot_inuse_add(sock_net(sk), sk->sk_prot, 1);
	return 0;
}

 
void mptcp_token_accept(struct mptcp_subflow_request_sock *req,
			struct mptcp_sock *msk)
{
	struct mptcp_subflow_request_sock *pos;
	struct sock *sk = (struct sock *)msk;
	struct token_bucket *bucket;

	sock_prot_inuse_add(sock_net(sk), sk->sk_prot, 1);
	bucket = token_bucket(req->token);
	spin_lock_bh(&bucket->lock);

	 
	pos = __token_lookup_req(bucket, req->token);
	if (!WARN_ON_ONCE(pos != req))
		hlist_nulls_del_init_rcu(&req->token_node);
	__sk_nulls_add_node_rcu((struct sock *)msk, &bucket->msk_chain);
	spin_unlock_bh(&bucket->lock);
}

bool mptcp_token_exists(u32 token)
{
	struct hlist_nulls_node *pos;
	struct token_bucket *bucket;
	struct mptcp_sock *msk;
	struct sock *sk;

	rcu_read_lock();
	bucket = token_bucket(token);

again:
	sk_nulls_for_each_rcu(sk, pos, &bucket->msk_chain) {
		msk = mptcp_sk(sk);
		if (READ_ONCE(msk->token) == token)
			goto found;
	}
	if (get_nulls_value(pos) != (token & token_mask))
		goto again;

	rcu_read_unlock();
	return false;
found:
	rcu_read_unlock();
	return true;
}

 
struct mptcp_sock *mptcp_token_get_sock(struct net *net, u32 token)
{
	struct hlist_nulls_node *pos;
	struct token_bucket *bucket;
	struct mptcp_sock *msk;
	struct sock *sk;

	rcu_read_lock();
	bucket = token_bucket(token);

again:
	sk_nulls_for_each_rcu(sk, pos, &bucket->msk_chain) {
		msk = mptcp_sk(sk);
		if (READ_ONCE(msk->token) != token ||
		    !net_eq(sock_net(sk), net))
			continue;

		if (!refcount_inc_not_zero(&sk->sk_refcnt))
			goto not_found;

		if (READ_ONCE(msk->token) != token ||
		    !net_eq(sock_net(sk), net)) {
			sock_put(sk);
			goto again;
		}
		goto found;
	}
	if (get_nulls_value(pos) != (token & token_mask))
		goto again;

not_found:
	msk = NULL;

found:
	rcu_read_unlock();
	return msk;
}
EXPORT_SYMBOL_GPL(mptcp_token_get_sock);

 
struct mptcp_sock *mptcp_token_iter_next(const struct net *net, long *s_slot,
					 long *s_num)
{
	struct mptcp_sock *ret = NULL;
	struct hlist_nulls_node *pos;
	int slot, num = 0;

	for (slot = *s_slot; slot <= token_mask; *s_num = 0, slot++) {
		struct token_bucket *bucket = &token_hash[slot];
		struct sock *sk;

		num = 0;

		if (hlist_nulls_empty(&bucket->msk_chain))
			continue;

		rcu_read_lock();
		sk_nulls_for_each_rcu(sk, pos, &bucket->msk_chain) {
			++num;
			if (!net_eq(sock_net(sk), net))
				continue;

			if (num <= *s_num)
				continue;

			if (!refcount_inc_not_zero(&sk->sk_refcnt))
				continue;

			if (!net_eq(sock_net(sk), net)) {
				sock_put(sk);
				continue;
			}

			ret = mptcp_sk(sk);
			rcu_read_unlock();
			goto out;
		}
		rcu_read_unlock();
	}

out:
	*s_slot = slot;
	*s_num = num;
	return ret;
}
EXPORT_SYMBOL_GPL(mptcp_token_iter_next);

 
void mptcp_token_destroy_request(struct request_sock *req)
{
	struct mptcp_subflow_request_sock *subflow_req = mptcp_subflow_rsk(req);
	struct mptcp_subflow_request_sock *pos;
	struct token_bucket *bucket;

	if (hlist_nulls_unhashed(&subflow_req->token_node))
		return;

	bucket = token_bucket(subflow_req->token);
	spin_lock_bh(&bucket->lock);
	pos = __token_lookup_req(bucket, subflow_req->token);
	if (!WARN_ON_ONCE(pos != subflow_req)) {
		hlist_nulls_del_init_rcu(&pos->token_node);
		bucket->chain_len--;
	}
	spin_unlock_bh(&bucket->lock);
}

 
void mptcp_token_destroy(struct mptcp_sock *msk)
{
	struct sock *sk = (struct sock *)msk;
	struct token_bucket *bucket;
	struct mptcp_sock *pos;

	if (sk_unhashed((struct sock *)msk))
		return;

	sock_prot_inuse_add(sock_net(sk), sk->sk_prot, -1);
	bucket = token_bucket(msk->token);
	spin_lock_bh(&bucket->lock);
	pos = __token_lookup_msk(bucket, msk->token);
	if (!WARN_ON_ONCE(pos != msk)) {
		__sk_nulls_del_node_init_rcu((struct sock *)pos);
		bucket->chain_len--;
	}
	spin_unlock_bh(&bucket->lock);
	WRITE_ONCE(msk->token, 0);
}

void __init mptcp_token_init(void)
{
	int i;

	token_hash = alloc_large_system_hash("MPTCP token",
					     sizeof(struct token_bucket),
					     0,
					     20, 
					     HASH_ZERO,
					     NULL,
					     &token_mask,
					     0,
					     64 * 1024);
	for (i = 0; i < token_mask + 1; ++i) {
		INIT_HLIST_NULLS_HEAD(&token_hash[i].req_chain, i);
		INIT_HLIST_NULLS_HEAD(&token_hash[i].msk_chain, i);
		spin_lock_init(&token_hash[i].lock);
	}
}

#if IS_MODULE(CONFIG_MPTCP_KUNIT_TEST)
EXPORT_SYMBOL_GPL(mptcp_token_new_request);
EXPORT_SYMBOL_GPL(mptcp_token_new_connect);
EXPORT_SYMBOL_GPL(mptcp_token_accept);
EXPORT_SYMBOL_GPL(mptcp_token_destroy_request);
EXPORT_SYMBOL_GPL(mptcp_token_destroy);
#endif
