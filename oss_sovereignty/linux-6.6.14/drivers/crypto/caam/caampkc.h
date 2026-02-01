 
 

#ifndef _PKC_DESC_H_
#define _PKC_DESC_H_
#include "compat.h"
#include "pdb.h"

 
enum caam_priv_key_form {
	FORM1,
	FORM2,
	FORM3
};

 
struct caam_rsa_key {
	u8 *n;
	u8 *e;
	u8 *d;
	u8 *p;
	u8 *q;
	u8 *dp;
	u8 *dq;
	u8 *qinv;
	u8 *tmp1;
	u8 *tmp2;
	size_t n_sz;
	size_t e_sz;
	size_t d_sz;
	size_t p_sz;
	size_t q_sz;
	enum caam_priv_key_form priv_form;
};

 
struct caam_rsa_ctx {
	struct caam_rsa_key key;
	struct device *dev;
	dma_addr_t padding_dma;

};

 
struct caam_rsa_req_ctx {
	struct scatterlist src[2];
	struct scatterlist *fixup_src;
	unsigned int fixup_src_len;
	struct rsa_edesc *edesc;
	void (*akcipher_op_done)(struct device *jrdev, u32 *desc, u32 err,
				 void *context);
};

 
struct rsa_edesc {
	int src_nents;
	int dst_nents;
	int mapped_src_nents;
	int mapped_dst_nents;
	int sec4_sg_bytes;
	bool bklog;
	dma_addr_t sec4_sg_dma;
	struct sec4_sg_entry *sec4_sg;
	union {
		struct rsa_pub_pdb pub;
		struct rsa_priv_f1_pdb priv_f1;
		struct rsa_priv_f2_pdb priv_f2;
		struct rsa_priv_f3_pdb priv_f3;
	} pdb;
	u32 hw_desc[];
};

 
void init_rsa_pub_desc(u32 *desc, struct rsa_pub_pdb *pdb);
void init_rsa_priv_f1_desc(u32 *desc, struct rsa_priv_f1_pdb *pdb);
void init_rsa_priv_f2_desc(u32 *desc, struct rsa_priv_f2_pdb *pdb);
void init_rsa_priv_f3_desc(u32 *desc, struct rsa_priv_f3_pdb *pdb);

#endif
