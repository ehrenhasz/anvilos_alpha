 

#include <linux/capability.h>
#include <linux/module.h>
#include <crypto/rng.h>
#include <linux/random.h>
#include <crypto/if_alg.h>
#include <linux/net.h>
#include <net/sock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stephan Mueller <smueller@chronox.de>");
MODULE_DESCRIPTION("User-space interface for random number generators");

struct rng_ctx {
#define MAXSIZE 128
	unsigned int len;
	struct crypto_rng *drng;
	u8 *addtl;
	size_t addtl_len;
};

struct rng_parent_ctx {
	struct crypto_rng *drng;
	u8 *entropy;
};

static void rng_reset_addtl(struct rng_ctx *ctx)
{
	kfree_sensitive(ctx->addtl);
	ctx->addtl = NULL;
	ctx->addtl_len = 0;
}

static int _rng_recvmsg(struct crypto_rng *drng, struct msghdr *msg, size_t len,
			u8 *addtl, size_t addtl_len)
{
	int err = 0;
	int genlen = 0;
	u8 result[MAXSIZE];

	if (len == 0)
		return 0;
	if (len > MAXSIZE)
		len = MAXSIZE;

	 
	memset(result, 0, len);

	 
	genlen = crypto_rng_generate(drng, addtl, addtl_len, result, len);
	if (genlen < 0)
		return genlen;

	err = memcpy_to_msg(msg, result, len);
	memzero_explicit(result, len);

	return err ? err : len;
}

static int rng_recvmsg(struct socket *sock, struct msghdr *msg, size_t len,
		       int flags)
{
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);
	struct rng_ctx *ctx = ask->private;

	return _rng_recvmsg(ctx->drng, msg, len, NULL, 0);
}

static int rng_test_recvmsg(struct socket *sock, struct msghdr *msg, size_t len,
			    int flags)
{
	struct sock *sk = sock->sk;
	struct alg_sock *ask = alg_sk(sk);
	struct rng_ctx *ctx = ask->private;
	int ret;

	lock_sock(sock->sk);
	ret = _rng_recvmsg(ctx->drng, msg, len, ctx->addtl, ctx->addtl_len);
	rng_reset_addtl(ctx);
	release_sock(sock->sk);

	return ret;
}

static int rng_test_sendmsg(struct socket *sock, struct msghdr *msg, size_t len)
{
	int err;
	struct alg_sock *ask = alg_sk(sock->sk);
	struct rng_ctx *ctx = ask->private;

	lock_sock(sock->sk);
	if (len > MAXSIZE) {
		err = -EMSGSIZE;
		goto unlock;
	}

	rng_reset_addtl(ctx);
	ctx->addtl = kmalloc(len, GFP_KERNEL);
	if (!ctx->addtl) {
		err = -ENOMEM;
		goto unlock;
	}

	err = memcpy_from_msg(ctx->addtl, msg, len);
	if (err) {
		rng_reset_addtl(ctx);
		goto unlock;
	}
	ctx->addtl_len = len;

unlock:
	release_sock(sock->sk);
	return err ? err : len;
}

static struct proto_ops algif_rng_ops = {
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
	.sendmsg	=	sock_no_sendmsg,

	.release	=	af_alg_release,
	.recvmsg	=	rng_recvmsg,
};

static struct proto_ops __maybe_unused algif_rng_test_ops = {
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
	.recvmsg	=	rng_test_recvmsg,
	.sendmsg	=	rng_test_sendmsg,
};

static void *rng_bind(const char *name, u32 type, u32 mask)
{
	struct rng_parent_ctx *pctx;
	struct crypto_rng *rng;

	pctx = kzalloc(sizeof(*pctx), GFP_KERNEL);
	if (!pctx)
		return ERR_PTR(-ENOMEM);

	rng = crypto_alloc_rng(name, type, mask);
	if (IS_ERR(rng)) {
		kfree(pctx);
		return ERR_CAST(rng);
	}

	pctx->drng = rng;
	return pctx;
}

static void rng_release(void *private)
{
	struct rng_parent_ctx *pctx = private;

	if (unlikely(!pctx))
		return;
	crypto_free_rng(pctx->drng);
	kfree_sensitive(pctx->entropy);
	kfree_sensitive(pctx);
}

static void rng_sock_destruct(struct sock *sk)
{
	struct alg_sock *ask = alg_sk(sk);
	struct rng_ctx *ctx = ask->private;

	rng_reset_addtl(ctx);
	sock_kfree_s(sk, ctx, ctx->len);
	af_alg_release_parent(sk);
}

static int rng_accept_parent(void *private, struct sock *sk)
{
	struct rng_ctx *ctx;
	struct rng_parent_ctx *pctx = private;
	struct alg_sock *ask = alg_sk(sk);
	unsigned int len = sizeof(*ctx);

	ctx = sock_kmalloc(sk, len, GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->len = len;
	ctx->addtl = NULL;
	ctx->addtl_len = 0;

	 

	ctx->drng = pctx->drng;
	ask->private = ctx;
	sk->sk_destruct = rng_sock_destruct;

	 
	if (IS_ENABLED(CONFIG_CRYPTO_USER_API_RNG_CAVP) && pctx->entropy)
		sk->sk_socket->ops = &algif_rng_test_ops;

	return 0;
}

static int rng_setkey(void *private, const u8 *seed, unsigned int seedlen)
{
	struct rng_parent_ctx *pctx = private;
	 
	return crypto_rng_reset(pctx->drng, seed, seedlen);
}

static int __maybe_unused rng_setentropy(void *private, sockptr_t entropy,
					 unsigned int len)
{
	struct rng_parent_ctx *pctx = private;
	u8 *kentropy = NULL;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (pctx->entropy)
		return -EINVAL;

	if (len > MAXSIZE)
		return -EMSGSIZE;

	if (len) {
		kentropy = memdup_sockptr(entropy, len);
		if (IS_ERR(kentropy))
			return PTR_ERR(kentropy);
	}

	crypto_rng_alg(pctx->drng)->set_ent(pctx->drng, kentropy, len);
	 
	pctx->entropy = kentropy;
	return 0;
}

static const struct af_alg_type algif_type_rng = {
	.bind		=	rng_bind,
	.release	=	rng_release,
	.accept		=	rng_accept_parent,
	.setkey		=	rng_setkey,
#ifdef CONFIG_CRYPTO_USER_API_RNG_CAVP
	.setentropy	=	rng_setentropy,
#endif
	.ops		=	&algif_rng_ops,
	.name		=	"rng",
	.owner		=	THIS_MODULE
};

static int __init rng_init(void)
{
	return af_alg_register_type(&algif_type_rng);
}

static void __exit rng_exit(void)
{
	int err = af_alg_unregister_type(&algif_type_rng);
	BUG_ON(err);
}

module_init(rng_init);
module_exit(rng_exit);
