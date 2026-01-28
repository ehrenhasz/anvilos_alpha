#ifndef _SMB2PDU_H
#define _SMB2PDU_H
#include <net/sock.h>
#include "cifsacl.h"
#define SMB2_TRANSFORM_HEADER_SIZE 52
#define MAX_SMB2_HDR_SIZE 204
#define SMB2_READWRITE_PDU_HEADER_SIZE (48 + sizeof(struct smb2_hdr))
struct smb2_rdma_transform {
	__le16 RdmaDescriptorOffset;
	__le16 RdmaDescriptorLength;
	__le32 Channel;  
	__le16 TransformCount;
	__le16 Reserved1;
	__le32 Reserved2;
} __packed;
#define SMB2_RDMA_TRANSFORM_TYPE_ENCRYPTION	0x0001
#define SMB2_RDMA_TRANSFORM_TYPE_SIGNING	0x0002
struct smb2_rdma_crypto_transform {
	__le16	TransformType;
	__le16	SignatureLength;
	__le16	NonceLength;
	__u16	Reserved;
	__u8	Signature[];  
} __packed;
#define COMPOUND_FID 0xFFFFFFFFFFFFFFFFULL
#define SMB2_SYMLINK_STRUCT_SIZE \
	(sizeof(struct smb2_err_rsp) + sizeof(struct smb2_symlink_err_rsp))
#define SYMLINK_ERROR_TAG 0x4c4d5953
struct smb2_symlink_err_rsp {
	__le32 SymLinkLength;
	__le32 SymLinkErrorTag;
	__le32 ReparseTag;
	__le16 ReparseDataLength;
	__le16 UnparsedPathLength;
	__le16 SubstituteNameOffset;
	__le16 SubstituteNameLength;
	__le16 PrintNameOffset;
	__le16 PrintNameLength;
	__le32 Flags;
	__u8  PathBuffer[];
} __packed;
struct smb2_error_context_rsp {
	__le32 ErrorDataLength;
	__le32 ErrorId;
	__u8  ErrorContextData;  
} __packed;
#define SMB2_ERROR_ID_DEFAULT		0x00000000
#define SMB2_ERROR_ID_SHARE_REDIRECT	cpu_to_le32(0x72645253)	 
#define MOVE_DST_IPADDR_V4	cpu_to_le32(0x00000001)
#define MOVE_DST_IPADDR_V6	cpu_to_le32(0x00000002)
struct move_dst_ipaddr {
	__le32 Type;
	__u32  Reserved;
	__u8   address[16];  
} __packed;
struct share_redirect_error_context_rsp {
	__le32 StructureSize;
	__le32 NotificationType;
	__le32 ResourceNameOffset;
	__le32 ResourceNameLength;
	__le16 Reserved;
	__le16 TargetType;
	__le32 IPAddrCount;
	struct move_dst_ipaddr IpAddrMoveList[];
} __packed;
#define SMB2_CREATE_IOV_SIZE 8
#define MAX_SMB2_CREATE_RESPONSE_SIZE 880
#define SMB2_LEASE_READ_CACHING_HE	0x01
#define SMB2_LEASE_HANDLE_CACHING_HE	0x02
#define SMB2_LEASE_WRITE_CACHING_HE	0x04
#define SMB2_DHANDLE_FLAG_PERSISTENT	0x00000002
struct durable_context_v2 {
	__le32 Timeout;
	__le32 Flags;
	__u64 Reserved;
	__u8 CreateGuid[16];
} __packed;
struct create_durable_v2 {
	struct create_context ccontext;
	__u8   Name[8];
	struct durable_context_v2 dcontext;
} __packed;
struct durable_reconnect_context_v2 {
	struct {
		__u64 PersistentFileId;
		__u64 VolatileFileId;
	} Fid;
	__u8 CreateGuid[16];
	__le32 Flags;  
} __packed;
struct durable_reconnect_context_v2_rsp {
	__le32 Timeout;
	__le32 Flags;  
} __packed;
struct create_durable_handle_reconnect_v2 {
	struct create_context ccontext;
	__u8   Name[8];
	struct durable_reconnect_context_v2 dcontext;
	__u8   Pad[4];
} __packed;
struct crt_twarp_ctxt {
	struct create_context ccontext;
	__u8	Name[8];
	__le64	Timestamp;
} __packed;
struct crt_query_id_ctxt {
	struct create_context ccontext;
	__u8	Name[8];
} __packed;
struct crt_sd_ctxt {
	struct create_context ccontext;
	__u8	Name[8];
	struct smb3_sd sd;
} __packed;
#define COPY_CHUNK_RES_KEY_SIZE	24
struct resume_key_req {
	char ResumeKey[COPY_CHUNK_RES_KEY_SIZE];
	__le32	ContextLength;	 
	char	Context[];	 
} __packed;
struct copychunk_ioctl {
	char SourceKey[COPY_CHUNK_RES_KEY_SIZE];
	__le32 ChunkCount;  
	__le32 Reserved;
	__le64 SourceOffset;
	__le64 TargetOffset;
	__le32 Length;  
	__u32 Reserved2;
} __packed;
struct copychunk_ioctl_rsp {
	__le32 ChunksWritten;
	__le32 ChunkBytesWritten;
	__le32 TotalBytesWritten;
} __packed;
struct get_retrieval_pointer_count_req {
	__le64 StartingVcn;  
} __packed;
struct get_retrieval_pointer_count_rsp {
	__le32 ExtentCount;
} __packed;
struct smb3_extents {
	__le64 NextVcn;
	__le64 Lcn;  
} __packed;
struct get_retrieval_pointers_refcount_rsp {
	__le32 ExtentCount;
	__u32  Reserved;
	__le64 StartingVcn;
	struct smb3_extents extents[];
} __packed;
struct fsctl_get_dfs_referral_req {
	__le16 MaxReferralLevel;
	__u8 RequestFileName[];
} __packed;
struct network_resiliency_req {
	__le32 Timeout;
	__le32 Reserved;
} __packed;
#define RSS_CAPABLE	cpu_to_le32(0x00000001)
#define RDMA_CAPABLE	cpu_to_le32(0x00000002)
#define INTERNETWORK	cpu_to_le16(0x0002)
#define INTERNETWORKV6	cpu_to_le16(0x0017)
struct network_interface_info_ioctl_rsp {
	__le32 Next;  
	__le32 IfIndex;
	__le32 Capability;  
	__le32 Reserved;
	__le64 LinkSpeed;
	__le16 Family;
	__u8 Buffer[126];
} __packed;
struct iface_info_ipv4 {
	__be16 Port;
	__be32 IPv4Address;
	__be64 Reserved;
} __packed;
struct iface_info_ipv6 {
	__be16 Port;
	__be32 FlowInfo;
	__u8   IPv6Address[16];
	__be32 ScopeId;
} __packed;
#define NO_FILE_ID 0xFFFFFFFFFFFFFFFFULL  
struct compress_ioctl {
	__le16 CompressionState;  
} __packed;
#define SMB2_IOCTL_IOV_SIZE 2
struct smb2_file_full_ea_info {  
	__le32 next_entry_offset;
	__u8   flags;
	__u8   ea_name_length;
	__le16 ea_value_length;
	char   ea_data[];  
} __packed;  
struct smb2_file_reparse_point_info {
	__le64 IndexNumber;
	__le32 Tag;
} __packed;
struct smb2_file_network_open_info {
	struct_group(network_open_info,
		__le64 CreationTime;
		__le64 LastAccessTime;
		__le64 LastWriteTime;
		__le64 ChangeTime;
		__le64 AllocationSize;
		__le64 EndOfFile;
		__le32 Attributes;
	);
	__le32 Reserved;
} __packed;  
struct smb2_file_id_information {
	__le64	VolumeSerialNumber;
	__u64  PersistentFileId;  
	__u64  VolatileFileId;  
} __packed;  
struct smb2_file_id_extd_directory_info {
	__le32 NextEntryOffset;
	__u32 FileIndex;
	__le64 CreationTime;
	__le64 LastAccessTime;
	__le64 LastWriteTime;
	__le64 ChangeTime;
	__le64 EndOfFile;
	__le64 AllocationSize;
	__le32 FileAttributes;
	__le32 FileNameLength;
	__le32 EaSize;  
	__le32 ReparsePointTag;  
	__le64 UniqueId;  
	char FileName[];
} __packed;  
extern char smb2_padding[7];
struct create_posix_rsp {
	u32 nlink;
	u32 reparse_tag;
	u32 mode;
	struct cifs_sid owner;  
	struct cifs_sid group;  
} __packed;
#define SMB2_QUERY_DIRECTORY_IOV_SIZE 2
struct smb2_posix_info {
	__le32 NextEntryOffset;
	__u32 Ignored;
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
} __packed;
struct smb2_posix_info_parsed {
	const struct smb2_posix_info *base;
	size_t size;
	struct cifs_sid owner;
	struct cifs_sid group;
	int name_len;
	const u8 *name;
};
#endif				 
