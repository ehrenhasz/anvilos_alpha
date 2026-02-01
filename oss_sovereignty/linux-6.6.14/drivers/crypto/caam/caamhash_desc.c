
 

#include "compat.h"
#include "desc_constr.h"
#include "caamhash_desc.h"

 
void cnstr_shdsc_ahash(u32 * const desc, struct alginfo *adata, u32 state,
		       int digestsize, int ctx_len, bool import_ctx, int era)
{
	u32 op = adata->algtype;

	init_sh_desc(desc, HDR_SHARE_SERIAL);

	 
	if (state != OP_ALG_AS_UPDATE && adata->keylen) {
		u32 *skip_key_load;

		 
		skip_key_load = append_jump(desc, JUMP_JSL | JUMP_TEST_ALL |
					    JUMP_COND_SHRD);

		if (era < 6)
			append_key_as_imm(desc, adata->key_virt,
					  adata->keylen_pad,
					  adata->keylen, CLASS_2 |
					  KEY_DEST_MDHA_SPLIT | KEY_ENC);
		else
			append_proto_dkp(desc, adata);

		set_jump_tgt_here(desc, skip_key_load);

		op |= OP_ALG_AAI_HMAC_PRECOMP;
	}

	 
	if (import_ctx)
		append_seq_load(desc, ctx_len, LDST_CLASS_2_CCB |
				LDST_SRCDST_BYTE_CONTEXT);

	 
	append_operation(desc, op | state | OP_ALG_ENCRYPT);

	 
	append_math_add(desc, VARSEQINLEN, SEQINLEN, REG0, CAAM_CMD_SZ);
	 
	append_seq_fifo_load(desc, 0, FIFOLD_CLASS_CLASS2 | FIFOLD_TYPE_LAST2 |
			     FIFOLD_TYPE_MSG | KEY_VLF);
	 
	append_seq_store(desc, digestsize, LDST_CLASS_2_CCB |
			 LDST_SRCDST_BYTE_CONTEXT);
}
EXPORT_SYMBOL(cnstr_shdsc_ahash);

 
void cnstr_shdsc_sk_hash(u32 * const desc, struct alginfo *adata, u32 state,
			 int digestsize, int ctx_len)
{
	u32 *skip_key_load;

	init_sh_desc(desc, HDR_SHARE_SERIAL | HDR_SAVECTX);

	 
	skip_key_load = append_jump(desc, JUMP_TEST_ALL | JUMP_COND_SHRD);

	if (state == OP_ALG_AS_INIT || state == OP_ALG_AS_INITFINAL) {
		append_key_as_imm(desc, adata->key_virt, adata->keylen,
				  adata->keylen, CLASS_1 | KEY_DEST_CLASS_REG);
	} else {  
		if (is_xcbc_aes(adata->algtype))
			 
			append_key(desc, adata->key_dma, adata->keylen,
				   CLASS_1 | KEY_DEST_CLASS_REG | KEY_ENC);
		else  
			append_key_as_imm(desc, adata->key_virt, adata->keylen,
					  adata->keylen, CLASS_1 |
					  KEY_DEST_CLASS_REG);
		 
		append_seq_load(desc, ctx_len, LDST_CLASS_1_CCB |
				LDST_SRCDST_BYTE_CONTEXT);
	}

	set_jump_tgt_here(desc, skip_key_load);

	 
	append_operation(desc, adata->algtype | state | OP_ALG_ENCRYPT);

	 
	append_math_add(desc, VARSEQINLEN, SEQINLEN, REG0, CAAM_CMD_SZ);

	 
	append_seq_fifo_load(desc, 0, FIFOLD_CLASS_CLASS1 | FIFOLD_TYPE_LAST1 |
			     FIFOLD_TYPE_MSG | FIFOLDST_VLF);

	 
	append_seq_store(desc, digestsize, LDST_CLASS_1_CCB |
			 LDST_SRCDST_BYTE_CONTEXT);
	if (is_xcbc_aes(adata->algtype) && state == OP_ALG_AS_INIT)
		 
		append_fifo_store(desc, adata->key_dma, adata->keylen,
				  LDST_CLASS_1_CCB | FIFOST_TYPE_KEY_KEK);
}
EXPORT_SYMBOL(cnstr_shdsc_sk_hash);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("FSL CAAM ahash descriptors support");
MODULE_AUTHOR("NXP Semiconductors");
