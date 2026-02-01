
 
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/rslib.h>
#include <linux/slab.h>
#include <linux/mutex.h>

enum {
	RS_DECODE_LAMBDA,
	RS_DECODE_SYN,
	RS_DECODE_B,
	RS_DECODE_T,
	RS_DECODE_OMEGA,
	RS_DECODE_ROOT,
	RS_DECODE_REG,
	RS_DECODE_LOC,
	RS_DECODE_NUM_BUFFERS
};

 
static LIST_HEAD(codec_list);
 
static DEFINE_MUTEX(rslistlock);

 
static struct rs_codec *codec_init(int symsize, int gfpoly, int (*gffunc)(int),
				   int fcr, int prim, int nroots, gfp_t gfp)
{
	int i, j, sr, root, iprim;
	struct rs_codec *rs;

	rs = kzalloc(sizeof(*rs), gfp);
	if (!rs)
		return NULL;

	INIT_LIST_HEAD(&rs->list);

	rs->mm = symsize;
	rs->nn = (1 << symsize) - 1;
	rs->fcr = fcr;
	rs->prim = prim;
	rs->nroots = nroots;
	rs->gfpoly = gfpoly;
	rs->gffunc = gffunc;

	 
	rs->alpha_to = kmalloc_array(rs->nn + 1, sizeof(uint16_t), gfp);
	if (rs->alpha_to == NULL)
		goto err;

	rs->index_of = kmalloc_array(rs->nn + 1, sizeof(uint16_t), gfp);
	if (rs->index_of == NULL)
		goto err;

	rs->genpoly = kmalloc_array(rs->nroots + 1, sizeof(uint16_t), gfp);
	if(rs->genpoly == NULL)
		goto err;

	 
	rs->index_of[0] = rs->nn;	 
	rs->alpha_to[rs->nn] = 0;	 
	if (gfpoly) {
		sr = 1;
		for (i = 0; i < rs->nn; i++) {
			rs->index_of[sr] = i;
			rs->alpha_to[i] = sr;
			sr <<= 1;
			if (sr & (1 << symsize))
				sr ^= gfpoly;
			sr &= rs->nn;
		}
	} else {
		sr = gffunc(0);
		for (i = 0; i < rs->nn; i++) {
			rs->index_of[sr] = i;
			rs->alpha_to[i] = sr;
			sr = gffunc(sr);
		}
	}
	 
	if(sr != rs->alpha_to[0])
		goto err;

	 
	for(iprim = 1; (iprim % prim) != 0; iprim += rs->nn);
	 
	rs->iprim = iprim / prim;

	 
	rs->genpoly[0] = 1;
	for (i = 0, root = fcr * prim; i < nroots; i++, root += prim) {
		rs->genpoly[i + 1] = 1;
		 
		for (j = i; j > 0; j--) {
			if (rs->genpoly[j] != 0) {
				rs->genpoly[j] = rs->genpoly[j -1] ^
					rs->alpha_to[rs_modnn(rs,
					rs->index_of[rs->genpoly[j]] + root)];
			} else
				rs->genpoly[j] = rs->genpoly[j - 1];
		}
		 
		rs->genpoly[0] =
			rs->alpha_to[rs_modnn(rs,
				rs->index_of[rs->genpoly[0]] + root)];
	}
	 
	for (i = 0; i <= nroots; i++)
		rs->genpoly[i] = rs->index_of[rs->genpoly[i]];

	rs->users = 1;
	list_add(&rs->list, &codec_list);
	return rs;

err:
	kfree(rs->genpoly);
	kfree(rs->index_of);
	kfree(rs->alpha_to);
	kfree(rs);
	return NULL;
}


 
void free_rs(struct rs_control *rs)
{
	struct rs_codec *cd;

	if (!rs)
		return;

	cd = rs->codec;
	mutex_lock(&rslistlock);
	cd->users--;
	if(!cd->users) {
		list_del(&cd->list);
		kfree(cd->alpha_to);
		kfree(cd->index_of);
		kfree(cd->genpoly);
		kfree(cd);
	}
	mutex_unlock(&rslistlock);
	kfree(rs);
}
EXPORT_SYMBOL_GPL(free_rs);

 
static struct rs_control *init_rs_internal(int symsize, int gfpoly,
					   int (*gffunc)(int), int fcr,
					   int prim, int nroots, gfp_t gfp)
{
	struct list_head *tmp;
	struct rs_control *rs;
	unsigned int bsize;

	 
	if (symsize < 1)
		return NULL;
	if (fcr < 0 || fcr >= (1<<symsize))
		return NULL;
	if (prim <= 0 || prim >= (1<<symsize))
		return NULL;
	if (nroots < 0 || nroots >= (1<<symsize))
		return NULL;

	 
	bsize = sizeof(uint16_t) * RS_DECODE_NUM_BUFFERS * (nroots + 1);
	rs = kzalloc(sizeof(*rs) + bsize, gfp);
	if (!rs)
		return NULL;

	mutex_lock(&rslistlock);

	 
	list_for_each(tmp, &codec_list) {
		struct rs_codec *cd = list_entry(tmp, struct rs_codec, list);

		if (symsize != cd->mm)
			continue;
		if (gfpoly != cd->gfpoly)
			continue;
		if (gffunc != cd->gffunc)
			continue;
		if (fcr != cd->fcr)
			continue;
		if (prim != cd->prim)
			continue;
		if (nroots != cd->nroots)
			continue;
		 
		cd->users++;
		rs->codec = cd;
		goto out;
	}

	 
	rs->codec = codec_init(symsize, gfpoly, gffunc, fcr, prim, nroots, gfp);
	if (!rs->codec) {
		kfree(rs);
		rs = NULL;
	}
out:
	mutex_unlock(&rslistlock);
	return rs;
}

 
struct rs_control *init_rs_gfp(int symsize, int gfpoly, int fcr, int prim,
			       int nroots, gfp_t gfp)
{
	return init_rs_internal(symsize, gfpoly, NULL, fcr, prim, nroots, gfp);
}
EXPORT_SYMBOL_GPL(init_rs_gfp);

 
struct rs_control *init_rs_non_canonical(int symsize, int (*gffunc)(int),
					 int fcr, int prim, int nroots)
{
	return init_rs_internal(symsize, 0, gffunc, fcr, prim, nroots,
				GFP_KERNEL);
}
EXPORT_SYMBOL_GPL(init_rs_non_canonical);

#ifdef CONFIG_REED_SOLOMON_ENC8
 
int encode_rs8(struct rs_control *rsc, uint8_t *data, int len, uint16_t *par,
	       uint16_t invmsk)
{
#include "encode_rs.c"
}
EXPORT_SYMBOL_GPL(encode_rs8);
#endif

#ifdef CONFIG_REED_SOLOMON_DEC8
 
int decode_rs8(struct rs_control *rsc, uint8_t *data, uint16_t *par, int len,
	       uint16_t *s, int no_eras, int *eras_pos, uint16_t invmsk,
	       uint16_t *corr)
{
#include "decode_rs.c"
}
EXPORT_SYMBOL_GPL(decode_rs8);
#endif

#ifdef CONFIG_REED_SOLOMON_ENC16
 
int encode_rs16(struct rs_control *rsc, uint16_t *data, int len, uint16_t *par,
	uint16_t invmsk)
{
#include "encode_rs.c"
}
EXPORT_SYMBOL_GPL(encode_rs16);
#endif

#ifdef CONFIG_REED_SOLOMON_DEC16
 
int decode_rs16(struct rs_control *rsc, uint16_t *data, uint16_t *par, int len,
		uint16_t *s, int no_eras, int *eras_pos, uint16_t invmsk,
		uint16_t *corr)
{
#include "decode_rs.c"
}
EXPORT_SYMBOL_GPL(decode_rs16);
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Reed Solomon encoder/decoder");
MODULE_AUTHOR("Phil Karn, Thomas Gleixner");

