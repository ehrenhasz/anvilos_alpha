
 

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/ccp.h>
#include <linux/scatterlist.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/akcipher.h>

#include "ccp-crypto.h"

MODULE_AUTHOR("Tom Lendacky <thomas.lendacky@amd.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
MODULE_DESCRIPTION("AMD Cryptographic Coprocessor crypto API support");

static unsigned int aes_disable;
module_param(aes_disable, uint, 0444);
MODULE_PARM_DESC(aes_disable, "Disable use of AES - any non-zero value");

static unsigned int sha_disable;
module_param(sha_disable, uint, 0444);
MODULE_PARM_DESC(sha_disable, "Disable use of SHA - any non-zero value");

static unsigned int des3_disable;
module_param(des3_disable, uint, 0444);
MODULE_PARM_DESC(des3_disable, "Disable use of 3DES - any non-zero value");

static unsigned int rsa_disable;
module_param(rsa_disable, uint, 0444);
MODULE_PARM_DESC(rsa_disable, "Disable use of RSA - any non-zero value");

 
static LIST_HEAD(hash_algs);
static LIST_HEAD(skcipher_algs);
static LIST_HEAD(aead_algs);
static LIST_HEAD(akcipher_algs);

 
struct ccp_crypto_queue {
	struct list_head cmds;
	struct list_head *backlog;
	unsigned int cmd_count;
};

#define CCP_CRYPTO_MAX_QLEN	100

static struct ccp_crypto_queue req_queue;
static DEFINE_SPINLOCK(req_queue_lock);

struct ccp_crypto_cmd {
	struct list_head entry;

	struct ccp_cmd *cmd;

	 
	struct crypto_async_request *req;
	struct crypto_tfm *tfm;

	 
	int ret;
};

static inline bool ccp_crypto_success(int err)
{
	if (err && (err != -EINPROGRESS) && (err != -EBUSY))
		return false;

	return true;
}

static struct ccp_crypto_cmd *ccp_crypto_cmd_complete(
	struct ccp_crypto_cmd *crypto_cmd, struct ccp_crypto_cmd **backlog)
{
	struct ccp_crypto_cmd *held = NULL, *tmp;
	unsigned long flags;

	*backlog = NULL;

	spin_lock_irqsave(&req_queue_lock, flags);

	 
	tmp = crypto_cmd;
	list_for_each_entry_continue(tmp, &req_queue.cmds, entry) {
		if (crypto_cmd->tfm != tmp->tfm)
			continue;
		held = tmp;
		break;
	}

	 
	if (req_queue.backlog != &req_queue.cmds) {
		 
		if (req_queue.backlog == &crypto_cmd->entry)
			req_queue.backlog = crypto_cmd->entry.next;

		*backlog = container_of(req_queue.backlog,
					struct ccp_crypto_cmd, entry);
		req_queue.backlog = req_queue.backlog->next;

		 
		if (req_queue.backlog == &crypto_cmd->entry)
			req_queue.backlog = crypto_cmd->entry.next;
	}

	 
	req_queue.cmd_count--;
	list_del(&crypto_cmd->entry);

	spin_unlock_irqrestore(&req_queue_lock, flags);

	return held;
}

static void ccp_crypto_complete(void *data, int err)
{
	struct ccp_crypto_cmd *crypto_cmd = data;
	struct ccp_crypto_cmd *held, *next, *backlog;
	struct crypto_async_request *req = crypto_cmd->req;
	struct ccp_ctx *ctx = crypto_tfm_ctx_dma(req->tfm);
	int ret;

	if (err == -EINPROGRESS) {
		 
		if (crypto_cmd->ret == -EBUSY) {
			crypto_cmd->ret = -EINPROGRESS;
			crypto_request_complete(req, -EINPROGRESS);
		}

		return;
	}

	 
	held = ccp_crypto_cmd_complete(crypto_cmd, &backlog);
	if (backlog) {
		backlog->ret = -EINPROGRESS;
		crypto_request_complete(backlog->req, -EINPROGRESS);
	}

	 
	if (crypto_cmd->ret == -EBUSY)
		crypto_request_complete(req, -EINPROGRESS);

	 
	ret = err;
	if (ctx->complete)
		ret = ctx->complete(req, ret);
	crypto_request_complete(req, ret);

	 
	while (held) {
		 
		held->cmd->flags |= CCP_CMD_MAY_BACKLOG;
		ret = ccp_enqueue_cmd(held->cmd);
		if (ccp_crypto_success(ret))
			break;

		 
		ctx = crypto_tfm_ctx_dma(held->req->tfm);
		if (ctx->complete)
			ret = ctx->complete(held->req, ret);
		crypto_request_complete(held->req, ret);

		next = ccp_crypto_cmd_complete(held, &backlog);
		if (backlog) {
			backlog->ret = -EINPROGRESS;
			crypto_request_complete(backlog->req, -EINPROGRESS);
		}

		kfree(held);
		held = next;
	}

	kfree(crypto_cmd);
}

static int ccp_crypto_enqueue_cmd(struct ccp_crypto_cmd *crypto_cmd)
{
	struct ccp_crypto_cmd *active = NULL, *tmp;
	unsigned long flags;
	bool free_cmd = true;
	int ret;

	spin_lock_irqsave(&req_queue_lock, flags);

	 
	if (req_queue.cmd_count >= CCP_CRYPTO_MAX_QLEN) {
		if (!(crypto_cmd->cmd->flags & CCP_CMD_MAY_BACKLOG)) {
			ret = -ENOSPC;
			goto e_lock;
		}
	}

	 
	list_for_each_entry(tmp, &req_queue.cmds, entry) {
		if (crypto_cmd->tfm != tmp->tfm)
			continue;
		active = tmp;
		break;
	}

	ret = -EINPROGRESS;
	if (!active) {
		ret = ccp_enqueue_cmd(crypto_cmd->cmd);
		if (!ccp_crypto_success(ret))
			goto e_lock;	 
	}

	if (req_queue.cmd_count >= CCP_CRYPTO_MAX_QLEN) {
		ret = -EBUSY;
		if (req_queue.backlog == &req_queue.cmds)
			req_queue.backlog = &crypto_cmd->entry;
	}
	crypto_cmd->ret = ret;

	req_queue.cmd_count++;
	list_add_tail(&crypto_cmd->entry, &req_queue.cmds);

	free_cmd = false;

e_lock:
	spin_unlock_irqrestore(&req_queue_lock, flags);

	if (free_cmd)
		kfree(crypto_cmd);

	return ret;
}

 
int ccp_crypto_enqueue_request(struct crypto_async_request *req,
			       struct ccp_cmd *cmd)
{
	struct ccp_crypto_cmd *crypto_cmd;
	gfp_t gfp;

	gfp = req->flags & CRYPTO_TFM_REQ_MAY_SLEEP ? GFP_KERNEL : GFP_ATOMIC;

	crypto_cmd = kzalloc(sizeof(*crypto_cmd), gfp);
	if (!crypto_cmd)
		return -ENOMEM;

	 
	crypto_cmd->cmd = cmd;
	crypto_cmd->req = req;
	crypto_cmd->tfm = req->tfm;

	cmd->callback = ccp_crypto_complete;
	cmd->data = crypto_cmd;

	if (req->flags & CRYPTO_TFM_REQ_MAY_BACKLOG)
		cmd->flags |= CCP_CMD_MAY_BACKLOG;
	else
		cmd->flags &= ~CCP_CMD_MAY_BACKLOG;

	return ccp_crypto_enqueue_cmd(crypto_cmd);
}

struct scatterlist *ccp_crypto_sg_table_add(struct sg_table *table,
					    struct scatterlist *sg_add)
{
	struct scatterlist *sg, *sg_last = NULL;

	for (sg = table->sgl; sg; sg = sg_next(sg))
		if (!sg_page(sg))
			break;
	if (WARN_ON(!sg))
		return NULL;

	for (; sg && sg_add; sg = sg_next(sg), sg_add = sg_next(sg_add)) {
		sg_set_page(sg, sg_page(sg_add), sg_add->length,
			    sg_add->offset);
		sg_last = sg;
	}
	if (WARN_ON(sg_add))
		return NULL;

	return sg_last;
}

static int ccp_register_algs(void)
{
	int ret;

	if (!aes_disable) {
		ret = ccp_register_aes_algs(&skcipher_algs);
		if (ret)
			return ret;

		ret = ccp_register_aes_cmac_algs(&hash_algs);
		if (ret)
			return ret;

		ret = ccp_register_aes_xts_algs(&skcipher_algs);
		if (ret)
			return ret;

		ret = ccp_register_aes_aeads(&aead_algs);
		if (ret)
			return ret;
	}

	if (!des3_disable) {
		ret = ccp_register_des3_algs(&skcipher_algs);
		if (ret)
			return ret;
	}

	if (!sha_disable) {
		ret = ccp_register_sha_algs(&hash_algs);
		if (ret)
			return ret;
	}

	if (!rsa_disable) {
		ret = ccp_register_rsa_algs(&akcipher_algs);
		if (ret)
			return ret;
	}

	return 0;
}

static void ccp_unregister_algs(void)
{
	struct ccp_crypto_ahash_alg *ahash_alg, *ahash_tmp;
	struct ccp_crypto_skcipher_alg *ablk_alg, *ablk_tmp;
	struct ccp_crypto_aead *aead_alg, *aead_tmp;
	struct ccp_crypto_akcipher_alg *akc_alg, *akc_tmp;

	list_for_each_entry_safe(ahash_alg, ahash_tmp, &hash_algs, entry) {
		crypto_unregister_ahash(&ahash_alg->alg);
		list_del(&ahash_alg->entry);
		kfree(ahash_alg);
	}

	list_for_each_entry_safe(ablk_alg, ablk_tmp, &skcipher_algs, entry) {
		crypto_unregister_skcipher(&ablk_alg->alg);
		list_del(&ablk_alg->entry);
		kfree(ablk_alg);
	}

	list_for_each_entry_safe(aead_alg, aead_tmp, &aead_algs, entry) {
		crypto_unregister_aead(&aead_alg->alg);
		list_del(&aead_alg->entry);
		kfree(aead_alg);
	}

	list_for_each_entry_safe(akc_alg, akc_tmp, &akcipher_algs, entry) {
		crypto_unregister_akcipher(&akc_alg->alg);
		list_del(&akc_alg->entry);
		kfree(akc_alg);
	}
}

static int __init ccp_crypto_init(void)
{
	int ret;

	ret = ccp_present();
	if (ret) {
		pr_err("Cannot load: there are no available CCPs\n");
		return ret;
	}

	INIT_LIST_HEAD(&req_queue.cmds);
	req_queue.backlog = &req_queue.cmds;
	req_queue.cmd_count = 0;

	ret = ccp_register_algs();
	if (ret)
		ccp_unregister_algs();

	return ret;
}

static void __exit ccp_crypto_exit(void)
{
	ccp_unregister_algs();
}

module_init(ccp_crypto_init);
module_exit(ccp_crypto_exit);
