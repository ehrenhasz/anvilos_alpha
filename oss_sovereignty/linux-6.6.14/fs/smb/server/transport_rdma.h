 
 

#ifndef __KSMBD_TRANSPORT_RDMA_H__
#define __KSMBD_TRANSPORT_RDMA_H__

#define SMBD_DEFAULT_IOSIZE (8 * 1024 * 1024)
#define SMBD_MIN_IOSIZE (512 * 1024)
#define SMBD_MAX_IOSIZE (16 * 1024 * 1024)

 
struct smb_direct_negotiate_req {
	__le16 min_version;
	__le16 max_version;
	__le16 reserved;
	__le16 credits_requested;
	__le32 preferred_send_size;
	__le32 max_receive_size;
	__le32 max_fragmented_size;
} __packed;

 
struct smb_direct_negotiate_resp {
	__le16 min_version;
	__le16 max_version;
	__le16 negotiated_version;
	__le16 reserved;
	__le16 credits_requested;
	__le16 credits_granted;
	__le32 status;
	__le32 max_readwrite_size;
	__le32 preferred_send_size;
	__le32 max_receive_size;
	__le32 max_fragmented_size;
} __packed;

#define SMB_DIRECT_RESPONSE_REQUESTED 0x0001

 
struct smb_direct_data_transfer {
	__le16 credits_requested;
	__le16 credits_granted;
	__le16 flags;
	__le16 reserved;
	__le32 remaining_data_length;
	__le32 data_offset;
	__le32 data_length;
	__le32 padding;
	__u8 buffer[];
} __packed;

#ifdef CONFIG_SMB_SERVER_SMBDIRECT
int ksmbd_rdma_init(void);
void ksmbd_rdma_destroy(void);
bool ksmbd_rdma_capable_netdev(struct net_device *netdev);
void init_smbd_max_io_size(unsigned int sz);
unsigned int get_smbd_max_read_write_size(void);
#else
static inline int ksmbd_rdma_init(void) { return 0; }
static inline int ksmbd_rdma_destroy(void) { return 0; }
static inline bool ksmbd_rdma_capable_netdev(struct net_device *netdev) { return false; }
static inline void init_smbd_max_io_size(unsigned int sz) { }
static inline unsigned int get_smbd_max_read_write_size(void) { return 0; }
#endif

#endif  
