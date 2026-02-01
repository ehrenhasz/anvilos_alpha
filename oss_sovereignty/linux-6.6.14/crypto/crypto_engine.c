
 

#include <crypto/internal/aead.h>
#include <crypto/internal/akcipher.h>
#include <crypto/internal/engine.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/kpp.h>
#include <crypto/internal/skcipher.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <uapi/linux/sched/types.h>
#include "internal.h"

#define CRYPTO_ENGINE_MAX_QLEN 10

 
#define CRYPTO_ALG_ENGINE 0x200

struct crypto_engine_alg {
	struct crypto_alg base;
	struct crypto_engine_op op;
};

 
static void crypto_finalize_request(struct crypto_engine *engine,
				    struct crypto_async_request *req, int err)
{
	unsigned long flags;

	 
	if (!engine->retry_support) {
		spin_lock_irqsave(&engine->queue_lock, flags);
		if (engine->cur_req == req) {
			engine->cur_req = NULL;
		}
		spin_unlock_irqrestore(&engine->queue_lock, flags);
	}

	lockdep_assert_in_softirq();
	crypto_request_complete(req, err);

	kthread_queue_work(engine->kworker, &engine->pump_requests);
}

 
static void crypto_pump_requests(struct crypto_engine *engine,
				 bool in_kthread)
{
	struct crypto_async_request *async_req, *backlog;
	struct crypto_engine_alg *alg;
	struct crypto_engine_op *op;
	unsigned long flags;
	bool was_busy = false;
	int ret;

	spin_lock_irqsave(&engine->queue_lock, flags);

	 
	if (!engine->retry_support && engine->cur_req)
		goto out;

	 
	if (engine->idling) {
		kthread_queue_work(engine->kworker, &engine->pump_requests);
		goto out;
	}

	 
	if (!crypto_queue_len(&engine->queue) || !engine->running) {
		if (!engine->busy)
			goto out;

		 
		if (!in_kthread) {
			kthread_queue_work(engine->kworker,
					   &engine->pump_requests);
			goto out;
		}

		engine->busy = false;
		engine->idling = true;
		spin_unlock_irqrestore(&engine->queue_lock, flags);

		if (engine->unprepare_crypt_hardware &&
		    engine->unprepare_crypt_hardware(engine))
			dev_err(engine->dev, "failed to unprepare crypt hardware\n");

		spin_lock_irqsave(&engine->queue_lock, flags);
		engine->idling = false;
		goto out;
	}

start_request:
	 
	backlog = crypto_get_backlog(&engine->queue);
	async_req = crypto_dequeue_request(&engine->queue);
	if (!async_req)
		goto out;

	 
	if (!engine->retry_support)
		engine->cur_req = async_req;

	if (engine->busy)
		was_busy = true;
	else
		engine->busy = true;

	spin_unlock_irqrestore(&engine->queue_lock, flags);

	 
	if (!was_busy && engine->prepare_crypt_hardware) {
		ret = engine->prepare_crypt_hardware(engine);
		if (ret) {
			dev_err(engine->dev, "failed to prepare crypt hardware\n");
			goto req_err_1;
		}
	}

	if (async_req->tfm->__crt_alg->cra_flags & CRYPTO_ALG_ENGINE) {
		alg = container_of(async_req->tfm->__crt_alg,
				   struct crypto_engine_alg, base);
		op = &alg->op;
	} else {
		dev_err(engine->dev, "failed to do request\n");
		ret = -EINVAL;
		goto req_err_1;
	}

	ret = op->do_one_request(engine, async_req);

	 
	if (ret < 0) {
		 
		if (!engine->retry_support ||
		    (ret != -ENOSPC)) {
			dev_err(engine->dev,
				"Failed to do one request from queue: %d\n",
				ret);
			goto req_err_1;
		}
		spin_lock_irqsave(&engine->queue_lock, flags);
		 
		crypto_enqueue_request_head(&engine->queue, async_req);

		kthread_queue_work(engine->kworker, &engine->pump_requests);
		goto out;
	}

	goto retry;

req_err_1:
	crypto_request_complete(async_req, ret);

retry:
	if (backlog)
		crypto_request_complete(backlog, -EINPROGRESS);

	 
	if (engine->retry_support) {
		spin_lock_irqsave(&engine->queue_lock, flags);
		goto start_request;
	}
	return;

out:
	spin_unlock_irqrestore(&engine->queue_lock, flags);

	 
	if (engine->do_batch_requests) {
		ret = engine->do_batch_requests(engine);
		if (ret)
			dev_err(engine->dev, "failed to do batch requests: %d\n",
				ret);
	}

	return;
}

static void crypto_pump_work(struct kthread_work *work)
{
	struct crypto_engine *engine =
		container_of(work, struct crypto_engine, pump_requests);

	crypto_pump_requests(engine, true);
}

 
static int crypto_transfer_request(struct crypto_engine *engine,
				   struct crypto_async_request *req,
				   bool need_pump)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&engine->queue_lock, flags);

	if (!engine->running) {
		spin_unlock_irqrestore(&engine->queue_lock, flags);
		return -ESHUTDOWN;
	}

	ret = crypto_enqueue_request(&engine->queue, req);

	if (!engine->busy && need_pump)
		kthread_queue_work(engine->kworker, &engine->pump_requests);

	spin_unlock_irqrestore(&engine->queue_lock, flags);
	return ret;
}

 
static int crypto_transfer_request_to_engine(struct crypto_engine *engine,
					     struct crypto_async_request *req)
{
	return crypto_transfer_request(engine, req, true);
}

 
int crypto_transfer_aead_request_to_engine(struct crypto_engine *engine,
					   struct aead_request *req)
{
	return crypto_transfer_request_to_engine(engine, &req->base);
}
EXPORT_SYMBOL_GPL(crypto_transfer_aead_request_to_engine);

 
int crypto_transfer_akcipher_request_to_engine(struct crypto_engine *engine,
					       struct akcipher_request *req)
{
	return crypto_transfer_request_to_engine(engine, &req->base);
}
EXPORT_SYMBOL_GPL(crypto_transfer_akcipher_request_to_engine);

 
int crypto_transfer_hash_request_to_engine(struct crypto_engine *engine,
					   struct ahash_request *req)
{
	return crypto_transfer_request_to_engine(engine, &req->base);
}
EXPORT_SYMBOL_GPL(crypto_transfer_hash_request_to_engine);

 
int crypto_transfer_kpp_request_to_engine(struct crypto_engine *engine,
					  struct kpp_request *req)
{
	return crypto_transfer_request_to_engine(engine, &req->base);
}
EXPORT_SYMBOL_GPL(crypto_transfer_kpp_request_to_engine);

 
int crypto_transfer_skcipher_request_to_engine(struct crypto_engine *engine,
					       struct skcipher_request *req)
{
	return crypto_transfer_request_to_engine(engine, &req->base);
}
EXPORT_SYMBOL_GPL(crypto_transfer_skcipher_request_to_engine);

 
void crypto_finalize_aead_request(struct crypto_engine *engine,
				  struct aead_request *req, int err)
{
	return crypto_finalize_request(engine, &req->base, err);
}
EXPORT_SYMBOL_GPL(crypto_finalize_aead_request);

 
void crypto_finalize_akcipher_request(struct crypto_engine *engine,
				      struct akcipher_request *req, int err)
{
	return crypto_finalize_request(engine, &req->base, err);
}
EXPORT_SYMBOL_GPL(crypto_finalize_akcipher_request);

 
void crypto_finalize_hash_request(struct crypto_engine *engine,
				  struct ahash_request *req, int err)
{
	return crypto_finalize_request(engine, &req->base, err);
}
EXPORT_SYMBOL_GPL(crypto_finalize_hash_request);

 
void crypto_finalize_kpp_request(struct crypto_engine *engine,
				 struct kpp_request *req, int err)
{
	return crypto_finalize_request(engine, &req->base, err);
}
EXPORT_SYMBOL_GPL(crypto_finalize_kpp_request);

 
void crypto_finalize_skcipher_request(struct crypto_engine *engine,
				      struct skcipher_request *req, int err)
{
	return crypto_finalize_request(engine, &req->base, err);
}
EXPORT_SYMBOL_GPL(crypto_finalize_skcipher_request);

 
int crypto_engine_start(struct crypto_engine *engine)
{
	unsigned long flags;

	spin_lock_irqsave(&engine->queue_lock, flags);

	if (engine->running || engine->busy) {
		spin_unlock_irqrestore(&engine->queue_lock, flags);
		return -EBUSY;
	}

	engine->running = true;
	spin_unlock_irqrestore(&engine->queue_lock, flags);

	kthread_queue_work(engine->kworker, &engine->pump_requests);

	return 0;
}
EXPORT_SYMBOL_GPL(crypto_engine_start);

 
int crypto_engine_stop(struct crypto_engine *engine)
{
	unsigned long flags;
	unsigned int limit = 500;
	int ret = 0;

	spin_lock_irqsave(&engine->queue_lock, flags);

	 
	while ((crypto_queue_len(&engine->queue) || engine->busy) && limit--) {
		spin_unlock_irqrestore(&engine->queue_lock, flags);
		msleep(20);
		spin_lock_irqsave(&engine->queue_lock, flags);
	}

	if (crypto_queue_len(&engine->queue) || engine->busy)
		ret = -EBUSY;
	else
		engine->running = false;

	spin_unlock_irqrestore(&engine->queue_lock, flags);

	if (ret)
		dev_warn(engine->dev, "could not stop engine\n");

	return ret;
}
EXPORT_SYMBOL_GPL(crypto_engine_stop);

 
struct crypto_engine *crypto_engine_alloc_init_and_set(struct device *dev,
						       bool retry_support,
						       int (*cbk_do_batch)(struct crypto_engine *engine),
						       bool rt, int qlen)
{
	struct crypto_engine *engine;

	if (!dev)
		return NULL;

	engine = devm_kzalloc(dev, sizeof(*engine), GFP_KERNEL);
	if (!engine)
		return NULL;

	engine->dev = dev;
	engine->rt = rt;
	engine->running = false;
	engine->busy = false;
	engine->idling = false;
	engine->retry_support = retry_support;
	engine->priv_data = dev;
	 
	engine->do_batch_requests = retry_support ? cbk_do_batch : NULL;

	snprintf(engine->name, sizeof(engine->name),
		 "%s-engine", dev_name(dev));

	crypto_init_queue(&engine->queue, qlen);
	spin_lock_init(&engine->queue_lock);

	engine->kworker = kthread_create_worker(0, "%s", engine->name);
	if (IS_ERR(engine->kworker)) {
		dev_err(dev, "failed to create crypto request pump task\n");
		return NULL;
	}
	kthread_init_work(&engine->pump_requests, crypto_pump_work);

	if (engine->rt) {
		dev_info(dev, "will run requests pump with realtime priority\n");
		sched_set_fifo(engine->kworker->task);
	}

	return engine;
}
EXPORT_SYMBOL_GPL(crypto_engine_alloc_init_and_set);

 
struct crypto_engine *crypto_engine_alloc_init(struct device *dev, bool rt)
{
	return crypto_engine_alloc_init_and_set(dev, false, NULL, rt,
						CRYPTO_ENGINE_MAX_QLEN);
}
EXPORT_SYMBOL_GPL(crypto_engine_alloc_init);

 
int crypto_engine_exit(struct crypto_engine *engine)
{
	int ret;

	ret = crypto_engine_stop(engine);
	if (ret)
		return ret;

	kthread_destroy_worker(engine->kworker);

	return 0;
}
EXPORT_SYMBOL_GPL(crypto_engine_exit);

int crypto_engine_register_aead(struct aead_engine_alg *alg)
{
	if (!alg->op.do_one_request)
		return -EINVAL;

	alg->base.base.cra_flags |= CRYPTO_ALG_ENGINE;

	return crypto_register_aead(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_engine_register_aead);

void crypto_engine_unregister_aead(struct aead_engine_alg *alg)
{
	crypto_unregister_aead(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_engine_unregister_aead);

int crypto_engine_register_aeads(struct aead_engine_alg *algs, int count)
{
	int i, ret;

	for (i = 0; i < count; i++) {
		ret = crypto_engine_register_aead(&algs[i]);
		if (ret)
			goto err;
	}

	return 0;

err:
	crypto_engine_unregister_aeads(algs, i);

	return ret;
}
EXPORT_SYMBOL_GPL(crypto_engine_register_aeads);

void crypto_engine_unregister_aeads(struct aead_engine_alg *algs, int count)
{
	int i;

	for (i = count - 1; i >= 0; --i)
		crypto_engine_unregister_aead(&algs[i]);
}
EXPORT_SYMBOL_GPL(crypto_engine_unregister_aeads);

int crypto_engine_register_ahash(struct ahash_engine_alg *alg)
{
	if (!alg->op.do_one_request)
		return -EINVAL;

	alg->base.halg.base.cra_flags |= CRYPTO_ALG_ENGINE;

	return crypto_register_ahash(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_engine_register_ahash);

void crypto_engine_unregister_ahash(struct ahash_engine_alg *alg)
{
	crypto_unregister_ahash(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_engine_unregister_ahash);

int crypto_engine_register_ahashes(struct ahash_engine_alg *algs, int count)
{
	int i, ret;

	for (i = 0; i < count; i++) {
		ret = crypto_engine_register_ahash(&algs[i]);
		if (ret)
			goto err;
	}

	return 0;

err:
	crypto_engine_unregister_ahashes(algs, i);

	return ret;
}
EXPORT_SYMBOL_GPL(crypto_engine_register_ahashes);

void crypto_engine_unregister_ahashes(struct ahash_engine_alg *algs,
				      int count)
{
	int i;

	for (i = count - 1; i >= 0; --i)
		crypto_engine_unregister_ahash(&algs[i]);
}
EXPORT_SYMBOL_GPL(crypto_engine_unregister_ahashes);

int crypto_engine_register_akcipher(struct akcipher_engine_alg *alg)
{
	if (!alg->op.do_one_request)
		return -EINVAL;

	alg->base.base.cra_flags |= CRYPTO_ALG_ENGINE;

	return crypto_register_akcipher(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_engine_register_akcipher);

void crypto_engine_unregister_akcipher(struct akcipher_engine_alg *alg)
{
	crypto_unregister_akcipher(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_engine_unregister_akcipher);

int crypto_engine_register_kpp(struct kpp_engine_alg *alg)
{
	if (!alg->op.do_one_request)
		return -EINVAL;

	alg->base.base.cra_flags |= CRYPTO_ALG_ENGINE;

	return crypto_register_kpp(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_engine_register_kpp);

void crypto_engine_unregister_kpp(struct kpp_engine_alg *alg)
{
	crypto_unregister_kpp(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_engine_unregister_kpp);

int crypto_engine_register_skcipher(struct skcipher_engine_alg *alg)
{
	if (!alg->op.do_one_request)
		return -EINVAL;

	alg->base.base.cra_flags |= CRYPTO_ALG_ENGINE;

	return crypto_register_skcipher(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_engine_register_skcipher);

void crypto_engine_unregister_skcipher(struct skcipher_engine_alg *alg)
{
	return crypto_unregister_skcipher(&alg->base);
}
EXPORT_SYMBOL_GPL(crypto_engine_unregister_skcipher);

int crypto_engine_register_skciphers(struct skcipher_engine_alg *algs,
				     int count)
{
	int i, ret;

	for (i = 0; i < count; i++) {
		ret = crypto_engine_register_skcipher(&algs[i]);
		if (ret)
			goto err;
	}

	return 0;

err:
	crypto_engine_unregister_skciphers(algs, i);

	return ret;
}
EXPORT_SYMBOL_GPL(crypto_engine_register_skciphers);

void crypto_engine_unregister_skciphers(struct skcipher_engine_alg *algs,
					int count)
{
	int i;

	for (i = count - 1; i >= 0; --i)
		crypto_engine_unregister_skcipher(&algs[i]);
}
EXPORT_SYMBOL_GPL(crypto_engine_unregister_skciphers);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Crypto hardware engine framework");
