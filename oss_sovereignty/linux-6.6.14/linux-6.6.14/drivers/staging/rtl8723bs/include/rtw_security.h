#ifndef __RTW_SECURITY_H_
#define __RTW_SECURITY_H_
#include <crypto/arc4.h>
#define _NO_PRIVACY_		0x0
#define _WEP40_				0x1
#define _TKIP_				0x2
#define _TKIP_WTMIC_		0x3
#define _AES_				0x4
#define _WEP104_			0x5
#define _WEP_WPA_MIXED_	0x07   
#define _SMS4_				0x06
#define _BIP_				0x8
#define is_wep_enc(alg) (((alg) == _WEP40_) || ((alg) == _WEP104_))
const char *security_type_str(u8 value);
#define SHA256_MAC_LEN 32
#define AES_BLOCK_SIZE 16
#define AES_PRIV_SIZE (4 * 44)
#define RTW_KEK_LEN 16
#define RTW_KCK_LEN 16
#define RTW_REPLAY_CTR_LEN 8
enum {
	ENCRYP_PROTOCOL_OPENSYS,    
	ENCRYP_PROTOCOL_WEP,        
	ENCRYP_PROTOCOL_WPA,        
	ENCRYP_PROTOCOL_WPA2,       
	ENCRYP_PROTOCOL_WAPI,       
	ENCRYP_PROTOCOL_MAX
};
#ifndef Ndis802_11AuthModeWPA2
#define Ndis802_11AuthModeWPA2 (Ndis802_11AuthModeWPANone + 1)
#endif
#ifndef Ndis802_11AuthModeWPA2PSK
#define Ndis802_11AuthModeWPA2PSK (Ndis802_11AuthModeWPANone + 2)
#endif
union pn48	{
	u64	val;
#ifdef __LITTLE_ENDIAN
struct {
  u8 TSC0;
  u8 TSC1;
  u8 TSC2;
  u8 TSC3;
  u8 TSC4;
  u8 TSC5;
  u8 TSC6;
  u8 TSC7;
} _byte_;
#else
struct {
  u8 TSC7;
  u8 TSC6;
  u8 TSC5;
  u8 TSC4;
  u8 TSC3;
  u8 TSC2;
  u8 TSC1;
  u8 TSC0;
} _byte_;
#endif
};
union Keytype {
        u8   skey[16];
        u32    lkey[4];
};
struct rt_pmkid_list {
	u8 				bUsed;
	u8 				Bssid[6];
	u8 				PMKID[16];
	u8 				SsidBuf[33];
	u8 *ssid_octet;
	u16 					ssid_length;
};
struct security_priv {
	u32   dot11AuthAlgrthm;		 
	u32   dot11PrivacyAlgrthm;	 
	u32   dot11PrivacyKeyIndex;	 
	union Keytype dot11DefKey[4];	 
	u32 dot11DefKeylen[4];
	u8 key_mask;  
	u32 dot118021XGrpPrivacy;	 
	u32 dot118021XGrpKeyid;		 
	union Keytype	dot118021XGrpKey[BIP_MAX_KEYID + 1];	 
	union Keytype	dot118021XGrptxmickey[BIP_MAX_KEYID + 1];
	union Keytype	dot118021XGrprxmickey[BIP_MAX_KEYID + 1];
	union pn48		dot11Grptxpn;			 
	union pn48		dot11Grprxpn;			 
	u32 dot11wBIPKeyid;						 
	union Keytype	dot11wBIPKey[BIP_MAX_KEYID + 1];	 
	union pn48		dot11wBIPtxpn;			 
	union pn48		dot11wBIPrxpn;			 
	unsigned int dot8021xalg; 
	unsigned int wpa_psk; 
	unsigned int wpa_group_cipher;
	unsigned int wpa2_group_cipher;
	unsigned int wpa_pairwise_cipher;
	unsigned int wpa2_pairwise_cipher;
	u8 wps_ie[MAX_WPS_IE_LEN]; 
	int wps_ie_len;
	struct arc4_ctx xmit_arc4_ctx;
	struct arc4_ctx recv_arc4_ctx;
	u8 binstallGrpkey;
	u8 binstallBIPkey;
	u8 busetkipkey;
	u8 bcheck_grpkey;
	u8 bgrpkey_handshake;
	s32	sw_encrypt; 
	s32	sw_decrypt; 
	s32	hw_decrypted; 
	u32 ndisauthtype;	 
	u32 ndisencryptstatus;	 
	struct wlan_bssid_ex sec_bss;   
	struct ndis_802_11_wep ndiswep;
	u8 assoc_info[600];
	u8 szofcapability[256];  
	u8 oidassociation[512];  
	u8 authenticator_ie[256];   
	u8 supplicant_ie[256];   
	unsigned long last_mic_err_time;
	u8 btkip_countermeasure;
	u8 btkip_wait_report;
	u32 btkip_countermeasure_time;
	struct rt_pmkid_list		PMKIDList[NUM_PMKID_CACHE];	 
	u8 		PMKIDIndex;
	u8 bWepDefaultKeyIdxSet;
};
#define GET_ENCRY_ALGO(psecuritypriv, psta, encry_algo, bmcst)\
do {\
	switch (psecuritypriv->dot11AuthAlgrthm)\
	{\
		case dot11AuthAlgrthm_Open:\
		case dot11AuthAlgrthm_Shared:\
		case dot11AuthAlgrthm_Auto:\
			encry_algo = (u8)psecuritypriv->dot11PrivacyAlgrthm;\
			break;\
		case dot11AuthAlgrthm_8021X:\
			if (bmcst)\
				encry_algo = (u8)psecuritypriv->dot118021XGrpPrivacy;\
			else\
				encry_algo = (u8)psta->dot118021XPrivacy;\
			break;\
	     case dot11AuthAlgrthm_WAPI:\
		     encry_algo = (u8)psecuritypriv->dot11PrivacyAlgrthm;\
		     break;\
	} \
} while (0)
#define SET_ICE_IV_LEN(iv_len, icv_len, encrypt)\
do {\
	switch (encrypt)\
	{\
		case _WEP40_:\
		case _WEP104_:\
			iv_len = 4;\
			icv_len = 4;\
			break;\
		case _TKIP_:\
			iv_len = 8;\
			icv_len = 4;\
			break;\
		case _AES_:\
			iv_len = 8;\
			icv_len = 8;\
			break;\
		case _SMS4_:\
			iv_len = 18;\
			icv_len = 16;\
			break;\
		default:\
			iv_len = 0;\
			icv_len = 0;\
			break;\
	} \
} while (0)
#define GET_TKIP_PN(iv, dot11txpn)\
do {\
	dot11txpn._byte_.TSC0 = iv[2];\
	dot11txpn._byte_.TSC1 = iv[0];\
	dot11txpn._byte_.TSC2 = iv[4];\
	dot11txpn._byte_.TSC3 = iv[5];\
	dot11txpn._byte_.TSC4 = iv[6];\
	dot11txpn._byte_.TSC5 = iv[7];\
} while (0)
#define ROL32(A, n)	(((A) << (n)) | (((A)>>(32-(n)))  & ((1UL << (n)) - 1)))
#define ROR32(A, n)	ROL32((A), 32-(n))
struct mic_data {
	u32  K0, K1;          
	u32  L, R;            
	u32  M;               
	u32     nBytesInM;       
};
int omac1_aes_128(u8 *key, u8 *data, size_t data_len, u8 *mac);
void rtw_secmicsetkey(struct mic_data *pmicdata, u8 *key);
void rtw_secmicappendbyte(struct mic_data *pmicdata, u8 b);
void rtw_secmicappend(struct mic_data *pmicdata, u8 *src, u32 nBytes);
void rtw_secgetmic(struct mic_data *pmicdata, u8 *dst);
void rtw_seccalctkipmic(
	u8 *key,
	u8 *header,
	u8 *data,
	u32 data_len,
	u8 *Miccode,
	u8   priority);
u32 rtw_aes_encrypt(struct adapter *padapter, u8 *pxmitframe);
u32 rtw_tkip_encrypt(struct adapter *padapter, u8 *pxmitframe);
void rtw_wep_encrypt(struct adapter *padapter, u8  *pxmitframe);
u32 rtw_aes_decrypt(struct adapter *padapter, u8  *precvframe);
u32 rtw_tkip_decrypt(struct adapter *padapter, u8  *precvframe);
void rtw_wep_decrypt(struct adapter *padapter, u8  *precvframe);
u32 rtw_BIP_verify(struct adapter *padapter, u8 *precvframe);
void rtw_sec_restore_wep_key(struct adapter *adapter);
u8 rtw_handle_tkip_countermeasure(struct adapter *adapter, const char *caller);
#endif	 
