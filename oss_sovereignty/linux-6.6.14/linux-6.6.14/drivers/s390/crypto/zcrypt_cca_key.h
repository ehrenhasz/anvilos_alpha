#ifndef _ZCRYPT_CCA_KEY_H_
#define _ZCRYPT_CCA_KEY_H_
struct t6_keyblock_hdr {
	unsigned short blen;
	unsigned short ulen;
	unsigned short flags;
};
struct cca_token_hdr {
	unsigned char  token_identifier;
	unsigned char  version;
	unsigned short token_length;
	unsigned char  reserved[4];
} __packed;
#define CCA_TKN_HDR_ID_EXT 0x1E
#define CCA_PVT_USAGE_ALL 0x80
struct cca_public_sec {
	unsigned char  section_identifier;
	unsigned char  version;
	unsigned short section_length;
	unsigned char  reserved[2];
	unsigned short exponent_len;
	unsigned short modulus_bit_len;
	unsigned short modulus_byte_len;     
} __packed;
struct cca_pvt_ext_crt_sec {
	unsigned char  section_identifier;
	unsigned char  version;
	unsigned short section_length;
	unsigned char  private_key_hash[20];
	unsigned char  reserved1[4];
	unsigned char  key_format;
	unsigned char  reserved2;
	unsigned char  key_name_hash[20];
	unsigned char  key_use_flags[4];
	unsigned short p_len;
	unsigned short q_len;
	unsigned short dp_len;
	unsigned short dq_len;
	unsigned short u_len;
	unsigned short mod_len;
	unsigned char  reserved3[4];
	unsigned short pad_len;
	unsigned char  reserved4[52];
	unsigned char  confounder[8];
} __packed;
#define CCA_PVT_EXT_CRT_SEC_ID_PVT 0x08
#define CCA_PVT_EXT_CRT_SEC_FMT_CL 0x40
static inline int zcrypt_type6_mex_key_en(struct ica_rsa_modexpo *mex, void *p)
{
	static struct cca_token_hdr static_pub_hdr = {
		.token_identifier	=  0x1E,
	};
	static struct cca_public_sec static_pub_sec = {
		.section_identifier	=  0x04,
	};
	struct {
		struct t6_keyblock_hdr t6_hdr;
		struct cca_token_hdr pubhdr;
		struct cca_public_sec pubsec;
		char exponent[];
	} __packed *key = p;
	unsigned char *ptr;
	if (WARN_ON_ONCE(mex->inputdatalength > 512))
		return -EINVAL;
	memset(key, 0, sizeof(*key));
	key->pubhdr = static_pub_hdr;
	key->pubsec = static_pub_sec;
	ptr = key->exponent;
	if (copy_from_user(ptr, mex->b_key, mex->inputdatalength))
		return -EFAULT;
	ptr += mex->inputdatalength;
	if (copy_from_user(ptr, mex->n_modulus, mex->inputdatalength))
		return -EFAULT;
	key->pubsec.modulus_bit_len = 8 * mex->inputdatalength;
	key->pubsec.modulus_byte_len = mex->inputdatalength;
	key->pubsec.exponent_len = mex->inputdatalength;
	key->pubsec.section_length = sizeof(key->pubsec) +
					2 * mex->inputdatalength;
	key->pubhdr.token_length =
		key->pubsec.section_length + sizeof(key->pubhdr);
	key->t6_hdr.ulen = key->pubhdr.token_length + 4;
	key->t6_hdr.blen = key->pubhdr.token_length + 6;
	return sizeof(*key) + 2 * mex->inputdatalength;
}
static inline int zcrypt_type6_crt_key(struct ica_rsa_modexpo_crt *crt, void *p)
{
	static struct cca_public_sec static_cca_pub_sec = {
		.section_identifier = 4,
		.section_length = 0x000f,
		.exponent_len = 0x0003,
	};
	static char pk_exponent[3] = { 0x01, 0x00, 0x01 };
	struct {
		struct t6_keyblock_hdr t6_hdr;
		struct cca_token_hdr token;
		struct cca_pvt_ext_crt_sec pvt;
		char key_parts[];
	} __packed *key = p;
	struct cca_public_sec *pub;
	int short_len, long_len, pad_len, key_len, size;
	if (WARN_ON_ONCE(crt->inputdatalength > 512))
		return -EINVAL;
	memset(key, 0, sizeof(*key));
	short_len = (crt->inputdatalength + 1) / 2;
	long_len = short_len + 8;
	pad_len = -(3 * long_len + 2 * short_len) & 7;
	key_len = 3 * long_len + 2 * short_len + pad_len + crt->inputdatalength;
	size = sizeof(*key) + key_len + sizeof(*pub) + 3;
	key->t6_hdr.blen = size;
	key->t6_hdr.ulen = size - 2;
	key->token.token_identifier = CCA_TKN_HDR_ID_EXT;
	key->token.token_length = size - 6;
	key->pvt.section_identifier = CCA_PVT_EXT_CRT_SEC_ID_PVT;
	key->pvt.section_length = sizeof(key->pvt) + key_len;
	key->pvt.key_format = CCA_PVT_EXT_CRT_SEC_FMT_CL;
	key->pvt.key_use_flags[0] = CCA_PVT_USAGE_ALL;
	key->pvt.p_len = key->pvt.dp_len = key->pvt.u_len = long_len;
	key->pvt.q_len = key->pvt.dq_len = short_len;
	key->pvt.mod_len = crt->inputdatalength;
	key->pvt.pad_len = pad_len;
	if (copy_from_user(key->key_parts, crt->np_prime, long_len) ||
	    copy_from_user(key->key_parts + long_len,
			   crt->nq_prime, short_len) ||
	    copy_from_user(key->key_parts + long_len + short_len,
			   crt->bp_key, long_len) ||
	    copy_from_user(key->key_parts + 2 * long_len + short_len,
			   crt->bq_key, short_len) ||
	    copy_from_user(key->key_parts + 2 * long_len + 2 * short_len,
			   crt->u_mult_inv, long_len))
		return -EFAULT;
	memset(key->key_parts + 3 * long_len + 2 * short_len + pad_len,
	       0xff, crt->inputdatalength);
	pub = (struct cca_public_sec *)(key->key_parts + key_len);
	*pub = static_cca_pub_sec;
	pub->modulus_bit_len = 8 * crt->inputdatalength;
	memcpy((char *)(pub + 1), pk_exponent, 3);
	return size;
}
#endif  
