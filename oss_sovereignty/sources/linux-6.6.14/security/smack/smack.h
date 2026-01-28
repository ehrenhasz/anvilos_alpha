


#ifndef _SECURITY_SMACK_H
#define _SECURITY_SMACK_H

#include <linux/capability.h>
#include <linux/spinlock.h>
#include <linux/lsm_hooks.h>
#include <linux/in.h>
#if IS_ENABLED(CONFIG_IPV6)
#include <linux/in6.h>
#endif 
#include <net/netlabel.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/lsm_audit.h>
#include <linux/msg.h>


#if IS_ENABLED(CONFIG_IPV6) && !defined(CONFIG_SECURITY_SMACK_NETFILTER)
#define SMACK_IPV6_PORT_LABELING 1
#endif

#if IS_ENABLED(CONFIG_IPV6) && defined(CONFIG_SECURITY_SMACK_NETFILTER)
#define SMACK_IPV6_SECMARK_LABELING 1
#endif


#define SMK_LABELLEN	24
#define SMK_LONGLABEL	256


struct smack_known {
	struct list_head		list;
	struct hlist_node		smk_hashed;
	char				*smk_known;
	u32				smk_secid;
	struct netlbl_lsm_secattr	smk_netlabel;	
	struct list_head		smk_rules;	
	struct mutex			smk_rules_lock;	
};


#define SMK_CIPSOLEN	24

struct superblock_smack {
	struct smack_known	*smk_root;
	struct smack_known	*smk_floor;
	struct smack_known	*smk_hat;
	struct smack_known	*smk_default;
	int			smk_flags;
};


#define SMK_SB_INITIALIZED	0x01
#define SMK_SB_UNTRUSTED	0x02

struct socket_smack {
	struct smack_known	*smk_out;	
	struct smack_known	*smk_in;	
	struct smack_known	*smk_packet;	
	int			smk_state;	
};
#define	SMK_NETLBL_UNSET	0
#define	SMK_NETLBL_UNLABELED	1
#define	SMK_NETLBL_LABELED	2
#define	SMK_NETLBL_REQSKB	3


struct inode_smack {
	struct smack_known	*smk_inode;	
	struct smack_known	*smk_task;	
	struct smack_known	*smk_mmap;	
	int			smk_flags;	
};

struct task_smack {
	struct smack_known	*smk_task;	
	struct smack_known	*smk_forked;	
	struct smack_known	*smk_transmuted;
	struct list_head	smk_rules;	
	struct mutex		smk_rules_lock;	
	struct list_head	smk_relabel;	
};

#define	SMK_INODE_INSTANT	0x01	
#define	SMK_INODE_TRANSMUTE	0x02	
#define	SMK_INODE_CHANGED	0x04	
#define	SMK_INODE_IMPURE	0x08	


struct smack_rule {
	struct list_head	list;
	struct smack_known	*smk_subject;
	struct smack_known	*smk_object;
	int			smk_access;
};


struct smk_net4addr {
	struct list_head	list;
	struct in_addr		smk_host;	
	struct in_addr		smk_mask;	
	int			smk_masks;	
	struct smack_known	*smk_label;	
};


struct smk_net6addr {
	struct list_head	list;
	struct in6_addr		smk_host;	
	struct in6_addr		smk_mask;	
	int			smk_masks;	
	struct smack_known	*smk_label;	
};


struct smk_port_label {
	struct list_head	list;
	struct sock		*smk_sock;	
	unsigned short		smk_port;	
	struct smack_known	*smk_in;	
	struct smack_known	*smk_out;	
	short			smk_sock_type;	
	short			smk_can_reuse;
};

struct smack_known_list_elem {
	struct list_head	list;
	struct smack_known	*smk_label;
};

enum {
	Opt_error = -1,
	Opt_fsdefault = 0,
	Opt_fsfloor = 1,
	Opt_fshat = 2,
	Opt_fsroot = 3,
	Opt_fstransmute = 4,
};

#define SMACK_DELETE_OPTION	"-DELETE"
#define SMACK_CIPSO_OPTION 	"-CIPSO"


#define SMACK_CIPSO_DOI_DEFAULT		3	
#define SMACK_CIPSO_DOI_INVALID		-1	
#define SMACK_CIPSO_DIRECT_DEFAULT	250	
#define SMACK_CIPSO_MAPPED_DEFAULT	251	
#define SMACK_CIPSO_MAXLEVEL            255     

#define SMACK_CIPSO_MAXCATNUM           184     


#define SMACK_PTRACE_DEFAULT	0
#define SMACK_PTRACE_EXACT	1
#define SMACK_PTRACE_DRACONIAN	2
#define SMACK_PTRACE_MAX	SMACK_PTRACE_DRACONIAN


#define MAY_TRANSMUTE	0x00001000	
#define MAY_LOCK	0x00002000	
#define MAY_BRINGUP	0x00004000	


#ifdef CONFIG_SECURITY_SMACK_APPEND_SIGNALS
#define MAY_DELIVER	MAY_APPEND	
#else
#define MAY_DELIVER	MAY_WRITE	
#endif

#define SMACK_BRINGUP_ALLOW		1	
#define SMACK_UNCONFINED_SUBJECT	2	
#define SMACK_UNCONFINED_OBJECT		3	


#define MAY_ANYREAD	(MAY_READ | MAY_EXEC)
#define MAY_READWRITE	(MAY_READ | MAY_WRITE)
#define MAY_NOT		0


#define SMK_NUM_ACCESS_TYPE 7


struct smack_audit_data {
	const char *function;
	char *subject;
	char *object;
	char *request;
	int result;
};


struct smk_audit_info {
#ifdef CONFIG_AUDIT
	struct common_audit_data a;
	struct smack_audit_data sad;
#endif
};


int smk_access_entry(char *, char *, struct list_head *);
int smk_access(struct smack_known *, struct smack_known *,
	       int, struct smk_audit_info *);
int smk_tskacc(struct task_smack *, struct smack_known *,
	       u32, struct smk_audit_info *);
int smk_curacc(struct smack_known *, u32, struct smk_audit_info *);
struct smack_known *smack_from_secid(const u32);
char *smk_parse_smack(const char *string, int len);
int smk_netlbl_mls(int, char *, struct netlbl_lsm_secattr *, int);
struct smack_known *smk_import_entry(const char *, int);
void smk_insert_entry(struct smack_known *skp);
struct smack_known *smk_find_entry(const char *);
bool smack_privileged(int cap);
bool smack_privileged_cred(int cap, const struct cred *cred);
void smk_destroy_label_list(struct list_head *list);
int smack_populate_secattr(struct smack_known *skp);


extern int smack_enabled __initdata;
extern int smack_cipso_direct;
extern int smack_cipso_mapped;
extern struct smack_known *smack_net_ambient;
extern struct smack_known *smack_syslog_label;
#ifdef CONFIG_SECURITY_SMACK_BRINGUP
extern struct smack_known *smack_unconfined;
#endif
extern int smack_ptrace_rule;
extern struct lsm_blob_sizes smack_blob_sizes;

extern struct smack_known smack_known_floor;
extern struct smack_known smack_known_hat;
extern struct smack_known smack_known_huh;
extern struct smack_known smack_known_star;
extern struct smack_known smack_known_web;

extern struct mutex	smack_known_lock;
extern struct list_head smack_known_list;
extern struct list_head smk_net4addr_list;
extern struct list_head smk_net6addr_list;

extern struct mutex     smack_onlycap_lock;
extern struct list_head smack_onlycap_list;

#define SMACK_HASH_SLOTS 16
extern struct hlist_head smack_known_hash[SMACK_HASH_SLOTS];
extern struct kmem_cache *smack_rule_cache;

static inline struct task_smack *smack_cred(const struct cred *cred)
{
	return cred->security + smack_blob_sizes.lbs_cred;
}

static inline struct smack_known **smack_file(const struct file *file)
{
	return (struct smack_known **)(file->f_security +
				       smack_blob_sizes.lbs_file);
}

static inline struct inode_smack *smack_inode(const struct inode *inode)
{
	return inode->i_security + smack_blob_sizes.lbs_inode;
}

static inline struct smack_known **smack_msg_msg(const struct msg_msg *msg)
{
	return msg->security + smack_blob_sizes.lbs_msg_msg;
}

static inline struct smack_known **smack_ipc(const struct kern_ipc_perm *ipc)
{
	return ipc->security + smack_blob_sizes.lbs_ipc;
}

static inline struct superblock_smack *smack_superblock(
					const struct super_block *superblock)
{
	return superblock->s_security + smack_blob_sizes.lbs_superblock;
}


static inline int smk_inode_transmutable(const struct inode *isp)
{
	struct inode_smack *sip = smack_inode(isp);
	return (sip->smk_flags & SMK_INODE_TRANSMUTE) != 0;
}


static inline struct smack_known *smk_of_inode(const struct inode *isp)
{
	struct inode_smack *sip = smack_inode(isp);
	return sip->smk_inode;
}


static inline struct smack_known *smk_of_task(const struct task_smack *tsp)
{
	return tsp->smk_task;
}

static inline struct smack_known *smk_of_task_struct_obj(
						const struct task_struct *t)
{
	struct smack_known *skp;
	const struct cred *cred;

	rcu_read_lock();

	cred = __task_cred(t);
	skp = smk_of_task(smack_cred(cred));

	rcu_read_unlock();

	return skp;
}


static inline struct smack_known *smk_of_forked(const struct task_smack *tsp)
{
	return tsp->smk_forked;
}


static inline struct smack_known *smk_of_current(void)
{
	return smk_of_task(smack_cred(current_cred()));
}


#define SMACK_AUDIT_DENIED 0x1
#define SMACK_AUDIT_ACCEPT 0x2
extern int log_policy;

void smack_log(char *subject_label, char *object_label,
		int request,
		int result, struct smk_audit_info *auditdata);

#ifdef CONFIG_AUDIT


static inline void smk_ad_init(struct smk_audit_info *a, const char *func,
			       char type)
{
	memset(&a->sad, 0, sizeof(a->sad));
	a->a.type = type;
	a->a.smack_audit_data = &a->sad;
	a->a.smack_audit_data->function = func;
}

static inline void smk_ad_init_net(struct smk_audit_info *a, const char *func,
				   char type, struct lsm_network_audit *net)
{
	smk_ad_init(a, func, type);
	memset(net, 0, sizeof(*net));
	a->a.u.net = net;
}

static inline void smk_ad_setfield_u_tsk(struct smk_audit_info *a,
					 struct task_struct *t)
{
	a->a.u.tsk = t;
}
static inline void smk_ad_setfield_u_fs_path_dentry(struct smk_audit_info *a,
						    struct dentry *d)
{
	a->a.u.dentry = d;
}
static inline void smk_ad_setfield_u_fs_inode(struct smk_audit_info *a,
					      struct inode *i)
{
	a->a.u.inode = i;
}
static inline void smk_ad_setfield_u_fs_path(struct smk_audit_info *a,
					     struct path p)
{
	a->a.u.path = p;
}
static inline void smk_ad_setfield_u_net_sk(struct smk_audit_info *a,
					    struct sock *sk)
{
	a->a.u.net->sk = sk;
}

#else 

static inline void smk_ad_init(struct smk_audit_info *a, const char *func,
			       char type)
{
}
static inline void smk_ad_setfield_u_tsk(struct smk_audit_info *a,
					 struct task_struct *t)
{
}
static inline void smk_ad_setfield_u_fs_path_dentry(struct smk_audit_info *a,
						    struct dentry *d)
{
}
static inline void smk_ad_setfield_u_fs_inode(struct smk_audit_info *a,
					      struct inode *i)
{
}
static inline void smk_ad_setfield_u_fs_path(struct smk_audit_info *a,
					     struct path p)
{
}
static inline void smk_ad_setfield_u_net_sk(struct smk_audit_info *a,
					    struct sock *sk)
{
}
#endif

#endif  
