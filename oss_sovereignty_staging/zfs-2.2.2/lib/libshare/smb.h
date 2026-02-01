 

 

 

#define	SMB_NAME_MAX		255
#define	SMB_COMMENT_MAX		255

#define	SHARE_DIR		"/var/lib/samba/usershares"
#define	NET_CMD_PATH		"/usr/bin/net"
#define	NET_CMD_ARG_HOST	"127.0.0.1"

typedef struct smb_share_s {
	char name[SMB_NAME_MAX];	 
	char path[PATH_MAX];		 
	char comment[SMB_COMMENT_MAX];	 
	boolean_t guest_ok;		 

	struct smb_share_s *next;
} smb_share_t;
