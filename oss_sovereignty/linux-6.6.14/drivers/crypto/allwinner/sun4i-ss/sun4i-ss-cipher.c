
 
#include "sun4i-ss.h"

static int noinline_for_stack sun4i_ss_opti_poll(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_ss_ctx *ss = op->ss;
	unsigned int ivsize = crypto_skcipher_ivsize(tfm);
	struct sun4i_cipher_req_ctx *ctx = skcipher_request_ctx(areq);
	u32 mode = ctx->mode;
	 
	u32 rx_cnt = SS_RX_DEFAULT;
	u32 tx_cnt = 0;
	u32 spaces;
	u32 v;
	int err = 0;
	unsigned int i;
	unsigned int ileft = areq->cryptlen;
	unsigned int oleft = areq->cryptlen;
	unsigned int todo;
	unsigned long pi = 0, po = 0;  
	bool miter_err;
	struct sg_mapping_iter mi, mo;
	unsigned int oi, oo;  
	unsigned long flags;
	struct skcipher_alg *alg = crypto_skcipher_alg(tfm);
	struct sun4i_ss_alg_template *algt;

	if (!areq->cryptlen)
		return 0;

	if (!areq->src || !areq->dst) {
		dev_err_ratelimited(ss->dev, "ERROR: Some SGs are NULL\n");
		return -EINVAL;
	}

	if (areq->iv && ivsize > 0 && mode & SS_DECRYPTION) {
		scatterwalk_map_and_copy(ctx->backup_iv, areq->src,
					 areq->cryptlen - ivsize, ivsize, 0);
	}

	if (IS_ENABLED(CONFIG_CRYPTO_DEV_SUN4I_SS_DEBUG)) {
		algt = container_of(alg, struct sun4i_ss_alg_template, alg.crypto);
		algt->stat_opti++;
		algt->stat_bytes += areq->cryptlen;
	}

	spin_lock_irqsave(&ss->slock, flags);

	for (i = 0; i < op->keylen / 4; i++)
		writesl(ss->base + SS_KEY0 + i * 4, &op->key[i], 1);

	if (areq->iv) {
		for (i = 0; i < 4 && i < ivsize / 4; i++) {
			v = *(u32 *)(areq->iv + i * 4);
			writesl(ss->base + SS_IV0 + i * 4, &v, 1);
		}
	}
	writel(mode, ss->base + SS_CTL);


	ileft = areq->cryptlen / 4;
	oleft = areq->cryptlen / 4;
	oi = 0;
	oo = 0;
	do {
		if (ileft) {
			sg_miter_start(&mi, areq->src, sg_nents(areq->src),
					SG_MITER_FROM_SG | SG_MITER_ATOMIC);
			if (pi)
				sg_miter_skip(&mi, pi);
			miter_err = sg_miter_next(&mi);
			if (!miter_err || !mi.addr) {
				dev_err_ratelimited(ss->dev, "ERROR: sg_miter return null\n");
				err = -EINVAL;
				goto release_ss;
			}
			todo = min(rx_cnt, ileft);
			todo = min_t(size_t, todo, (mi.length - oi) / 4);
			if (todo) {
				ileft -= todo;
				writesl(ss->base + SS_RXFIFO, mi.addr + oi, todo);
				oi += todo * 4;
			}
			if (oi == mi.length) {
				pi += mi.length;
				oi = 0;
			}
			sg_miter_stop(&mi);
		}

		spaces = readl(ss->base + SS_FCSR);
		rx_cnt = SS_RXFIFO_SPACES(spaces);
		tx_cnt = SS_TXFIFO_SPACES(spaces);

		sg_miter_start(&mo, areq->dst, sg_nents(areq->dst),
			       SG_MITER_TO_SG | SG_MITER_ATOMIC);
		if (po)
			sg_miter_skip(&mo, po);
		miter_err = sg_miter_next(&mo);
		if (!miter_err || !mo.addr) {
			dev_err_ratelimited(ss->dev, "ERROR: sg_miter return null\n");
			err = -EINVAL;
			goto release_ss;
		}
		todo = min(tx_cnt, oleft);
		todo = min_t(size_t, todo, (mo.length - oo) / 4);
		if (todo) {
			oleft -= todo;
			readsl(ss->base + SS_TXFIFO, mo.addr + oo, todo);
			oo += todo * 4;
		}
		if (oo == mo.length) {
			oo = 0;
			po += mo.length;
		}
		sg_miter_stop(&mo);
	} while (oleft);

	if (areq->iv) {
		if (mode & SS_DECRYPTION) {
			memcpy(areq->iv, ctx->backup_iv, ivsize);
			memzero_explicit(ctx->backup_iv, ivsize);
		} else {
			scatterwalk_map_and_copy(areq->iv, areq->dst, areq->cryptlen - ivsize,
						 ivsize, 0);
		}
	}

release_ss:
	writel(0, ss->base + SS_CTL);
	spin_unlock_irqrestore(&ss->slock, flags);
	return err;
}

static int noinline_for_stack sun4i_ss_cipher_poll_fallback(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *ctx = skcipher_request_ctx(areq);
	int err;
	struct skcipher_alg *alg = crypto_skcipher_alg(tfm);
	struct sun4i_ss_alg_template *algt;

	if (IS_ENABLED(CONFIG_CRYPTO_DEV_SUN4I_SS_DEBUG)) {
		algt = container_of(alg, struct sun4i_ss_alg_template, alg.crypto);
		algt->stat_fb++;
	}

	skcipher_request_set_tfm(&ctx->fallback_req, op->fallback_tfm);
	skcipher_request_set_callback(&ctx->fallback_req, areq->base.flags,
				      areq->base.complete, areq->base.data);
	skcipher_request_set_crypt(&ctx->fallback_req, areq->src, areq->dst,
				   areq->cryptlen, areq->iv);
	if (ctx->mode & SS_DECRYPTION)
		err = crypto_skcipher_decrypt(&ctx->fallback_req);
	else
		err = crypto_skcipher_encrypt(&ctx->fallback_req);

	return err;
}

 
static int sun4i_ss_cipher_poll(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_ss_ctx *ss = op->ss;
	int no_chunk = 1;
	struct scatterlist *in_sg = areq->src;
	struct scatterlist *out_sg = areq->dst;
	unsigned int ivsize = crypto_skcipher_ivsize(tfm);
	struct sun4i_cipher_req_ctx *ctx = skcipher_request_ctx(areq);
	struct skcipher_alg *alg = crypto_skcipher_alg(tfm);
	struct sun4i_ss_alg_template *algt;
	u32 mode = ctx->mode;
	 
	u32 rx_cnt = SS_RX_DEFAULT;
	u32 tx_cnt = 0;
	u32 v;
	u32 spaces;
	int err = 0;
	unsigned int i;
	unsigned int ileft = areq->cryptlen;
	unsigned int oleft = areq->cryptlen;
	unsigned int todo;
	struct sg_mapping_iter mi, mo;
	unsigned long pi = 0, po = 0;  
	bool miter_err;
	unsigned int oi, oo;	 
	unsigned int ob = 0;	 
	unsigned int obo = 0;	 
	unsigned int obl = 0;	 
	unsigned long flags;
	bool need_fallback = false;

	if (!areq->cryptlen)
		return 0;

	if (!areq->src || !areq->dst) {
		dev_err_ratelimited(ss->dev, "ERROR: Some SGs are NULL\n");
		return -EINVAL;
	}

	algt = container_of(alg, struct sun4i_ss_alg_template, alg.crypto);
	if (areq->cryptlen % algt->alg.crypto.base.cra_blocksize)
		need_fallback = true;

	 
	while (in_sg && no_chunk == 1) {
		if ((in_sg->length | in_sg->offset) & 3u)
			no_chunk = 0;
		in_sg = sg_next(in_sg);
	}
	while (out_sg && no_chunk == 1) {
		if ((out_sg->length | out_sg->offset) & 3u)
			no_chunk = 0;
		out_sg = sg_next(out_sg);
	}

	if (no_chunk == 1 && !need_fallback)
		return sun4i_ss_opti_poll(areq);

	if (need_fallback)
		return sun4i_ss_cipher_poll_fallback(areq);

	if (areq->iv && ivsize > 0 && mode & SS_DECRYPTION) {
		scatterwalk_map_and_copy(ctx->backup_iv, areq->src,
					 areq->cryptlen - ivsize, ivsize, 0);
	}

	if (IS_ENABLED(CONFIG_CRYPTO_DEV_SUN4I_SS_DEBUG)) {
		algt->stat_req++;
		algt->stat_bytes += areq->cryptlen;
	}

	spin_lock_irqsave(&ss->slock, flags);

	for (i = 0; i < op->keylen / 4; i++)
		writesl(ss->base + SS_KEY0 + i * 4, &op->key[i], 1);

	if (areq->iv) {
		for (i = 0; i < 4 && i < ivsize / 4; i++) {
			v = *(u32 *)(areq->iv + i * 4);
			writesl(ss->base + SS_IV0 + i * 4, &v, 1);
		}
	}
	writel(mode, ss->base + SS_CTL);

	ileft = areq->cryptlen;
	oleft = areq->cryptlen;
	oi = 0;
	oo = 0;

	while (oleft) {
		if (ileft) {
			sg_miter_start(&mi, areq->src, sg_nents(areq->src),
				       SG_MITER_FROM_SG | SG_MITER_ATOMIC);
			if (pi)
				sg_miter_skip(&mi, pi);
			miter_err = sg_miter_next(&mi);
			if (!miter_err || !mi.addr) {
				dev_err_ratelimited(ss->dev, "ERROR: sg_miter return null\n");
				err = -EINVAL;
				goto release_ss;
			}
			 
			todo = min(rx_cnt, ileft / 4);
			todo = min_t(size_t, todo, (mi.length - oi) / 4);
			if (todo && !ob) {
				writesl(ss->base + SS_RXFIFO, mi.addr + oi,
					todo);
				ileft -= todo * 4;
				oi += todo * 4;
			} else {
				 
				todo = min(rx_cnt * 4 - ob, ileft);
				todo = min_t(size_t, todo, mi.length - oi);
				memcpy(ss->buf + ob, mi.addr + oi, todo);
				ileft -= todo;
				oi += todo;
				ob += todo;
				if (!(ob % 4)) {
					writesl(ss->base + SS_RXFIFO, ss->buf,
						ob / 4);
					ob = 0;
				}
			}
			if (oi == mi.length) {
				pi += mi.length;
				oi = 0;
			}
			sg_miter_stop(&mi);
		}

		spaces = readl(ss->base + SS_FCSR);
		rx_cnt = SS_RXFIFO_SPACES(spaces);
		tx_cnt = SS_TXFIFO_SPACES(spaces);

		if (!tx_cnt)
			continue;
		sg_miter_start(&mo, areq->dst, sg_nents(areq->dst),
			       SG_MITER_TO_SG | SG_MITER_ATOMIC);
		if (po)
			sg_miter_skip(&mo, po);
		miter_err = sg_miter_next(&mo);
		if (!miter_err || !mo.addr) {
			dev_err_ratelimited(ss->dev, "ERROR: sg_miter return null\n");
			err = -EINVAL;
			goto release_ss;
		}
		 
		todo = min(tx_cnt, oleft / 4);
		todo = min_t(size_t, todo, (mo.length - oo) / 4);

		if (todo) {
			readsl(ss->base + SS_TXFIFO, mo.addr + oo, todo);
			oleft -= todo * 4;
			oo += todo * 4;
			if (oo == mo.length) {
				po += mo.length;
				oo = 0;
			}
		} else {
			 
			readsl(ss->base + SS_TXFIFO, ss->bufo, tx_cnt);
			obl = tx_cnt * 4;
			obo = 0;
			do {
				 
				todo = min_t(size_t,
					     mo.length - oo, obl - obo);
				memcpy(mo.addr + oo, ss->bufo + obo, todo);
				oleft -= todo;
				obo += todo;
				oo += todo;
				if (oo == mo.length) {
					po += mo.length;
					sg_miter_next(&mo);
					oo = 0;
				}
			} while (obo < obl);
			 
		}
		sg_miter_stop(&mo);
	}
	if (areq->iv) {
		if (mode & SS_DECRYPTION) {
			memcpy(areq->iv, ctx->backup_iv, ivsize);
			memzero_explicit(ctx->backup_iv, ivsize);
		} else {
			scatterwalk_map_and_copy(areq->iv, areq->dst, areq->cryptlen - ivsize,
						 ivsize, 0);
		}
	}

release_ss:
	writel(0, ss->base + SS_CTL);
	spin_unlock_irqrestore(&ss->slock, flags);

	return err;
}

 
int sun4i_ss_cbc_aes_encrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_AES | SS_CBC | SS_ENABLED | SS_ENCRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

int sun4i_ss_cbc_aes_decrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_AES | SS_CBC | SS_ENABLED | SS_DECRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

 
int sun4i_ss_ecb_aes_encrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_AES | SS_ECB | SS_ENABLED | SS_ENCRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

int sun4i_ss_ecb_aes_decrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_AES | SS_ECB | SS_ENABLED | SS_DECRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

 
int sun4i_ss_cbc_des_encrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_DES | SS_CBC | SS_ENABLED | SS_ENCRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

int sun4i_ss_cbc_des_decrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_DES | SS_CBC | SS_ENABLED | SS_DECRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

 
int sun4i_ss_ecb_des_encrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_DES | SS_ECB | SS_ENABLED | SS_ENCRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

int sun4i_ss_ecb_des_decrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_DES | SS_ECB | SS_ENABLED | SS_DECRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

 
int sun4i_ss_cbc_des3_encrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_3DES | SS_CBC | SS_ENABLED | SS_ENCRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

int sun4i_ss_cbc_des3_decrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_3DES | SS_CBC | SS_ENABLED | SS_DECRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

 
int sun4i_ss_ecb_des3_encrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_3DES | SS_ECB | SS_ENABLED | SS_ENCRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

int sun4i_ss_ecb_des3_decrypt(struct skcipher_request *areq)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = skcipher_request_ctx(areq);

	rctx->mode = SS_OP_3DES | SS_ECB | SS_ENABLED | SS_DECRYPTION |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

int sun4i_ss_cipher_init(struct crypto_tfm *tfm)
{
	struct sun4i_tfm_ctx *op = crypto_tfm_ctx(tfm);
	struct sun4i_ss_alg_template *algt;
	const char *name = crypto_tfm_alg_name(tfm);
	int err;

	memset(op, 0, sizeof(struct sun4i_tfm_ctx));

	algt = container_of(tfm->__crt_alg, struct sun4i_ss_alg_template,
			    alg.crypto.base);
	op->ss = algt->ss;

	op->fallback_tfm = crypto_alloc_skcipher(name, 0, CRYPTO_ALG_NEED_FALLBACK);
	if (IS_ERR(op->fallback_tfm)) {
		dev_err(op->ss->dev, "ERROR: Cannot allocate fallback for %s %ld\n",
			name, PTR_ERR(op->fallback_tfm));
		return PTR_ERR(op->fallback_tfm);
	}

	crypto_skcipher_set_reqsize(__crypto_skcipher_cast(tfm),
				    sizeof(struct sun4i_cipher_req_ctx) +
				    crypto_skcipher_reqsize(op->fallback_tfm));

	err = pm_runtime_resume_and_get(op->ss->dev);
	if (err < 0)
		goto error_pm;

	return 0;
error_pm:
	crypto_free_skcipher(op->fallback_tfm);
	return err;
}

void sun4i_ss_cipher_exit(struct crypto_tfm *tfm)
{
	struct sun4i_tfm_ctx *op = crypto_tfm_ctx(tfm);

	crypto_free_skcipher(op->fallback_tfm);
	pm_runtime_put(op->ss->dev);
}

 
int sun4i_ss_aes_setkey(struct crypto_skcipher *tfm, const u8 *key,
			unsigned int keylen)
{
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	struct sun4i_ss_ctx *ss = op->ss;

	switch (keylen) {
	case 128 / 8:
		op->keymode = SS_AES_128BITS;
		break;
	case 192 / 8:
		op->keymode = SS_AES_192BITS;
		break;
	case 256 / 8:
		op->keymode = SS_AES_256BITS;
		break;
	default:
		dev_dbg(ss->dev, "ERROR: Invalid keylen %u\n", keylen);
		return -EINVAL;
	}
	op->keylen = keylen;
	memcpy(op->key, key, keylen);

	crypto_skcipher_clear_flags(op->fallback_tfm, CRYPTO_TFM_REQ_MASK);
	crypto_skcipher_set_flags(op->fallback_tfm, tfm->base.crt_flags & CRYPTO_TFM_REQ_MASK);

	return crypto_skcipher_setkey(op->fallback_tfm, key, keylen);
}

 
int sun4i_ss_des_setkey(struct crypto_skcipher *tfm, const u8 *key,
			unsigned int keylen)
{
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	int err;

	err = verify_skcipher_des_key(tfm, key);
	if (err)
		return err;

	op->keylen = keylen;
	memcpy(op->key, key, keylen);

	crypto_skcipher_clear_flags(op->fallback_tfm, CRYPTO_TFM_REQ_MASK);
	crypto_skcipher_set_flags(op->fallback_tfm, tfm->base.crt_flags & CRYPTO_TFM_REQ_MASK);

	return crypto_skcipher_setkey(op->fallback_tfm, key, keylen);
}

 
int sun4i_ss_des3_setkey(struct crypto_skcipher *tfm, const u8 *key,
			 unsigned int keylen)
{
	struct sun4i_tfm_ctx *op = crypto_skcipher_ctx(tfm);
	int err;

	err = verify_skcipher_des3_key(tfm, key);
	if (err)
		return err;

	op->keylen = keylen;
	memcpy(op->key, key, keylen);

	crypto_skcipher_clear_flags(op->fallback_tfm, CRYPTO_TFM_REQ_MASK);
	crypto_skcipher_set_flags(op->fallback_tfm, tfm->base.crt_flags & CRYPTO_TFM_REQ_MASK);

	return crypto_skcipher_setkey(op->fallback_tfm, key, keylen);
}
