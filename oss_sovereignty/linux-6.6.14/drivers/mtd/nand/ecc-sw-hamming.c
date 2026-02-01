
 

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand-ecc-sw-hamming.h>
#include <linux/slab.h>
#include <asm/byteorder.h>

 
static const char invparity[256] = {
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};

 
static const char bitsperbyte[256] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

 
static const char addressbits[256] = {
	0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
	0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
	0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
	0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
	0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
	0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
	0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
	0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
	0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
	0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
	0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01,
	0x02, 0x02, 0x03, 0x03, 0x02, 0x02, 0x03, 0x03,
	0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
	0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
	0x04, 0x04, 0x05, 0x05, 0x04, 0x04, 0x05, 0x05,
	0x06, 0x06, 0x07, 0x07, 0x06, 0x06, 0x07, 0x07,
	0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
	0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b, 0x0b,
	0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
	0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b, 0x0b,
	0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d, 0x0d,
	0x0e, 0x0e, 0x0f, 0x0f, 0x0e, 0x0e, 0x0f, 0x0f,
	0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d, 0x0d,
	0x0e, 0x0e, 0x0f, 0x0f, 0x0e, 0x0e, 0x0f, 0x0f,
	0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
	0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b, 0x0b,
	0x08, 0x08, 0x09, 0x09, 0x08, 0x08, 0x09, 0x09,
	0x0a, 0x0a, 0x0b, 0x0b, 0x0a, 0x0a, 0x0b, 0x0b,
	0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d, 0x0d,
	0x0e, 0x0e, 0x0f, 0x0f, 0x0e, 0x0e, 0x0f, 0x0f,
	0x0c, 0x0c, 0x0d, 0x0d, 0x0c, 0x0c, 0x0d, 0x0d,
	0x0e, 0x0e, 0x0f, 0x0f, 0x0e, 0x0e, 0x0f, 0x0f
};

int ecc_sw_hamming_calculate(const unsigned char *buf, unsigned int step_size,
			     unsigned char *code, bool sm_order)
{
	const u32 *bp = (uint32_t *)buf;
	const u32 eccsize_mult = (step_size == 256) ? 1 : 2;
	 
	u32 cur;
	 
	u32 rp0, rp1, rp2, rp3, rp4, rp5, rp6, rp7, rp8, rp9, rp10, rp11, rp12,
		rp13, rp14, rp15, rp16, rp17;
	 
	u32 par;
	 
	u32 tmppar;
	int i;

	par = 0;
	rp4 = 0;
	rp6 = 0;
	rp8 = 0;
	rp10 = 0;
	rp12 = 0;
	rp14 = 0;
	rp16 = 0;
	rp17 = 0;

	 
	for (i = 0; i < eccsize_mult << 2; i++) {
		cur = *bp++;
		tmppar = cur;
		rp4 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp6 ^= tmppar;
		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp8 ^= tmppar;

		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		rp6 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp6 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp10 ^= tmppar;

		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		rp6 ^= cur;
		rp8 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp6 ^= cur;
		rp8 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		rp8 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp8 ^= cur;

		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		rp6 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp6 ^= cur;
		cur = *bp++;
		tmppar ^= cur;
		rp4 ^= cur;
		cur = *bp++;
		tmppar ^= cur;

		par ^= tmppar;
		if ((i & 0x1) == 0)
			rp12 ^= tmppar;
		if ((i & 0x2) == 0)
			rp14 ^= tmppar;
		if (eccsize_mult == 2 && (i & 0x4) == 0)
			rp16 ^= tmppar;
	}

	 
	rp4 ^= (rp4 >> 16);
	rp4 ^= (rp4 >> 8);
	rp4 &= 0xff;
	rp6 ^= (rp6 >> 16);
	rp6 ^= (rp6 >> 8);
	rp6 &= 0xff;
	rp8 ^= (rp8 >> 16);
	rp8 ^= (rp8 >> 8);
	rp8 &= 0xff;
	rp10 ^= (rp10 >> 16);
	rp10 ^= (rp10 >> 8);
	rp10 &= 0xff;
	rp12 ^= (rp12 >> 16);
	rp12 ^= (rp12 >> 8);
	rp12 &= 0xff;
	rp14 ^= (rp14 >> 16);
	rp14 ^= (rp14 >> 8);
	rp14 &= 0xff;
	if (eccsize_mult == 2) {
		rp16 ^= (rp16 >> 16);
		rp16 ^= (rp16 >> 8);
		rp16 &= 0xff;
	}

	 
#ifdef __BIG_ENDIAN
	rp2 = (par >> 16);
	rp2 ^= (rp2 >> 8);
	rp2 &= 0xff;
	rp3 = par & 0xffff;
	rp3 ^= (rp3 >> 8);
	rp3 &= 0xff;
#else
	rp3 = (par >> 16);
	rp3 ^= (rp3 >> 8);
	rp3 &= 0xff;
	rp2 = par & 0xffff;
	rp2 ^= (rp2 >> 8);
	rp2 &= 0xff;
#endif

	 
	par ^= (par >> 16);
#ifdef __BIG_ENDIAN
	rp0 = (par >> 8) & 0xff;
	rp1 = (par & 0xff);
#else
	rp1 = (par >> 8) & 0xff;
	rp0 = (par & 0xff);
#endif

	 
	par ^= (par >> 8);
	par &= 0xff;

	 
	rp5 = (par ^ rp4) & 0xff;
	rp7 = (par ^ rp6) & 0xff;
	rp9 = (par ^ rp8) & 0xff;
	rp11 = (par ^ rp10) & 0xff;
	rp13 = (par ^ rp12) & 0xff;
	rp15 = (par ^ rp14) & 0xff;
	if (eccsize_mult == 2)
		rp17 = (par ^ rp16) & 0xff;

	 
	if (sm_order) {
		code[0] = (invparity[rp7] << 7) | (invparity[rp6] << 6) |
			  (invparity[rp5] << 5) | (invparity[rp4] << 4) |
			  (invparity[rp3] << 3) | (invparity[rp2] << 2) |
			  (invparity[rp1] << 1) | (invparity[rp0]);
		code[1] = (invparity[rp15] << 7) | (invparity[rp14] << 6) |
			  (invparity[rp13] << 5) | (invparity[rp12] << 4) |
			  (invparity[rp11] << 3) | (invparity[rp10] << 2) |
			  (invparity[rp9] << 1) | (invparity[rp8]);
	} else {
		code[1] = (invparity[rp7] << 7) | (invparity[rp6] << 6) |
			  (invparity[rp5] << 5) | (invparity[rp4] << 4) |
			  (invparity[rp3] << 3) | (invparity[rp2] << 2) |
			  (invparity[rp1] << 1) | (invparity[rp0]);
		code[0] = (invparity[rp15] << 7) | (invparity[rp14] << 6) |
			  (invparity[rp13] << 5) | (invparity[rp12] << 4) |
			  (invparity[rp11] << 3) | (invparity[rp10] << 2) |
			  (invparity[rp9] << 1) | (invparity[rp8]);
	}

	if (eccsize_mult == 1)
		code[2] =
		    (invparity[par & 0xf0] << 7) |
		    (invparity[par & 0x0f] << 6) |
		    (invparity[par & 0xcc] << 5) |
		    (invparity[par & 0x33] << 4) |
		    (invparity[par & 0xaa] << 3) |
		    (invparity[par & 0x55] << 2) |
		    3;
	else
		code[2] =
		    (invparity[par & 0xf0] << 7) |
		    (invparity[par & 0x0f] << 6) |
		    (invparity[par & 0xcc] << 5) |
		    (invparity[par & 0x33] << 4) |
		    (invparity[par & 0xaa] << 3) |
		    (invparity[par & 0x55] << 2) |
		    (invparity[rp17] << 1) |
		    (invparity[rp16] << 0);

	return 0;
}
EXPORT_SYMBOL(ecc_sw_hamming_calculate);

 
int nand_ecc_sw_hamming_calculate(struct nand_device *nand,
				  const unsigned char *buf, unsigned char *code)
{
	struct nand_ecc_sw_hamming_conf *engine_conf = nand->ecc.ctx.priv;
	unsigned int step_size = nand->ecc.ctx.conf.step_size;
	bool sm_order = engine_conf ? engine_conf->sm_order : false;

	return ecc_sw_hamming_calculate(buf, step_size, code, sm_order);
}
EXPORT_SYMBOL(nand_ecc_sw_hamming_calculate);

int ecc_sw_hamming_correct(unsigned char *buf, unsigned char *read_ecc,
			   unsigned char *calc_ecc, unsigned int step_size,
			   bool sm_order)
{
	const u32 eccsize_mult = step_size >> 8;
	unsigned char b0, b1, b2, bit_addr;
	unsigned int byte_addr;

	 
	if (sm_order) {
		b0 = read_ecc[0] ^ calc_ecc[0];
		b1 = read_ecc[1] ^ calc_ecc[1];
	} else {
		b0 = read_ecc[1] ^ calc_ecc[1];
		b1 = read_ecc[0] ^ calc_ecc[0];
	}

	b2 = read_ecc[2] ^ calc_ecc[2];

	 

	 
	 

	if ((b0 | b1 | b2) == 0)
		return 0;	 

	if ((((b0 ^ (b0 >> 1)) & 0x55) == 0x55) &&
	    (((b1 ^ (b1 >> 1)) & 0x55) == 0x55) &&
	    ((eccsize_mult == 1 && ((b2 ^ (b2 >> 1)) & 0x54) == 0x54) ||
	     (eccsize_mult == 2 && ((b2 ^ (b2 >> 1)) & 0x55) == 0x55))) {
	 
		 
		if (eccsize_mult == 1)
			byte_addr = (addressbits[b1] << 4) + addressbits[b0];
		else
			byte_addr = (addressbits[b2 & 0x3] << 8) +
				    (addressbits[b1] << 4) + addressbits[b0];
		bit_addr = addressbits[b2 >> 2];
		 
		buf[byte_addr] ^= (1 << bit_addr);
		return 1;

	}
	 
	if ((bitsperbyte[b0] + bitsperbyte[b1] + bitsperbyte[b2]) == 1)
		return 1;	 

	pr_err("%s: uncorrectable ECC error\n", __func__);
	return -EBADMSG;
}
EXPORT_SYMBOL(ecc_sw_hamming_correct);

 
int nand_ecc_sw_hamming_correct(struct nand_device *nand, unsigned char *buf,
				unsigned char *read_ecc,
				unsigned char *calc_ecc)
{
	struct nand_ecc_sw_hamming_conf *engine_conf = nand->ecc.ctx.priv;
	unsigned int step_size = nand->ecc.ctx.conf.step_size;
	bool sm_order = engine_conf ? engine_conf->sm_order : false;

	return ecc_sw_hamming_correct(buf, read_ecc, calc_ecc, step_size,
				      sm_order);
}
EXPORT_SYMBOL(nand_ecc_sw_hamming_correct);

int nand_ecc_sw_hamming_init_ctx(struct nand_device *nand)
{
	struct nand_ecc_props *conf = &nand->ecc.ctx.conf;
	struct nand_ecc_sw_hamming_conf *engine_conf;
	struct mtd_info *mtd = nanddev_to_mtd(nand);
	int ret;

	if (!mtd->ooblayout) {
		switch (mtd->oobsize) {
		case 8:
		case 16:
			mtd_set_ooblayout(mtd, nand_get_small_page_ooblayout());
			break;
		case 64:
		case 128:
			mtd_set_ooblayout(mtd,
					  nand_get_large_page_hamming_ooblayout());
			break;
		default:
			return -ENOTSUPP;
		}
	}

	conf->engine_type = NAND_ECC_ENGINE_TYPE_SOFT;
	conf->algo = NAND_ECC_ALGO_HAMMING;
	conf->step_size = nand->ecc.user_conf.step_size;
	conf->strength = 1;

	 
	if (conf->step_size != 256 && conf->step_size != 512)
		conf->step_size = 256;

	engine_conf = kzalloc(sizeof(*engine_conf), GFP_KERNEL);
	if (!engine_conf)
		return -ENOMEM;

	ret = nand_ecc_init_req_tweaking(&engine_conf->req_ctx, nand);
	if (ret)
		goto free_engine_conf;

	engine_conf->code_size = 3;
	engine_conf->calc_buf = kzalloc(mtd->oobsize, GFP_KERNEL);
	engine_conf->code_buf = kzalloc(mtd->oobsize, GFP_KERNEL);
	if (!engine_conf->calc_buf || !engine_conf->code_buf) {
		ret = -ENOMEM;
		goto free_bufs;
	}

	nand->ecc.ctx.priv = engine_conf;
	nand->ecc.ctx.nsteps = mtd->writesize / conf->step_size;
	nand->ecc.ctx.total = nand->ecc.ctx.nsteps * engine_conf->code_size;

	return 0;

free_bufs:
	nand_ecc_cleanup_req_tweaking(&engine_conf->req_ctx);
	kfree(engine_conf->calc_buf);
	kfree(engine_conf->code_buf);
free_engine_conf:
	kfree(engine_conf);

	return ret;
}
EXPORT_SYMBOL(nand_ecc_sw_hamming_init_ctx);

void nand_ecc_sw_hamming_cleanup_ctx(struct nand_device *nand)
{
	struct nand_ecc_sw_hamming_conf *engine_conf = nand->ecc.ctx.priv;

	if (engine_conf) {
		nand_ecc_cleanup_req_tweaking(&engine_conf->req_ctx);
		kfree(engine_conf->calc_buf);
		kfree(engine_conf->code_buf);
		kfree(engine_conf);
	}
}
EXPORT_SYMBOL(nand_ecc_sw_hamming_cleanup_ctx);

static int nand_ecc_sw_hamming_prepare_io_req(struct nand_device *nand,
					      struct nand_page_io_req *req)
{
	struct nand_ecc_sw_hamming_conf *engine_conf = nand->ecc.ctx.priv;
	struct mtd_info *mtd = nanddev_to_mtd(nand);
	int eccsize = nand->ecc.ctx.conf.step_size;
	int eccbytes = engine_conf->code_size;
	int eccsteps = nand->ecc.ctx.nsteps;
	int total = nand->ecc.ctx.total;
	u8 *ecccalc = engine_conf->calc_buf;
	const u8 *data;
	int i;

	 
	if (req->mode == MTD_OPS_RAW)
		return 0;

	 
	if (!req->datalen)
		return 0;

	nand_ecc_tweak_req(&engine_conf->req_ctx, req);

	 
	if (req->type == NAND_PAGE_READ)
		return 0;

	 
	for (i = 0, data = req->databuf.out;
	     eccsteps;
	     eccsteps--, i += eccbytes, data += eccsize)
		nand_ecc_sw_hamming_calculate(nand, data, &ecccalc[i]);

	return mtd_ooblayout_set_eccbytes(mtd, ecccalc, (void *)req->oobbuf.out,
					  0, total);
}

static int nand_ecc_sw_hamming_finish_io_req(struct nand_device *nand,
					     struct nand_page_io_req *req)
{
	struct nand_ecc_sw_hamming_conf *engine_conf = nand->ecc.ctx.priv;
	struct mtd_info *mtd = nanddev_to_mtd(nand);
	int eccsize = nand->ecc.ctx.conf.step_size;
	int total = nand->ecc.ctx.total;
	int eccbytes = engine_conf->code_size;
	int eccsteps = nand->ecc.ctx.nsteps;
	u8 *ecccalc = engine_conf->calc_buf;
	u8 *ecccode = engine_conf->code_buf;
	unsigned int max_bitflips = 0;
	u8 *data = req->databuf.in;
	int i, ret;

	 
	if (req->mode == MTD_OPS_RAW)
		return 0;

	 
	if (!req->datalen)
		return 0;

	 
	if (req->type == NAND_PAGE_WRITE) {
		nand_ecc_restore_req(&engine_conf->req_ctx, req);
		return 0;
	}

	 
	ret = mtd_ooblayout_get_eccbytes(mtd, ecccode, req->oobbuf.in, 0,
					 total);
	if (ret)
		return ret;

	 
	for (i = 0; eccsteps; eccsteps--, i += eccbytes, data += eccsize)
		nand_ecc_sw_hamming_calculate(nand, data, &ecccalc[i]);

	 
	for (eccsteps = nand->ecc.ctx.nsteps, i = 0, data = req->databuf.in;
	     eccsteps;
	     eccsteps--, i += eccbytes, data += eccsize) {
		int stat =  nand_ecc_sw_hamming_correct(nand, data,
							&ecccode[i],
							&ecccalc[i]);
		if (stat < 0) {
			mtd->ecc_stats.failed++;
		} else {
			mtd->ecc_stats.corrected += stat;
			max_bitflips = max_t(unsigned int, max_bitflips, stat);
		}
	}

	nand_ecc_restore_req(&engine_conf->req_ctx, req);

	return max_bitflips;
}

static struct nand_ecc_engine_ops nand_ecc_sw_hamming_engine_ops = {
	.init_ctx = nand_ecc_sw_hamming_init_ctx,
	.cleanup_ctx = nand_ecc_sw_hamming_cleanup_ctx,
	.prepare_io_req = nand_ecc_sw_hamming_prepare_io_req,
	.finish_io_req = nand_ecc_sw_hamming_finish_io_req,
};

static struct nand_ecc_engine nand_ecc_sw_hamming_engine = {
	.ops = &nand_ecc_sw_hamming_engine_ops,
};

struct nand_ecc_engine *nand_ecc_sw_hamming_get_engine(void)
{
	return &nand_ecc_sw_hamming_engine;
}
EXPORT_SYMBOL(nand_ecc_sw_hamming_get_engine);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frans Meulenbroeks <fransmeulenbroeks@gmail.com>");
MODULE_DESCRIPTION("NAND software Hamming ECC support");
