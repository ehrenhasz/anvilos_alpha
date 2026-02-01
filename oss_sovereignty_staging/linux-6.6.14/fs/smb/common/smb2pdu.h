 
#ifndef _COMMON_SMB2PDU_H
#define _COMMON_SMB2PDU_H

 

 
#define SMB2_NEGOTIATE_HE	0x0000
#define SMB2_SESSION_SETUP_HE	0x0001
#define SMB2_LOGOFF_HE		0x0002  
#define SMB2_TREE_CONNECT_HE	0x0003
#define SMB2_TREE_DISCONNECT_HE	0x0004  
#define SMB2_CREATE_HE		0x0005
#define SMB2_CLOSE_HE		0x0006
#define SMB2_FLUSH_HE		0x0007  
#define SMB2_READ_HE		0x0008
#define SMB2_WRITE_HE		0x0009
#define SMB2_LOCK_HE		0x000A
#define SMB2_IOCTL_HE		0x000B
#define SMB2_CANCEL_HE		0x000C
#define SMB2_ECHO_HE		0x000D
#define SMB2_QUERY_DIRECTORY_HE	0x000E
#define SMB2_CHANGE_NOTIFY_HE	0x000F
#define SMB2_QUERY_INFO_HE	0x0010
#define SMB2_SET_INFO_HE	0x0011
#define SMB2_OPLOCK_BREAK_HE	0x0012

 
#define SMB2_NEGOTIATE		cpu_to_le16(SMB2_NEGOTIATE_HE)
#define SMB2_SESSION_SETUP	cpu_to_le16(SMB2_SESSION_SETUP_HE)
#define SMB2_LOGOFF		cpu_to_le16(SMB2_LOGOFF_HE)
#define SMB2_TREE_CONNECT	cpu_to_le16(SMB2_TREE_CONNECT_HE)
#define SMB2_TREE_DISCONNECT	cpu_to_le16(SMB2_TREE_DISCONNECT_HE)
#define SMB2_CREATE		cpu_to_le16(SMB2_CREATE_HE)
#define SMB2_CLOSE		cpu_to_le16(SMB2_CLOSE_HE)
#define SMB2_FLUSH		cpu_to_le16(SMB2_FLUSH_HE)
#define SMB2_READ		cpu_to_le16(SMB2_READ_HE)
#define SMB2_WRITE		cpu_to_le16(SMB2_WRITE_HE)
#define SMB2_LOCK		cpu_to_le16(SMB2_LOCK_HE)
#define SMB2_IOCTL		cpu_to_le16(SMB2_IOCTL_HE)
#define SMB2_CANCEL		cpu_to_le16(SMB2_CANCEL_HE)
#define SMB2_ECHO		cpu_to_le16(SMB2_ECHO_HE)
#define SMB2_QUERY_DIRECTORY	cpu_to_le16(SMB2_QUERY_DIRECTORY_HE)
#define SMB2_CHANGE_NOTIFY	cpu_to_le16(SMB2_CHANGE_NOTIFY_HE)
#define SMB2_QUERY_INFO		cpu_to_le16(SMB2_QUERY_INFO_HE)
#define SMB2_SET_INFO		cpu_to_le16(SMB2_SET_INFO_HE)
#define SMB2_OPLOCK_BREAK	cpu_to_le16(SMB2_OPLOCK_BREAK_HE)

#define SMB2_INTERNAL_CMD	cpu_to_le16(0xFFFF)

#define NUMBER_OF_SMB2_COMMANDS	0x0013

 
#define SMB2_NTLMV2_SESSKEY_SIZE	16
#define SMB2_SIGNATURE_SIZE		16
#define SMB2_HMACSHA256_SIZE		32
#define SMB2_CMACAES_SIZE		16
#define SMB3_GCM128_CRYPTKEY_SIZE	16
#define SMB3_GCM256_CRYPTKEY_SIZE	32

 
#define SMB3_ENC_DEC_KEY_SIZE		32

 
#define SMB3_SIGN_KEY_SIZE		16

#define CIFS_CLIENT_CHALLENGE_SIZE	8

 
#define SMB2_MAX_BUFFER_SIZE 65536

 
#define SMB3_DEFAULT_IOSIZE (4 * 1024 * 1024)

 

#define __SMB2_HEADER_STRUCTURE_SIZE	64
#define SMB2_HEADER_STRUCTURE_SIZE				\
	cpu_to_le16(__SMB2_HEADER_STRUCTURE_SIZE)

#define SMB2_PROTO_NUMBER cpu_to_le32(0x424d53fe)
#define SMB2_TRANSFORM_PROTO_NUM cpu_to_le32(0x424d53fd)
#define SMB2_COMPRESSION_TRANSFORM_ID cpu_to_le32(0x424d53fc)

 
#define SMB2_FLAGS_SERVER_TO_REDIR	cpu_to_le32(0x00000001)
#define SMB2_FLAGS_ASYNC_COMMAND	cpu_to_le32(0x00000002)
#define SMB2_FLAGS_RELATED_OPERATIONS	cpu_to_le32(0x00000004)
#define SMB2_FLAGS_SIGNED		cpu_to_le32(0x00000008)
#define SMB2_FLAGS_PRIORITY_MASK	cpu_to_le32(0x00000070)  
#define SMB2_FLAGS_DFS_OPERATIONS	cpu_to_le32(0x10000000)
#define SMB2_FLAGS_REPLAY_OPERATION	cpu_to_le32(0x20000000)  

 

 
struct smb2_hdr {
	__le32 ProtocolId;	 
	__le16 StructureSize;	 
	__le16 CreditCharge;	 
	__le32 Status;		 
	__le16 Command;
	__le16 CreditRequest;	 
	__le32 Flags;
	__le32 NextCommand;
	__le64 MessageId;
	union {
		struct {
			__le32 ProcessId;
			__le32  TreeId;
		} __packed SyncId;
		__le64  AsyncId;
	} __packed Id;
	__le64  SessionId;
	__u8   Signature[16];
} __packed;

struct smb3_hdr_req {
	__le32 ProtocolId;	 
	__le16 StructureSize;	 
	__le16 CreditCharge;	 
	__le16 ChannelSequence;  
	__le16 Reserved;
	__le16 Command;
	__le16 CreditRequest;	 
	__le32 Flags;
	__le32 NextCommand;
	__le64 MessageId;
	union {
		struct {
			__le32 ProcessId;
			__le32  TreeId;
		} __packed SyncId;
		__le64  AsyncId;
	} __packed Id;
	__le64  SessionId;
	__u8   Signature[16];
} __packed;

struct smb2_pdu {
	struct smb2_hdr hdr;
	__le16 StructureSize2;  
} __packed;

#define SMB2_ERROR_STRUCTURE_SIZE2	9
#define SMB2_ERROR_STRUCTURE_SIZE2_LE	cpu_to_le16(SMB2_ERROR_STRUCTURE_SIZE2)

struct smb2_err_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;
	__u8   ErrorContextCount;
	__u8   Reserved;
	__le32 ByteCount;   
	__u8   ErrorData[];   
} __packed;

#define SMB3_AES_CCM_NONCE 11
#define SMB3_AES_GCM_NONCE 12

 
#define TRANSFORM_FLAG_ENCRYPTED	0x0001
struct smb2_transform_hdr {
	__le32 ProtocolId;	 
	__u8   Signature[16];
	__u8   Nonce[16];
	__le32 OriginalMessageSize;
	__u16  Reserved1;
	__le16 Flags;  
	__le64  SessionId;
} __packed;


 
struct smb2_compression_transform_hdr_unchained {
	__le32 ProtocolId;	 
	__le32 OriginalCompressedSegmentSize;
	__le16 CompressionAlgorithm;
	__le16 Flags;
	__le16 Length;  
} __packed;

 
#define SMB2_COMPRESSION_FLAG_NONE	0x0000
#define SMB2_COMPRESSION_FLAG_CHAINED	0x0001

struct compression_payload_header {
	__le16	CompressionAlgorithm;
	__le16	Flags;
	__le32	Length;  
	   
} __packed;

 
struct smb2_compression_transform_hdr_chained {
	__le32 ProtocolId;	 
	__le32 OriginalCompressedSegmentSize;
	 
} __packed;

 
struct compression_pattern_payload_v1 {
	__le16	Pattern;
	__le16	Reserved1;
	__le16	Reserved2;
	__le32	Repetitions;
} __packed;

 
 
#define SMB2_RESERVED_TREE_CONNECT_CONTEXT_ID 0x0000
#define SMB2_REMOTED_IDENTITY_TREE_CONNECT_CONTEXT_ID cpu_to_le16(0x0001)

struct tree_connect_contexts {
	__le16 ContextType;
	__le16 DataLength;
	__le32 Reserved;
	__u8   Data[];
} __packed;

 
struct smb3_blob_data {
	__le16 BlobSize;
	__u8   BlobData[];
} __packed;

 
#define SE_GROUP_MANDATORY		0x00000001
#define SE_GROUP_ENABLED_BY_DEFAULT	0x00000002
#define SE_GROUP_ENABLED		0x00000004
#define SE_GROUP_OWNER			0x00000008
#define SE_GROUP_USE_FOR_DENY_ONLY	0x00000010
#define SE_GROUP_INTEGRITY		0x00000020
#define SE_GROUP_INTEGRITY_ENABLED	0x00000040
#define SE_GROUP_RESOURCE		0x20000000
#define SE_GROUP_LOGON_ID		0xC0000000

 

struct sid_array_data {
	__le16 SidAttrCount;
	 
} __packed;

struct luid_attr_data {

} __packed;

 

struct privilege_array_data {
	__le16 PrivilegeCount;
	 
} __packed;

struct remoted_identity_tcon_context {
	__le16 TicketType;  
	__le16 TicketSize;  
	__le16 User;  
	__le16 UserName;  
	__le16 Domain;  
	__le16 Groups;  
	__le16 RestrictedGroups;  
	__le16 Privileges;  
	__le16 PrimaryGroup;  
	__le16 Owner;  
	__le16 DefaultDacl;  
	__le16 DeviceGroups;  
	__le16 UserClaims;  
	__le16 DeviceClaims;  
	__u8   TicketInfo[];  
} __packed;

struct smb2_tree_connect_req_extension {
	__le32 TreeConnectContextOffset;
	__le16 TreeConnectContextCount;
	__u8  Reserved[10];
	__u8  PathName[];  
	 
} __packed;

 
#define SMB2_TREE_CONNECT_FLAG_CLUSTER_RECONNECT cpu_to_le16(0x0001)
#define SMB2_TREE_CONNECT_FLAG_REDIRECT_TO_OWNER cpu_to_le16(0x0002)
#define SMB2_TREE_CONNECT_FLAG_EXTENSION_PRESENT cpu_to_le16(0x0004)

struct smb2_tree_connect_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__le16 Flags;		 
	__le16 PathOffset;
	__le16 PathLength;
	__u8   Buffer[];	 
} __packed;

 
#define SMB2_SHARE_TYPE_DISK	0x01
#define SMB2_SHARE_TYPE_PIPE	0x02
#define	SMB2_SHARE_TYPE_PRINT	0x03

 
#define SMB2_SHAREFLAG_MANUAL_CACHING			0x00000000
#define SMB2_SHAREFLAG_AUTO_CACHING			0x00000010
#define SMB2_SHAREFLAG_VDO_CACHING			0x00000020
#define SMB2_SHAREFLAG_NO_CACHING			0x00000030
#define SHI1005_FLAGS_DFS				0x00000001
#define SHI1005_FLAGS_DFS_ROOT				0x00000002
#define SMB2_SHAREFLAG_RESTRICT_EXCLUSIVE_OPENS		0x00000100
#define SMB2_SHAREFLAG_FORCE_SHARED_DELETE		0x00000200
#define SMB2_SHAREFLAG_ALLOW_NAMESPACE_CACHING		0x00000400
#define SMB2_SHAREFLAG_ACCESS_BASED_DIRECTORY_ENUM	0x00000800
#define SMB2_SHAREFLAG_FORCE_LEVELII_OPLOCK		0x00001000
#define SMB2_SHAREFLAG_ENABLE_HASH_V1			0x00002000
#define SMB2_SHAREFLAG_ENABLE_HASH_V2			0x00004000
#define SHI1005_FLAGS_ENCRYPT_DATA			0x00008000
#define SMB2_SHAREFLAG_IDENTITY_REMOTING		0x00040000  
#define SMB2_SHAREFLAG_COMPRESS_DATA			0x00100000  
#define SMB2_SHAREFLAG_ISOLATED_TRANSPORT		0x00200000
#define SHI1005_FLAGS_ALL				0x0034FF33

 
#define SMB2_SHARE_CAP_DFS	cpu_to_le32(0x00000008)  
#define SMB2_SHARE_CAP_CONTINUOUS_AVAILABILITY cpu_to_le32(0x00000010)  
#define SMB2_SHARE_CAP_SCALEOUT	cpu_to_le32(0x00000020)  
#define SMB2_SHARE_CAP_CLUSTER	cpu_to_le32(0x00000040)  
#define SMB2_SHARE_CAP_ASYMMETRIC cpu_to_le32(0x00000080)  
#define SMB2_SHARE_CAP_REDIRECT_TO_OWNER cpu_to_le32(0x00000100)  

struct smb2_tree_connect_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__u8   ShareType;	 
	__u8   Reserved;
	__le32 ShareFlags;	 
	__le32 Capabilities;	 
	__le32 MaximalAccess;
} __packed;

struct smb2_tree_disconnect_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__le16 Reserved;
} __packed;

struct smb2_tree_disconnect_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__le16 Reserved;
} __packed;


 
 
#define	SMB2_NEGOTIATE_SIGNING_ENABLED     0x0001
#define	SMB2_NEGOTIATE_SIGNING_ENABLED_LE  cpu_to_le16(0x0001)
#define SMB2_NEGOTIATE_SIGNING_REQUIRED	   0x0002
#define SMB2_NEGOTIATE_SIGNING_REQUIRED_LE cpu_to_le16(0x0002)
#define SMB2_SEC_MODE_FLAGS_ALL            0x0003

 
#define SMB2_GLOBAL_CAP_DFS		0x00000001
#define SMB2_GLOBAL_CAP_LEASING		0x00000002  
#define SMB2_GLOBAL_CAP_LARGE_MTU	0x00000004  
#define SMB2_GLOBAL_CAP_MULTI_CHANNEL	0x00000008  
#define SMB2_GLOBAL_CAP_PERSISTENT_HANDLES 0x00000010  
#define SMB2_GLOBAL_CAP_DIRECTORY_LEASING  0x00000020  
#define SMB2_GLOBAL_CAP_ENCRYPTION	0x00000040  
 
#define SMB2_NT_FIND			0x00100000
#define SMB2_LARGE_FILES		0x00200000

#define SMB2_CLIENT_GUID_SIZE		16
#define SMB2_CREATE_GUID_SIZE		16

 
#define SMB10_PROT_ID  0x0000  
#define SMB20_PROT_ID  0x0202
#define SMB21_PROT_ID  0x0210
#define SMB2X_PROT_ID  0x02FF
#define SMB30_PROT_ID  0x0300
#define SMB302_PROT_ID 0x0302
#define SMB311_PROT_ID 0x0311
#define BAD_PROT_ID    0xFFFF

#define SMB311_SALT_SIZE			32
 
#define SMB2_PREAUTH_INTEGRITY_SHA512	cpu_to_le16(0x0001)
#define SMB2_PREAUTH_HASH_SIZE 64

 
#define SMB2_PREAUTH_INTEGRITY_CAPABILITIES	cpu_to_le16(1)
#define SMB2_ENCRYPTION_CAPABILITIES		cpu_to_le16(2)
#define SMB2_COMPRESSION_CAPABILITIES		cpu_to_le16(3)
#define SMB2_NETNAME_NEGOTIATE_CONTEXT_ID	cpu_to_le16(5)
#define SMB2_TRANSPORT_CAPABILITIES		cpu_to_le16(6)
#define SMB2_RDMA_TRANSFORM_CAPABILITIES	cpu_to_le16(7)
#define SMB2_SIGNING_CAPABILITIES		cpu_to_le16(8)
#define SMB2_POSIX_EXTENSIONS_AVAILABLE		cpu_to_le16(0x100)

struct smb2_neg_context {
	__le16	ContextType;
	__le16	DataLength;
	__le32	Reserved;
	 
} __packed;

 
#define MIN_PREAUTH_CTXT_DATA_LEN 6

struct smb2_preauth_neg_context {
	__le16	ContextType;  
	__le16	DataLength;
	__le32	Reserved;
	__le16	HashAlgorithmCount;  
	__le16	SaltLength;
	__le16	HashAlgorithms;  
	__u8	Salt[SMB311_SALT_SIZE];
} __packed;

 
#define SMB2_ENCRYPTION_AES128_CCM	cpu_to_le16(0x0001)
#define SMB2_ENCRYPTION_AES128_GCM	cpu_to_le16(0x0002)
#define SMB2_ENCRYPTION_AES256_CCM      cpu_to_le16(0x0003)
#define SMB2_ENCRYPTION_AES256_GCM      cpu_to_le16(0x0004)

 
#define MIN_ENCRYPT_CTXT_DATA_LEN	4
struct smb2_encryption_neg_context {
	__le16	ContextType;  
	__le16	DataLength;
	__le32	Reserved;
	 
	__le16	CipherCount;  
	__le16	Ciphers[];
} __packed;

 
#define SMB3_COMPRESS_NONE	cpu_to_le16(0x0000)
#define SMB3_COMPRESS_LZNT1	cpu_to_le16(0x0001)
#define SMB3_COMPRESS_LZ77	cpu_to_le16(0x0002)
#define SMB3_COMPRESS_LZ77_HUFF	cpu_to_le16(0x0003)
 
#define SMB3_COMPRESS_PATTERN	cpu_to_le16(0x0004)  

 
#define SMB2_COMPRESSION_CAPABILITIES_FLAG_NONE		cpu_to_le32(0x00000000)
#define SMB2_COMPRESSION_CAPABILITIES_FLAG_CHAINED	cpu_to_le32(0x00000001)

struct smb2_compression_capabilities_context {
	__le16	ContextType;  
	__le16  DataLength;
	__le32	Reserved;
	__le16	CompressionAlgorithmCount;
	__le16	Padding;
	__le32	Flags;
	__le16	CompressionAlgorithms[3];
	__u16	Pad;   
	 
} __packed;

 
struct smb2_netname_neg_context {
	__le16	ContextType;  
	__le16	DataLength;
	__le32	Reserved;
	__le16	NetName[];  
} __packed;

 

 
#define SMB2_ACCEPT_TRANSPORT_LEVEL_SECURITY	0x00000001

struct smb2_transport_capabilities_context {
	__le16	ContextType;  
	__le16  DataLength;
	__u32	Reserved;
	__le32	Flags;
	__u32	Pad;
} __packed;

 

 
#define SMB2_RDMA_TRANSFORM_NONE	0x0000
#define SMB2_RDMA_TRANSFORM_ENCRYPTION	0x0001
#define SMB2_RDMA_TRANSFORM_SIGNING	0x0002

struct smb2_rdma_transform_capabilities_context {
	__le16	ContextType;  
	__le16  DataLength;
	__u32	Reserved;
	__le16	TransformCount;
	__u16	Reserved1;
	__u32	Reserved2;
	__le16	RDMATransformIds[];
} __packed;

 

 
#define SIGNING_ALG_HMAC_SHA256    0
#define SIGNING_ALG_HMAC_SHA256_LE cpu_to_le16(0)
#define SIGNING_ALG_AES_CMAC       1
#define SIGNING_ALG_AES_CMAC_LE    cpu_to_le16(1)
#define SIGNING_ALG_AES_GMAC       2
#define SIGNING_ALG_AES_GMAC_LE    cpu_to_le16(2)

struct smb2_signing_capabilities {
	__le16	ContextType;  
	__le16	DataLength;
	__le32	Reserved;
	__le16	SigningAlgorithmCount;
	__le16	SigningAlgorithms[];
	 
} __packed;

#define POSIX_CTXT_DATA_LEN	16
struct smb2_posix_neg_context {
	__le16	ContextType;  
	__le16	DataLength;
	__le32	Reserved;
	__u8	Name[16];  
} __packed;

struct smb2_negotiate_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 DialectCount;
	__le16 SecurityMode;
	__le16 Reserved;	 
	__le32 Capabilities;
	__u8   ClientGUID[SMB2_CLIENT_GUID_SIZE];
	 
	__le32 NegotiateContextOffset;  
	__le16 NegotiateContextCount;   
	__le16 Reserved2;
	__le16 Dialects[];
} __packed;

struct smb2_negotiate_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__le16 SecurityMode;
	__le16 DialectRevision;
	__le16 NegotiateContextCount;	 
	__u8   ServerGUID[16];
	__le32 Capabilities;
	__le32 MaxTransactSize;
	__le32 MaxReadSize;
	__le32 MaxWriteSize;
	__le64 SystemTime;	 
	__le64 ServerStartTime;
	__le16 SecurityBufferOffset;
	__le16 SecurityBufferLength;
	__le32 NegotiateContextOffset;	 
	__u8   Buffer[];	 
} __packed;


 
 
#define SMB2_SESSION_REQ_FLAG_BINDING		0x01
#define SMB2_SESSION_REQ_FLAG_ENCRYPT_DATA	0x04

struct smb2_sess_setup_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__u8   Flags;
	__u8   SecurityMode;
	__le32 Capabilities;
	__le32 Channel;
	__le16 SecurityBufferOffset;
	__le16 SecurityBufferLength;
	__le64 PreviousSessionId;
	__u8   Buffer[];	 
} __packed;

 
#define SMB2_SESSION_FLAG_IS_GUEST        0x0001
#define SMB2_SESSION_FLAG_IS_GUEST_LE     cpu_to_le16(0x0001)
#define SMB2_SESSION_FLAG_IS_NULL         0x0002
#define SMB2_SESSION_FLAG_IS_NULL_LE      cpu_to_le16(0x0002)
#define SMB2_SESSION_FLAG_ENCRYPT_DATA    0x0004
#define SMB2_SESSION_FLAG_ENCRYPT_DATA_LE cpu_to_le16(0x0004)

struct smb2_sess_setup_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 SessionFlags;
	__le16 SecurityBufferOffset;
	__le16 SecurityBufferLength;
	__u8   Buffer[];	 
} __packed;


 
struct smb2_logoff_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__le16 Reserved;
} __packed;

struct smb2_logoff_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__le16 Reserved;
} __packed;


 
 
#define SMB2_CLOSE_FLAG_POSTQUERY_ATTRIB	cpu_to_le16(0x0001)
struct smb2_close_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__le16 Flags;
	__le32 Reserved;
	__u64  PersistentFileId;  
	__u64  VolatileFileId;  
} __packed;

 
#define MAX_SMB2_CLOSE_RESPONSE_SIZE 124

struct smb2_close_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 Flags;
	__le32 Reserved;
	struct_group(network_open_info,
		__le64 CreationTime;
		__le64 LastAccessTime;
		__le64 LastWriteTime;
		__le64 ChangeTime;
		 
		__le64 AllocationSize;
		__le64 EndOfFile;
		__le32 Attributes;
	);
} __packed;


 
 
#define SMB2_READFLAG_READ_UNBUFFERED	0x01
#define SMB2_READFLAG_REQUEST_COMPRESSED 0x02  

 
#define SMB2_CHANNEL_NONE               cpu_to_le32(0x00000000)
#define SMB2_CHANNEL_RDMA_V1            cpu_to_le32(0x00000001)
#define SMB2_CHANNEL_RDMA_V1_INVALIDATE cpu_to_le32(0x00000002)
#define SMB2_CHANNEL_RDMA_TRANSFORM     cpu_to_le32(0x00000003)

 
struct smb2_read_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__u8   Padding;  
	__u8   Flags;  
	__le32 Length;
	__le64 Offset;
	__u64  PersistentFileId;
	__u64  VolatileFileId;
	__le32 MinimumCount;
	__le32 Channel;  
	__le32 RemainingBytes;
	__le16 ReadChannelInfoOffset;
	__le16 ReadChannelInfoLength;
	__u8   Buffer[];
} __packed;

 
#define SMB2_READFLAG_RESPONSE_NONE            cpu_to_le32(0x00000000)
#define SMB2_READFLAG_RESPONSE_RDMA_TRANSFORM  cpu_to_le32(0x00000001)

struct smb2_read_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__u8   DataOffset;
	__u8   Reserved;
	__le32 DataLength;
	__le32 DataRemaining;
	__le32 Flags;
	__u8   Buffer[];
} __packed;


 
 
#define SMB2_WRITEFLAG_WRITE_THROUGH	0x00000001	 
#define SMB2_WRITEFLAG_WRITE_UNBUFFERED	0x00000002	 

struct smb2_write_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 DataOffset;  
	__le32 Length;
	__le64 Offset;
	__u64  PersistentFileId;  
	__u64  VolatileFileId;  
	__le32 Channel;  
	__le32 RemainingBytes;
	__le16 WriteChannelInfoOffset;
	__le16 WriteChannelInfoLength;
	__le32 Flags;
	__u8   Buffer[];
} __packed;

struct smb2_write_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__u8   DataOffset;
	__u8   Reserved;
	__le32 DataLength;
	__le32 DataRemaining;
	__u32  Reserved2;
	__u8   Buffer[];
} __packed;


 
struct smb2_flush_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__le16 Reserved1;
	__le32 Reserved2;
	__u64  PersistentFileId;
	__u64  VolatileFileId;
} __packed;

struct smb2_flush_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;
	__le16 Reserved;
} __packed;

#define SMB2_LOCKFLAG_SHARED		0x0001
#define SMB2_LOCKFLAG_EXCLUSIVE		0x0002
#define SMB2_LOCKFLAG_UNLOCK		0x0004
#define SMB2_LOCKFLAG_FAIL_IMMEDIATELY	0x0010
#define SMB2_LOCKFLAG_MASK		0x0007

struct smb2_lock_element {
	__le64 Offset;
	__le64 Length;
	__le32 Flags;
	__le32 Reserved;
} __packed;

struct smb2_lock_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 LockCount;
	 
	__le32 LockSequenceNumber;
	__u64  PersistentFileId;
	__u64  VolatileFileId;
	 
	union {
		struct smb2_lock_element lock;
		DECLARE_FLEX_ARRAY(struct smb2_lock_element, locks);
	};
} __packed;

struct smb2_lock_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 Reserved;
} __packed;

struct smb2_echo_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__u16  Reserved;
} __packed;

struct smb2_echo_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__u16  Reserved;
} __packed;

 

 
#define SMB2_RESTART_SCANS		0x01
#define SMB2_RETURN_SINGLE_ENTRY	0x02
#define SMB2_INDEX_SPECIFIED		0x04
#define SMB2_REOPEN			0x10

struct smb2_query_directory_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__u8   FileInformationClass;
	__u8   Flags;
	__le32 FileIndex;
	__u64  PersistentFileId;
	__u64  VolatileFileId;
	__le16 FileNameOffset;
	__le16 FileNameLength;
	__le32 OutputBufferLength;
	__u8   Buffer[];
} __packed;

struct smb2_query_directory_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 OutputBufferOffset;
	__le32 OutputBufferLength;
	__u8   Buffer[];
} __packed;

 
#define SMB2_SET_INFO_IOV_SIZE 3

struct smb2_set_info_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__u8   InfoType;
	__u8   FileInfoClass;
	__le32 BufferLength;
	__le16 BufferOffset;
	__u16  Reserved;
	__le32 AdditionalInformation;
	__u64  PersistentFileId;
	__u64  VolatileFileId;
	__u8   Buffer[];
} __packed;

struct smb2_set_info_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
} __packed;

 
 
#define SMB2_WATCH_TREE			0x0001

 
#define FILE_NOTIFY_CHANGE_FILE_NAME		0x00000001
#define FILE_NOTIFY_CHANGE_DIR_NAME		0x00000002
#define FILE_NOTIFY_CHANGE_ATTRIBUTES		0x00000004
#define FILE_NOTIFY_CHANGE_SIZE			0x00000008
#define FILE_NOTIFY_CHANGE_LAST_WRITE		0x00000010
#define FILE_NOTIFY_CHANGE_LAST_ACCESS		0x00000020
#define FILE_NOTIFY_CHANGE_CREATION		0x00000040
#define FILE_NOTIFY_CHANGE_EA			0x00000080
#define FILE_NOTIFY_CHANGE_SECURITY		0x00000100
#define FILE_NOTIFY_CHANGE_STREAM_NAME		0x00000200
#define FILE_NOTIFY_CHANGE_STREAM_SIZE		0x00000400
#define FILE_NOTIFY_CHANGE_STREAM_WRITE		0x00000800

 
#define FILE_ACTION_ADDED                       0x00000001
#define FILE_ACTION_REMOVED                     0x00000002
#define FILE_ACTION_MODIFIED                    0x00000003
#define FILE_ACTION_RENAMED_OLD_NAME            0x00000004
#define FILE_ACTION_RENAMED_NEW_NAME            0x00000005
#define FILE_ACTION_ADDED_STREAM                0x00000006
#define FILE_ACTION_REMOVED_STREAM              0x00000007
#define FILE_ACTION_MODIFIED_STREAM             0x00000008
#define FILE_ACTION_REMOVED_BY_DELETE           0x00000009

struct smb2_change_notify_req {
	struct smb2_hdr hdr;
	__le16	StructureSize;
	__le16	Flags;
	__le32	OutputBufferLength;
	__u64	PersistentFileId;  
	__u64	VolatileFileId;  
	__le32	CompletionFilter;
	__u32	Reserved;
} __packed;

struct smb2_change_notify_rsp {
	struct smb2_hdr hdr;
	__le16	StructureSize;   
	__le16	OutputBufferOffset;
	__le32	OutputBufferLength;
	__u8	Buffer[];  
} __packed;


 
 
#define SMB2_OPLOCK_LEVEL_NONE		0x00
#define SMB2_OPLOCK_LEVEL_II		0x01
#define SMB2_OPLOCK_LEVEL_EXCLUSIVE	0x08
#define SMB2_OPLOCK_LEVEL_BATCH		0x09
#define SMB2_OPLOCK_LEVEL_LEASE		0xFF
 
#define SMB2_OPLOCK_LEVEL_NOCHANGE	0x99

 
#define IL_ANONYMOUS		cpu_to_le32(0x00000000)
#define IL_IDENTIFICATION	cpu_to_le32(0x00000001)
#define IL_IMPERSONATION	cpu_to_le32(0x00000002)
#define IL_DELEGATE		cpu_to_le32(0x00000003)

 
#define FILE_ATTRIBUTE_READONLY			0x00000001
#define FILE_ATTRIBUTE_HIDDEN			0x00000002
#define FILE_ATTRIBUTE_SYSTEM			0x00000004
#define FILE_ATTRIBUTE_DIRECTORY		0x00000010
#define FILE_ATTRIBUTE_ARCHIVE			0x00000020
#define FILE_ATTRIBUTE_NORMAL			0x00000080
#define FILE_ATTRIBUTE_TEMPORARY		0x00000100
#define FILE_ATTRIBUTE_SPARSE_FILE		0x00000200
#define FILE_ATTRIBUTE_REPARSE_POINT		0x00000400
#define FILE_ATTRIBUTE_COMPRESSED		0x00000800
#define FILE_ATTRIBUTE_OFFLINE			0x00001000
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED	0x00002000
#define FILE_ATTRIBUTE_ENCRYPTED		0x00004000
#define FILE_ATTRIBUTE_INTEGRITY_STREAM		0x00008000
#define FILE_ATTRIBUTE_NO_SCRUB_DATA		0x00020000
#define FILE_ATTRIBUTE__MASK			0x00007FB7

#define FILE_ATTRIBUTE_READONLY_LE              cpu_to_le32(0x00000001)
#define FILE_ATTRIBUTE_HIDDEN_LE		cpu_to_le32(0x00000002)
#define FILE_ATTRIBUTE_SYSTEM_LE		cpu_to_le32(0x00000004)
#define FILE_ATTRIBUTE_DIRECTORY_LE		cpu_to_le32(0x00000010)
#define FILE_ATTRIBUTE_ARCHIVE_LE		cpu_to_le32(0x00000020)
#define FILE_ATTRIBUTE_NORMAL_LE		cpu_to_le32(0x00000080)
#define FILE_ATTRIBUTE_TEMPORARY_LE		cpu_to_le32(0x00000100)
#define FILE_ATTRIBUTE_SPARSE_FILE_LE		cpu_to_le32(0x00000200)
#define FILE_ATTRIBUTE_REPARSE_POINT_LE		cpu_to_le32(0x00000400)
#define FILE_ATTRIBUTE_COMPRESSED_LE		cpu_to_le32(0x00000800)
#define FILE_ATTRIBUTE_OFFLINE_LE		cpu_to_le32(0x00001000)
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED_LE	cpu_to_le32(0x00002000)
#define FILE_ATTRIBUTE_ENCRYPTED_LE		cpu_to_le32(0x00004000)
#define FILE_ATTRIBUTE_INTEGRITY_STREAM_LE	cpu_to_le32(0x00008000)
#define FILE_ATTRIBUTE_NO_SCRUB_DATA_LE		cpu_to_le32(0x00020000)
#define FILE_ATTRIBUTE_MASK_LE			cpu_to_le32(0x00007FB7)

 
#define FILE_READ_DATA_LE		cpu_to_le32(0x00000001)
#define FILE_LIST_DIRECTORY_LE		cpu_to_le32(0x00000001)
#define FILE_WRITE_DATA_LE		cpu_to_le32(0x00000002)
#define FILE_APPEND_DATA_LE		cpu_to_le32(0x00000004)
#define FILE_ADD_SUBDIRECTORY_LE	cpu_to_le32(0x00000004)
#define FILE_READ_EA_LE			cpu_to_le32(0x00000008)
#define FILE_WRITE_EA_LE		cpu_to_le32(0x00000010)
#define FILE_EXECUTE_LE			cpu_to_le32(0x00000020)
#define FILE_DELETE_CHILD_LE		cpu_to_le32(0x00000040)
#define FILE_READ_ATTRIBUTES_LE		cpu_to_le32(0x00000080)
#define FILE_WRITE_ATTRIBUTES_LE	cpu_to_le32(0x00000100)
#define FILE_DELETE_LE			cpu_to_le32(0x00010000)
#define FILE_READ_CONTROL_LE		cpu_to_le32(0x00020000)
#define FILE_WRITE_DAC_LE		cpu_to_le32(0x00040000)
#define FILE_WRITE_OWNER_LE		cpu_to_le32(0x00080000)
#define FILE_SYNCHRONIZE_LE		cpu_to_le32(0x00100000)
#define FILE_ACCESS_SYSTEM_SECURITY_LE	cpu_to_le32(0x01000000)
#define FILE_MAXIMAL_ACCESS_LE		cpu_to_le32(0x02000000)
#define FILE_GENERIC_ALL_LE		cpu_to_le32(0x10000000)
#define FILE_GENERIC_EXECUTE_LE		cpu_to_le32(0x20000000)
#define FILE_GENERIC_WRITE_LE		cpu_to_le32(0x40000000)
#define FILE_GENERIC_READ_LE		cpu_to_le32(0x80000000)
#define DESIRED_ACCESS_MASK             cpu_to_le32(0xF21F01FF)


#define FILE_READ_DESIRED_ACCESS_LE     (FILE_READ_DATA_LE        |	\
					 FILE_READ_EA_LE          |     \
					 FILE_GENERIC_READ_LE)
#define FILE_WRITE_DESIRE_ACCESS_LE     (FILE_WRITE_DATA_LE       |	\
					 FILE_APPEND_DATA_LE      |	\
					 FILE_WRITE_EA_LE         |	\
					 FILE_WRITE_ATTRIBUTES_LE |	\
					 FILE_GENERIC_WRITE_LE)

 
#define FILE_SHARE_READ_LE		cpu_to_le32(0x00000001)
#define FILE_SHARE_WRITE_LE		cpu_to_le32(0x00000002)
#define FILE_SHARE_DELETE_LE		cpu_to_le32(0x00000004)
#define FILE_SHARE_ALL_LE		cpu_to_le32(0x00000007)

 
#define FILE_SUPERSEDE_LE		cpu_to_le32(0x00000000)
#define FILE_OPEN_LE			cpu_to_le32(0x00000001)
#define FILE_CREATE_LE			cpu_to_le32(0x00000002)
#define	FILE_OPEN_IF_LE			cpu_to_le32(0x00000003)
#define FILE_OVERWRITE_LE		cpu_to_le32(0x00000004)
#define FILE_OVERWRITE_IF_LE		cpu_to_le32(0x00000005)
#define FILE_CREATE_MASK_LE             cpu_to_le32(0x00000007)

#define FILE_READ_RIGHTS (FILE_READ_DATA | FILE_READ_EA \
			| FILE_READ_ATTRIBUTES)
#define FILE_WRITE_RIGHTS (FILE_WRITE_DATA | FILE_APPEND_DATA \
			| FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES)
#define FILE_EXEC_RIGHTS (FILE_EXECUTE)

 
#define FILE_DIRECTORY_FILE_LE		cpu_to_le32(0x00000001)
 
#define FILE_WRITE_THROUGH_LE		cpu_to_le32(0x00000002)
#define FILE_SEQUENTIAL_ONLY_LE		cpu_to_le32(0x00000004)
#define FILE_NO_INTERMEDIATE_BUFFERING_LE cpu_to_le32(0x00000008)
#define FILE_NON_DIRECTORY_FILE_LE	cpu_to_le32(0x00000040)
#define FILE_COMPLETE_IF_OPLOCKED_LE	cpu_to_le32(0x00000100)
#define FILE_NO_EA_KNOWLEDGE_LE		cpu_to_le32(0x00000200)
#define FILE_RANDOM_ACCESS_LE		cpu_to_le32(0x00000800)
#define FILE_DELETE_ON_CLOSE_LE		cpu_to_le32(0x00001000)
#define FILE_OPEN_BY_FILE_ID_LE		cpu_to_le32(0x00002000)
#define FILE_OPEN_FOR_BACKUP_INTENT_LE	cpu_to_le32(0x00004000)
#define FILE_NO_COMPRESSION_LE		cpu_to_le32(0x00008000)
#define FILE_OPEN_REPARSE_POINT_LE	cpu_to_le32(0x00200000)
#define FILE_OPEN_NO_RECALL_LE		cpu_to_le32(0x00400000)
#define CREATE_OPTIONS_MASK_LE          cpu_to_le32(0x00FFFFFF)

#define FILE_READ_RIGHTS_LE (FILE_READ_DATA_LE | FILE_READ_EA_LE \
			| FILE_READ_ATTRIBUTES_LE)
#define FILE_WRITE_RIGHTS_LE (FILE_WRITE_DATA_LE | FILE_APPEND_DATA_LE \
			| FILE_WRITE_EA_LE | FILE_WRITE_ATTRIBUTES_LE)
#define FILE_EXEC_RIGHTS_LE (FILE_EXECUTE_LE)

 
#define SMB2_CREATE_EA_BUFFER			"ExtA"  
#define SMB2_CREATE_SD_BUFFER			"SecD"  
#define SMB2_CREATE_DURABLE_HANDLE_REQUEST	"DHnQ"
#define SMB2_CREATE_DURABLE_HANDLE_RECONNECT	"DHnC"
#define SMB2_CREATE_ALLOCATION_SIZE		"AlSi"
#define SMB2_CREATE_QUERY_MAXIMAL_ACCESS_REQUEST "MxAc"
#define SMB2_CREATE_TIMEWARP_REQUEST		"TWrp"
#define SMB2_CREATE_QUERY_ON_DISK_ID		"QFid"
#define SMB2_CREATE_REQUEST_LEASE		"RqLs"
#define SMB2_CREATE_DURABLE_HANDLE_REQUEST_V2	"DH2Q"
#define SMB2_CREATE_DURABLE_HANDLE_RECONNECT_V2	"DH2C"
#define SMB2_CREATE_TAG_POSIX		"\x93\xAD\x25\x50\x9C\xB4\x11\xE7\xB4\x23\x83\xDE\x96\x8B\xCD\x7C"
#define SMB2_CREATE_APP_INSTANCE_ID	"\x45\xBC\xA6\x6A\xEF\xA7\xF7\x4A\x90\x08\xFA\x46\x2E\x14\x4D\x74"
#define SMB2_CREATE_APP_INSTANCE_VERSION "\xB9\x82\xD0\xB7\x3B\x56\x07\x4F\xA0\x7B\x52\x4A\x81\x16\xA0\x10"
#define SVHDX_OPEN_DEVICE_CONTEXT	"\x9C\xCB\xCF\x9E\x04\xC1\xE6\x43\x98\x0E\x15\x8D\xA1\xF6\xEC\x83"
#define SMB2_CREATE_TAG_AAPL			"AAPL"

 
#define SMB2_CREATE_FLAG_REPARSEPOINT 0x01

struct create_context {
	__le32 Next;
	__le16 NameOffset;
	__le16 NameLength;
	__le16 Reserved;
	__le16 DataOffset;
	__le32 DataLength;
	__u8 Buffer[];
} __packed;

struct smb2_create_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__u8   SecurityFlags;
	__u8   RequestedOplockLevel;
	__le32 ImpersonationLevel;
	__le64 SmbCreateFlags;
	__le64 Reserved;
	__le32 DesiredAccess;
	__le32 FileAttributes;
	__le32 ShareAccess;
	__le32 CreateDisposition;
	__le32 CreateOptions;
	__le16 NameOffset;
	__le16 NameLength;
	__le32 CreateContextsOffset;
	__le32 CreateContextsLength;
	__u8   Buffer[];
} __packed;

struct smb2_create_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;	 
	__u8   OplockLevel;
	__u8   Flags;   
	__le32 CreateAction;
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le64 AllocationSize;
	__le64 EndofFile;
	__le32 FileAttributes;
	__le32 Reserved2;
	__u64  PersistentFileId;
	__u64  VolatileFileId;
	__le32 CreateContextsOffset;
	__le32 CreateContextsLength;
	__u8   Buffer[];
} __packed;

struct create_posix {
	struct create_context ccontext;
	__u8    Name[16];
	__le32  Mode;
	__u32   Reserved;
} __packed;

 
struct create_durable {
	struct create_context ccontext;
	__u8   Name[8];
	union {
		__u8  Reserved[16];
		struct {
			__u64 PersistentFileId;
			__u64 VolatileFileId;
		} Fid;
	} Data;
} __packed;

 
struct create_mxac_req {
	struct create_context ccontext;
	__u8   Name[8];
	__le64 Timestamp;
} __packed;

 
struct create_mxac_rsp {
	struct create_context ccontext;
	__u8   Name[8];
	__le32 QueryStatus;
	__le32 MaximalAccess;
} __packed;

#define SMB2_LEASE_NONE_LE			cpu_to_le32(0x00)
#define SMB2_LEASE_READ_CACHING_LE		cpu_to_le32(0x01)
#define SMB2_LEASE_HANDLE_CACHING_LE		cpu_to_le32(0x02)
#define SMB2_LEASE_WRITE_CACHING_LE		cpu_to_le32(0x04)

#define SMB2_LEASE_FLAG_BREAK_IN_PROGRESS_LE	cpu_to_le32(0x02)
#define SMB2_LEASE_FLAG_PARENT_LEASE_KEY_SET_LE	cpu_to_le32(0x04)

#define SMB2_LEASE_KEY_SIZE			16

 
struct lease_context {
	__u8 LeaseKey[SMB2_LEASE_KEY_SIZE];
	__le32 LeaseState;
	__le32 LeaseFlags;
	__le64 LeaseDuration;
} __packed;

 
struct lease_context_v2 {
	__u8 LeaseKey[SMB2_LEASE_KEY_SIZE];
	__le32 LeaseState;
	__le32 LeaseFlags;
	__le64 LeaseDuration;
	__u8 ParentLeaseKey[SMB2_LEASE_KEY_SIZE];
	__le16 Epoch;
	__le16 Reserved;
} __packed;

struct create_lease {
	struct create_context ccontext;
	__u8   Name[8];
	struct lease_context lcontext;
} __packed;

struct create_lease_v2 {
	struct create_context ccontext;
	__u8   Name[8];
	struct lease_context_v2 lcontext;
	__u8   Pad[4];
} __packed;

 
struct create_disk_id_rsp {
	struct create_context ccontext;
	__u8   Name[8];
	__le64 DiskFileId;
	__le64 VolumeId;
	__u8  Reserved[16];
} __packed;

 
struct create_app_inst_id {
	struct create_context ccontext;
	__u8 Name[16];
	__le32 StructureSize;  
	__u16 Reserved;
	__u8 AppInstanceId[16];
} __packed;

 
struct create_app_inst_id_vers {
	struct create_context ccontext;
	__u8 Name[16];
	__le32 StructureSize;  
	__u16 Reserved;
	__u32 Padding;
	__le64 AppInstanceVersionHigh;
	__le64 AppInstanceVersionLow;
} __packed;

 
struct smb2_ioctl_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 Reserved;  
	__le32 CtlCode;
	__u64  PersistentFileId;
	__u64  VolatileFileId;
	__le32 InputOffset;  
	__le32 InputCount;
	__le32 MaxInputResponse;
	__le32 OutputOffset;
	__le32 OutputCount;
	__le32 MaxOutputResponse;
	__le32 Flags;
	__le32 Reserved2;
	__u8   Buffer[];
} __packed;

struct smb2_ioctl_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 Reserved;
	__le32 CtlCode;
	__u64  PersistentFileId;
	__u64  VolatileFileId;
	__le32 InputOffset;  
	__le32 InputCount;
	__le32 OutputOffset;
	__le32 OutputCount;
	__le32 Flags;
	__le32 Reserved2;
	__u8   Buffer[];
} __packed;

 
struct file_zero_data_information {
	__le64	FileOffset;
	__le64	BeyondFinalZero;
} __packed;

 
struct duplicate_extents_to_file {
	__u64 PersistentFileHandle;  
	__u64 VolatileFileHandle;
	__le64 SourceFileOffset;
	__le64 TargetFileOffset;
	__le64 ByteCount;   
} __packed;

 
#define DUPLICATE_EXTENTS_DATA_EX_SOURCE_ATOMIC	0x00000001
struct duplicate_extents_to_file_ex {
	__u64 PersistentFileHandle;  
	__u64 VolatileFileHandle;
	__le64 SourceFileOffset;
	__le64 TargetFileOffset;
	__le64 ByteCount;   
	__le32 Flags;
	__le32 Reserved;
} __packed;


 
struct fsctl_get_integrity_information_rsp {
	__le16	ChecksumAlgorithm;
	__le16	Reserved;
	__le32	Flags;
	__le32	ChecksumChunkSizeInBytes;
	__le32	ClusterSizeInBytes;
} __packed;

 
struct fsctl_query_file_regions_req {
	__le64	FileOffset;
	__le64	Length;
	__le32	DesiredUsage;
	__le32	Reserved;
} __packed;

 
#define FILE_USAGE_INVALID_RANGE	0x00000000
#define FILE_USAGE_VALID_CACHED_DATA	0x00000001
#define FILE_USAGE_NONCACHED_DATA	0x00000002

struct file_region_info {
	__le64	FileOffset;
	__le64	Length;
	__le32	DesiredUsage;
	__le32	Reserved;
} __packed;

 
struct fsctl_query_file_region_rsp {
	__le32 Flags;
	__le32 TotalRegionEntryCount;
	__le32 RegionEntryCount;
	__u32  Reserved;
	struct  file_region_info Regions[];
} __packed;

 
struct fsctl_query_on_disk_vol_info_rsp {
	__le64	DirectoryCount;
	__le64	FileCount;
	__le16	FsFormatMajVersion;
	__le16	FsFormatMinVersion;
	__u8	FsFormatName[24];
	__le64	FormatTime;
	__le64	LastUpdateTime;
	__u8	CopyrightInfo[68];
	__u8	AbstractInfo[68];
	__u8	FormatImplInfo[68];
	__u8	LastModifyImplInfo[68];
} __packed;

 
struct fsctl_set_integrity_information_req {
	__le16	ChecksumAlgorithm;
	__le16	Reserved;
	__le32	Flags;
} __packed;

 
struct fsctl_set_integrity_info_ex_req {
	__u8	EnableIntegrity;
	__u8	KeepState;
	__u16	Reserved;
	__le32	Flags;
	__u8	Version;
	__u8	Reserved2[7];
} __packed;

 
#define	CHECKSUM_TYPE_NONE	0x0000
#define	CHECKSUM_TYPE_CRC64	0x0002
#define	CHECKSUM_TYPE_UNCHANGED	0xFFFF	 

 
#define FSCTL_INTEGRITY_FLAG_CHECKSUM_ENFORCEMENT_OFF	0x00000001

 

 
struct reparse_data_buffer {
	__le32	ReparseTag;
	__le16	ReparseDataLength;
	__u16	Reserved;
	__u8	DataBuffer[];  
} __packed;

struct reparse_guid_data_buffer {
	__le32	ReparseTag;
	__le16	ReparseDataLength;
	__u16	Reserved;
	__u8	ReparseGuid[16];
	__u8	DataBuffer[];  
} __packed;

struct reparse_mount_point_data_buffer {
	__le32	ReparseTag;
	__le16	ReparseDataLength;
	__u16	Reserved;
	__le16	SubstituteNameOffset;
	__le16	SubstituteNameLength;
	__le16	PrintNameOffset;
	__le16	PrintNameLength;
	__u8	PathBuffer[];  
} __packed;

#define SYMLINK_FLAG_RELATIVE 0x00000001

struct reparse_symlink_data_buffer {
	__le32	ReparseTag;
	__le16	ReparseDataLength;
	__u16	Reserved;
	__le16	SubstituteNameOffset;
	__le16	SubstituteNameLength;
	__le16	PrintNameOffset;
	__le16	PrintNameLength;
	__le32	Flags;
	__u8	PathBuffer[];  
} __packed;

 

struct validate_negotiate_info_req {
	__le32 Capabilities;
	__u8   Guid[SMB2_CLIENT_GUID_SIZE];
	__le16 SecurityMode;
	__le16 DialectCount;
	__le16 Dialects[4];  
} __packed;

struct validate_negotiate_info_rsp {
	__le32 Capabilities;
	__u8   Guid[SMB2_CLIENT_GUID_SIZE];
	__le16 SecurityMode;
	__le16 Dialect;  
} __packed;


 
#define SMB2_O_INFO_FILE	0x01
#define SMB2_O_INFO_FILESYSTEM	0x02
#define SMB2_O_INFO_SECURITY	0x03
#define SMB2_O_INFO_QUOTA	0x04

 

 
#define FILE_DIRECTORY_INFORMATION	1	 
#define FILE_FULL_DIRECTORY_INFORMATION 2	 
#define FILE_BOTH_DIRECTORY_INFORMATION 3	 
#define FILE_BASIC_INFORMATION		4
#define FILE_STANDARD_INFORMATION	5
#define FILE_INTERNAL_INFORMATION	6
#define FILE_EA_INFORMATION	        7
#define FILE_ACCESS_INFORMATION		8
#define FILE_NAME_INFORMATION		9
#define FILE_RENAME_INFORMATION		10
#define FILE_LINK_INFORMATION		11
#define FILE_NAMES_INFORMATION		12	 
#define FILE_DISPOSITION_INFORMATION	13
#define FILE_POSITION_INFORMATION	14
#define FILE_FULL_EA_INFORMATION	15
#define FILE_MODE_INFORMATION		16
#define FILE_ALIGNMENT_INFORMATION	17
#define FILE_ALL_INFORMATION		18
#define FILE_ALLOCATION_INFORMATION	19
#define FILE_END_OF_FILE_INFORMATION	20
#define FILE_ALTERNATE_NAME_INFORMATION 21
#define FILE_STREAM_INFORMATION		22
#define FILE_PIPE_INFORMATION		23
#define FILE_PIPE_LOCAL_INFORMATION	24
#define FILE_PIPE_REMOTE_INFORMATION	25
#define FILE_MAILSLOT_QUERY_INFORMATION 26
#define FILE_MAILSLOT_SET_INFORMATION	27
#define FILE_COMPRESSION_INFORMATION	28
#define FILE_OBJECT_ID_INFORMATION	29
 
#define FILE_MOVE_CLUSTER_INFORMATION	31
#define FILE_QUOTA_INFORMATION		32
#define FILE_REPARSE_POINT_INFORMATION	33
#define FILE_NETWORK_OPEN_INFORMATION	34
#define FILE_ATTRIBUTE_TAG_INFORMATION	35
#define FILE_TRACKING_INFORMATION	36
#define FILEID_BOTH_DIRECTORY_INFORMATION 37	 
#define FILEID_FULL_DIRECTORY_INFORMATION 38	 
#define FILE_VALID_DATA_LENGTH_INFORMATION 39
#define FILE_SHORT_NAME_INFORMATION	40
#define FILE_SFIO_RESERVE_INFORMATION	44
#define FILE_SFIO_VOLUME_INFORMATION	45
#define FILE_HARD_LINK_INFORMATION	46
#define FILE_NORMALIZED_NAME_INFORMATION 48
#define FILEID_GLOBAL_TX_DIRECTORY_INFORMATION 50
#define FILE_STANDARD_LINK_INFORMATION	54
#define FILE_ID_INFORMATION		59
#define FILE_ID_EXTD_DIRECTORY_INFORMATION 60	 
 
#define SMB_FIND_FILE_POSIX_INFO	0x064

 
#define OWNER_SECINFO   0x00000001
#define GROUP_SECINFO   0x00000002
#define DACL_SECINFO   0x00000004
#define SACL_SECINFO   0x00000008
#define LABEL_SECINFO   0x00000010
#define ATTRIBUTE_SECINFO   0x00000020
#define SCOPE_SECINFO   0x00000040
#define BACKUP_SECINFO   0x00010000
#define UNPROTECTED_SACL_SECINFO   0x10000000
#define UNPROTECTED_DACL_SECINFO   0x20000000
#define PROTECTED_SACL_SECINFO   0x40000000
#define PROTECTED_DACL_SECINFO   0x80000000

 
#define SL_RESTART_SCAN		0x00000001
#define SL_RETURN_SINGLE_ENTRY	0x00000002
#define SL_INDEX_SPECIFIED	0x00000004

struct smb2_query_info_req {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__u8   InfoType;
	__u8   FileInfoClass;
	__le32 OutputBufferLength;
	__le16 InputBufferOffset;
	__u16  Reserved;
	__le32 InputBufferLength;
	__le32 AdditionalInformation;
	__le32 Flags;
	__u64  PersistentFileId;
	__u64  VolatileFileId;
	__u8   Buffer[];
} __packed;

struct smb2_query_info_rsp {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 OutputBufferOffset;
	__le32 OutputBufferLength;
	__u8   Buffer[];
} __packed;

 

 
struct file_allocated_range_buffer {
	__le64	file_offset;
	__le64	length;
} __packed;

struct smb2_file_internal_info {
	__le64 IndexNumber;
} __packed;  

struct smb2_file_rename_info {  
	__u8   ReplaceIfExists;  
				 
	__u8   Reserved[7];
	__u64  RootDirectory;   
	__le32 FileNameLength;
	char   FileName[];      
	 
} __packed;  

struct smb2_file_link_info {  
	__u8   ReplaceIfExists;  
				 
	__u8   Reserved[7];
	__u64  RootDirectory;   
	__le32 FileNameLength;
	char   FileName[];      
} __packed;  

 
struct smb2_file_all_info {  
	__le64 CreationTime;	 
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le32 Attributes;
	__u32  Pad1;		 
	__le64 AllocationSize;	 
	__le64 EndOfFile;	 
	__le32 NumberOfLinks;	 
	__u8   DeletePending;
	__u8   Directory;
	__u16  Pad2;		 
	__le64 IndexNumber;
	__le32 EASize;
	__le32 AccessFlags;
	__le64 CurrentByteOffset;
	__le32 Mode;
	__le32 AlignmentRequirement;
	__le32 FileNameLength;
	union {
		char __pad;	 
		DECLARE_FLEX_ARRAY(char, FileName);
	};
} __packed;  

struct smb2_file_eof_info {  
	__le64 EndOfFile;  
} __packed;  

 
struct smb311_posix_qinfo {
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le64 EndOfFile;
	__le64 AllocationSize;
	__le32 DosAttributes;
	__le64 Inode;
	__le32 DeviceId;
	__le32 Zero;
	 
	__le32 HardLinks;
	__le32 ReparseTag;
	__le32 Mode;
	u8     Sids[];
	 
} __packed;

 
#define FS_VOLUME_INFORMATION		1  
#define FS_LABEL_INFORMATION		2  
#define FS_SIZE_INFORMATION		3  
#define FS_DEVICE_INFORMATION		4  
#define FS_ATTRIBUTE_INFORMATION	5  
#define FS_CONTROL_INFORMATION		6  
#define FS_FULL_SIZE_INFORMATION	7  
#define FS_OBJECT_ID_INFORMATION	8  
#define FS_DRIVER_PATH_INFORMATION	9  
#define FS_SECTOR_SIZE_INFORMATION	11  
#define FS_POSIX_INFORMATION		100  

struct smb2_fs_full_size_info {
	__le64 TotalAllocationUnits;
	__le64 CallerAvailableAllocationUnits;
	__le64 ActualAvailableAllocationUnits;
	__le32 SectorsPerAllocationUnit;
	__le32 BytesPerSector;
} __packed;

#define SSINFO_FLAGS_ALIGNED_DEVICE		0x00000001
#define SSINFO_FLAGS_PARTITION_ALIGNED_ON_DEVICE 0x00000002
#define SSINFO_FLAGS_NO_SEEK_PENALTY		0x00000004
#define SSINFO_FLAGS_TRIM_ENABLED		0x00000008

 
struct smb3_fs_ss_info {
	__le32 LogicalBytesPerSector;
	__le32 PhysicalBytesPerSectorForAtomicity;
	__le32 PhysicalBytesPerSectorForPerf;
	__le32 FSEffPhysicalBytesPerSectorForAtomicity;
	__le32 Flags;
	__le32 ByteOffsetForSectorAlignment;
	__le32 ByteOffsetForPartitionAlignment;
} __packed;

 
struct smb2_fs_control_info {
	__le64 FreeSpaceStartFiltering;
	__le64 FreeSpaceThreshold;
	__le64 FreeSpaceStopFiltering;
	__le64 DefaultQuotaThreshold;
	__le64 DefaultQuotaLimit;
	__le32 FileSystemControlFlags;
	__le32 Padding;
} __packed;

 
#define MAX_VOL_LABEL_LEN	32
struct smb3_fs_vol_info {
	__le64	VolumeCreationTime;
	__u32	VolumeSerialNumber;
	__le32	VolumeLabelLength;  
	__u8	SupportsObjects;  
	__u8	Reserved;
	__u8	VolumeLabel[];  
} __packed;

 
struct smb2_oplock_break {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__u8   OplockLevel;
	__u8   Reserved;
	__le32 Reserved2;
	__u64  PersistentFid;
	__u64  VolatileFid;
} __packed;

#define SMB2_NOTIFY_BREAK_LEASE_FLAG_ACK_REQUIRED cpu_to_le32(0x01)

struct smb2_lease_break {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 Epoch;
	__le32 Flags;
	__u8   LeaseKey[16];
	__le32 CurrentLeaseState;
	__le32 NewLeaseState;
	__le32 BreakReason;
	__le32 AccessMaskHint;
	__le32 ShareMaskHint;
} __packed;

struct smb2_lease_ack {
	struct smb2_hdr hdr;
	__le16 StructureSize;  
	__le16 Reserved;
	__le32 Flags;
	__u8   LeaseKey[16];
	__le32 LeaseState;
	__le64 LeaseDuration;
} __packed;

#define OP_BREAK_STRUCT_SIZE_20		24
#define OP_BREAK_STRUCT_SIZE_21		36
#endif				 
