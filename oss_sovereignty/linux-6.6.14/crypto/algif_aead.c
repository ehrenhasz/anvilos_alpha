
 

#include <crypto/internal/aead.h>
#include <crypto/scatterwalk.h>
#include <crypto/if_alg.h>
#include <crypto/skcipher.h>
#include <crypto/null.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/net.h>
#include <net/sock.h>

struct aead_tfm {
	struct crypto_aead *aead;
	struct crypto_sync_skcipher *null_tfm;
};

static inline bool aead_sufficient_data(struct sock *sk)
{
	struct alg_sock *ask = alg_sk(sk);
	struct sock *psk = ask->parent;
	struct alg_sock *pask = alg_sk(psk);
	struct af_alg_ctx *ctx = ask->private;
	struct aead_tfm *aeadc = pask->private;
	struct crypto_aead *tfm = aeadc->aead;
	unsigned int as = crypto_aead_authsize(tfm);

	 
	return ctx->used >= ctx->aead_assoclen + (ctx->enc ? 0 : as);
}

static int aead_sendmsg(struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);
	struct sock *psk = ask->parent;
	struct alg_sock *pask = alg_sk(psk);
	struct aead_tfm *aeadc = pask->private;
	struct crypto_aead *tfm = aeadc->aead;
	unsigned int ivsize = crypto_aead_ivsize(tfm);

	return af_alg_sendmsg(sock, msg, size, ivsize);
}

static int crypto_aead_copy_sgl(struct crypto_sync_skcipher *null_tfm,
				struct scatterlist *src,
				struct scatterlist *dst, unsigned int len)
{
	SYNC_SKCIPHER_REQUEST_ON_STACK(skreq, null_tfm);

	skcipher_request_set_sync_tfm(skreq, null_tfm);
	skcipher_request_set_callback(skreq, CRYPTO_TFM_REQ_MAY_SLEEP,
				      NULL, NULL);
	skcipher_request_set_crypt(skreq, src, dst, len, NULL);

	return crypto_skcipher_encrypt(skreq);
}

static int _aead_recvmsg(struct socket *sock, struct msghdr *msg,
			 size_t ignored, int flags)
{
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);
	struct sock *psk = ask->parent;
	struct alg_sock *pask = alg_sk(psk);
	struct af_alg_ctx *ctx = ask->private;
	struct aead_tfm *aeadc = pask->private;
	struct crypto_aead *tfm = aeadc->aead;
	struct crypto_sync_skcipher *null_tfm = aeadc->null_tfm;
	unsigned int i, as = crypto_aead_authsize(tfm);
	struct af_alg_async_req *areq;
	struct af_alg_tsgl *tsgl, *tmp;
	struct scatterlist *rsgl_src, *tsgl_src = NULL;
	int err = 0;
	size_t used = 0;		 
	size_t outlen = 0;		 
	size_t usedpages = 0;		 
	size_t processed = 0;		 

	if (!ctx->init || ctx->more) {
		err = af_alg_wait_for_data(sk, flags, 0);
		if (err)
			return err;
	}

	 
	used = ctx->used;

	 
	if (!aead_sufficient_data(sk))
		return -EINVAL;

	 
	if (ctx->enc)
		outlen = used + as;
	else
		outlen = used - as;

	 
	used -= ctx->aead_assoclen;

	 
	areq = af_alg_alloc_areq(sk, sizeof(struct af_alg_async_req) +
				     crypto_aead_reqsize(tfm));
	if (IS_ERR(areq))
		return PTR_ERR(areq);

	 
	err = af_alg_get_rsgl(sk, msg, flags, areq, outlen, &usedpages);
	if (err)
		goto free;

	 
	if (usedpages < outlen) {
		size_t less = outlen - usedpages;

		if (used < less) {
			err = -EINVAL;
			goto free;
		}
		used -= less;
		outlen -= less;
	}

	processed = used + ctx->aead_assoclen;
	list_for_each_entry_safe(tsgl, tmp, &ctx->tsgl_list, list) {
		for (i = 0; i < tsgl->cur; i++) {
			struct scatterlist *process_sg = tsgl->sg + i;

			if (!(process_sg->length) || !sg_page(process_sg))
				continue;
			tsgl_src = process_sg;
			break;
		}
		if (tsgl_src)
			break;
	}
	if (processed && !tsgl_src) {
		err = -EFAULT;
		goto free;
	}

	 

	 
	rsgl_src = areq->first_rsgl.sgl.sgt.sgl;

	if (ctx->enc) {
		 
		err = crypto_aead_copy_sgl(null_tfm, tsgl_src,
					   areq->first_rsgl.sgl.sgt.sgl,
					   processed);
		if (err)
			goto free;
		af_alg_pull_tsgl(sk, processed, NULL, 0);
	} else {
		 

		  
		err = crypto_aead_copy_sgl(null_tfm, tsgl_src,
					   areq->first_rsgl.sgl.sgt.sgl,
					   outlen);
		if (err)
			goto free;

		 
		areq->tsgl_entries = af_alg_count_tsgl(sk, processed,
						       processed - as);
		if (!areq->tsgl_entries)
			areq->tsgl_entries = 1;
		areq->tsgl = sock_kmalloc(sk, array_size(sizeof(*areq->tsgl),
							 areq->tsgl_entries),
					  GFP_KERNEL);
		if (!areq->tsgl) {
			err = -ENOMEM;
			goto free;
		}
		sg_init_table(areq->tsgl, areq->tsgl_entries);

		 
		af_alg_pull_tsgl(sk, processed, areq->tsgl, processed - as);

		 
		if (usedpages) {
			 
			struct af_alg_sgl *sgl_prev = &areq->last_rsgl->sgl;
			struct scatterlist *sg = sgl_prev->sgt.sgl;

			sg_unmark_end(sg + sgl_prev->sgt.nents - 1);
			sg_chain(sg, sgl_prev->sgt.nents + 1, areq->tsgl);
		} else
			 
			rsgl_src = areq->tsgl;
	}

	 
	aead_request_set_crypt(&areq->cra_u.aead_req, rsgl_src,
			       areq->first_rsgl.sgl.sgt.sgl, used, ctx->iv);
	aead_request_set_ad(&areq->cra_u.aead_req, ctx->aead_assoclen);
	aead_request_set_tfm(&areq->cra_u.aead_req, tfm);

	if (msg->msg_iocb && !is_sync_kiocb(msg->msg_iocb)) {
		 
		sock_hold(sk);
		areq->iocb = msg->msg_iocb;

		 
		areq->outlen = outlen;

		aead_request_set_callback(&areq->cra_u.aead_req,
					  CRYPTO_TFM_REQ_MAY_SLEEP,
					  af_alg_async_cb, areq);
		err = ctx->enc ? crypto_aead_encrypt(&areq->cra_u.aead_req) :
				 crypto_aead_decrypt(&areq->cra_u.aead_req);

		 
		if (err == -EINPROGRESS)
			return -EIOCBQUEUED;

		sock_put(sk);
	} else {
		 
		aead_request_set_callback(&areq->cra_u.aead_req,
					  CRYPTO_TFM_REQ_MAY_SLEEP |
					  CRYPTO_TFM_REQ_MAY_BACKLOG,
					  crypto_req_done, &ctx->wait);
		err = crypto_wait_req(ctx->enc ?
				crypto_aead_encrypt(&areq->cra_u.aead_req) :
				crypto_aead_decrypt(&areq->cra_u.aead_req),
				&ctx->wait);
	}


free:
	af_alg_free_resources(areq);

	return err ? err : outlen;
}

static int aead_recvmsg(struct socket *sock, struct msghdr *msg,
			size_t ignored, int flags)
{
	struct sock *sk = sock->sk;
	int ret = 0;

	lock_sock(sk);
	while (msg_data_left(msg)) {
		int err = _aead_recvmsg(sock, msg, ignored, flags);

		 
		if (err <= 0) {
			if (err == -EIOCBQUEUED || err == -EBADMSG || !ret)
				ret = err;
			goto out;
		}

		ret += err;
	}

out:
	af_alg_wmem_wakeup(sk);
	release_sock(sk);
	return ret;
}

static struct proto_ops algif_aead_ops = {
	.family		=	PF_ALG,

	.connect	=	sock_no_connect,
	.socketpair	=	sock_no_socketpair,
	.getname	=	sock_no_getname,
	.ioctl		=	sock_no_ioctl,
	.listen		=	sock_no_listen,
	.shutdown	=	sock_no_shutdown,
	.mmap		=	sock_no_mmap,
	.bind		=	sock_no_bind,
	.accept		=	sock_no_accept,

	.release	=	af_alg_release,
	.sendmsg	=	aead_sendmsg,
	.recvmsg	=	aead_recvmsg,
	.poll		=	af_alg_poll,
};

static int aead_check_key(struct socket *sock)
{
	int err = 0;
	struct sock *psk;
	struct alg_sock *pask;
	struct aead_tfm *tfm;
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);

	lock_sock(sk);
	if (!atomic_read(&ask->nokey_refcnt))
		goto unlock_child;

	psk = ask->parent;
	pask = alg_sk(ask->parent);
	tfm = pask->private;

	err = -ENOKEY;
	lock_sock_nested(psk, SINGLE_DEPTH_NESTING);
	if (crypto_aead_get_flags(tfm->aead) & CRYPTO_TFM_NEED_KEY)
		goto unlock;

	atomic_dec(&pask->nokey_refcnt);
	atomic_set(&ask->nokey_refcnt, 0);

	err = 0;

unlock:
	release_sock(psk);
unlock_child:
	release_sock(sk);

	return err;
}

static int aead_sendmsg_nokey(struct socket *sock, struct msghdr *msg,
				  size_t size)
{
	int err;

	err = aead_check_key(sock);
	if (err)
		return err;

	return aead_sendmsg(sock, msg, size);
}

static int aead_recvmsg_nokey(struct socket *sock, struct msghdr *msg,
				  size_t ignored, int flags)
{
	int err;

	err = aead_check_key(sock);
	if (err)
		return err;

	return aead_recvmsg(sock, msg, ignored, flags);
}

static struct proto_ops algif_aead_ops_nokey = {
	.family		=	PF_ALG,

	.connect	=	sock_no_connect,
	.socketpair	=	sock_no_socketpair,
	.getname	=	sock_no_getname,
	.ioctl		=	sock_no_ioctl,
	.listen		=	sock_no_listen,
	.shutdown	=	sock_no_shutdown,
	.mmap		=	sock_no_mmap,
	.bind		=	sock_no_bind,
	.accept		=	sock_no_accept,

	.release	=	af_alg_release,
	.sendmsg	=	aead_sendmsg_nokey,
	.recvmsg	=	aead_recvmsg_nokey,
	.poll		=	af_alg_poll,
};

static void *aead_bind(const char *name, u32 type, u32 mask)
{
	struct aead_tfm *tfm;
	struct crypto_aead *aead;
	struct crypto_sync_skcipher *null_tfm;

	tfm = kzalloc(sizeof(*tfm), GFP_KERNEL);
	if (!tfm)
		return ERR_PTR(-ENOMEM);

	aead = crypto_alloc_aead(name, type, mask);
	if (IS_ERR(aead)) {
		kfree(tfm);
		return ERR_CAST(aead);
	}

	null_tfm = crypto_get_default_null_skcipher();
	if (IS_ERR(null_tfm)) {
		crypto_free_aead(aead);
		kfree(tfm);
		return ERR_CAST(null_tfm);
	}

	tfm->aead = aead;
	tfm->null_tfm = null_tfm;

	return tfm;
}

static void aead_release(void *private)
{
	struct aead_tfm *tfm = private;

	crypto_free_aead(tfm->aead);
	crypto_put_default_null_skcipher();
	kfree(tfm);
}

static int aead_setauthsize(void *private, unsigned int authsize)
{
	struct aead_tfm *tfm = private;

	return crypto_aead_setauthsize(tfm->aead, authsize);
}

static int aead_setkey(void *private, const u8 *key, unsigned int keylen)
{
	struct aead_tfm *tfm = private;

	return crypto_aead_setkey(tfm->aead, key, keylen);
}

static void aead_sock_destruct(struct sock *sk)
{
	struct alg_sock *ask = alg_sk(sk);
	struct af_alg_ctx *ctx = ask->private;
	struct sock *psk = ask->parent;
	struct alg_sock *pask = alg_sk(psk);
	struct aead_tfm *aeadc = pask->private;
	struct crypto_aead *tfm = aeadc->aead;
	unsigned int ivlen = crypto_aead_ivsize(tfm);

	af_alg_pull_tsgl(sk, ctx->used, NULL, 0);
	sock_kzfree_s(sk, ctx->iv, ivlen);
	sock_kfree_s(sk, ctx, ctx->len);
	af_alg_release_parent(sk);
}

static int aead_accept_parent_nokey(void *private, struct sock *sk)
{
	struct af_alg_ctx *ctx;
	struct alg_sock *ask = alg_sk(sk);
	struct aead_tfm *tfm = private;
	struct crypto_aead *aead = tfm->aead;
	unsigned int len = sizeof(*ctx);
	unsigned int ivlen = crypto_aead_ivsize(aead);

	ctx = sock_kmalloc(sk, len, GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	memset(ctx, 0, len);

	ctx->iv = sock_kmalloc(sk, ivlen, GFP_KERNEL);
	if (!ctx->iv) {
		sock_kfree_s(sk, ctx, len);
		return -ENOMEM;
	}
	memset(ctx->iv, 0, ivlen);

	INIT_LIST_HEAD(&ctx->tsgl_list);
	ctx->len = len;
	crypto_init_wait(&ctx->wait);

	ask->private = ctx;

	sk->sk_destruct = aead_sock_destruct;

	return 0;
}

static int aead_accept_parent(void *private, struct sock *sk)
{
	struct aead_tfm *tfm = private;

	if (crypto_aead_get_flags(tfm->aead) & CRYPTO_TFM_NEED_KEY)
		return -ENOKEY;

	return aead_accept_parent_nokey(private, sk);
}

static const struct af_alg_type algif_type_aead = {
	.bind		=	aead_bind,
	.release	=	aead_release,
	.setkey		=	aead_setkey,
	.setauthsize	=	aead_setauthsize,
	.accept		=	aead_accept_parent,
	.accept_nokey	=	aead_accept_parent_nokey,
	.ops		=	&algif_aead_ops,
	.ops_nokey	=	&algif_aead_ops_nokey,
	.name		=	"aead",
	.owner		=	THIS_MODULE
};

static int __init algif_aead_init(void)
{
	return af_alg_register_type(&algif_type_aead);
}

static void __exit algif_aead_exit(void)
{
	int err = af_alg_unregister_type(&algif_type_aead);
	BUG_ON(err);
}

module_init(algif_aead_init);
module_exit(algif_aead_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stephan Mueller <smueller@chronox.de>");
MODULE_DESCRIPTION("AEAD kernel crypto API user space interface");
