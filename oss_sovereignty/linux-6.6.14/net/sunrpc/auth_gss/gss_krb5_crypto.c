 

 

#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <crypto/skcipher.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/random.h>
#include <linux/sunrpc/gss_krb5.h>
#include <linux/sunrpc/xdr.h>
#include <kunit/visibility.h>

#include "gss_krb5_internal.h"

#if IS_ENABLED(CONFIG_SUNRPC_DEBUG)
# define RPCDBG_FACILITY        RPCDBG_AUTH
#endif

 
void krb5_make_confounder(u8 *p, int conflen)
{
	get_random_bytes(p, conflen);
}

 
u32
krb5_encrypt(
	struct crypto_sync_skcipher *tfm,
	void * iv,
	void * in,
	void * out,
	int length)
{
	u32 ret = -EINVAL;
	struct scatterlist sg[1];
	u8 local_iv[GSS_KRB5_MAX_BLOCKSIZE] = {0};
	SYNC_SKCIPHER_REQUEST_ON_STACK(req, tfm);

	if (length % crypto_sync_skcipher_blocksize(tfm) != 0)
		goto out;

	if (crypto_sync_skcipher_ivsize(tfm) > GSS_KRB5_MAX_BLOCKSIZE) {
		dprintk("RPC:       gss_k5encrypt: tfm iv size too large %d\n",
			crypto_sync_skcipher_ivsize(tfm));
		goto out;
	}

	if (iv)
		memcpy(local_iv, iv, crypto_sync_skcipher_ivsize(tfm));

	memcpy(out, in, length);
	sg_init_one(sg, out, length);

	skcipher_request_set_sync_tfm(req, tfm);
	skcipher_request_set_callback(req, 0, NULL, NULL);
	skcipher_request_set_crypt(req, sg, sg, length, local_iv);

	ret = crypto_skcipher_encrypt(req);
	skcipher_request_zero(req);
out:
	dprintk("RPC:       krb5_encrypt returns %d\n", ret);
	return ret;
}

 
u32
krb5_decrypt(
     struct crypto_sync_skcipher *tfm,
     void * iv,
     void * in,
     void * out,
     int length)
{
	u32 ret = -EINVAL;
	struct scatterlist sg[1];
	u8 local_iv[GSS_KRB5_MAX_BLOCKSIZE] = {0};
	SYNC_SKCIPHER_REQUEST_ON_STACK(req, tfm);

	if (length % crypto_sync_skcipher_blocksize(tfm) != 0)
		goto out;

	if (crypto_sync_skcipher_ivsize(tfm) > GSS_KRB5_MAX_BLOCKSIZE) {
		dprintk("RPC:       gss_k5decrypt: tfm iv size too large %d\n",
			crypto_sync_skcipher_ivsize(tfm));
		goto out;
	}
	if (iv)
		memcpy(local_iv, iv, crypto_sync_skcipher_ivsize(tfm));

	memcpy(out, in, length);
	sg_init_one(sg, out, length);

	skcipher_request_set_sync_tfm(req, tfm);
	skcipher_request_set_callback(req, 0, NULL, NULL);
	skcipher_request_set_crypt(req, sg, sg, length, local_iv);

	ret = crypto_skcipher_decrypt(req);
	skcipher_request_zero(req);
out:
	dprintk("RPC:       gss_k5decrypt returns %d\n",ret);
	return ret;
}

static int
checksummer(struct scatterlist *sg, void *data)
{
	struct ahash_request *req = data;

	ahash_request_set_crypt(req, sg, NULL, sg->length);

	return crypto_ahash_update(req);
}

 
u32
make_checksum(struct krb5_ctx *kctx, char *header, int hdrlen,
	      struct xdr_buf *body, int body_offset, u8 *cksumkey,
	      unsigned int usage, struct xdr_netobj *cksumout)
{
	struct crypto_ahash *tfm;
	struct ahash_request *req;
	struct scatterlist              sg[1];
	int err = -1;
	u8 *checksumdata;
	unsigned int checksumlen;

	if (cksumout->len < kctx->gk5e->cksumlength) {
		dprintk("%s: checksum buffer length, %u, too small for %s\n",
			__func__, cksumout->len, kctx->gk5e->name);
		return GSS_S_FAILURE;
	}

	checksumdata = kmalloc(GSS_KRB5_MAX_CKSUM_LEN, GFP_KERNEL);
	if (checksumdata == NULL)
		return GSS_S_FAILURE;

	tfm = crypto_alloc_ahash(kctx->gk5e->cksum_name, 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(tfm))
		goto out_free_cksum;

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req)
		goto out_free_ahash;

	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_SLEEP, NULL, NULL);

	checksumlen = crypto_ahash_digestsize(tfm);

	if (cksumkey != NULL) {
		err = crypto_ahash_setkey(tfm, cksumkey,
					  kctx->gk5e->keylength);
		if (err)
			goto out;
	}

	err = crypto_ahash_init(req);
	if (err)
		goto out;
	sg_init_one(sg, header, hdrlen);
	ahash_request_set_crypt(req, sg, NULL, hdrlen);
	err = crypto_ahash_update(req);
	if (err)
		goto out;
	err = xdr_process_buf(body, body_offset, body->len - body_offset,
			      checksummer, req);
	if (err)
		goto out;
	ahash_request_set_crypt(req, NULL, checksumdata, 0);
	err = crypto_ahash_final(req);
	if (err)
		goto out;

	switch (kctx->gk5e->ctype) {
	case CKSUMTYPE_RSA_MD5:
		err = krb5_encrypt(kctx->seq, NULL, checksumdata,
				   checksumdata, checksumlen);
		if (err)
			goto out;
		memcpy(cksumout->data,
		       checksumdata + checksumlen - kctx->gk5e->cksumlength,
		       kctx->gk5e->cksumlength);
		break;
	case CKSUMTYPE_HMAC_SHA1_DES3:
		memcpy(cksumout->data, checksumdata, kctx->gk5e->cksumlength);
		break;
	default:
		BUG();
		break;
	}
	cksumout->len = kctx->gk5e->cksumlength;
out:
	ahash_request_free(req);
out_free_ahash:
	crypto_free_ahash(tfm);
out_free_cksum:
	kfree(checksumdata);
	return err ? GSS_S_FAILURE : 0;
}

 
u32
gss_krb5_checksum(struct crypto_ahash *tfm, char *header, int hdrlen,
		  const struct xdr_buf *body, int body_offset,
		  struct xdr_netobj *cksumout)
{
	struct ahash_request *req;
	int err = -ENOMEM;
	u8 *checksumdata;

	checksumdata = kmalloc(crypto_ahash_digestsize(tfm), GFP_KERNEL);
	if (!checksumdata)
		return GSS_S_FAILURE;

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req)
		goto out_free_cksum;
	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_SLEEP, NULL, NULL);
	err = crypto_ahash_init(req);
	if (err)
		goto out_free_ahash;

	 
	err = xdr_process_buf(body, body_offset, body->len - body_offset,
			      checksummer, req);
	if (err)
		goto out_free_ahash;
	if (header) {
		struct scatterlist sg[1];

		sg_init_one(sg, header, hdrlen);
		ahash_request_set_crypt(req, sg, NULL, hdrlen);
		err = crypto_ahash_update(req);
		if (err)
			goto out_free_ahash;
	}

	ahash_request_set_crypt(req, NULL, checksumdata, 0);
	err = crypto_ahash_final(req);
	if (err)
		goto out_free_ahash;

	memcpy(cksumout->data, checksumdata,
	       min_t(int, cksumout->len, crypto_ahash_digestsize(tfm)));

out_free_ahash:
	ahash_request_free(req);
out_free_cksum:
	kfree_sensitive(checksumdata);
	return err ? GSS_S_FAILURE : GSS_S_COMPLETE;
}
EXPORT_SYMBOL_IF_KUNIT(gss_krb5_checksum);

struct encryptor_desc {
	u8 iv[GSS_KRB5_MAX_BLOCKSIZE];
	struct skcipher_request *req;
	int pos;
	struct xdr_buf *outbuf;
	struct page **pages;
	struct scatterlist infrags[4];
	struct scatterlist outfrags[4];
	int fragno;
	int fraglen;
};

static int
encryptor(struct scatterlist *sg, void *data)
{
	struct encryptor_desc *desc = data;
	struct xdr_buf *outbuf = desc->outbuf;
	struct crypto_sync_skcipher *tfm =
		crypto_sync_skcipher_reqtfm(desc->req);
	struct page *in_page;
	int thislen = desc->fraglen + sg->length;
	int fraglen, ret;
	int page_pos;

	 
	BUG_ON(desc->fragno > 3);

	page_pos = desc->pos - outbuf->head[0].iov_len;
	if (page_pos >= 0 && page_pos < outbuf->page_len) {
		 
		int i = (page_pos + outbuf->page_base) >> PAGE_SHIFT;
		in_page = desc->pages[i];
	} else {
		in_page = sg_page(sg);
	}
	sg_set_page(&desc->infrags[desc->fragno], in_page, sg->length,
		    sg->offset);
	sg_set_page(&desc->outfrags[desc->fragno], sg_page(sg), sg->length,
		    sg->offset);
	desc->fragno++;
	desc->fraglen += sg->length;
	desc->pos += sg->length;

	fraglen = thislen & (crypto_sync_skcipher_blocksize(tfm) - 1);
	thislen -= fraglen;

	if (thislen == 0)
		return 0;

	sg_mark_end(&desc->infrags[desc->fragno - 1]);
	sg_mark_end(&desc->outfrags[desc->fragno - 1]);

	skcipher_request_set_crypt(desc->req, desc->infrags, desc->outfrags,
				   thislen, desc->iv);

	ret = crypto_skcipher_encrypt(desc->req);
	if (ret)
		return ret;

	sg_init_table(desc->infrags, 4);
	sg_init_table(desc->outfrags, 4);

	if (fraglen) {
		sg_set_page(&desc->outfrags[0], sg_page(sg), fraglen,
				sg->offset + sg->length - fraglen);
		desc->infrags[0] = desc->outfrags[0];
		sg_assign_page(&desc->infrags[0], in_page);
		desc->fragno = 1;
		desc->fraglen = fraglen;
	} else {
		desc->fragno = 0;
		desc->fraglen = 0;
	}
	return 0;
}

int
gss_encrypt_xdr_buf(struct crypto_sync_skcipher *tfm, struct xdr_buf *buf,
		    int offset, struct page **pages)
{
	int ret;
	struct encryptor_desc desc;
	SYNC_SKCIPHER_REQUEST_ON_STACK(req, tfm);

	BUG_ON((buf->len - offset) % crypto_sync_skcipher_blocksize(tfm) != 0);

	skcipher_request_set_sync_tfm(req, tfm);
	skcipher_request_set_callback(req, 0, NULL, NULL);

	memset(desc.iv, 0, sizeof(desc.iv));
	desc.req = req;
	desc.pos = offset;
	desc.outbuf = buf;
	desc.pages = pages;
	desc.fragno = 0;
	desc.fraglen = 0;

	sg_init_table(desc.infrags, 4);
	sg_init_table(desc.outfrags, 4);

	ret = xdr_process_buf(buf, offset, buf->len - offset, encryptor, &desc);
	skcipher_request_zero(req);
	return ret;
}

struct decryptor_desc {
	u8 iv[GSS_KRB5_MAX_BLOCKSIZE];
	struct skcipher_request *req;
	struct scatterlist frags[4];
	int fragno;
	int fraglen;
};

static int
decryptor(struct scatterlist *sg, void *data)
{
	struct decryptor_desc *desc = data;
	int thislen = desc->fraglen + sg->length;
	struct crypto_sync_skcipher *tfm =
		crypto_sync_skcipher_reqtfm(desc->req);
	int fraglen, ret;

	 
	BUG_ON(desc->fragno > 3);
	sg_set_page(&desc->frags[desc->fragno], sg_page(sg), sg->length,
		    sg->offset);
	desc->fragno++;
	desc->fraglen += sg->length;

	fraglen = thislen & (crypto_sync_skcipher_blocksize(tfm) - 1);
	thislen -= fraglen;

	if (thislen == 0)
		return 0;

	sg_mark_end(&desc->frags[desc->fragno - 1]);

	skcipher_request_set_crypt(desc->req, desc->frags, desc->frags,
				   thislen, desc->iv);

	ret = crypto_skcipher_decrypt(desc->req);
	if (ret)
		return ret;

	sg_init_table(desc->frags, 4);

	if (fraglen) {
		sg_set_page(&desc->frags[0], sg_page(sg), fraglen,
				sg->offset + sg->length - fraglen);
		desc->fragno = 1;
		desc->fraglen = fraglen;
	} else {
		desc->fragno = 0;
		desc->fraglen = 0;
	}
	return 0;
}

int
gss_decrypt_xdr_buf(struct crypto_sync_skcipher *tfm, struct xdr_buf *buf,
		    int offset)
{
	int ret;
	struct decryptor_desc desc;
	SYNC_SKCIPHER_REQUEST_ON_STACK(req, tfm);

	 
	BUG_ON((buf->len - offset) % crypto_sync_skcipher_blocksize(tfm) != 0);

	skcipher_request_set_sync_tfm(req, tfm);
	skcipher_request_set_callback(req, 0, NULL, NULL);

	memset(desc.iv, 0, sizeof(desc.iv));
	desc.req = req;
	desc.fragno = 0;
	desc.fraglen = 0;

	sg_init_table(desc.frags, 4);

	ret = xdr_process_buf(buf, offset, buf->len - offset, decryptor, &desc);
	skcipher_request_zero(req);
	return ret;
}

 

int
xdr_extend_head(struct xdr_buf *buf, unsigned int base, unsigned int shiftlen)
{
	u8 *p;

	if (shiftlen == 0)
		return 0;

	BUG_ON(shiftlen > RPC_MAX_AUTH_SIZE);

	p = buf->head[0].iov_base + base;

	memmove(p + shiftlen, p, buf->head[0].iov_len - base);

	buf->head[0].iov_len += shiftlen;
	buf->len += shiftlen;

	return 0;
}

static u32
gss_krb5_cts_crypt(struct crypto_sync_skcipher *cipher, struct xdr_buf *buf,
		   u32 offset, u8 *iv, struct page **pages, int encrypt)
{
	u32 ret;
	struct scatterlist sg[1];
	SYNC_SKCIPHER_REQUEST_ON_STACK(req, cipher);
	u8 *data;
	struct page **save_pages;
	u32 len = buf->len - offset;

	if (len > GSS_KRB5_MAX_BLOCKSIZE * 2) {
		WARN_ON(0);
		return -ENOMEM;
	}
	data = kmalloc(GSS_KRB5_MAX_BLOCKSIZE * 2, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	 
	save_pages = buf->pages;
	if (encrypt)
		buf->pages = pages;

	ret = read_bytes_from_xdr_buf(buf, offset, data, len);
	buf->pages = save_pages;
	if (ret)
		goto out;

	sg_init_one(sg, data, len);

	skcipher_request_set_sync_tfm(req, cipher);
	skcipher_request_set_callback(req, 0, NULL, NULL);
	skcipher_request_set_crypt(req, sg, sg, len, iv);

	if (encrypt)
		ret = crypto_skcipher_encrypt(req);
	else
		ret = crypto_skcipher_decrypt(req);

	skcipher_request_zero(req);

	if (ret)
		goto out;

	ret = write_bytes_to_xdr_buf(buf, offset, data, len);

#if IS_ENABLED(CONFIG_KUNIT)
	 
	if (encrypt)
		memcpy(iv, data, crypto_sync_skcipher_ivsize(cipher));
#endif

out:
	kfree(data);
	return ret;
}

 
VISIBLE_IF_KUNIT
int krb5_cbc_cts_encrypt(struct crypto_sync_skcipher *cts_tfm,
			 struct crypto_sync_skcipher *cbc_tfm,
			 u32 offset, struct xdr_buf *buf, struct page **pages,
			 u8 *iv, unsigned int ivsize)
{
	u32 blocksize, nbytes, nblocks, cbcbytes;
	struct encryptor_desc desc;
	int err;

	blocksize = crypto_sync_skcipher_blocksize(cts_tfm);
	nbytes = buf->len - offset;
	nblocks = (nbytes + blocksize - 1) / blocksize;
	cbcbytes = 0;
	if (nblocks > 2)
		cbcbytes = (nblocks - 2) * blocksize;

	memset(desc.iv, 0, sizeof(desc.iv));

	 
	if (cbcbytes) {
		SYNC_SKCIPHER_REQUEST_ON_STACK(req, cbc_tfm);

		desc.pos = offset;
		desc.fragno = 0;
		desc.fraglen = 0;
		desc.pages = pages;
		desc.outbuf = buf;
		desc.req = req;

		skcipher_request_set_sync_tfm(req, cbc_tfm);
		skcipher_request_set_callback(req, 0, NULL, NULL);

		sg_init_table(desc.infrags, 4);
		sg_init_table(desc.outfrags, 4);

		err = xdr_process_buf(buf, offset, cbcbytes, encryptor, &desc);
		skcipher_request_zero(req);
		if (err)
			return err;
	}

	 
	err = gss_krb5_cts_crypt(cts_tfm, buf, offset + cbcbytes,
				 desc.iv, pages, 1);
	if (err)
		return err;

	if (unlikely(iv))
		memcpy(iv, desc.iv, ivsize);
	return 0;
}
EXPORT_SYMBOL_IF_KUNIT(krb5_cbc_cts_encrypt);

 
VISIBLE_IF_KUNIT
int krb5_cbc_cts_decrypt(struct crypto_sync_skcipher *cts_tfm,
			 struct crypto_sync_skcipher *cbc_tfm,
			 u32 offset, struct xdr_buf *buf)
{
	u32 blocksize, nblocks, cbcbytes;
	struct decryptor_desc desc;
	int err;

	blocksize = crypto_sync_skcipher_blocksize(cts_tfm);
	nblocks = (buf->len + blocksize - 1) / blocksize;
	cbcbytes = 0;
	if (nblocks > 2)
		cbcbytes = (nblocks - 2) * blocksize;

	memset(desc.iv, 0, sizeof(desc.iv));

	 
	if (cbcbytes) {
		SYNC_SKCIPHER_REQUEST_ON_STACK(req, cbc_tfm);

		desc.fragno = 0;
		desc.fraglen = 0;
		desc.req = req;

		skcipher_request_set_sync_tfm(req, cbc_tfm);
		skcipher_request_set_callback(req, 0, NULL, NULL);

		sg_init_table(desc.frags, 4);

		err = xdr_process_buf(buf, 0, cbcbytes, decryptor, &desc);
		skcipher_request_zero(req);
		if (err)
			return err;
	}

	 
	return gss_krb5_cts_crypt(cts_tfm, buf, cbcbytes, desc.iv, NULL, 0);
}
EXPORT_SYMBOL_IF_KUNIT(krb5_cbc_cts_decrypt);

u32
gss_krb5_aes_encrypt(struct krb5_ctx *kctx, u32 offset,
		     struct xdr_buf *buf, struct page **pages)
{
	u32 err;
	struct xdr_netobj hmac;
	u8 *ecptr;
	struct crypto_sync_skcipher *cipher, *aux_cipher;
	struct crypto_ahash *ahash;
	struct page **save_pages;
	unsigned int conflen;

	if (kctx->initiate) {
		cipher = kctx->initiator_enc;
		aux_cipher = kctx->initiator_enc_aux;
		ahash = kctx->initiator_integ;
	} else {
		cipher = kctx->acceptor_enc;
		aux_cipher = kctx->acceptor_enc_aux;
		ahash = kctx->acceptor_integ;
	}
	conflen = crypto_sync_skcipher_blocksize(cipher);

	 
	offset += GSS_KRB5_TOK_HDR_LEN;
	if (xdr_extend_head(buf, offset, conflen))
		return GSS_S_FAILURE;
	krb5_make_confounder(buf->head[0].iov_base + offset, conflen);
	offset -= GSS_KRB5_TOK_HDR_LEN;

	if (buf->tail[0].iov_base != NULL) {
		ecptr = buf->tail[0].iov_base + buf->tail[0].iov_len;
	} else {
		buf->tail[0].iov_base = buf->head[0].iov_base
							+ buf->head[0].iov_len;
		buf->tail[0].iov_len = 0;
		ecptr = buf->tail[0].iov_base;
	}

	 
	memcpy(ecptr, buf->head[0].iov_base + offset, GSS_KRB5_TOK_HDR_LEN);
	buf->tail[0].iov_len += GSS_KRB5_TOK_HDR_LEN;
	buf->len += GSS_KRB5_TOK_HDR_LEN;

	hmac.len = kctx->gk5e->cksumlength;
	hmac.data = buf->tail[0].iov_base + buf->tail[0].iov_len;

	 
	save_pages = buf->pages;
	buf->pages = pages;

	err = gss_krb5_checksum(ahash, NULL, 0, buf,
				offset + GSS_KRB5_TOK_HDR_LEN, &hmac);
	buf->pages = save_pages;
	if (err)
		return GSS_S_FAILURE;

	err = krb5_cbc_cts_encrypt(cipher, aux_cipher,
				   offset + GSS_KRB5_TOK_HDR_LEN,
				   buf, pages, NULL, 0);
	if (err)
		return GSS_S_FAILURE;

	 
	buf->tail[0].iov_len += kctx->gk5e->cksumlength;
	buf->len += kctx->gk5e->cksumlength;

	return GSS_S_COMPLETE;
}

u32
gss_krb5_aes_decrypt(struct krb5_ctx *kctx, u32 offset, u32 len,
		     struct xdr_buf *buf, u32 *headskip, u32 *tailskip)
{
	struct crypto_sync_skcipher *cipher, *aux_cipher;
	struct crypto_ahash *ahash;
	struct xdr_netobj our_hmac_obj;
	u8 our_hmac[GSS_KRB5_MAX_CKSUM_LEN];
	u8 pkt_hmac[GSS_KRB5_MAX_CKSUM_LEN];
	struct xdr_buf subbuf;
	u32 ret = 0;

	if (kctx->initiate) {
		cipher = kctx->acceptor_enc;
		aux_cipher = kctx->acceptor_enc_aux;
		ahash = kctx->acceptor_integ;
	} else {
		cipher = kctx->initiator_enc;
		aux_cipher = kctx->initiator_enc_aux;
		ahash = kctx->initiator_integ;
	}

	 
	xdr_buf_subsegment(buf, &subbuf, offset + GSS_KRB5_TOK_HDR_LEN,
				    (len - offset - GSS_KRB5_TOK_HDR_LEN -
				     kctx->gk5e->cksumlength));

	ret = krb5_cbc_cts_decrypt(cipher, aux_cipher, 0, &subbuf);
	if (ret)
		goto out_err;

	our_hmac_obj.len = kctx->gk5e->cksumlength;
	our_hmac_obj.data = our_hmac;
	ret = gss_krb5_checksum(ahash, NULL, 0, &subbuf, 0, &our_hmac_obj);
	if (ret)
		goto out_err;

	 
	ret = read_bytes_from_xdr_buf(buf, len - kctx->gk5e->cksumlength,
				      pkt_hmac, kctx->gk5e->cksumlength);
	if (ret)
		goto out_err;

	if (crypto_memneq(pkt_hmac, our_hmac, kctx->gk5e->cksumlength) != 0) {
		ret = GSS_S_BAD_SIG;
		goto out_err;
	}
	*headskip = crypto_sync_skcipher_blocksize(cipher);
	*tailskip = kctx->gk5e->cksumlength;
out_err:
	if (ret && ret != GSS_S_BAD_SIG)
		ret = GSS_S_FAILURE;
	return ret;
}

 
VISIBLE_IF_KUNIT
u32 krb5_etm_checksum(struct crypto_sync_skcipher *cipher,
		      struct crypto_ahash *tfm, const struct xdr_buf *body,
		      int body_offset, struct xdr_netobj *cksumout)
{
	unsigned int ivsize = crypto_sync_skcipher_ivsize(cipher);
	struct ahash_request *req;
	struct scatterlist sg[1];
	u8 *iv, *checksumdata;
	int err = -ENOMEM;

	checksumdata = kmalloc(crypto_ahash_digestsize(tfm), GFP_KERNEL);
	if (!checksumdata)
		return GSS_S_FAILURE;
	 
	iv = kzalloc(ivsize, GFP_KERNEL);
	if (!iv)
		goto out_free_mem;

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req)
		goto out_free_mem;
	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_SLEEP, NULL, NULL);
	err = crypto_ahash_init(req);
	if (err)
		goto out_free_ahash;

	sg_init_one(sg, iv, ivsize);
	ahash_request_set_crypt(req, sg, NULL, ivsize);
	err = crypto_ahash_update(req);
	if (err)
		goto out_free_ahash;
	err = xdr_process_buf(body, body_offset, body->len - body_offset,
			      checksummer, req);
	if (err)
		goto out_free_ahash;

	ahash_request_set_crypt(req, NULL, checksumdata, 0);
	err = crypto_ahash_final(req);
	if (err)
		goto out_free_ahash;
	memcpy(cksumout->data, checksumdata, cksumout->len);

out_free_ahash:
	ahash_request_free(req);
out_free_mem:
	kfree(iv);
	kfree_sensitive(checksumdata);
	return err ? GSS_S_FAILURE : GSS_S_COMPLETE;
}
EXPORT_SYMBOL_IF_KUNIT(krb5_etm_checksum);

 
u32
krb5_etm_encrypt(struct krb5_ctx *kctx, u32 offset,
		 struct xdr_buf *buf, struct page **pages)
{
	struct crypto_sync_skcipher *cipher, *aux_cipher;
	struct crypto_ahash *ahash;
	struct xdr_netobj hmac;
	unsigned int conflen;
	u8 *ecptr;
	u32 err;

	if (kctx->initiate) {
		cipher = kctx->initiator_enc;
		aux_cipher = kctx->initiator_enc_aux;
		ahash = kctx->initiator_integ;
	} else {
		cipher = kctx->acceptor_enc;
		aux_cipher = kctx->acceptor_enc_aux;
		ahash = kctx->acceptor_integ;
	}
	conflen = crypto_sync_skcipher_blocksize(cipher);

	offset += GSS_KRB5_TOK_HDR_LEN;
	if (xdr_extend_head(buf, offset, conflen))
		return GSS_S_FAILURE;
	krb5_make_confounder(buf->head[0].iov_base + offset, conflen);
	offset -= GSS_KRB5_TOK_HDR_LEN;

	if (buf->tail[0].iov_base) {
		ecptr = buf->tail[0].iov_base + buf->tail[0].iov_len;
	} else {
		buf->tail[0].iov_base = buf->head[0].iov_base
							+ buf->head[0].iov_len;
		buf->tail[0].iov_len = 0;
		ecptr = buf->tail[0].iov_base;
	}

	memcpy(ecptr, buf->head[0].iov_base + offset, GSS_KRB5_TOK_HDR_LEN);
	buf->tail[0].iov_len += GSS_KRB5_TOK_HDR_LEN;
	buf->len += GSS_KRB5_TOK_HDR_LEN;

	err = krb5_cbc_cts_encrypt(cipher, aux_cipher,
				   offset + GSS_KRB5_TOK_HDR_LEN,
				   buf, pages, NULL, 0);
	if (err)
		return GSS_S_FAILURE;

	hmac.data = buf->tail[0].iov_base + buf->tail[0].iov_len;
	hmac.len = kctx->gk5e->cksumlength;
	err = krb5_etm_checksum(cipher, ahash,
				buf, offset + GSS_KRB5_TOK_HDR_LEN, &hmac);
	if (err)
		goto out_err;
	buf->tail[0].iov_len += kctx->gk5e->cksumlength;
	buf->len += kctx->gk5e->cksumlength;

	return GSS_S_COMPLETE;

out_err:
	return GSS_S_FAILURE;
}

 
u32
krb5_etm_decrypt(struct krb5_ctx *kctx, u32 offset, u32 len,
		 struct xdr_buf *buf, u32 *headskip, u32 *tailskip)
{
	struct crypto_sync_skcipher *cipher, *aux_cipher;
	u8 our_hmac[GSS_KRB5_MAX_CKSUM_LEN];
	u8 pkt_hmac[GSS_KRB5_MAX_CKSUM_LEN];
	struct xdr_netobj our_hmac_obj;
	struct crypto_ahash *ahash;
	struct xdr_buf subbuf;
	u32 ret = 0;

	if (kctx->initiate) {
		cipher = kctx->acceptor_enc;
		aux_cipher = kctx->acceptor_enc_aux;
		ahash = kctx->acceptor_integ;
	} else {
		cipher = kctx->initiator_enc;
		aux_cipher = kctx->initiator_enc_aux;
		ahash = kctx->initiator_integ;
	}

	 
	xdr_buf_subsegment(buf, &subbuf, offset + GSS_KRB5_TOK_HDR_LEN,
			   (len - offset - GSS_KRB5_TOK_HDR_LEN -
			    kctx->gk5e->cksumlength));

	our_hmac_obj.data = our_hmac;
	our_hmac_obj.len = kctx->gk5e->cksumlength;
	ret = krb5_etm_checksum(cipher, ahash, &subbuf, 0, &our_hmac_obj);
	if (ret)
		goto out_err;
	ret = read_bytes_from_xdr_buf(buf, len - kctx->gk5e->cksumlength,
				      pkt_hmac, kctx->gk5e->cksumlength);
	if (ret)
		goto out_err;
	if (crypto_memneq(pkt_hmac, our_hmac, kctx->gk5e->cksumlength) != 0) {
		ret = GSS_S_BAD_SIG;
		goto out_err;
	}

	ret = krb5_cbc_cts_decrypt(cipher, aux_cipher, 0, &subbuf);
	if (ret) {
		ret = GSS_S_FAILURE;
		goto out_err;
	}

	*headskip = crypto_sync_skcipher_blocksize(cipher);
	*tailskip = kctx->gk5e->cksumlength;
	return GSS_S_COMPLETE;

out_err:
	if (ret != GSS_S_BAD_SIG)
		ret = GSS_S_FAILURE;
	return ret;
}
