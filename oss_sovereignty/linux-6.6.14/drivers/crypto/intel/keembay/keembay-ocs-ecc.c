
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <crypto/ecc_curve.h>
#include <crypto/ecdh.h>
#include <crypto/engine.h>
#include <crypto/internal/ecc.h>
#include <crypto/internal/kpp.h>
#include <crypto/kpp.h>
#include <crypto/rng.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/fips.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/string.h>

#define DRV_NAME			"keembay-ocs-ecc"

#define KMB_OCS_ECC_PRIORITY		350

#define HW_OFFS_OCS_ECC_COMMAND		0x00000000
#define HW_OFFS_OCS_ECC_STATUS		0x00000004
#define HW_OFFS_OCS_ECC_DATA_IN		0x00000080
#define HW_OFFS_OCS_ECC_CX_DATA_OUT	0x00000100
#define HW_OFFS_OCS_ECC_CY_DATA_OUT	0x00000180
#define HW_OFFS_OCS_ECC_ISR		0x00000400
#define HW_OFFS_OCS_ECC_IER		0x00000404

#define HW_OCS_ECC_ISR_INT_STATUS_DONE	BIT(0)
#define HW_OCS_ECC_COMMAND_INS_BP	BIT(0)

#define HW_OCS_ECC_COMMAND_START_VAL	BIT(0)

#define OCS_ECC_OP_SIZE_384		BIT(8)
#define OCS_ECC_OP_SIZE_256		0

 
#define OCS_ECC_INST_WRITE_AX		(0x1 << HW_OCS_ECC_COMMAND_INS_BP)
#define OCS_ECC_INST_WRITE_AY		(0x2 << HW_OCS_ECC_COMMAND_INS_BP)
#define OCS_ECC_INST_WRITE_BX_D		(0x3 << HW_OCS_ECC_COMMAND_INS_BP)
#define OCS_ECC_INST_WRITE_BY_L		(0x4 << HW_OCS_ECC_COMMAND_INS_BP)
#define OCS_ECC_INST_WRITE_P		(0x5 << HW_OCS_ECC_COMMAND_INS_BP)
#define OCS_ECC_INST_WRITE_A		(0x6 << HW_OCS_ECC_COMMAND_INS_BP)
#define OCS_ECC_INST_CALC_D_IDX_A	(0x8 << HW_OCS_ECC_COMMAND_INS_BP)
#define OCS_ECC_INST_CALC_A_POW_B_MODP	(0xB << HW_OCS_ECC_COMMAND_INS_BP)
#define OCS_ECC_INST_CALC_A_MUL_B_MODP	(0xC  << HW_OCS_ECC_COMMAND_INS_BP)
#define OCS_ECC_INST_CALC_A_ADD_B_MODP	(0xD << HW_OCS_ECC_COMMAND_INS_BP)

#define ECC_ENABLE_INTR			1

#define POLL_USEC			100
#define TIMEOUT_USEC			10000

#define KMB_ECC_VLI_MAX_DIGITS		ECC_CURVE_NIST_P384_DIGITS
#define KMB_ECC_VLI_MAX_BYTES		(KMB_ECC_VLI_MAX_DIGITS \
					 << ECC_DIGITS_TO_BYTES_SHIFT)

#define POW_CUBE			3

 
struct ocs_ecc_dev {
	struct list_head list;
	struct device *dev;
	void __iomem *base_reg;
	struct crypto_engine *engine;
	struct completion irq_done;
	int irq;
};

 
struct ocs_ecc_ctx {
	struct ocs_ecc_dev *ecc_dev;
	const struct ecc_curve *curve;
	u64 private_key[KMB_ECC_VLI_MAX_DIGITS];
};

 
struct ocs_ecc_drv {
	struct list_head dev_list;
	spinlock_t lock;	 
};

 
static struct ocs_ecc_drv ocs_ecc = {
	.dev_list = LIST_HEAD_INIT(ocs_ecc.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(ocs_ecc.lock),
};

 
static inline struct ocs_ecc_ctx *kmb_ocs_ecc_tctx(struct kpp_request *req)
{
	return kpp_tfm_ctx(crypto_kpp_reqtfm(req));
}

 
static inline unsigned int digits_to_bytes(unsigned int n)
{
	return n << ECC_DIGITS_TO_BYTES_SHIFT;
}

 
static inline int ocs_ecc_wait_idle(struct ocs_ecc_dev *dev)
{
	u32 value;

	return readl_poll_timeout((dev->base_reg + HW_OFFS_OCS_ECC_STATUS),
				  value,
				  !(value & HW_OCS_ECC_ISR_INT_STATUS_DONE),
				  POLL_USEC, TIMEOUT_USEC);
}

static void ocs_ecc_cmd_start(struct ocs_ecc_dev *ecc_dev, u32 op_size)
{
	iowrite32(op_size | HW_OCS_ECC_COMMAND_START_VAL,
		  ecc_dev->base_reg + HW_OFFS_OCS_ECC_COMMAND);
}

 
static void ocs_ecc_write_cmd_and_data(struct ocs_ecc_dev *dev,
				       u32 op_size,
				       u32 inst,
				       const void *data_in,
				       size_t data_size)
{
	iowrite32(op_size | inst, dev->base_reg + HW_OFFS_OCS_ECC_COMMAND);

	 
	memcpy_toio(dev->base_reg + HW_OFFS_OCS_ECC_DATA_IN, data_in,
		    data_size);
}

 
static int ocs_ecc_trigger_op(struct ocs_ecc_dev *ecc_dev, u32 op_size,
			      u32 inst)
{
	reinit_completion(&ecc_dev->irq_done);

	iowrite32(ECC_ENABLE_INTR, ecc_dev->base_reg + HW_OFFS_OCS_ECC_IER);
	iowrite32(op_size | inst, ecc_dev->base_reg + HW_OFFS_OCS_ECC_COMMAND);

	return wait_for_completion_interruptible(&ecc_dev->irq_done);
}

 
static inline void ocs_ecc_read_cx_out(struct ocs_ecc_dev *dev, void *cx_out,
				       size_t byte_count)
{
	memcpy_fromio(cx_out, dev->base_reg + HW_OFFS_OCS_ECC_CX_DATA_OUT,
		      byte_count);
}

 
static inline void ocs_ecc_read_cy_out(struct ocs_ecc_dev *dev, void *cy_out,
				       size_t byte_count)
{
	memcpy_fromio(cy_out, dev->base_reg + HW_OFFS_OCS_ECC_CY_DATA_OUT,
		      byte_count);
}

static struct ocs_ecc_dev *kmb_ocs_ecc_find_dev(struct ocs_ecc_ctx *tctx)
{
	if (tctx->ecc_dev)
		return tctx->ecc_dev;

	spin_lock(&ocs_ecc.lock);

	 
	tctx->ecc_dev = list_first_entry(&ocs_ecc.dev_list, struct ocs_ecc_dev,
					 list);

	spin_unlock(&ocs_ecc.lock);

	return tctx->ecc_dev;
}

 
static int kmb_ecc_point_mult(struct ocs_ecc_dev *ecc_dev,
			      struct ecc_point *result,
			      const struct ecc_point *point,
			      u64 *scalar,
			      const struct ecc_curve *curve)
{
	u8 sca[KMB_ECC_VLI_MAX_BYTES];  
	u32 op_size = (curve->g.ndigits > ECC_CURVE_NIST_P256_DIGITS) ?
		      OCS_ECC_OP_SIZE_384 : OCS_ECC_OP_SIZE_256;
	size_t nbytes = digits_to_bytes(curve->g.ndigits);
	int rc = 0;

	 
	rc = crypto_get_default_rng();
	if (rc)
		return rc;

	rc = crypto_rng_get_bytes(crypto_default_rng, sca, nbytes);
	crypto_put_default_rng();
	if (rc)
		return rc;

	 
	rc = ocs_ecc_wait_idle(ecc_dev);
	if (rc)
		return rc;

	 
	ocs_ecc_cmd_start(ecc_dev, op_size);

	 
	ocs_ecc_write_cmd_and_data(ecc_dev, op_size, OCS_ECC_INST_WRITE_AX,
				   point->x, nbytes);

	 
	ocs_ecc_write_cmd_and_data(ecc_dev, op_size, OCS_ECC_INST_WRITE_AY,
				   point->y, nbytes);

	 
	ocs_ecc_write_cmd_and_data(ecc_dev, op_size, OCS_ECC_INST_WRITE_BX_D,
				   scalar, nbytes);

	 
	ocs_ecc_write_cmd_and_data(ecc_dev, op_size, OCS_ECC_INST_WRITE_BY_L,
				   sca, nbytes);
	memzero_explicit(sca, sizeof(sca));

	 
	ocs_ecc_write_cmd_and_data(ecc_dev, op_size, OCS_ECC_INST_WRITE_P,
				   curve->p, nbytes);

	 
	ocs_ecc_write_cmd_and_data(ecc_dev, op_size, OCS_ECC_INST_WRITE_A,
				   curve->a, nbytes);

	 
	rc = ocs_ecc_trigger_op(ecc_dev, op_size, OCS_ECC_INST_CALC_D_IDX_A);
	if (rc)
		return rc;

	 
	ocs_ecc_read_cx_out(ecc_dev, result->x, nbytes);
	ocs_ecc_read_cy_out(ecc_dev, result->y, nbytes);

	return 0;
}

 
static int kmb_ecc_do_scalar_op(struct ocs_ecc_dev *ecc_dev, u64 *scalar_out,
				const u64 *scalar_a, const u64 *scalar_b,
				const struct ecc_curve *curve,
				unsigned int ndigits, const u32 inst)
{
	u32 op_size = (ndigits > ECC_CURVE_NIST_P256_DIGITS) ?
		      OCS_ECC_OP_SIZE_384 : OCS_ECC_OP_SIZE_256;
	size_t nbytes = digits_to_bytes(ndigits);
	int rc;

	 
	rc = ocs_ecc_wait_idle(ecc_dev);
	if (rc)
		return rc;

	 
	ocs_ecc_cmd_start(ecc_dev, op_size);

	 
	ocs_ecc_write_cmd_and_data(ecc_dev, op_size, OCS_ECC_INST_WRITE_AX,
				   scalar_a, nbytes);

	 
	ocs_ecc_write_cmd_and_data(ecc_dev, op_size, OCS_ECC_INST_WRITE_AY,
				   scalar_b, nbytes);

	 
	ocs_ecc_write_cmd_and_data(ecc_dev, op_size, OCS_ECC_INST_WRITE_P,
				   curve->p, nbytes);

	 
	rc = ocs_ecc_trigger_op(ecc_dev, op_size, inst);
	if (rc)
		return rc;

	ocs_ecc_read_cx_out(ecc_dev, scalar_out, nbytes);

	if (vli_is_zero(scalar_out, ndigits))
		return -EINVAL;

	return 0;
}

 
static int kmb_ocs_ecc_is_pubkey_valid_partial(struct ocs_ecc_dev *ecc_dev,
					       const struct ecc_curve *curve,
					       struct ecc_point *pk)
{
	u64 xxx[KMB_ECC_VLI_MAX_DIGITS] = { 0 };
	u64 yy[KMB_ECC_VLI_MAX_DIGITS] = { 0 };
	u64 w[KMB_ECC_VLI_MAX_DIGITS] = { 0 };
	int rc;

	if (WARN_ON(pk->ndigits != curve->g.ndigits))
		return -EINVAL;

	 
	if (ecc_point_is_zero(pk))
		return -EINVAL;

	 
	if (vli_cmp(curve->p, pk->x, pk->ndigits) != 1)
		return -EINVAL;

	if (vli_cmp(curve->p, pk->y, pk->ndigits) != 1)
		return -EINVAL;

	 

	  
	 
	rc = kmb_ecc_do_scalar_op(ecc_dev, yy, pk->y, pk->y, curve, pk->ndigits,
				  OCS_ECC_INST_CALC_A_MUL_B_MODP);
	if (rc)
		goto exit;

	 
	 
	w[0] = POW_CUBE;
	 
	rc = kmb_ecc_do_scalar_op(ecc_dev, xxx, pk->x, w, curve, pk->ndigits,
				  OCS_ECC_INST_CALC_A_POW_B_MODP);
	if (rc)
		goto exit;

	 
	rc = kmb_ecc_do_scalar_op(ecc_dev, w, curve->a, pk->x, curve,
				  pk->ndigits,
				  OCS_ECC_INST_CALC_A_MUL_B_MODP);
	if (rc)
		goto exit;

	 
	rc = kmb_ecc_do_scalar_op(ecc_dev, w, w, curve->b, curve,
				  pk->ndigits,
				  OCS_ECC_INST_CALC_A_ADD_B_MODP);
	if (rc)
		goto exit;

	 
	rc = kmb_ecc_do_scalar_op(ecc_dev, w, xxx, w, curve, pk->ndigits,
				  OCS_ECC_INST_CALC_A_ADD_B_MODP);
	if (rc)
		goto exit;

	 
	rc = vli_cmp(yy, w, pk->ndigits);
	if (rc)
		rc = -EINVAL;

exit:
	memzero_explicit(xxx, sizeof(xxx));
	memzero_explicit(yy, sizeof(yy));
	memzero_explicit(w, sizeof(w));

	return rc;
}

 
static int kmb_ocs_ecc_is_pubkey_valid_full(struct ocs_ecc_dev *ecc_dev,
					    const struct ecc_curve *curve,
					    struct ecc_point *pk)
{
	struct ecc_point *nQ;
	int rc;

	 
	rc = kmb_ocs_ecc_is_pubkey_valid_partial(ecc_dev, curve, pk);
	if (rc)
		return rc;

	 
	nQ = ecc_alloc_point(pk->ndigits);
	if (!nQ)
		return -ENOMEM;

	rc = kmb_ecc_point_mult(ecc_dev, nQ, pk, curve->n, curve);
	if (rc)
		goto exit;

	if (!ecc_point_is_zero(nQ))
		rc = -EINVAL;

exit:
	ecc_free_point(nQ);

	return rc;
}

static int kmb_ecc_is_key_valid(const struct ecc_curve *curve,
				const u64 *private_key, size_t private_key_len)
{
	size_t ndigits = curve->g.ndigits;
	u64 one[KMB_ECC_VLI_MAX_DIGITS] = {1};
	u64 res[KMB_ECC_VLI_MAX_DIGITS];

	if (private_key_len != digits_to_bytes(ndigits))
		return -EINVAL;

	if (!private_key)
		return -EINVAL;

	 
	if (vli_cmp(one, private_key, ndigits) != -1)
		return -EINVAL;

	vli_sub(res, curve->n, one, ndigits);
	vli_sub(res, res, one, ndigits);
	if (vli_cmp(res, private_key, ndigits) != 1)
		return -EINVAL;

	return 0;
}

 
static int kmb_ecc_gen_privkey(const struct ecc_curve *curve, u64 *privkey)
{
	size_t nbytes = digits_to_bytes(curve->g.ndigits);
	u64 priv[KMB_ECC_VLI_MAX_DIGITS];
	size_t nbits;
	int rc;

	nbits = vli_num_bits(curve->n, curve->g.ndigits);

	 
	if (nbits < 160 || curve->g.ndigits > ARRAY_SIZE(priv))
		return -EINVAL;

	 
	if (crypto_get_default_rng())
		return -EFAULT;

	rc = crypto_rng_get_bytes(crypto_default_rng, (u8 *)priv, nbytes);
	crypto_put_default_rng();
	if (rc)
		goto cleanup;

	rc = kmb_ecc_is_key_valid(curve, priv, nbytes);
	if (rc)
		goto cleanup;

	ecc_swap_digits(priv, privkey, curve->g.ndigits);

cleanup:
	memzero_explicit(&priv, sizeof(priv));

	return rc;
}

static int kmb_ocs_ecdh_set_secret(struct crypto_kpp *tfm, const void *buf,
				   unsigned int len)
{
	struct ocs_ecc_ctx *tctx = kpp_tfm_ctx(tfm);
	struct ecdh params;
	int rc = 0;

	rc = crypto_ecdh_decode_key(buf, len, &params);
	if (rc)
		goto cleanup;

	 
	if (params.key_size > digits_to_bytes(tctx->curve->g.ndigits)) {
		rc = -EINVAL;
		goto cleanup;
	}

	 
	if (!params.key || !params.key_size) {
		rc = kmb_ecc_gen_privkey(tctx->curve, tctx->private_key);
		goto cleanup;
	}

	rc = kmb_ecc_is_key_valid(tctx->curve, (const u64 *)params.key,
				  params.key_size);
	if (rc)
		goto cleanup;

	ecc_swap_digits((const u64 *)params.key, tctx->private_key,
			tctx->curve->g.ndigits);
cleanup:
	memzero_explicit(&params, sizeof(params));

	if (rc)
		tctx->curve = NULL;

	return rc;
}

 
static int kmb_ecc_do_shared_secret(struct ocs_ecc_ctx *tctx,
				    struct kpp_request *req)
{
	struct ocs_ecc_dev *ecc_dev = tctx->ecc_dev;
	const struct ecc_curve *curve = tctx->curve;
	u64 shared_secret[KMB_ECC_VLI_MAX_DIGITS];
	u64 pubk_buf[KMB_ECC_VLI_MAX_DIGITS * 2];
	size_t copied, nbytes, pubk_len;
	struct ecc_point *pk, *result;
	int rc;

	nbytes = digits_to_bytes(curve->g.ndigits);

	 
	pubk_len = 2 * nbytes;

	 
	copied = sg_copy_to_buffer(req->src,
				   sg_nents_for_len(req->src, pubk_len),
				   pubk_buf, pubk_len);
	if (copied != pubk_len)
		return -EINVAL;

	 
	pk = ecc_alloc_point(curve->g.ndigits);
	if (!pk)
		return -ENOMEM;

	ecc_swap_digits(pubk_buf, pk->x, curve->g.ndigits);
	ecc_swap_digits(&pubk_buf[curve->g.ndigits], pk->y, curve->g.ndigits);

	 
	rc = kmb_ocs_ecc_is_pubkey_valid_partial(ecc_dev, curve, pk);
	if (rc)
		goto exit_free_pk;

	 
	result = ecc_alloc_point(pk->ndigits);
	if (!result) {
		rc = -ENOMEM;
		goto exit_free_pk;
	}

	 
	rc = kmb_ecc_point_mult(ecc_dev, result, pk, tctx->private_key, curve);
	if (rc)
		goto exit_free_result;

	if (ecc_point_is_zero(result)) {
		rc = -EFAULT;
		goto exit_free_result;
	}

	 
	ecc_swap_digits(result->x, shared_secret, result->ndigits);

	 
	nbytes = min_t(size_t, nbytes, req->dst_len);

	copied = sg_copy_from_buffer(req->dst,
				     sg_nents_for_len(req->dst, nbytes),
				     shared_secret, nbytes);

	if (copied != nbytes)
		rc = -EINVAL;

	memzero_explicit(shared_secret, sizeof(shared_secret));

exit_free_result:
	ecc_free_point(result);

exit_free_pk:
	ecc_free_point(pk);

	return rc;
}

 
static int kmb_ecc_do_public_key(struct ocs_ecc_ctx *tctx,
				 struct kpp_request *req)
{
	const struct ecc_curve *curve = tctx->curve;
	u64 pubk_buf[KMB_ECC_VLI_MAX_DIGITS * 2];
	struct ecc_point *pk;
	size_t pubk_len;
	size_t copied;
	int rc;

	 
	pubk_len = 2 * digits_to_bytes(curve->g.ndigits);

	pk = ecc_alloc_point(curve->g.ndigits);
	if (!pk)
		return -ENOMEM;

	 
	rc = kmb_ecc_point_mult(tctx->ecc_dev, pk, &curve->g, tctx->private_key,
				curve);
	if (rc)
		goto exit;

	 
	if (kmb_ocs_ecc_is_pubkey_valid_full(tctx->ecc_dev, curve, pk)) {
		rc = -EAGAIN;
		goto exit;
	}

	 
	ecc_swap_digits(pk->x, pubk_buf, pk->ndigits);
	ecc_swap_digits(pk->y, &pubk_buf[pk->ndigits], pk->ndigits);

	 
	copied = sg_copy_from_buffer(req->dst,
				     sg_nents_for_len(req->dst, pubk_len),
				     pubk_buf, pubk_len);

	if (copied != pubk_len)
		rc = -EINVAL;

exit:
	ecc_free_point(pk);

	return rc;
}

static int kmb_ocs_ecc_do_one_request(struct crypto_engine *engine,
				      void *areq)
{
	struct kpp_request *req = container_of(areq, struct kpp_request, base);
	struct ocs_ecc_ctx *tctx = kmb_ocs_ecc_tctx(req);
	struct ocs_ecc_dev *ecc_dev = tctx->ecc_dev;
	int rc;

	if (req->src)
		rc = kmb_ecc_do_shared_secret(tctx, req);
	else
		rc = kmb_ecc_do_public_key(tctx, req);

	crypto_finalize_kpp_request(ecc_dev->engine, req, rc);

	return 0;
}

static int kmb_ocs_ecdh_generate_public_key(struct kpp_request *req)
{
	struct ocs_ecc_ctx *tctx = kmb_ocs_ecc_tctx(req);
	const struct ecc_curve *curve = tctx->curve;

	 
	if (!tctx->curve)
		return -EINVAL;

	 
	if (!req->dst)
		return -EINVAL;

	 
	if (req->dst_len < (2 * digits_to_bytes(curve->g.ndigits)))
		return -EINVAL;

	 
	if (req->src)
		return -EINVAL;

	return crypto_transfer_kpp_request_to_engine(tctx->ecc_dev->engine,
						     req);
}

static int kmb_ocs_ecdh_compute_shared_secret(struct kpp_request *req)
{
	struct ocs_ecc_ctx *tctx = kmb_ocs_ecc_tctx(req);
	const struct ecc_curve *curve = tctx->curve;

	 
	if (!tctx->curve)
		return -EINVAL;

	 
	if (!req->dst)
		return -EINVAL;

	 
	if (!req->src)
		return -EINVAL;

	 
	if (req->src_len != 2 * digits_to_bytes(curve->g.ndigits))
		return -EINVAL;

	return crypto_transfer_kpp_request_to_engine(tctx->ecc_dev->engine,
						     req);
}

static int kmb_ecc_tctx_init(struct ocs_ecc_ctx *tctx, unsigned int curve_id)
{
	memset(tctx, 0, sizeof(*tctx));

	tctx->ecc_dev = kmb_ocs_ecc_find_dev(tctx);

	if (IS_ERR(tctx->ecc_dev)) {
		pr_err("Failed to find the device : %ld\n",
		       PTR_ERR(tctx->ecc_dev));
		return PTR_ERR(tctx->ecc_dev);
	}

	tctx->curve = ecc_get_curve(curve_id);
	if (!tctx->curve)
		return -EOPNOTSUPP;

	return 0;
}

static int kmb_ocs_ecdh_nist_p256_init_tfm(struct crypto_kpp *tfm)
{
	struct ocs_ecc_ctx *tctx = kpp_tfm_ctx(tfm);

	return kmb_ecc_tctx_init(tctx, ECC_CURVE_NIST_P256);
}

static int kmb_ocs_ecdh_nist_p384_init_tfm(struct crypto_kpp *tfm)
{
	struct ocs_ecc_ctx *tctx = kpp_tfm_ctx(tfm);

	return kmb_ecc_tctx_init(tctx, ECC_CURVE_NIST_P384);
}

static void kmb_ocs_ecdh_exit_tfm(struct crypto_kpp *tfm)
{
	struct ocs_ecc_ctx *tctx = kpp_tfm_ctx(tfm);

	memzero_explicit(tctx->private_key, sizeof(*tctx->private_key));
}

static unsigned int kmb_ocs_ecdh_max_size(struct crypto_kpp *tfm)
{
	struct ocs_ecc_ctx *tctx = kpp_tfm_ctx(tfm);

	 
	return digits_to_bytes(tctx->curve->g.ndigits) * 2;
}

static struct kpp_engine_alg ocs_ecdh_p256 = {
	.base.set_secret = kmb_ocs_ecdh_set_secret,
	.base.generate_public_key = kmb_ocs_ecdh_generate_public_key,
	.base.compute_shared_secret = kmb_ocs_ecdh_compute_shared_secret,
	.base.init = kmb_ocs_ecdh_nist_p256_init_tfm,
	.base.exit = kmb_ocs_ecdh_exit_tfm,
	.base.max_size = kmb_ocs_ecdh_max_size,
	.base.base = {
		.cra_name = "ecdh-nist-p256",
		.cra_driver_name = "ecdh-nist-p256-keembay-ocs",
		.cra_priority = KMB_OCS_ECC_PRIORITY,
		.cra_module = THIS_MODULE,
		.cra_ctxsize = sizeof(struct ocs_ecc_ctx),
	},
	.op.do_one_request = kmb_ocs_ecc_do_one_request,
};

static struct kpp_engine_alg ocs_ecdh_p384 = {
	.base.set_secret = kmb_ocs_ecdh_set_secret,
	.base.generate_public_key = kmb_ocs_ecdh_generate_public_key,
	.base.compute_shared_secret = kmb_ocs_ecdh_compute_shared_secret,
	.base.init = kmb_ocs_ecdh_nist_p384_init_tfm,
	.base.exit = kmb_ocs_ecdh_exit_tfm,
	.base.max_size = kmb_ocs_ecdh_max_size,
	.base.base = {
		.cra_name = "ecdh-nist-p384",
		.cra_driver_name = "ecdh-nist-p384-keembay-ocs",
		.cra_priority = KMB_OCS_ECC_PRIORITY,
		.cra_module = THIS_MODULE,
		.cra_ctxsize = sizeof(struct ocs_ecc_ctx),
	},
	.op.do_one_request = kmb_ocs_ecc_do_one_request,
};

static irqreturn_t ocs_ecc_irq_handler(int irq, void *dev_id)
{
	struct ocs_ecc_dev *ecc_dev = dev_id;
	u32 status;

	 
	status = ioread32(ecc_dev->base_reg + HW_OFFS_OCS_ECC_ISR);
	iowrite32(status, ecc_dev->base_reg + HW_OFFS_OCS_ECC_ISR);

	if (!(status & HW_OCS_ECC_ISR_INT_STATUS_DONE))
		return IRQ_NONE;

	complete(&ecc_dev->irq_done);

	return IRQ_HANDLED;
}

static int kmb_ocs_ecc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ocs_ecc_dev *ecc_dev;
	int rc;

	ecc_dev = devm_kzalloc(dev, sizeof(*ecc_dev), GFP_KERNEL);
	if (!ecc_dev)
		return -ENOMEM;

	ecc_dev->dev = dev;

	platform_set_drvdata(pdev, ecc_dev);

	INIT_LIST_HEAD(&ecc_dev->list);
	init_completion(&ecc_dev->irq_done);

	 
	ecc_dev->base_reg = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ecc_dev->base_reg)) {
		dev_err(dev, "Failed to get base address\n");
		rc = PTR_ERR(ecc_dev->base_reg);
		goto list_del;
	}

	 
	ecc_dev->irq = platform_get_irq(pdev, 0);
	if (ecc_dev->irq < 0) {
		rc = ecc_dev->irq;
		goto list_del;
	}

	rc = devm_request_threaded_irq(dev, ecc_dev->irq, ocs_ecc_irq_handler,
				       NULL, 0, "keembay-ocs-ecc", ecc_dev);
	if (rc < 0) {
		dev_err(dev, "Could not request IRQ\n");
		goto list_del;
	}

	 
	spin_lock(&ocs_ecc.lock);
	list_add_tail(&ecc_dev->list, &ocs_ecc.dev_list);
	spin_unlock(&ocs_ecc.lock);

	 
	ecc_dev->engine = crypto_engine_alloc_init(dev, 1);
	if (!ecc_dev->engine) {
		dev_err(dev, "Could not allocate crypto engine\n");
		rc = -ENOMEM;
		goto list_del;
	}

	rc = crypto_engine_start(ecc_dev->engine);
	if (rc) {
		dev_err(dev, "Could not start crypto engine\n");
		goto cleanup;
	}

	 
	rc = crypto_engine_register_kpp(&ocs_ecdh_p256);
	if (rc) {
		dev_err(dev,
			"Could not register OCS algorithms with Crypto API\n");
		goto cleanup;
	}

	rc = crypto_engine_register_kpp(&ocs_ecdh_p384);
	if (rc) {
		dev_err(dev,
			"Could not register OCS algorithms with Crypto API\n");
		goto ocs_ecdh_p384_error;
	}

	return 0;

ocs_ecdh_p384_error:
	crypto_engine_unregister_kpp(&ocs_ecdh_p256);

cleanup:
	crypto_engine_exit(ecc_dev->engine);

list_del:
	spin_lock(&ocs_ecc.lock);
	list_del(&ecc_dev->list);
	spin_unlock(&ocs_ecc.lock);

	return rc;
}

static int kmb_ocs_ecc_remove(struct platform_device *pdev)
{
	struct ocs_ecc_dev *ecc_dev;

	ecc_dev = platform_get_drvdata(pdev);

	crypto_engine_unregister_kpp(&ocs_ecdh_p384);
	crypto_engine_unregister_kpp(&ocs_ecdh_p256);

	spin_lock(&ocs_ecc.lock);
	list_del(&ecc_dev->list);
	spin_unlock(&ocs_ecc.lock);

	crypto_engine_exit(ecc_dev->engine);

	return 0;
}

 
static const struct of_device_id kmb_ocs_ecc_of_match[] = {
	{
		.compatible = "intel,keembay-ocs-ecc",
	},
	{}
};

 
static struct platform_driver kmb_ocs_ecc_driver = {
	.probe = kmb_ocs_ecc_probe,
	.remove = kmb_ocs_ecc_remove,
	.driver = {
			.name = DRV_NAME,
			.of_match_table = kmb_ocs_ecc_of_match,
		},
};
module_platform_driver(kmb_ocs_ecc_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Intel Keem Bay OCS ECC Driver");
MODULE_ALIAS_CRYPTO("ecdh-nist-p256");
MODULE_ALIAS_CRYPTO("ecdh-nist-p384");
MODULE_ALIAS_CRYPTO("ecdh-nist-p256-keembay-ocs");
MODULE_ALIAS_CRYPTO("ecdh-nist-p384-keembay-ocs");
