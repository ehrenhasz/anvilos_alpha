#ifndef _ZCRYPT_CCAMISC_H_
#define _ZCRYPT_CCAMISC_H_
#include <asm/zcrypt.h>
#include <asm/pkey.h>
#define TOKTYPE_NON_CCA		 0x00  
#define TOKTYPE_CCA_INTERNAL	 0x01  
#define TOKTYPE_CCA_INTERNAL_PKA 0x1f  
#define TOKVER_PROTECTED_KEY	0x01  
#define TOKVER_CLEAR_KEY	0x02  
#define TOKVER_CCA_AES		0x04  
#define TOKVER_CCA_VLSC		0x05  
#define MAXCCAVLSCTOKENSIZE 725
struct keytoken_header {
	u8  type;      
	u8  res0[1];
	u16 len;       
	u8  version;   
	u8  res1[3];
} __packed;
struct secaeskeytoken {
	u8  type;      
	u8  res0[3];
	u8  version;   
	u8  res1[1];
	u8  flag;      
	u8  res2[1];
	u64 mkvp;      
	u8  key[32];   
	u8  cv[8];     
	u16 bitsize;   
	u16 keysize;   
	u8  tvv[4];    
} __packed;
struct cipherkeytoken {
	u8  type;      
	u8  res0[1];
	u16 len;       
	u8  version;   
	u8  res1[3];
	u8  kms;       
	u8  kvpt;      
	u64 mkvp0;     
	u64 mkvp1;     
	u8  eskwm;     
	u8  hashalg;   
	u8  plfver;    
	u8  res2[1];
	u8  adsver;    
	u8  res3[1];
	u16 adslen;    
	u8  kllen;     
	u8  ieaslen;   
	u8  uadlen;    
	u8  res4[1];
	u16 wpllen;    
	u8  res5[1];
	u8  algtype;   
	u16 keytype;   
	u8  kufc;      
	u16 kuf1;      
	u16 kuf2;      
	u8  kmfc;      
	u16 kmf1;      
	u16 kmf2;      
	u16 kmf3;      
	u8  vdata[];  
} __packed;
struct eccprivkeytoken {
	u8  type;      
	u8  version;   
	u16 len;       
	u8  res1[4];
	u8  secid;     
	u8  secver;    
	u16 seclen;    
	u8  wtype;     
	u8  htype;     
	u8  res2[2];
	u8  kutc;      
	u8  ctype;     
	u8  kfs;       
	u8  ksrc;      
	u16 pbitlen;   
	u16 ibmadlen;  
	u64 mkvp;      
	u8  opk[48];   
	u16 adatalen;  
	u16 fseclen;   
	u8  more_data[];  
} __packed;
#define KMF1_XPRT_SYM  0x8000
#define KMF1_XPRT_UASY 0x4000
#define KMF1_XPRT_AASY 0x2000
#define KMF1_XPRT_RAW  0x1000
#define KMF1_XPRT_CPAC 0x0800
#define KMF1_XPRT_DES  0x0080
#define KMF1_XPRT_AES  0x0040
#define KMF1_XPRT_RSA  0x0008
int cca_check_secaeskeytoken(debug_info_t *dbg, int dbflvl,
			     const u8 *token, int keybitsize);
int cca_check_secaescipherkey(debug_info_t *dbg, int dbflvl,
			      const u8 *token, int keybitsize,
			      int checkcpacfexport);
int cca_check_sececckeytoken(debug_info_t *dbg, int dbflvl,
			     const u8 *token, size_t keysize,
			     int checkcpacfexport);
int cca_genseckey(u16 cardnr, u16 domain, u32 keybitsize, u8 *seckey);
int cca_clr2seckey(u16 cardnr, u16 domain, u32 keybitsize,
		   const u8 *clrkey, u8 *seckey);
int cca_sec2protkey(u16 cardnr, u16 domain,
		    const u8 *seckey, u8 *protkey, u32 *protkeylen,
		    u32 *protkeytype);
int cca_gencipherkey(u16 cardnr, u16 domain, u32 keybitsize, u32 keygenflags,
		     u8 *keybuf, size_t *keybufsize);
int cca_cipher2protkey(u16 cardnr, u16 domain, const u8 *ckey,
		       u8 *protkey, u32 *protkeylen, u32 *protkeytype);
int cca_clr2cipherkey(u16 cardnr, u16 domain, u32 keybitsize, u32 keygenflags,
		      const u8 *clrkey, u8 *keybuf, size_t *keybufsize);
int cca_ecc2protkey(u16 cardnr, u16 domain, const u8 *key,
		    u8 *protkey, u32 *protkeylen, u32 *protkeytype);
int cca_query_crypto_facility(u16 cardnr, u16 domain,
			      const char *keyword,
			      u8 *rarray, size_t *rarraylen,
			      u8 *varray, size_t *varraylen);
int cca_findcard(const u8 *key, u16 *pcardnr, u16 *pdomain, int verify);
int cca_findcard2(u32 **apqns, u32 *nr_apqns, u16 cardnr, u16 domain,
		  int minhwtype, int mktype, u64 cur_mkvp, u64 old_mkvp,
		  int verify);
#define AES_MK_SET  0
#define APKA_MK_SET 1
struct cca_info {
	int  hwtype;		 
	char new_aes_mk_state;	 
	char cur_aes_mk_state;	 
	char old_aes_mk_state;	 
	char new_apka_mk_state;  
	char cur_apka_mk_state;  
	char old_apka_mk_state;  
	char new_asym_mk_state;	 
	char cur_asym_mk_state;	 
	char old_asym_mk_state;	 
	u64  new_aes_mkvp;	 
	u64  cur_aes_mkvp;	 
	u64  old_aes_mkvp;	 
	u64  new_apka_mkvp;	 
	u64  cur_apka_mkvp;	 
	u64  old_apka_mkvp;	 
	u8   new_asym_mkvp[16];	 
	u8   cur_asym_mkvp[16];	 
	u8   old_asym_mkvp[16];	 
	char serial[9];		 
};
int cca_get_info(u16 card, u16 dom, struct cca_info *ci, int verify);
void zcrypt_ccamisc_exit(void);
#endif  
