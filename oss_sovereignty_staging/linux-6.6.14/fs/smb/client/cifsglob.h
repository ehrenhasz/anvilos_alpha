 
 
#ifndef _CIFS_GLOB_H
#define _CIFS_GLOB_H

#include <linux/in.h>
#include <linux/in6.h>
#include <linux/inet.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/mm.h>
#include <linux/mempool.h>
#include <linux/workqueue.h>
#include <linux/utsname.h>
#include <linux/sched/mm.h>
#include <linux/netfs.h>
#include "cifs_fs_sb.h"
#include "cifsacl.h"
#include <crypto/internal/hash.h>
#include <uapi/linux/cifs/cifs_mount.h>
#include "../common/smb2pdu.h"
#include "smb2pdu.h"
#include <linux/filelock.h>

#define SMB_PATH_MAX 260
#define CIFS_PORT 445
#define RFC1001_PORT 139

 
#define MAX_UID_INFO 16
#define MAX_SES_INFO 2
#define MAX_TCON_INFO 4

#define MAX_TREE_SIZE (2 + CIFS_NI_MAXHOST + 1 + CIFS_MAX_SHARE_LEN + 1)

#define CIFS_MIN_RCV_POOL 4

#define MAX_REOPEN_ATT	5  
 
#define CIFS_DEF_ACTIMEO (1 * HZ)

 
#define CIFS_MAX_ACTIMEO (1 << 30)

 
#define SMB3_MAX_HANDLE_TIMEOUT 960000

 
#define CIFS_MAX_REQ 32767

#define RFC1001_NAME_LEN 15
#define RFC1001_NAME_LEN_WITH_NULL (RFC1001_NAME_LEN + 1)

 
#define SERVER_NAME_LENGTH 80
#define SERVER_NAME_LEN_WITH_NULL     (SERVER_NAME_LENGTH + 1)

 
#define SMB_ECHO_INTERVAL_MIN 1
#define SMB_ECHO_INTERVAL_MAX 600
#define SMB_ECHO_INTERVAL_DEFAULT 60

 
#define SMB_INTERFACE_POLL_INTERVAL	600

 
#define MAX_COMPOUND 5

 
#define SMB2_MAX_CREDITS_AVAILABLE 32000

#include "cifspdu.h"

#ifndef XATTR_DOS_ATTRIB
#define XATTR_DOS_ATTRIB "user.DOSATTRIB"
#endif

#define CIFS_MAX_WORKSTATION_LEN  (__NEW_UTS_LEN + 1)   

#define CIFS_DFS_ROOT_SES(ses) ((ses)->dfs_root_ses ?: (ses))

 

 
enum statusEnum {
	CifsNew = 0,
	CifsGood,
	CifsExiting,
	CifsNeedReconnect,
	CifsNeedNegotiate,
	CifsInNegotiate,
};

 
enum ses_status_enum {
	SES_NEW = 0,
	SES_GOOD,
	SES_EXITING,
	SES_NEED_RECON,
	SES_IN_SETUP
};

 
enum tid_status_enum {
	TID_NEW = 0,
	TID_GOOD,
	TID_EXITING,
	TID_NEED_RECON,
	TID_NEED_TCON,
	TID_IN_TCON,
	TID_NEED_FILES_INVALIDATE,  
	TID_IN_FILES_INVALIDATE
};

enum securityEnum {
	Unspecified = 0,	 
	NTLMv2,			 
	RawNTLMSSP,		 
	Kerberos,		 
};

struct session_key {
	unsigned int len;
	char *response;
};

 
struct cifs_secmech {
	struct shash_desc *hmacmd5;  
	struct shash_desc *md5;  
	struct shash_desc *hmacsha256;  
	struct shash_desc *sha512;  
	struct shash_desc *aes_cmac;  

	struct crypto_aead *enc;  
	struct crypto_aead *dec;  
};

 
struct ntlmssp_auth {
	bool sesskey_per_smbsess;  
	__u32 client_flags;  
	__u32 server_flags;  
	unsigned char ciphertext[CIFS_CPHTXT_SIZE];  
	char cryptkey[CIFS_CRYPTO_KEY_SIZE];  
};

struct cifs_cred {
	int uid;
	int gid;
	int mode;
	int cecount;
	struct cifs_sid osid;
	struct cifs_sid gsid;
	struct cifs_ntace *ntaces;
	struct cifs_ace *aces;
};

struct cifs_open_info_data {
	bool adjust_tz;
	union {
		bool reparse_point;
		bool symlink;
	};
	struct {
		__u32 tag;
		union {
			struct reparse_data_buffer *buf;
			struct reparse_posix_data *posix;
		};
	} reparse;
	char *symlink_target;
	union {
		struct smb2_file_all_info fi;
		struct smb311_posix_qinfo posix_fi;
	};
};

#define cifs_open_data_reparse(d) \
	((d)->reparse_point || \
	 (le32_to_cpu((d)->fi.Attributes) & ATTR_REPARSE))

static inline void cifs_free_open_info(struct cifs_open_info_data *data)
{
	kfree(data->symlink_target);
}

 

 
struct smb_rqst {
	struct kvec	*rq_iov;	 
	unsigned int	rq_nvec;	 
	size_t		rq_iter_size;	 
	struct iov_iter	rq_iter;	 
	struct xarray	rq_buffer;	 
};

struct mid_q_entry;
struct TCP_Server_Info;
struct cifsFileInfo;
struct cifs_ses;
struct cifs_tcon;
struct dfs_info3_param;
struct cifs_fattr;
struct smb3_fs_context;
struct cifs_fid;
struct cifs_readdata;
struct cifs_writedata;
struct cifs_io_parms;
struct cifs_search_info;
struct cifsInodeInfo;
struct cifs_open_parms;
struct cifs_credits;

struct smb_version_operations {
	int (*send_cancel)(struct TCP_Server_Info *, struct smb_rqst *,
			   struct mid_q_entry *);
	bool (*compare_fids)(struct cifsFileInfo *, struct cifsFileInfo *);
	 
	struct mid_q_entry *(*setup_request)(struct cifs_ses *,
					     struct TCP_Server_Info *,
					     struct smb_rqst *);
	 
	struct mid_q_entry *(*setup_async_request)(struct TCP_Server_Info *,
						struct smb_rqst *);
	 
	int (*check_receive)(struct mid_q_entry *, struct TCP_Server_Info *,
			     bool);
	void (*add_credits)(struct TCP_Server_Info *server,
			    const struct cifs_credits *credits,
			    const int optype);
	void (*set_credits)(struct TCP_Server_Info *, const int);
	int * (*get_credits_field)(struct TCP_Server_Info *, const int);
	unsigned int (*get_credits)(struct mid_q_entry *);
	__u64 (*get_next_mid)(struct TCP_Server_Info *);
	void (*revert_current_mid)(struct TCP_Server_Info *server,
				   const unsigned int val);
	 
	unsigned int (*read_data_offset)(char *);
	 
	unsigned int (*read_data_length)(char *, bool in_remaining);
	 
	int (*map_error)(char *, bool);
	 
	struct mid_q_entry * (*find_mid)(struct TCP_Server_Info *, char *);
	void (*dump_detail)(void *buf, struct TCP_Server_Info *ptcp_info);
	void (*clear_stats)(struct cifs_tcon *);
	void (*print_stats)(struct seq_file *m, struct cifs_tcon *);
	void (*dump_share_caps)(struct seq_file *, struct cifs_tcon *);
	 
	int (*check_message)(char *, unsigned int, struct TCP_Server_Info *);
	bool (*is_oplock_break)(char *, struct TCP_Server_Info *);
	int (*handle_cancelled_mid)(struct mid_q_entry *, struct TCP_Server_Info *);
	void (*downgrade_oplock)(struct TCP_Server_Info *server,
				 struct cifsInodeInfo *cinode, __u32 oplock,
				 unsigned int epoch, bool *purge_cache);
	 
	bool (*check_trans2)(struct mid_q_entry *, struct TCP_Server_Info *,
			     char *, int);
	 
	bool (*need_neg)(struct TCP_Server_Info *);
	 
	int (*negotiate)(const unsigned int xid,
			 struct cifs_ses *ses,
			 struct TCP_Server_Info *server);
	 
	unsigned int (*negotiate_wsize)(struct cifs_tcon *tcon, struct smb3_fs_context *ctx);
	 
	unsigned int (*negotiate_rsize)(struct cifs_tcon *tcon, struct smb3_fs_context *ctx);
	 
	int (*sess_setup)(const unsigned int, struct cifs_ses *,
			  struct TCP_Server_Info *server,
			  const struct nls_table *);
	 
	int (*logoff)(const unsigned int, struct cifs_ses *);
	 
	int (*tree_connect)(const unsigned int, struct cifs_ses *, const char *,
			    struct cifs_tcon *, const struct nls_table *);
	 
	int (*tree_disconnect)(const unsigned int, struct cifs_tcon *);
	 
	int (*get_dfs_refer)(const unsigned int, struct cifs_ses *,
			     const char *, struct dfs_info3_param **,
			     unsigned int *, const struct nls_table *, int);
	 
	void (*qfs_tcon)(const unsigned int, struct cifs_tcon *,
			 struct cifs_sb_info *);
	 
	int (*is_path_accessible)(const unsigned int, struct cifs_tcon *,
				  struct cifs_sb_info *, const char *);
	 
	int (*query_path_info)(const unsigned int xid,
			       struct cifs_tcon *tcon,
			       struct cifs_sb_info *cifs_sb,
			       const char *full_path,
			       struct cifs_open_info_data *data);
	 
	int (*query_file_info)(const unsigned int xid, struct cifs_tcon *tcon,
			       struct cifsFileInfo *cfile, struct cifs_open_info_data *data);
	 
	int (*query_reparse_point)(const unsigned int xid,
				   struct cifs_tcon *tcon,
				   struct cifs_sb_info *cifs_sb,
				   const char *full_path,
				   u32 *tag, struct kvec *rsp,
				   int *rsp_buftype);
	 
	int (*get_srv_inum)(const unsigned int xid, struct cifs_tcon *tcon,
			    struct cifs_sb_info *cifs_sb, const char *full_path, u64 *uniqueid,
			    struct cifs_open_info_data *data);
	 
	int (*set_path_size)(const unsigned int, struct cifs_tcon *,
			     const char *, __u64, struct cifs_sb_info *, bool);
	 
	int (*set_file_size)(const unsigned int, struct cifs_tcon *,
			     struct cifsFileInfo *, __u64, bool);
	 
	int (*set_file_info)(struct inode *, const char *, FILE_BASIC_INFO *,
			     const unsigned int);
	int (*set_compression)(const unsigned int, struct cifs_tcon *,
			       struct cifsFileInfo *);
	 
	bool (*can_echo)(struct TCP_Server_Info *);
	 
	int (*echo)(struct TCP_Server_Info *);
	 
	int (*posix_mkdir)(const unsigned int xid, struct inode *inode,
			umode_t mode, struct cifs_tcon *tcon,
			const char *full_path,
			struct cifs_sb_info *cifs_sb);
	int (*mkdir)(const unsigned int xid, struct inode *inode, umode_t mode,
		     struct cifs_tcon *tcon, const char *name,
		     struct cifs_sb_info *sb);
	 
	void (*mkdir_setinfo)(struct inode *, const char *,
			      struct cifs_sb_info *, struct cifs_tcon *,
			      const unsigned int);
	 
	int (*rmdir)(const unsigned int, struct cifs_tcon *, const char *,
		     struct cifs_sb_info *);
	 
	int (*unlink)(const unsigned int, struct cifs_tcon *, const char *,
		      struct cifs_sb_info *);
	 
	int (*rename_pending_delete)(const char *, struct dentry *,
				     const unsigned int);
	 
	int (*rename)(const unsigned int, struct cifs_tcon *, const char *,
		      const char *, struct cifs_sb_info *);
	 
	int (*create_hardlink)(const unsigned int, struct cifs_tcon *,
			       const char *, const char *,
			       struct cifs_sb_info *);
	 
	int (*query_symlink)(const unsigned int xid,
			     struct cifs_tcon *tcon,
			     struct cifs_sb_info *cifs_sb,
			     const char *full_path,
			     char **target_path);
	 
	int (*open)(const unsigned int xid, struct cifs_open_parms *oparms, __u32 *oplock,
		    void *buf);
	 
	void (*set_fid)(struct cifsFileInfo *, struct cifs_fid *, __u32);
	 
	void (*close)(const unsigned int, struct cifs_tcon *,
		      struct cifs_fid *);
	 
	void (*close_getattr)(const unsigned int xid, struct cifs_tcon *tcon,
		      struct cifsFileInfo *pfile_info);
	 
	int (*flush)(const unsigned int, struct cifs_tcon *, struct cifs_fid *);
	 
	int (*async_readv)(struct cifs_readdata *);
	 
	int (*async_writev)(struct cifs_writedata *,
			    void (*release)(struct kref *));
	 
	int (*sync_read)(const unsigned int, struct cifs_fid *,
			 struct cifs_io_parms *, unsigned int *, char **,
			 int *);
	 
	int (*sync_write)(const unsigned int, struct cifs_fid *,
			  struct cifs_io_parms *, unsigned int *, struct kvec *,
			  unsigned long);
	 
	int (*query_dir_first)(const unsigned int, struct cifs_tcon *,
			       const char *, struct cifs_sb_info *,
			       struct cifs_fid *, __u16,
			       struct cifs_search_info *);
	 
	int (*query_dir_next)(const unsigned int, struct cifs_tcon *,
			      struct cifs_fid *,
			      __u16, struct cifs_search_info *srch_inf);
	 
	int (*close_dir)(const unsigned int, struct cifs_tcon *,
			 struct cifs_fid *);
	 
	unsigned int (*calc_smb_size)(void *buf);
	 
	bool (*is_status_pending)(char *buf, struct TCP_Server_Info *server);
	 
	bool (*is_session_expired)(char *);
	 
	int (*oplock_response)(struct cifs_tcon *tcon, __u64 persistent_fid, __u64 volatile_fid,
			__u16 net_fid, struct cifsInodeInfo *cifs_inode);
	 
	int (*queryfs)(const unsigned int, struct cifs_tcon *,
		       struct cifs_sb_info *, struct kstatfs *);
	 
	int (*mand_lock)(const unsigned int, struct cifsFileInfo *, __u64,
			 __u64, __u32, int, int, bool);
	 
	int (*mand_unlock_range)(struct cifsFileInfo *, struct file_lock *,
				 const unsigned int);
	 
	int (*push_mand_locks)(struct cifsFileInfo *);
	 
	void (*get_lease_key)(struct inode *, struct cifs_fid *);
	 
	void (*set_lease_key)(struct inode *, struct cifs_fid *);
	 
	void (*new_lease_key)(struct cifs_fid *);
	int (*generate_signingkey)(struct cifs_ses *ses,
				   struct TCP_Server_Info *server);
	int (*calc_signature)(struct smb_rqst *, struct TCP_Server_Info *,
				bool allocate_crypto);
	int (*set_integrity)(const unsigned int, struct cifs_tcon *tcon,
			     struct cifsFileInfo *src_file);
	int (*enum_snapshots)(const unsigned int xid, struct cifs_tcon *tcon,
			     struct cifsFileInfo *src_file, void __user *);
	int (*notify)(const unsigned int xid, struct file *pfile,
			     void __user *pbuf, bool return_changes);
	int (*query_mf_symlink)(unsigned int, struct cifs_tcon *,
				struct cifs_sb_info *, const unsigned char *,
				char *, unsigned int *);
	int (*create_mf_symlink)(unsigned int, struct cifs_tcon *,
				 struct cifs_sb_info *, const unsigned char *,
				 char *, unsigned int *);
	 
	bool (*is_read_op)(__u32);
	 
	void (*set_oplock_level)(struct cifsInodeInfo *, __u32, unsigned int,
				 bool *);
	 
	char * (*create_lease_buf)(u8 *lease_key, u8 oplock);
	 
	__u8 (*parse_lease_buf)(void *buf, unsigned int *epoch, char *lkey);
	ssize_t (*copychunk_range)(const unsigned int,
			struct cifsFileInfo *src_file,
			struct cifsFileInfo *target_file,
			u64 src_off, u64 len, u64 dest_off);
	int (*duplicate_extents)(const unsigned int, struct cifsFileInfo *src,
			struct cifsFileInfo *target_file, u64 src_off, u64 len,
			u64 dest_off);
	int (*validate_negotiate)(const unsigned int, struct cifs_tcon *);
	ssize_t (*query_all_EAs)(const unsigned int, struct cifs_tcon *,
			const unsigned char *, const unsigned char *, char *,
			size_t, struct cifs_sb_info *);
	int (*set_EA)(const unsigned int, struct cifs_tcon *, const char *,
			const char *, const void *, const __u16,
			const struct nls_table *, struct cifs_sb_info *);
	struct cifs_ntsd * (*get_acl)(struct cifs_sb_info *, struct inode *,
			const char *, u32 *, u32);
	struct cifs_ntsd * (*get_acl_by_fid)(struct cifs_sb_info *,
			const struct cifs_fid *, u32 *, u32);
	int (*set_acl)(struct cifs_ntsd *, __u32, struct inode *, const char *,
			int);
	 
	unsigned int (*wp_retry_size)(struct inode *);
	 
	int (*wait_mtu_credits)(struct TCP_Server_Info *, unsigned int,
				unsigned int *, struct cifs_credits *);
	 
	int (*adjust_credits)(struct TCP_Server_Info *server,
			      struct cifs_credits *credits,
			      const unsigned int payload_size);
	 
	bool (*dir_needs_close)(struct cifsFileInfo *);
	long (*fallocate)(struct file *, struct cifs_tcon *, int, loff_t,
			  loff_t);
	 
	int (*init_transform_rq)(struct TCP_Server_Info *, int num_rqst,
				 struct smb_rqst *, struct smb_rqst *);
	int (*is_transform_hdr)(void *buf);
	int (*receive_transform)(struct TCP_Server_Info *,
				 struct mid_q_entry **, char **, int *);
	enum securityEnum (*select_sectype)(struct TCP_Server_Info *,
			    enum securityEnum);
	int (*next_header)(struct TCP_Server_Info *server, char *buf,
			   unsigned int *noff);
	 
	int (*ioctl_query_info)(const unsigned int xid,
				struct cifs_tcon *tcon,
				struct cifs_sb_info *cifs_sb,
				__le16 *path, int is_dir,
				unsigned long p);
	 
	int (*make_node)(unsigned int xid,
			 struct inode *inode,
			 struct dentry *dentry,
			 struct cifs_tcon *tcon,
			 const char *full_path,
			 umode_t mode,
			 dev_t device_number);
	 
	int (*fiemap)(struct cifs_tcon *tcon, struct cifsFileInfo *,
		      struct fiemap_extent_info *, u64, u64);
	 
	loff_t (*llseek)(struct file *, struct cifs_tcon *, loff_t, int);
	 
	bool (*is_status_io_timeout)(char *buf);
	 
	bool (*is_network_name_deleted)(char *buf, struct TCP_Server_Info *srv);
	int (*parse_reparse_point)(struct cifs_sb_info *cifs_sb,
				   struct kvec *rsp_iov,
				   struct cifs_open_info_data *data);
};

struct smb_version_values {
	char		*version_string;
	__u16		protocol_id;
	__u32		req_capabilities;
	__u32		large_lock_type;
	__u32		exclusive_lock_type;
	__u32		shared_lock_type;
	__u32		unlock_lock_type;
	size_t		header_preamble_size;
	size_t		header_size;
	size_t		max_header_size;
	size_t		read_rsp_size;
	__le16		lock_cmd;
	unsigned int	cap_unix;
	unsigned int	cap_nt_find;
	unsigned int	cap_large_files;
	__u16		signing_enabled;
	__u16		signing_required;
	size_t		create_lease_size;
};

#define HEADER_SIZE(server) (server->vals->header_size)
#define MAX_HEADER_SIZE(server) (server->vals->max_header_size)
#define HEADER_PREAMBLE_SIZE(server) (server->vals->header_preamble_size)
#define MID_HEADER_SIZE(server) (HEADER_SIZE(server) - 1 - HEADER_PREAMBLE_SIZE(server))

 
#define CIFS_MOUNT_MASK (CIFS_MOUNT_NO_PERM | CIFS_MOUNT_SET_UID | \
			 CIFS_MOUNT_SERVER_INUM | CIFS_MOUNT_DIRECT_IO | \
			 CIFS_MOUNT_NO_XATTR | CIFS_MOUNT_MAP_SPECIAL_CHR | \
			 CIFS_MOUNT_MAP_SFM_CHR | \
			 CIFS_MOUNT_UNX_EMUL | CIFS_MOUNT_NO_BRL | \
			 CIFS_MOUNT_CIFS_ACL | CIFS_MOUNT_OVERR_UID | \
			 CIFS_MOUNT_OVERR_GID | CIFS_MOUNT_DYNPERM | \
			 CIFS_MOUNT_NOPOSIXBRL | CIFS_MOUNT_NOSSYNC | \
			 CIFS_MOUNT_FSCACHE | CIFS_MOUNT_MF_SYMLINKS | \
			 CIFS_MOUNT_MULTIUSER | CIFS_MOUNT_STRICT_IO | \
			 CIFS_MOUNT_CIFS_BACKUPUID | CIFS_MOUNT_CIFS_BACKUPGID | \
			 CIFS_MOUNT_UID_FROM_ACL | CIFS_MOUNT_NO_HANDLE_CACHE | \
			 CIFS_MOUNT_NO_DFS | CIFS_MOUNT_MODE_FROM_SID | \
			 CIFS_MOUNT_RO_CACHE | CIFS_MOUNT_RW_CACHE)

 
#define CIFS_MS_MASK (SB_RDONLY | SB_MANDLOCK | SB_NOEXEC | SB_NOSUID | \
		      SB_NODEV | SB_SYNCHRONOUS)

struct cifs_mnt_data {
	struct cifs_sb_info *cifs_sb;
	struct smb3_fs_context *ctx;
	int flags;
};

static inline unsigned int
get_rfc1002_length(void *buf)
{
	return be32_to_cpu(*((__be32 *)buf)) & 0xffffff;
}

static inline void
inc_rfc1001_len(void *buf, int count)
{
	be32_add_cpu((__be32 *)buf, count);
}

struct TCP_Server_Info {
	struct list_head tcp_ses_list;
	struct list_head smb_ses_list;
	spinlock_t srv_lock;   
	__u64 conn_id;  
	int srv_count;  
	 
	char server_RFC1001_name[RFC1001_NAME_LEN_WITH_NULL];
	struct smb_version_operations	*ops;
	struct smb_version_values	*vals;
	 
	enum statusEnum tcpStatus;  
	char *hostname;  
	struct socket *ssocket;
	struct sockaddr_storage dstaddr;
	struct sockaddr_storage srcaddr;  
#ifdef CONFIG_NET_NS
	struct net *net;
#endif
	wait_queue_head_t response_q;
	wait_queue_head_t request_q;  
	spinlock_t mid_lock;   
	struct list_head pending_mid_q;
	bool noblocksnd;		 
	bool noautotune;		 
	bool nosharesock;
	bool tcp_nodelay;
	unsigned int credits;   
	unsigned int max_credits;  
	unsigned int in_flight;   
	unsigned int max_in_flight;  
	spinlock_t req_lock;   
	struct mutex _srv_mutex;
	unsigned int nofs_flag;
	struct task_struct *tsk;
	char server_GUID[16];
	__u16 sec_mode;
	bool sign;  
	bool ignore_signature:1;  
	bool session_estab;  
	int echo_credits;   
	int oplock_credits;   
	bool echoes:1;  
	__u8 client_guid[SMB2_CLIENT_GUID_SIZE];  
	u16 dialect;  
	bool oplocks:1;  
	unsigned int maxReq;	 
	 
	 
	unsigned int maxBuf;	 
	 
	 
	 
	unsigned int max_rw;	 
	 
	 
	unsigned int capabilities;  
	int timeAdj;   
	__u64 CurrentMid;          
	char cryptkey[CIFS_CRYPTO_KEY_SIZE];  
	 
	char workstation_RFC1001_name[RFC1001_NAME_LEN_WITH_NULL];
	__u32 sequence_number;  
	__u32 reconnect_instance;  
	struct session_key session_key;
	unsigned long lstrp;  
	struct cifs_secmech secmech;  
#define	CIFS_NEGFLAVOR_UNENCAP	1	 
#define	CIFS_NEGFLAVOR_EXTENDED	2	 
	char	negflavor;	 
	 
	bool	sec_ntlmssp;		 
	bool	sec_kerberosu2u;	 
	bool	sec_kerberos;		 
	bool	sec_mskerberos;		 
	bool	large_buf;		 
	 
	bool	rdma;
	 
	struct smbd_connection *smbd_conn;
	struct delayed_work	echo;  
	char	*smallbuf;	 
	char	*bigbuf;	 
	 
	unsigned int pdu_size;
	unsigned int total_read;  
	atomic_t in_send;  
	atomic_t num_waiters;    
#ifdef CONFIG_CIFS_STATS2
	atomic_t num_cmds[NUMBER_OF_SMB2_COMMANDS];  
	atomic_t smb2slowcmd[NUMBER_OF_SMB2_COMMANDS];  
	__u64 time_per_cmd[NUMBER_OF_SMB2_COMMANDS];  
	__u32 slowest_cmd[NUMBER_OF_SMB2_COMMANDS];
	__u32 fastest_cmd[NUMBER_OF_SMB2_COMMANDS];
#endif  
	unsigned int	max_read;
	unsigned int	max_write;
	unsigned int	min_offload;
	__le16	compress_algorithm;
	__u16	signing_algorithm;
	__le16	cipher_type;
	  
	__u8	preauth_sha_hash[SMB2_PREAUTH_HASH_SIZE];
	bool	signing_negotiated;  
	bool	posix_ext_supported;
	struct delayed_work reconnect;  
	struct mutex reconnect_mutex;  
	unsigned long echo_interval;

	 
	int nr_targets;
	bool noblockcnt;  

	 
#define SERVER_IS_CHAN(server)	(!!(server)->primary_server)
	struct TCP_Server_Info *primary_server;
	__u16 channel_sequence_num;   

#ifdef CONFIG_CIFS_SWN_UPCALL
	bool use_swn_dstaddr;
	struct sockaddr_storage swn_dstaddr;
#endif
	struct mutex refpath_lock;  
	 
	char *leaf_fullpath;
};

static inline bool is_smb1(struct TCP_Server_Info *server)
{
	return HEADER_PREAMBLE_SIZE(server) != 0;
}

static inline void cifs_server_lock(struct TCP_Server_Info *server)
{
	unsigned int nofs_flag = memalloc_nofs_save();

	mutex_lock(&server->_srv_mutex);
	server->nofs_flag = nofs_flag;
}

static inline void cifs_server_unlock(struct TCP_Server_Info *server)
{
	unsigned int nofs_flag = server->nofs_flag;

	mutex_unlock(&server->_srv_mutex);
	memalloc_nofs_restore(nofs_flag);
}

struct cifs_credits {
	unsigned int value;
	unsigned int instance;
};

static inline unsigned int
in_flight(struct TCP_Server_Info *server)
{
	unsigned int num;

	spin_lock(&server->req_lock);
	num = server->in_flight;
	spin_unlock(&server->req_lock);
	return num;
}

static inline bool
has_credits(struct TCP_Server_Info *server, int *credits, int num_credits)
{
	int num;

	spin_lock(&server->req_lock);
	num = *credits;
	spin_unlock(&server->req_lock);
	return num >= num_credits;
}

static inline void
add_credits(struct TCP_Server_Info *server, const struct cifs_credits *credits,
	    const int optype)
{
	server->ops->add_credits(server, credits, optype);
}

static inline void
add_credits_and_wake_if(struct TCP_Server_Info *server,
			const struct cifs_credits *credits, const int optype)
{
	if (credits->value) {
		server->ops->add_credits(server, credits, optype);
		wake_up(&server->request_q);
	}
}

static inline void
set_credits(struct TCP_Server_Info *server, const int val)
{
	server->ops->set_credits(server, val);
}

static inline int
adjust_credits(struct TCP_Server_Info *server, struct cifs_credits *credits,
	       const unsigned int payload_size)
{
	return server->ops->adjust_credits ?
		server->ops->adjust_credits(server, credits, payload_size) : 0;
}

static inline __le64
get_next_mid64(struct TCP_Server_Info *server)
{
	return cpu_to_le64(server->ops->get_next_mid(server));
}

static inline __le16
get_next_mid(struct TCP_Server_Info *server)
{
	__u16 mid = server->ops->get_next_mid(server);
	 
	return cpu_to_le16(mid);
}

static inline void
revert_current_mid(struct TCP_Server_Info *server, const unsigned int val)
{
	if (server->ops->revert_current_mid)
		server->ops->revert_current_mid(server, val);
}

static inline void
revert_current_mid_from_hdr(struct TCP_Server_Info *server,
			    const struct smb2_hdr *shdr)
{
	unsigned int num = le16_to_cpu(shdr->CreditCharge);

	return revert_current_mid(server, num > 0 ? num : 1);
}

static inline __u16
get_mid(const struct smb_hdr *smb)
{
	return le16_to_cpu(smb->Mid);
}

static inline bool
compare_mid(__u16 mid, const struct smb_hdr *smb)
{
	return mid == le16_to_cpu(smb->Mid);
}

 
#define CIFS_MAX_WSIZE ((1<<24) - 1 - sizeof(WRITE_REQ) + 4)
#define CIFS_MAX_RSIZE ((1<<24) - sizeof(READ_RSP) + 4)

 
#define CIFS_MAX_RFC1002_WSIZE ((1<<17) - 1 - sizeof(WRITE_REQ) + 4)
#define CIFS_MAX_RFC1002_RSIZE ((1<<17) - 1 - sizeof(READ_RSP) + 4)

#define CIFS_DEFAULT_IOSIZE (1024 * 1024)

 
#define CIFS_DEFAULT_NON_POSIX_RSIZE (60 * 1024)
#define CIFS_DEFAULT_NON_POSIX_WSIZE (65536)

 

#ifdef CONFIG_NET_NS

static inline struct net *cifs_net_ns(struct TCP_Server_Info *srv)
{
	return srv->net;
}

static inline void cifs_set_net_ns(struct TCP_Server_Info *srv, struct net *net)
{
	srv->net = net;
}

#else

static inline struct net *cifs_net_ns(struct TCP_Server_Info *srv)
{
	return &init_net;
}

static inline void cifs_set_net_ns(struct TCP_Server_Info *srv, struct net *net)
{
}

#endif

struct cifs_server_iface {
	struct list_head iface_head;
	struct kref refcount;
	size_t speed;
	size_t weight_fulfilled;
	unsigned int num_channels;
	unsigned int rdma_capable : 1;
	unsigned int rss_capable : 1;
	unsigned int is_active : 1;  
	struct sockaddr_storage sockaddr;
};

 
static inline void
release_iface(struct kref *ref)
{
	struct cifs_server_iface *iface = container_of(ref,
						       struct cifs_server_iface,
						       refcount);
	kfree(iface);
}

struct cifs_chan {
	unsigned int in_reconnect : 1;  
	struct TCP_Server_Info *server;
	struct cifs_server_iface *iface;  
	__u8 signkey[SMB3_SIGN_KEY_SIZE];
};

 
struct cifs_ses {
	struct list_head smb_ses_list;
	struct list_head rlist;  
	struct list_head tcon_list;
	struct cifs_tcon *tcon_ipc;
	spinlock_t ses_lock;   
	struct mutex session_mutex;
	struct TCP_Server_Info *server;	 
	int ses_count;		 
	enum ses_status_enum ses_status;   
	unsigned int overrideSecFlg;  
	char *serverOS;		 
	char *serverNOS;	 
	char *serverDomain;	 
	__u64 Suid;		 
	kuid_t linux_uid;	 
	kuid_t cred_uid;	 
	unsigned int capabilities;
	char ip_addr[INET6_ADDRSTRLEN + 1];  
	char *user_name;	 
	char *domainName;
	char *password;
	char workstation_name[CIFS_MAX_WORKSTATION_LEN];
	struct session_key auth_key;
	struct ntlmssp_auth *ntlmssp;  
	enum securityEnum sectype;  
	bool sign;		 
	bool domainAuto:1;
	__u16 session_flags;
	__u8 smb3signingkey[SMB3_SIGN_KEY_SIZE];
	__u8 smb3encryptionkey[SMB3_ENC_DEC_KEY_SIZE];
	__u8 smb3decryptionkey[SMB3_ENC_DEC_KEY_SIZE];
	__u8 preauth_sha_hash[SMB2_PREAUTH_HASH_SIZE];

	 
	spinlock_t iface_lock;
	 
	struct list_head iface_list;
	size_t iface_count;
	unsigned long iface_last_update;  
	 

	spinlock_t chan_lock;
	 
#define CIFS_MAX_CHANNELS 16
#define CIFS_ALL_CHANNELS_SET(ses)	\
	((1UL << (ses)->chan_count) - 1)
#define CIFS_ALL_CHANS_GOOD(ses)		\
	(!(ses)->chans_need_reconnect)
#define CIFS_ALL_CHANS_NEED_RECONNECT(ses)	\
	((ses)->chans_need_reconnect == CIFS_ALL_CHANNELS_SET(ses))
#define CIFS_SET_ALL_CHANS_NEED_RECONNECT(ses)	\
	((ses)->chans_need_reconnect = CIFS_ALL_CHANNELS_SET(ses))
#define CIFS_CHAN_NEEDS_RECONNECT(ses, index)	\
	test_bit((index), &(ses)->chans_need_reconnect)
#define CIFS_CHAN_IN_RECONNECT(ses, index)	\
	((ses)->chans[(index)].in_reconnect)

	struct cifs_chan chans[CIFS_MAX_CHANNELS];
	size_t chan_count;
	size_t chan_max;
	atomic_t chan_seq;  

	 
	unsigned long chans_need_reconnect;
	 
	struct cifs_ses *dfs_root_ses;
	struct nls_table *local_nls;
};

static inline bool
cap_unix(struct cifs_ses *ses)
{
	return ses->server->vals->cap_unix & ses->capabilities;
}

 

#define CIFS_FATTR_JUNCTION		0x1
#define CIFS_FATTR_DELETE_PENDING	0x2
#define CIFS_FATTR_NEED_REVAL		0x4
#define CIFS_FATTR_INO_COLLISION	0x8
#define CIFS_FATTR_UNKNOWN_NLINK	0x10
#define CIFS_FATTR_FAKE_ROOT_INO	0x20

struct cifs_fattr {
	u32		cf_flags;
	u32		cf_cifsattrs;
	u64		cf_uniqueid;
	u64		cf_eof;
	u64		cf_bytes;
	u64		cf_createtime;
	kuid_t		cf_uid;
	kgid_t		cf_gid;
	umode_t		cf_mode;
	dev_t		cf_rdev;
	unsigned int	cf_nlink;
	unsigned int	cf_dtype;
	struct timespec64 cf_atime;
	struct timespec64 cf_mtime;
	struct timespec64 cf_ctime;
	u32             cf_cifstag;
	char            *cf_symlink_target;
};

 
struct cifs_tcon {
	struct list_head tcon_list;
	int tc_count;
	struct list_head rlist;  
	spinlock_t tc_lock;   
	atomic_t num_local_opens;   
	atomic_t num_remote_opens;  
	struct list_head openFileList;
	spinlock_t open_file_lock;  
	struct cifs_ses *ses;	 
	char tree_name[MAX_TREE_SIZE + 1];  
	char *nativeFileSystem;
	char *password;		 
	__u32 tid;		 
	__u16 Flags;		 
	enum tid_status_enum status;
	atomic_t num_smbs_sent;
	union {
		struct {
			atomic_t num_writes;
			atomic_t num_reads;
			atomic_t num_flushes;
			atomic_t num_oplock_brks;
			atomic_t num_opens;
			atomic_t num_closes;
			atomic_t num_deletes;
			atomic_t num_mkdirs;
			atomic_t num_posixopens;
			atomic_t num_posixmkdirs;
			atomic_t num_rmdirs;
			atomic_t num_renames;
			atomic_t num_t2renames;
			atomic_t num_ffirst;
			atomic_t num_fnext;
			atomic_t num_fclose;
			atomic_t num_hardlinks;
			atomic_t num_symlinks;
			atomic_t num_locks;
			atomic_t num_acl_get;
			atomic_t num_acl_set;
		} cifs_stats;
		struct {
			atomic_t smb2_com_sent[NUMBER_OF_SMB2_COMMANDS];
			atomic_t smb2_com_failed[NUMBER_OF_SMB2_COMMANDS];
		} smb2_stats;
	} stats;
	__u64    bytes_read;
	__u64    bytes_written;
	spinlock_t stat_lock;   
	FILE_SYSTEM_DEVICE_INFO fsDevInfo;
	FILE_SYSTEM_ATTRIBUTE_INFO fsAttrInfo;  
	FILE_SYSTEM_UNIX_INFO fsUnixInfo;
	bool ipc:1;    
	bool pipe:1;   
	bool print:1;  
	bool retry:1;
	bool nocase:1;
	bool nohandlecache:1;  
	bool nodelete:1;
	bool seal:1;       
	bool unix_ext:1;   
	bool posix_extensions;  
	bool local_lease:1;  
	bool broken_posix_open;  
	bool broken_sparse_sup;  
	bool need_reconnect:1;  
	bool need_reopen_files:1;  
	bool use_resilient:1;  
	bool use_persistent:1;  
	bool no_lease:1;     
	bool use_witness:1;  
	__le32 capabilities;
	__u32 share_flags;
	__u32 maximal_access;
	__u32 vol_serial_number;
	__le64 vol_create_time;
	__u64 snapshot_time;  
	__u32 handle_timeout;  
	__u32 ss_flags;		 
	__u32 perf_sector_size;  
	__u32 max_chunks;
	__u32 max_bytes_chunk;
	__u32 max_bytes_copy;
	__u32 max_cached_dirs;
#ifdef CONFIG_CIFS_FSCACHE
	u64 resource_id;		 
	struct fscache_volume *fscache;	 
#endif
	struct list_head pending_opens;	 
	struct cached_fids *cfids;
	 
#ifdef CONFIG_CIFS_DFS_UPCALL
	struct list_head dfs_ses_list;
	struct delayed_work dfs_cache_work;
#endif
	struct delayed_work	query_interfaces;  
	char *origin_fullpath;  
};

 
struct tcon_link {
	struct rb_node		tl_rbnode;
	kuid_t			tl_uid;
	unsigned long		tl_flags;
#define TCON_LINK_MASTER	0
#define TCON_LINK_PENDING	1
#define TCON_LINK_IN_TREE	2
	unsigned long		tl_time;
	atomic_t		tl_count;
	struct cifs_tcon	*tl_tcon;
};

extern struct tcon_link *cifs_sb_tlink(struct cifs_sb_info *cifs_sb);
extern void smb3_free_compound_rqst(int num_rqst, struct smb_rqst *rqst);

static inline struct cifs_tcon *
tlink_tcon(struct tcon_link *tlink)
{
	return tlink->tl_tcon;
}

static inline struct tcon_link *
cifs_sb_master_tlink(struct cifs_sb_info *cifs_sb)
{
	return cifs_sb->master_tlink;
}

extern void cifs_put_tlink(struct tcon_link *tlink);

static inline struct tcon_link *
cifs_get_tlink(struct tcon_link *tlink)
{
	if (tlink && !IS_ERR(tlink))
		atomic_inc(&tlink->tl_count);
	return tlink;
}

 
extern struct cifs_tcon *cifs_sb_master_tcon(struct cifs_sb_info *cifs_sb);

#define CIFS_OPLOCK_NO_CHANGE 0xfe

struct cifs_pending_open {
	struct list_head olist;
	struct tcon_link *tlink;
	__u8 lease_key[16];
	__u32 oplock;
};

struct cifs_deferred_close {
	struct list_head dlist;
	struct tcon_link *tlink;
	__u16  netfid;
	__u64  persistent_fid;
	__u64  volatile_fid;
};

 
struct cifsLockInfo {
	struct list_head llist;	 
	struct list_head blist;  
	wait_queue_head_t block_q;
	__u64 offset;
	__u64 length;
	__u32 pid;
	__u16 type;
	__u16 flags;
};

 
struct cifs_search_info {
	loff_t index_of_last_entry;
	__u16 entries_in_buffer;
	__u16 info_level;
	__u32 resume_key;
	char *ntwrk_buf_start;
	char *srch_entries_start;
	char *last_entry;
	const char *presume_name;
	unsigned int resume_name_len;
	bool endOfSearch:1;
	bool emptyDir:1;
	bool unicode:1;
	bool smallBuf:1;  
};

#define ACL_NO_MODE	((umode_t)(-1))
struct cifs_open_parms {
	struct cifs_tcon *tcon;
	struct cifs_sb_info *cifs_sb;
	int disposition;
	int desired_access;
	int create_options;
	const char *path;
	struct cifs_fid *fid;
	umode_t mode;
	bool reconnect:1;
};

struct cifs_fid {
	__u16 netfid;
	__u64 persistent_fid;	 
	__u64 volatile_fid;	 
	__u8 lease_key[SMB2_LEASE_KEY_SIZE];	 
	__u8 create_guid[16];
	__u32 access;
	struct cifs_pending_open *pending_open;
	unsigned int epoch;
#ifdef CONFIG_CIFS_DEBUG2
	__u64 mid;
#endif  
	bool purge_cache;
};

struct cifs_fid_locks {
	struct list_head llist;
	struct cifsFileInfo *cfile;	 
	struct list_head locks;		 
};

struct cifsFileInfo {
	 
	struct list_head tlist;	 
	struct list_head flist;	 
	 
	struct cifs_fid_locks *llist;	 
	kuid_t uid;		 
	__u32 pid;		 
	struct cifs_fid fid;	 
	struct list_head rlist;  
	 
	 
	struct dentry *dentry;
	struct tcon_link *tlink;
	unsigned int f_flags;
	bool invalidHandle:1;	 
	bool swapfile:1;
	bool oplock_break_cancelled:1;
	unsigned int oplock_epoch;  
	__u32 oplock_level;  
	int count;
	spinlock_t file_info_lock;  
	struct mutex fh_mutex;  
	struct cifs_search_info srch_inf;
	struct work_struct oplock_break;  
	struct work_struct put;  
	struct delayed_work deferred;
	bool deferred_close_scheduled;  
	char *symlink_target;
};

struct cifs_io_parms {
	__u16 netfid;
	__u64 persistent_fid;	 
	__u64 volatile_fid;	 
	__u32 pid;
	__u64 offset;
	unsigned int length;
	struct cifs_tcon *tcon;
	struct TCP_Server_Info *server;
};

struct cifs_aio_ctx {
	struct kref		refcount;
	struct list_head	list;
	struct mutex		aio_mutex;
	struct completion	done;
	struct iov_iter		iter;
	struct kiocb		*iocb;
	struct cifsFileInfo	*cfile;
	struct bio_vec		*bv;
	loff_t			pos;
	unsigned int		nr_pinned_pages;
	ssize_t			rc;
	unsigned int		len;
	unsigned int		total_len;
	unsigned int		bv_need_unpin;	 
	bool			should_dirty;
	 
	bool			direct_io;
};

 
struct cifs_readdata {
	struct kref			refcount;
	struct list_head		list;
	struct completion		done;
	struct cifsFileInfo		*cfile;
	struct address_space		*mapping;
	struct cifs_aio_ctx		*ctx;
	__u64				offset;
	ssize_t				got_bytes;
	unsigned int			bytes;
	pid_t				pid;
	int				result;
	struct work_struct		work;
	struct iov_iter			iter;
	struct kvec			iov[2];
	struct TCP_Server_Info		*server;
#ifdef CONFIG_CIFS_SMB_DIRECT
	struct smbd_mr			*mr;
#endif
	struct cifs_credits		credits;
};

 
struct cifs_writedata {
	struct kref			refcount;
	struct list_head		list;
	struct completion		done;
	enum writeback_sync_modes	sync_mode;
	struct work_struct		work;
	struct cifsFileInfo		*cfile;
	struct cifs_aio_ctx		*ctx;
	struct iov_iter			iter;
	struct bio_vec			*bv;
	__u64				offset;
	pid_t				pid;
	unsigned int			bytes;
	int				result;
	struct TCP_Server_Info		*server;
#ifdef CONFIG_CIFS_SMB_DIRECT
	struct smbd_mr			*mr;
#endif
	struct cifs_credits		credits;
};

 
static inline void
cifsFileInfo_get_locked(struct cifsFileInfo *cifs_file)
{
	++cifs_file->count;
}

struct cifsFileInfo *cifsFileInfo_get(struct cifsFileInfo *cifs_file);
void _cifsFileInfo_put(struct cifsFileInfo *cifs_file, bool wait_oplock_hdlr,
		       bool offload);
void cifsFileInfo_put(struct cifsFileInfo *cifs_file);

#define CIFS_CACHE_READ_FLG	1
#define CIFS_CACHE_HANDLE_FLG	2
#define CIFS_CACHE_RH_FLG	(CIFS_CACHE_READ_FLG | CIFS_CACHE_HANDLE_FLG)
#define CIFS_CACHE_WRITE_FLG	4
#define CIFS_CACHE_RW_FLG	(CIFS_CACHE_READ_FLG | CIFS_CACHE_WRITE_FLG)
#define CIFS_CACHE_RHW_FLG	(CIFS_CACHE_RW_FLG | CIFS_CACHE_HANDLE_FLG)

#define CIFS_CACHE_READ(cinode) ((cinode->oplock & CIFS_CACHE_READ_FLG) || (CIFS_SB(cinode->netfs.inode.i_sb)->mnt_cifs_flags & CIFS_MOUNT_RO_CACHE))
#define CIFS_CACHE_HANDLE(cinode) (cinode->oplock & CIFS_CACHE_HANDLE_FLG)
#define CIFS_CACHE_WRITE(cinode) ((cinode->oplock & CIFS_CACHE_WRITE_FLG) || (CIFS_SB(cinode->netfs.inode.i_sb)->mnt_cifs_flags & CIFS_MOUNT_RW_CACHE))

 

struct cifsInodeInfo {
	struct netfs_inode netfs;  
	bool can_cache_brlcks;
	struct list_head llist;	 
	 
	struct rw_semaphore lock_sem;	 
	 
	struct list_head openFileList;
	spinlock_t	open_file_lock;	 
	__u32 cifsAttrs;  
	unsigned int oplock;		 
	unsigned int epoch;		 
#define CIFS_INODE_PENDING_OPLOCK_BREAK   (0)  
#define CIFS_INODE_PENDING_WRITERS	  (1)  
#define CIFS_INODE_FLAG_UNUSED		  (2)  
#define CIFS_INO_DELETE_PENDING		  (3)  
#define CIFS_INO_INVALID_MAPPING	  (4)  
#define CIFS_INO_LOCK			  (5)  
#define CIFS_INO_MODIFIED_ATTR            (6)  
#define CIFS_INO_CLOSE_ON_LOCK            (7)  
	unsigned long flags;
	spinlock_t writers_lock;
	unsigned int writers;		 
	unsigned long time;		 
	u64  server_eof;		 
	u64  uniqueid;			 
	u64  createtime;		 
	__u8 lease_key[SMB2_LEASE_KEY_SIZE];	 
	struct list_head deferred_closes;  
	spinlock_t deferred_lock;  
	bool lease_granted;  
	char *symlink_target;
};

static inline struct cifsInodeInfo *
CIFS_I(struct inode *inode)
{
	return container_of(inode, struct cifsInodeInfo, netfs.inode);
}

static inline struct cifs_sb_info *
CIFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct cifs_sb_info *
CIFS_FILE_SB(struct file *file)
{
	return CIFS_SB(file_inode(file)->i_sb);
}

static inline char CIFS_DIR_SEP(const struct cifs_sb_info *cifs_sb)
{
	if (cifs_sb->mnt_cifs_flags & CIFS_MOUNT_POSIX_PATHS)
		return '/';
	else
		return '\\';
}

static inline void
convert_delimiter(char *path, char delim)
{
	char old_delim, *pos;

	if (delim == '/')
		old_delim = '\\';
	else
		old_delim = '/';

	pos = path;
	while ((pos = strchr(pos, old_delim)))
		*pos = delim;
}

#define cifs_stats_inc atomic_inc

static inline void cifs_stats_bytes_written(struct cifs_tcon *tcon,
					    unsigned int bytes)
{
	if (bytes) {
		spin_lock(&tcon->stat_lock);
		tcon->bytes_written += bytes;
		spin_unlock(&tcon->stat_lock);
	}
}

static inline void cifs_stats_bytes_read(struct cifs_tcon *tcon,
					 unsigned int bytes)
{
	spin_lock(&tcon->stat_lock);
	tcon->bytes_read += bytes;
	spin_unlock(&tcon->stat_lock);
}


 
typedef int (mid_receive_t)(struct TCP_Server_Info *server,
			    struct mid_q_entry *mid);

 
typedef void (mid_callback_t)(struct mid_q_entry *mid);

 
typedef int (mid_handle_t)(struct TCP_Server_Info *server,
			    struct mid_q_entry *mid);

 
struct mid_q_entry {
	struct list_head qhead;	 
	struct kref refcount;
	struct TCP_Server_Info *server;	 
	__u64 mid;		 
	__u16 credits;		 
	__u16 credits_received;	 
	__u32 pid;		 
	__u32 sequence_number;   
	unsigned long when_alloc;   
#ifdef CONFIG_CIFS_STATS2
	unsigned long when_sent;  
	unsigned long when_received;  
#endif
	mid_receive_t *receive;  
	mid_callback_t *callback;  
	mid_handle_t *handle;  
	void *callback_data;	   
	struct task_struct *creator;
	void *resp_buf;		 
	unsigned int resp_buf_size;
	int mid_state;	 
	unsigned int mid_flags;
	__le16 command;		 
	unsigned int optype;	 
	bool large_buf:1;	 
	bool multiRsp:1;	 
	bool multiEnd:1;	 
	bool decrypted:1;	 
};

struct close_cancelled_open {
	struct cifs_fid         fid;
	struct cifs_tcon        *tcon;
	struct work_struct      work;
	__u64 mid;
	__u16 cmd;
};

 
static inline void cifs_in_send_inc(struct TCP_Server_Info *server)
{
	atomic_inc(&server->in_send);
}

static inline void cifs_in_send_dec(struct TCP_Server_Info *server)
{
	atomic_dec(&server->in_send);
}

static inline void cifs_num_waiters_inc(struct TCP_Server_Info *server)
{
	atomic_inc(&server->num_waiters);
}

static inline void cifs_num_waiters_dec(struct TCP_Server_Info *server)
{
	atomic_dec(&server->num_waiters);
}

#ifdef CONFIG_CIFS_STATS2
static inline void cifs_save_when_sent(struct mid_q_entry *mid)
{
	mid->when_sent = jiffies;
}
#else
static inline void cifs_save_when_sent(struct mid_q_entry *mid)
{
}
#endif

 
struct dir_notify_req {
	struct list_head lhead;
	__le16 Pid;
	__le16 PidHigh;
	__u16 Mid;
	__u16 Tid;
	__u16 Uid;
	__u16 netfid;
	__u32 filter;  
	int multishot;
	struct file *pfile;
};

struct dfs_info3_param {
	int flags;  
	int path_consumed;
	int server_type;
	int ref_flag;
	char *path_name;
	char *node_name;
	int ttl;
};

struct file_list {
	struct list_head list;
	struct cifsFileInfo *cfile;
};

struct cifs_mount_ctx {
	struct cifs_sb_info *cifs_sb;
	struct smb3_fs_context *fs_ctx;
	unsigned int xid;
	struct TCP_Server_Info *server;
	struct cifs_ses *ses;
	struct cifs_tcon *tcon;
	struct list_head dfs_ses_list;
};

static inline void __free_dfs_info_param(struct dfs_info3_param *param)
{
	kfree(param->path_name);
	kfree(param->node_name);
}

static inline void free_dfs_info_param(struct dfs_info3_param *param)
{
	if (param)
		__free_dfs_info_param(param);
}

static inline void zfree_dfs_info_param(struct dfs_info3_param *param)
{
	if (param) {
		__free_dfs_info_param(param);
		memset(param, 0, sizeof(*param));
	}
}

static inline void free_dfs_info_array(struct dfs_info3_param *param,
				       int number_of_items)
{
	int i;

	if ((number_of_items == 0) || (param == NULL))
		return;
	for (i = 0; i < number_of_items; i++) {
		kfree(param[i].path_name);
		kfree(param[i].node_name);
	}
	kfree(param);
}

static inline bool is_interrupt_error(int error)
{
	switch (error) {
	case -EINTR:
	case -ERESTARTSYS:
	case -ERESTARTNOHAND:
	case -ERESTARTNOINTR:
		return true;
	}
	return false;
}

static inline bool is_retryable_error(int error)
{
	if (is_interrupt_error(error) || error == -EAGAIN)
		return true;
	return false;
}


 
#define FIND_WR_ANY         0
#define FIND_WR_FSUID_ONLY  1
#define FIND_WR_WITH_DELETE 2

#define   MID_FREE 0
#define   MID_REQUEST_ALLOCATED 1
#define   MID_REQUEST_SUBMITTED 2
#define   MID_RESPONSE_RECEIVED 4
#define   MID_RETRY_NEEDED      8  
#define   MID_RESPONSE_MALFORMED 0x10
#define   MID_SHUTDOWN		 0x20
#define   MID_RESPONSE_READY 0x40  

 
#define   MID_WAIT_CANCELLED	 1  
#define   MID_DELETED            2  

 
#define   CIFS_NO_BUFFER        0     
#define   CIFS_SMALL_BUFFER     1
#define   CIFS_LARGE_BUFFER     2
#define   CIFS_IOVEC            4     

 
#define   CIFS_BLOCKING_OP      1     
#define   CIFS_NON_BLOCKING     2     
#define   CIFS_TIMEOUT_MASK 0x003     
#define   CIFS_LOG_ERROR    0x010     
#define   CIFS_LARGE_BUF_OP 0x020     
#define   CIFS_NO_RSP_BUF   0x040     

 
#define   CIFS_ECHO_OP            0x080   
#define   CIFS_OBREAK_OP          0x0100  
#define   CIFS_NEG_OP             0x0200  
#define   CIFS_CP_CREATE_CLOSE_OP 0x0400  
 
#define   CIFS_SESS_OP            0x2000  
#define   CIFS_OP_MASK            0x2780  

#define   CIFS_HAS_CREDITS        0x0400  
#define   CIFS_TRANSFORM_REQ      0x0800  
#define   CIFS_NO_SRV_RSP         0x1000  

 
#define   CIFSSEC_MAY_SIGN	0x00001
#define   CIFSSEC_MAY_NTLMV2	0x00004
#define   CIFSSEC_MAY_KRB5	0x00008
#define   CIFSSEC_MAY_SEAL	0x00040  
#define   CIFSSEC_MAY_NTLMSSP	0x00080  

#define   CIFSSEC_MUST_SIGN	0x01001
 
#define   CIFSSEC_MUST_NTLMV2	0x04004
#define   CIFSSEC_MUST_KRB5	0x08008
#ifdef CONFIG_CIFS_UPCALL
#define   CIFSSEC_MASK          0x8F08F  
#else
#define	  CIFSSEC_MASK          0x87087  
#endif  
#define   CIFSSEC_MUST_SEAL	0x40040  
#define   CIFSSEC_MUST_NTLMSSP	0x80080  

#define   CIFSSEC_DEF (CIFSSEC_MAY_SIGN | CIFSSEC_MAY_NTLMV2 | CIFSSEC_MAY_NTLMSSP)
#define   CIFSSEC_MAX (CIFSSEC_MUST_NTLMV2)
#define   CIFSSEC_AUTH_MASK (CIFSSEC_MAY_NTLMV2 | CIFSSEC_MAY_KRB5 | CIFSSEC_MAY_NTLMSSP)
 

#define UID_HASH (16)

 

 

#ifdef DECLARE_GLOBALS_HERE
#define GLOBAL_EXTERN
#else
#define GLOBAL_EXTERN extern
#endif

 
extern struct list_head		cifs_tcp_ses_list;

 
extern spinlock_t		cifs_tcp_ses_lock;

 
extern unsigned int GlobalCurrentXid;	 
extern unsigned int GlobalTotalActiveXid;  
extern unsigned int GlobalMaxActiveXid;	 
extern spinlock_t GlobalMid_Lock;  

 
extern atomic_t sesInfoAllocCount;
extern atomic_t tconInfoAllocCount;
extern atomic_t tcpSesNextId;
extern atomic_t tcpSesAllocCount;
extern atomic_t tcpSesReconnectCount;
extern atomic_t tconInfoReconnectCount;

 
extern atomic_t buf_alloc_count;	 
extern atomic_t small_buf_alloc_count;
#ifdef CONFIG_CIFS_STATS2
extern atomic_t total_buf_alloc_count;  
extern atomic_t total_small_buf_alloc_count;
extern unsigned int slow_rsp_threshold;  
#endif

 
extern bool enable_oplocks;  
extern bool lookupCacheEnabled;
extern unsigned int global_secflags;	 
extern unsigned int sign_CIFS_PDUs;   
extern bool enable_gcm_256;  
extern bool require_gcm_256;  
extern bool enable_negotiate_signing;  
extern bool linuxExtEnabled; 
extern unsigned int CIFSMaxBufSize;   
extern unsigned int cifs_min_rcv;     
extern unsigned int cifs_min_small;   
extern unsigned int cifs_max_pending;  
extern unsigned int dir_cache_timeout;  
extern bool disable_legacy_dialects;   
extern atomic_t mid_count;

void cifs_oplock_break(struct work_struct *work);
void cifs_queue_oplock_break(struct cifsFileInfo *cfile);
void smb2_deferred_work_close(struct work_struct *work);

extern const struct slow_work_ops cifs_oplock_break_ops;
extern struct workqueue_struct *cifsiod_wq;
extern struct workqueue_struct *decrypt_wq;
extern struct workqueue_struct *fileinfo_put_wq;
extern struct workqueue_struct *cifsoplockd_wq;
extern struct workqueue_struct *deferredclose_wq;
extern __u32 cifs_lock_secret;

extern mempool_t *cifs_mid_poolp;

 
#define SMB1_VERSION_STRING	"1.0"
#define SMB20_VERSION_STRING    "2.0"
#ifdef CONFIG_CIFS_ALLOW_INSECURE_LEGACY
extern struct smb_version_operations smb1_operations;
extern struct smb_version_values smb1_values;
extern struct smb_version_operations smb20_operations;
extern struct smb_version_values smb20_values;
#endif  
#define SMB21_VERSION_STRING	"2.1"
extern struct smb_version_operations smb21_operations;
extern struct smb_version_values smb21_values;
#define SMBDEFAULT_VERSION_STRING "default"
extern struct smb_version_values smbdefault_values;
#define SMB3ANY_VERSION_STRING "3"
extern struct smb_version_values smb3any_values;
#define SMB30_VERSION_STRING	"3.0"
extern struct smb_version_operations smb30_operations;
extern struct smb_version_values smb30_values;
#define SMB302_VERSION_STRING	"3.02"
#define ALT_SMB302_VERSION_STRING "3.0.2"
   
extern struct smb_version_values smb302_values;
#define SMB311_VERSION_STRING	"3.1.1"
#define ALT_SMB311_VERSION_STRING "3.11"
extern struct smb_version_operations smb311_operations;
extern struct smb_version_values smb311_values;

static inline char *get_security_type_str(enum securityEnum sectype)
{
	switch (sectype) {
	case RawNTLMSSP:
		return "RawNTLMSSP";
	case Kerberos:
		return "Kerberos";
	case NTLMv2:
		return "NTLMv2";
	default:
		return "Unknown";
	}
}

static inline bool is_smb1_server(struct TCP_Server_Info *server)
{
	return strcmp(server->vals->version_string, SMB1_VERSION_STRING) == 0;
}

static inline bool is_tcon_dfs(struct cifs_tcon *tcon)
{
	 
	if (!tcon || !tcon->ses || !tcon->ses->server)
		return false;
	return is_smb1_server(tcon->ses->server) ? tcon->Flags & SMB_SHARE_IS_IN_DFS :
		tcon->share_flags & (SHI1005_FLAGS_DFS | SHI1005_FLAGS_DFS_ROOT);
}

static inline bool cifs_is_referral_server(struct cifs_tcon *tcon,
					   const struct dfs_info3_param *ref)
{
	 
	return is_tcon_dfs(tcon) || (ref && (ref->flags & DFSREF_REFERRAL_SERVER));
}

static inline u64 cifs_flock_len(const struct file_lock *fl)
{
	return (u64)fl->fl_end - fl->fl_start + 1;
}

static inline size_t ntlmssp_workstation_name_size(const struct cifs_ses *ses)
{
	if (WARN_ON_ONCE(!ses || !ses->server))
		return 0;
	 
	if (ses->server->dialect <= SMB20_PROT_ID)
		return min_t(size_t, sizeof(ses->workstation_name), RFC1001_NAME_LEN_WITH_NULL);
	return sizeof(ses->workstation_name);
}

static inline void move_cifs_info_to_smb2(struct smb2_file_all_info *dst, const FILE_ALL_INFO *src)
{
	memcpy(dst, src, (size_t)((u8 *)&src->AccessFlags - (u8 *)src));
	dst->AccessFlags = src->AccessFlags;
	dst->CurrentByteOffset = src->CurrentByteOffset;
	dst->Mode = src->Mode;
	dst->AlignmentRequirement = src->AlignmentRequirement;
	dst->FileNameLength = src->FileNameLength;
}

static inline int cifs_get_num_sgs(const struct smb_rqst *rqst,
				   int num_rqst,
				   const u8 *sig)
{
	unsigned int len, skip;
	unsigned int nents = 0;
	unsigned long addr;
	size_t data_size;
	int i, j;

	 
	skip = 20;

	 
	for (i = 0; i < num_rqst; i++) {
		data_size = iov_iter_count(&rqst[i].rq_iter);

		 
		if (data_size &&
		    WARN_ON_ONCE(user_backed_iter(&rqst[i].rq_iter)))
			return -EIO;

		 
		if (data_size &&
		    WARN_ON_ONCE(iov_iter_extract_will_pin(&rqst[i].rq_iter)))
			return -EIO;

		for (j = 0; j < rqst[i].rq_nvec; j++) {
			struct kvec *iov = &rqst[i].rq_iov[j];

			addr = (unsigned long)iov->iov_base + skip;
			if (unlikely(is_vmalloc_addr((void *)addr))) {
				len = iov->iov_len - skip;
				nents += DIV_ROUND_UP(offset_in_page(addr) + len,
						      PAGE_SIZE);
			} else {
				nents++;
			}
			skip = 0;
		}
		if (data_size)
			nents += iov_iter_npages(&rqst[i].rq_iter, INT_MAX);
	}
	nents += DIV_ROUND_UP(offset_in_page(sig) + SMB2_SIGNATURE_SIZE, PAGE_SIZE);
	return nents;
}

 
static inline void cifs_sg_set_buf(struct sg_table *sgtable,
				   const void *buf,
				   unsigned int buflen)
{
	unsigned long addr = (unsigned long)buf;
	unsigned int off = offset_in_page(addr);

	addr &= PAGE_MASK;
	if (unlikely(is_vmalloc_addr((void *)addr))) {
		do {
			unsigned int len = min_t(unsigned int, buflen, PAGE_SIZE - off);

			sg_set_page(&sgtable->sgl[sgtable->nents++],
				    vmalloc_to_page((void *)addr), len, off);

			off = 0;
			addr += PAGE_SIZE;
			buflen -= len;
		} while (buflen);
	} else {
		sg_set_page(&sgtable->sgl[sgtable->nents++],
			    virt_to_page((void *)addr), buflen, off);
	}
}

struct smb2_compound_vars {
	struct cifs_open_parms oparms;
	struct kvec rsp_iov[3];
	struct smb_rqst rqst[3];
	struct kvec open_iov[SMB2_CREATE_IOV_SIZE];
	struct kvec qi_iov;
	struct kvec io_iov[SMB2_IOCTL_IOV_SIZE];
	struct kvec si_iov[SMB2_SET_INFO_IOV_SIZE];
	struct kvec close_iov;
	struct smb2_file_rename_info rename_info;
	struct smb2_file_link_info link_info;
};

#endif	 
