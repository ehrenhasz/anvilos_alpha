 
 

 
static inline u32 split_key_len(u32 hash)
{
	 
	static const u8 mdpadlen[] = { 16, 20, 32, 32, 64, 64 };
	u32 idx;

	idx = (hash & OP_ALG_ALGSEL_SUBMASK) >> OP_ALG_ALGSEL_SHIFT;

	return (u32)(mdpadlen[idx] * 2);
}

 
static inline u32 split_key_pad_len(u32 hash)
{
	return ALIGN(split_key_len(hash), 16);
}

struct split_key_result {
	struct completion completion;
	int err;
};

void split_key_done(struct device *dev, u32 *desc, u32 err, void *context);

int gen_split_key(struct device *jrdev, u8 *key_out,
		  struct alginfo * const adata, const u8 *key_in, u32 keylen,
		  int max_keylen);
