#ifndef _SPUM_H_
#define _SPUM_H_
#define SPU_CRYPTO_OPERATION_GENERIC	0x1
#define SPU_TX_STATUS_LEN  4
#define SPU_STATUS_MASK                 0x0000FF00
#define SPU_STATUS_SUCCESS              0x00000000
#define SPU_STATUS_INVALID_ICV          0x00000100
#define SPU_STATUS_ERROR_FLAG           0x00020000
#define SPU_REQ_FIXED_LEN 24
#define SPU_HEADER_ALLOC_LEN  (SPU_REQ_FIXED_LEN + MAX_KEY_SIZE + \
				MAX_KEY_SIZE + MAX_IV_SIZE)
#define SPU_RESP_HDR_LEN 12
#define SPU_HASH_RESP_HDR_LEN 8
#define SPUM_NS2_MAX_PAYLOAD  (BIT(16) - 1)
#define SPUM_NSP_MAX_PAYLOAD	8192
struct BDESC_HEADER {
	__be16 offset_mac;		 
	__be16 length_mac;		 
	__be16 offset_crypto;		 
	__be16 length_crypto;		 
	__be16 offset_icv;		 
	__be16 offset_iv;		 
};
struct BD_HEADER {
	__be16 size;
	__be16 prev_length;
};
struct MHEADER {
	u8 flags;	 
	u8 op_code;	 
	u16 reserved;	 
};
#define MH_SUPDT_PRES   BIT(0)
#define MH_HASH_PRES    BIT(2)
#define MH_BD_PRES      BIT(3)
#define MH_MFM_PRES     BIT(4)
#define MH_BDESC_PRES   BIT(5)
#define MH_SCTX_PRES	BIT(7)
#define SCTX_SIZE               0x000000FF
#define  UPDT_OFST              0x000000FF    
#define  HASH_TYPE              0x00000300    
#define  HASH_TYPE_SHIFT                 8
#define  HASH_MODE              0x00001C00    
#define  HASH_MODE_SHIFT                10
#define  HASH_ALG               0x0000E000    
#define  HASH_ALG_SHIFT                 13
#define  CIPHER_TYPE            0x00030000    
#define  CIPHER_TYPE_SHIFT              16
#define  CIPHER_MODE            0x001C0000    
#define  CIPHER_MODE_SHIFT              18
#define  CIPHER_ALG             0x00E00000    
#define  CIPHER_ALG_SHIFT               21
#define  ICV_IS_512                BIT(27)
#define  ICV_IS_512_SHIFT		27
#define  CIPHER_ORDER               BIT(30)
#define  CIPHER_ORDER_SHIFT             30
#define  CIPHER_INBOUND             BIT(31)
#define  CIPHER_INBOUND_SHIFT           31
#define  EXP_IV_SIZE                   0x7
#define  IV_OFFSET                   BIT(3)
#define  IV_OFFSET_SHIFT                 3
#define  GEN_IV                      BIT(5)
#define  GEN_IV_SHIFT                    5
#define  EXPLICIT_IV                 BIT(6)
#define  EXPLICIT_IV_SHIFT               6
#define  SCTX_IV                     BIT(7)
#define  SCTX_IV_SHIFT                   7
#define  ICV_SIZE                   0x0F00
#define  ICV_SIZE_SHIFT                  8
#define  CHECK_ICV                  BIT(12)
#define  CHECK_ICV_SHIFT                12
#define  INSERT_ICV                 BIT(13)
#define  INSERT_ICV_SHIFT               13
#define  BD_SUPPRESS                BIT(19)
#define  BD_SUPPRESS_SHIFT              19
struct SCTX {
	__be32 proto_flags;
	__be32 cipher_flags;
	__be32 ecf;
};
struct SPUHEADER {
	struct MHEADER mh;
	u32 emh;
	struct SCTX sa;
};
#endif  
