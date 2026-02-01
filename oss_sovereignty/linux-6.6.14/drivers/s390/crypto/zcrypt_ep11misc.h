 
 

#ifndef _ZCRYPT_EP11MISC_H_
#define _ZCRYPT_EP11MISC_H_

#include <asm/zcrypt.h>
#include <asm/pkey.h>

#define EP11_API_V1 1   
#define EP11_API_V4 4   
#define EP11_API_V6 6   
#define EP11_STRUCT_MAGIC 0x1234
#define EP11_BLOB_PKEY_EXTRACTABLE 0x00200000

 
#define TOKVER_EP11_AES  0x03   
#define TOKVER_EP11_AES_WITH_HEADER 0x06  
#define TOKVER_EP11_ECC_WITH_HEADER 0x07  

 
struct ep11keyblob {
	union {
		u8 session[32];
		 
		struct ep11kblob_header head;
	};
	u8  wkvp[16];   
	u64 attr;       
	u64 mode;       
	u16 version;    
	u8  iv[14];
	u8  encrypted_key_data[144];
	u8  mac[32];
} __packed;

 
static inline bool is_ep11_keyblob(const u8 *key)
{
	struct ep11keyblob *kb = (struct ep11keyblob *)key;

	return (kb->version == EP11_STRUCT_MAGIC);
}

 
const u8 *ep11_kb_wkvp(const u8 *kblob, size_t kbloblen);

 
int ep11_check_aes_key_with_hdr(debug_info_t *dbg, int dbflvl,
				const u8 *key, size_t keylen, int checkcpacfexp);

 
int ep11_check_ecc_key_with_hdr(debug_info_t *dbg, int dbflvl,
				const u8 *key, size_t keylen, int checkcpacfexp);

 
int ep11_check_aes_key(debug_info_t *dbg, int dbflvl,
		       const u8 *key, size_t keylen, int checkcpacfexp);

 
struct ep11_card_info {
	u32  API_ord_nr;     
	u16  FW_version;     
	char serial[16];     
	u64  op_mode;	     
};

 
struct ep11_domain_info {
	char cur_wk_state;   
	char new_wk_state;   
	u8   cur_wkvp[32];   
	u8   new_wkvp[32];   
	u64  op_mode;	     
};

 
int ep11_get_card_info(u16 card, struct ep11_card_info *info, int verify);

 
int ep11_get_domain_info(u16 card, u16 domain, struct ep11_domain_info *info);

 
int ep11_genaeskey(u16 card, u16 domain, u32 keybitsize, u32 keygenflags,
		   u8 *keybuf, size_t *keybufsize, u32 keybufver);

 
int ep11_clr2keyblob(u16 cardnr, u16 domain, u32 keybitsize, u32 keygenflags,
		     const u8 *clrkey, u8 *keybuf, size_t *keybufsize,
		     u32 keytype);

 
int ep11_findcard2(u32 **apqns, u32 *nr_apqns, u16 cardnr, u16 domain,
		   int minhwtype, int minapi, const u8 *wkvp);

 
int ep11_kblob2protkey(u16 card, u16 dom, const u8 *key, size_t keylen,
		       u8 *protkey, u32 *protkeylen, u32 *protkeytype);

void zcrypt_ep11misc_exit(void);

#endif  
