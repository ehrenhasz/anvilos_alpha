 
 

#ifndef _FS_CONTEXT_H
#define _FS_CONTEXT_H

#include "cifsglob.h"
#include <linux/parser.h>
#include <linux/fs_parser.h>

 
#define cifs_errorf(fc, fmt, ...)			\
	do {						\
		errorf(fc, fmt, ## __VA_ARGS__);	\
		cifs_dbg(VFS, fmt, ## __VA_ARGS__);	\
	} while (0)

enum smb_version {
	Smb_1 = 1,
	Smb_20,
	Smb_21,
	Smb_30,
	Smb_302,
	Smb_311,
	Smb_3any,
	Smb_default,
	Smb_version_err
};

enum {
	Opt_cache_loose,
	Opt_cache_strict,
	Opt_cache_none,
	Opt_cache_ro,
	Opt_cache_rw,
	Opt_cache_err
};

enum cifs_sec_param {
	Opt_sec_krb5,
	Opt_sec_krb5i,
	Opt_sec_krb5p,
	Opt_sec_ntlmsspi,
	Opt_sec_ntlmssp,
	Opt_sec_ntlmv2,
	Opt_sec_ntlmv2i,
	Opt_sec_none,

	Opt_sec_err
};

enum cifs_param {
	 
	Opt_user_xattr,
	Opt_forceuid,
	Opt_forcegid,
	Opt_noblocksend,
	Opt_noautotune,
	Opt_nolease,
	Opt_nosparse,
	Opt_hard,
	Opt_soft,
	Opt_perm,
	Opt_nodelete,
	Opt_mapposix,
	Opt_mapchars,
	Opt_nomapchars,
	Opt_sfu,
	Opt_nodfs,
	Opt_posixpaths,
	Opt_unix,
	Opt_nocase,
	Opt_brl,
	Opt_handlecache,
	Opt_forcemandatorylock,
	Opt_setuidfromacl,
	Opt_setuids,
	Opt_dynperm,
	Opt_intr,
	Opt_strictsync,
	Opt_serverino,
	Opt_rwpidforward,
	Opt_cifsacl,
	Opt_acl,
	Opt_locallease,
	Opt_sign,
	Opt_ignore_signature,
	Opt_seal,
	Opt_noac,
	Opt_fsc,
	Opt_mfsymlinks,
	Opt_multiuser,
	Opt_sloppy,
	Opt_nosharesock,
	Opt_persistent,
	Opt_resilient,
	Opt_tcp_nodelay,
	Opt_domainauto,
	Opt_rdma,
	Opt_modesid,
	Opt_rootfs,
	Opt_multichannel,
	Opt_compress,
	Opt_witness,

	 
	Opt_backupuid,
	Opt_backupgid,
	Opt_uid,
	Opt_cruid,
	Opt_gid,
	Opt_port,
	Opt_file_mode,
	Opt_dirmode,
	Opt_min_enc_offload,
	Opt_blocksize,
	Opt_rasize,
	Opt_rsize,
	Opt_wsize,
	Opt_actimeo,
	Opt_acdirmax,
	Opt_acregmax,
	Opt_closetimeo,
	Opt_echo_interval,
	Opt_max_credits,
	Opt_max_cached_dirs,
	Opt_snapshot,
	Opt_max_channels,
	Opt_handletimeout,

	 
	Opt_source,
	Opt_user,
	Opt_pass,
	Opt_ip,
	Opt_domain,
	Opt_srcaddr,
	Opt_iocharset,
	Opt_netbiosname,
	Opt_servern,
	Opt_ver,
	Opt_vers,
	Opt_sec,
	Opt_cache,

	 
	Opt_ignore,

	Opt_err
};

struct smb3_fs_context {
	bool uid_specified;
	bool cruid_specified;
	bool gid_specified;
	bool sloppy;
	bool got_ip;
	bool got_version;
	bool got_rsize;
	bool got_wsize;
	bool got_bsize;
	unsigned short port;

	char *username;
	char *password;
	char *domainname;
	char *source;
	char *server_hostname;
	char *UNC;
	char *nodename;
	char workstation_name[CIFS_MAX_WORKSTATION_LEN];
	char *iocharset;   
	char source_rfc1001_name[RFC1001_NAME_LEN_WITH_NULL];  
	char target_rfc1001_name[RFC1001_NAME_LEN_WITH_NULL];  
	kuid_t cred_uid;
	kuid_t linux_uid;
	kgid_t linux_gid;
	kuid_t backupuid;
	kgid_t backupgid;
	umode_t file_mode;
	umode_t dir_mode;
	enum securityEnum sectype;  
	bool sign;  
	bool ignore_signature:1;
	bool retry:1;
	bool intr:1;
	bool setuids:1;
	bool setuidfromacl:1;
	bool override_uid:1;
	bool override_gid:1;
	bool dynperm:1;
	bool noperm:1;
	bool nodelete:1;
	bool mode_ace:1;
	bool no_psx_acl:1;  
	bool cifs_acl:1;
	bool backupuid_specified;  
	bool backupgid_specified;  
	bool no_xattr:1;    
	bool server_ino:1;  
	bool direct_io:1;
	bool strict_io:1;  
	bool cache_ro:1;
	bool cache_rw:1;
	bool remap:1;       
	bool sfu_remap:1;   
	bool posix_paths:1;  
	bool no_linux_ext:1;
	bool linux_ext:1;
	bool sfu_emul:1;
	bool nullauth:1;    
	bool nocase:1;      
	bool nobrl:1;       
	bool nohandlecache:1;  
	bool mand_lock:1;   
	bool seal:1;        
	bool nodfs:1;       
	bool local_lease:1;  
	bool noblocksnd:1;
	bool noautotune:1;
	bool nostrictsync:1;  
	bool no_lease:1;      
	bool no_sparse:1;     
	bool fsc:1;	 
	bool mfsymlinks:1;  
	bool multiuser:1;
	bool rwpidforward:1;  
	bool nosharesock:1;
	bool persistent:1;
	bool nopersistent:1;
	bool resilient:1;  
	bool domainauto:1;
	bool rdma:1;
	bool multichannel:1;
	bool use_client_guid:1;
	 
	u8 client_guid[SMB2_CLIENT_GUID_SIZE];
	unsigned int bsize;
	unsigned int rasize;
	unsigned int rsize;
	unsigned int wsize;
	unsigned int min_offload;
	bool sockopt_tcp_nodelay:1;
	 
	unsigned long acregmax;
	unsigned long acdirmax;
	 
	unsigned long closetimeo;
	struct smb_version_operations *ops;
	struct smb_version_values *vals;
	char *prepath;
	struct sockaddr_storage dstaddr;  
	struct sockaddr_storage srcaddr;  
	struct nls_table *local_nls;  
	unsigned int echo_interval;  
	__u64 snapshot_time;  
	__u32 handle_timeout;  
	unsigned int max_credits;  
	unsigned int max_channels;
	unsigned int max_cached_dirs;
	__u16 compression;  
	bool rootfs:1;  
	bool witness:1;  
	char *leaf_fullpath;
	struct cifs_ses *dfs_root_ses;
	bool dfs_automount:1;  
};

extern const struct fs_parameter_spec smb3_fs_parameters[];

extern int smb3_init_fs_context(struct fs_context *fc);
extern void smb3_cleanup_fs_context_contents(struct smb3_fs_context *ctx);
extern void smb3_cleanup_fs_context(struct smb3_fs_context *ctx);

static inline struct smb3_fs_context *smb3_fc2context(const struct fs_context *fc)
{
	return fc->fs_private;
}

extern int smb3_fs_context_dup(struct smb3_fs_context *new_ctx, struct smb3_fs_context *ctx);
extern void smb3_update_mnt_flags(struct cifs_sb_info *cifs_sb);

 
#define SMB3_MAX_DCLOSETIMEO (1 << 30)
#define SMB3_DEF_DCLOSETIMEO (1 * HZ)  
#define MAX_CACHED_FIDS 16
extern char *cifs_sanitize_prepath(char *prepath, gfp_t gfp);

#endif
