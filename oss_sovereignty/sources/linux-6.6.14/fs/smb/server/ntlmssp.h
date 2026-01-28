


#ifndef __KSMBD_NTLMSSP_H
#define __KSMBD_NTLMSSP_H

#define NTLMSSP_SIGNATURE "NTLMSSP"


#define TGT_Name        "KSMBD"


#define CIFS_CRYPTO_KEY_SIZE	(8)
#define CIFS_KEY_SIZE	(40)


#define CIFS_ENCPWD_SIZE	(16)
#define CIFS_CPHTXT_SIZE	(16)


#define NtLmNegotiate     cpu_to_le32(1)
#define NtLmChallenge     cpu_to_le32(2)
#define NtLmAuthenticate  cpu_to_le32(3)
#define UnknownMessage    cpu_to_le32(8)


#define NTLMSSP_NEGOTIATE_UNICODE         0x01 
#define NTLMSSP_NEGOTIATE_OEM             0x02 
#define NTLMSSP_REQUEST_TARGET            0x04 

#define NTLMSSP_NEGOTIATE_SIGN          0x0010 
#define NTLMSSP_NEGOTIATE_SEAL          0x0020 
#define NTLMSSP_NEGOTIATE_DGRAM         0x0040
#define NTLMSSP_NEGOTIATE_LM_KEY        0x0080 

#define NTLMSSP_NEGOTIATE_NTLM          0x0200 
#define NTLMSSP_NEGOTIATE_NT_ONLY       0x0400 
#define NTLMSSP_ANONYMOUS               0x0800
#define NTLMSSP_NEGOTIATE_DOMAIN_SUPPLIED 0x1000 
#define NTLMSSP_NEGOTIATE_WORKSTATION_SUPPLIED 0x2000
#define NTLMSSP_NEGOTIATE_LOCAL_CALL    0x4000 
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN   0x8000 
#define NTLMSSP_TARGET_TYPE_DOMAIN     0x10000
#define NTLMSSP_TARGET_TYPE_SERVER     0x20000
#define NTLMSSP_TARGET_TYPE_SHARE      0x40000
#define NTLMSSP_NEGOTIATE_EXTENDED_SEC 0x80000 

#define NTLMSSP_NEGOTIATE_IDENTIFY    0x100000
#define NTLMSSP_REQUEST_ACCEPT_RESP   0x200000 
#define NTLMSSP_REQUEST_NON_NT_KEY    0x400000
#define NTLMSSP_NEGOTIATE_TARGET_INFO 0x800000

#define NTLMSSP_NEGOTIATE_VERSION    0x2000000 



#define NTLMSSP_NEGOTIATE_128       0x20000000
#define NTLMSSP_NEGOTIATE_KEY_XCH   0x40000000
#define NTLMSSP_NEGOTIATE_56        0x80000000


enum av_field_type {
	NTLMSSP_AV_EOL = 0,
	NTLMSSP_AV_NB_COMPUTER_NAME,
	NTLMSSP_AV_NB_DOMAIN_NAME,
	NTLMSSP_AV_DNS_COMPUTER_NAME,
	NTLMSSP_AV_DNS_DOMAIN_NAME,
	NTLMSSP_AV_DNS_TREE_NAME,
	NTLMSSP_AV_FLAGS,
	NTLMSSP_AV_TIMESTAMP,
	NTLMSSP_AV_RESTRICTION,
	NTLMSSP_AV_TARGET_NAME,
	NTLMSSP_AV_CHANNEL_BINDINGS
};







struct security_buffer {
	__le16 Length;
	__le16 MaximumLength;
	__le32 BufferOffset;	
} __packed;

struct target_info {
	__le16 Type;
	__le16 Length;
	__u8 Content[];
} __packed;

struct negotiate_message {
	__u8 Signature[sizeof(NTLMSSP_SIGNATURE)];
	__le32 MessageType;     
	__le32 NegotiateFlags;
	struct security_buffer DomainName;	
	struct security_buffer WorkstationName;	
	
	char DomainString[];
	
} __packed;

struct challenge_message {
	__u8 Signature[sizeof(NTLMSSP_SIGNATURE)];
	__le32 MessageType;   
	struct security_buffer TargetName;
	__le32 NegotiateFlags;
	__u8 Challenge[CIFS_CRYPTO_KEY_SIZE];
	__u8 Reserved[8];
	struct security_buffer TargetInfoArray;
	
} __packed;

struct authenticate_message {
	__u8 Signature[sizeof(NTLMSSP_SIGNATURE)];
	__le32 MessageType;  
	struct security_buffer LmChallengeResponse;
	struct security_buffer NtChallengeResponse;
	struct security_buffer DomainName;
	struct security_buffer UserName;
	struct security_buffer WorkstationName;
	struct security_buffer SessionKey;
	__le32 NegotiateFlags;
	
	char UserString[];
} __packed;

struct ntlmv2_resp {
	char ntlmv2_hash[CIFS_ENCPWD_SIZE];
	__le32 blob_signature;
	__u32  reserved;
	__le64  time;
	__u64  client_chal; 
	__u32  reserved2;
	
} __packed;


struct ntlmssp_auth {
	
	bool		sesskey_per_smbsess;
	
	__u32		client_flags;
	
	__u32		conn_flags;
	
	unsigned char	ciphertext[CIFS_CPHTXT_SIZE];
	
	char		cryptkey[CIFS_CRYPTO_KEY_SIZE];
};
#endif 
