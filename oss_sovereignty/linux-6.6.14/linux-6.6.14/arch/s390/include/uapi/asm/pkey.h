#ifndef _UAPI_PKEY_H
#define _UAPI_PKEY_H
#include <linux/ioctl.h>
#include <linux/types.h>
#define PKEY_IOCTL_MAGIC 'p'
#define SECKEYBLOBSIZE	64	    
#define PROTKEYBLOBSIZE 80	 
#define MAXPROTKEYSIZE	64	 
#define MAXCLRKEYSIZE	32	    
#define MAXAESCIPHERKEYSIZE 136   
#define MINEP11AESKEYBLOBSIZE 256   
#define MAXEP11AESKEYBLOBSIZE 336   
#define MINKEYBLOBSIZE	SECKEYBLOBSIZE
#define PKEY_KEYTYPE_AES_128		1
#define PKEY_KEYTYPE_AES_192		2
#define PKEY_KEYTYPE_AES_256		3
#define PKEY_KEYTYPE_ECC		4
#define PKEY_KEYTYPE_ECC_P256		5
#define PKEY_KEYTYPE_ECC_P384		6
#define PKEY_KEYTYPE_ECC_P521		7
#define PKEY_KEYTYPE_ECC_ED25519	8
#define PKEY_KEYTYPE_ECC_ED448		9
enum pkey_key_type {
	PKEY_TYPE_CCA_DATA   = (__u32) 1,
	PKEY_TYPE_CCA_CIPHER = (__u32) 2,
	PKEY_TYPE_EP11	     = (__u32) 3,
	PKEY_TYPE_CCA_ECC    = (__u32) 0x1f,
	PKEY_TYPE_EP11_AES   = (__u32) 6,
	PKEY_TYPE_EP11_ECC   = (__u32) 7,
};
enum pkey_key_size {
	PKEY_SIZE_AES_128 = (__u32) 128,
	PKEY_SIZE_AES_192 = (__u32) 192,
	PKEY_SIZE_AES_256 = (__u32) 256,
	PKEY_SIZE_UNKNOWN = (__u32) 0xFFFFFFFF,
};
#define PKEY_FLAGS_MATCH_CUR_MKVP  0x00000002
#define PKEY_FLAGS_MATCH_ALT_MKVP  0x00000004
#define PKEY_KEYGEN_XPRT_SYM  0x00008000
#define PKEY_KEYGEN_XPRT_UASY 0x00004000
#define PKEY_KEYGEN_XPRT_AASY 0x00002000
#define PKEY_KEYGEN_XPRT_RAW  0x00001000
#define PKEY_KEYGEN_XPRT_CPAC 0x00000800
#define PKEY_KEYGEN_XPRT_DES  0x00000080
#define PKEY_KEYGEN_XPRT_AES  0x00000040
#define PKEY_KEYGEN_XPRT_RSA  0x00000008
struct pkey_apqn {
	__u16 card;
	__u16 domain;
};
struct pkey_seckey {
	__u8  seckey[SECKEYBLOBSIZE];		   
};
struct pkey_protkey {
	__u32 type;	  
	__u32 len;		 
	__u8  protkey[MAXPROTKEYSIZE];	        
};
struct pkey_clrkey {
	__u8  clrkey[MAXCLRKEYSIZE];  
};
struct ep11kblob_header {
	__u8  type;	 
	__u8  hver;	 
	__u16 len;	 
	__u8  version;	 
	__u8  res0;	 
	__u16 bitlen;	 
	__u8  res1[8];	 
} __packed;
struct pkey_genseck {
	__u16 cardnr;		     
	__u16 domain;		     
	__u32 keytype;		     
	struct pkey_seckey seckey;   
};
#define PKEY_GENSECK _IOWR(PKEY_IOCTL_MAGIC, 0x01, struct pkey_genseck)
struct pkey_clr2seck {
	__u16 cardnr;		     
	__u16 domain;		     
	__u32 keytype;		     
	struct pkey_clrkey clrkey;   
	struct pkey_seckey seckey;   
};
#define PKEY_CLR2SECK _IOWR(PKEY_IOCTL_MAGIC, 0x02, struct pkey_clr2seck)
struct pkey_sec2protk {
	__u16 cardnr;		      
	__u16 domain;		      
	struct pkey_seckey seckey;    
	struct pkey_protkey protkey;  
};
#define PKEY_SEC2PROTK _IOWR(PKEY_IOCTL_MAGIC, 0x03, struct pkey_sec2protk)
struct pkey_clr2protk {
	__u32 keytype;		      
	struct pkey_clrkey clrkey;    
	struct pkey_protkey protkey;  
};
#define PKEY_CLR2PROTK _IOWR(PKEY_IOCTL_MAGIC, 0x04, struct pkey_clr2protk)
struct pkey_findcard {
	struct pkey_seckey seckey;	        
	__u16  cardnr;			        
	__u16  domain;			        
};
#define PKEY_FINDCARD _IOWR(PKEY_IOCTL_MAGIC, 0x05, struct pkey_findcard)
struct pkey_skey2pkey {
	struct pkey_seckey seckey;    
	struct pkey_protkey protkey;  
};
#define PKEY_SKEY2PKEY _IOWR(PKEY_IOCTL_MAGIC, 0x06, struct pkey_skey2pkey)
struct pkey_verifykey {
	struct pkey_seckey seckey;	        
	__u16  cardnr;			        
	__u16  domain;			        
	__u16  keysize;			        
	__u32  attributes;		        
};
#define PKEY_VERIFYKEY _IOWR(PKEY_IOCTL_MAGIC, 0x07, struct pkey_verifykey)
#define PKEY_VERIFY_ATTR_AES	   0x00000001   
#define PKEY_VERIFY_ATTR_OLD_MKVP  0x00000100   
struct pkey_genprotk {
	__u32 keytype;			        
	struct pkey_protkey protkey;	        
};
#define PKEY_GENPROTK _IOWR(PKEY_IOCTL_MAGIC, 0x08, struct pkey_genprotk)
struct pkey_verifyprotk {
	struct pkey_protkey protkey;	 
};
#define PKEY_VERIFYPROTK _IOW(PKEY_IOCTL_MAGIC, 0x09, struct pkey_verifyprotk)
struct pkey_kblob2pkey {
	__u8 __user *key;		 
	__u32 keylen;			 
	struct pkey_protkey protkey;	 
};
#define PKEY_KBLOB2PROTK _IOWR(PKEY_IOCTL_MAGIC, 0x0A, struct pkey_kblob2pkey)
struct pkey_genseck2 {
	struct pkey_apqn __user *apqns;  
	__u32 apqn_entries;	     
	enum pkey_key_type type;     
	enum pkey_key_size size;     
	__u32 keygenflags;	     
	__u8 __user *key;	     
	__u32 keylen;		     
};
#define PKEY_GENSECK2 _IOWR(PKEY_IOCTL_MAGIC, 0x11, struct pkey_genseck2)
struct pkey_clr2seck2 {
	struct pkey_apqn __user *apqns;  
	__u32 apqn_entries;	     
	enum pkey_key_type type;     
	enum pkey_key_size size;     
	__u32 keygenflags;	     
	struct pkey_clrkey clrkey;   
	__u8 __user *key;	     
	__u32 keylen;		     
};
#define PKEY_CLR2SECK2 _IOWR(PKEY_IOCTL_MAGIC, 0x12, struct pkey_clr2seck2)
struct pkey_verifykey2 {
	__u8 __user *key;	     
	__u32 keylen;		     
	__u16 cardnr;		     
	__u16 domain;		     
	enum pkey_key_type type;     
	enum pkey_key_size size;     
	__u32 flags;		     
};
#define PKEY_VERIFYKEY2 _IOWR(PKEY_IOCTL_MAGIC, 0x17, struct pkey_verifykey2)
struct pkey_kblob2pkey2 {
	__u8 __user *key;	      
	__u32 keylen;		      
	struct pkey_apqn __user *apqns;  
	__u32 apqn_entries;	      
	struct pkey_protkey protkey;  
};
#define PKEY_KBLOB2PROTK2 _IOWR(PKEY_IOCTL_MAGIC, 0x1A, struct pkey_kblob2pkey2)
struct pkey_apqns4key {
	__u8 __user *key;	    
	__u32 keylen;		    
	__u32 flags;		    
	struct pkey_apqn __user *apqns;  
	__u32 apqn_entries;	    
};
#define PKEY_APQNS4K _IOWR(PKEY_IOCTL_MAGIC, 0x1B, struct pkey_apqns4key)
struct pkey_apqns4keytype {
	enum pkey_key_type type;    
	__u8  cur_mkvp[32];	    
	__u8  alt_mkvp[32];	    
	__u32 flags;		    
	struct pkey_apqn __user *apqns;  
	__u32 apqn_entries;	    
};
#define PKEY_APQNS4KT _IOWR(PKEY_IOCTL_MAGIC, 0x1C, struct pkey_apqns4keytype)
struct pkey_kblob2pkey3 {
	__u8 __user *key;	      
	__u32 keylen;		      
	struct pkey_apqn __user *apqns;  
	__u32 apqn_entries;	      
	__u32 pkeytype;		 
	__u32 pkeylen;	  
	__u8 __user *pkey;		  
};
#define PKEY_KBLOB2PROTK3 _IOWR(PKEY_IOCTL_MAGIC, 0x1D, struct pkey_kblob2pkey3)
#endif  
