

#ifndef __TCM_FC_H__
#define __TCM_FC_H__

#include <linux/types.h>
#include <target/target_core_base.h>

#define FT_VERSION "0.4"

#define FT_NAMELEN 32		
#define FT_TPG_NAMELEN 32	
#define FT_LUN_NAMELEN 32	
#define TCM_FC_DEFAULT_TAGS 512	

struct ft_transport_id {
	__u8	format;
	__u8	__resvd1[7];
	__u8	wwpn[8];
	__u8	__resvd2[8];
} __attribute__((__packed__));


struct ft_sess {
	u32 port_id;			
	u32 params;
	u16 max_frame;			
	u64 port_name;			
	struct ft_tport *tport;
	struct se_session *se_sess;
	struct hlist_node hash;		
	struct rcu_head rcu;
	struct kref kref;		
};


#define	FT_SESS_HASH_BITS	6
#define	FT_SESS_HASH_SIZE	(1 << FT_SESS_HASH_BITS)


struct ft_tport {
	struct fc_lport *lport;
	struct ft_tpg *tpg;		
	u32	sess_count;		
	struct rcu_head rcu;
	struct hlist_head hash[FT_SESS_HASH_SIZE];	
};


struct ft_node_auth {
	u64	port_name;
	u64	node_name;
};


struct ft_node_acl {
	struct se_node_acl se_node_acl;
	struct ft_node_auth node_auth;
};

struct ft_lun {
	u32 index;
	char name[FT_LUN_NAMELEN];
};


struct ft_tpg {
	u32 index;
	struct ft_lport_wwn *lport_wwn;
	struct ft_tport *tport;		
	struct list_head lun_list;	
	struct se_portal_group se_tpg;
	struct workqueue_struct *workqueue;
};

struct ft_lport_wwn {
	u64 wwpn;
	char name[FT_NAMELEN];
	struct list_head ft_wwn_node;
	struct ft_tpg *tpg;
	struct se_wwn se_wwn;
};


struct ft_cmd {
	struct ft_sess *sess;		
	struct fc_seq *seq;		
	struct se_cmd se_cmd;		
	struct fc_frame *req_frame;
	u32 write_data_len;		
	struct work_struct work;
	
	unsigned char ft_sense_buffer[TRANSPORT_SENSE_BUFFER];
	u32 was_ddp_setup:1;		
	u32 aborted:1;			
	struct scatterlist *sg;		
	u32 sg_cnt;			
};

extern struct mutex ft_lport_lock;
extern struct fc4_prov ft_prov;
extern unsigned int ft_debug_logging;




void ft_sess_put(struct ft_sess *);
void ft_sess_close(struct se_session *);
u32 ft_sess_get_index(struct se_session *);
u32 ft_sess_get_port_name(struct se_session *, unsigned char *, u32);

void ft_lport_add(struct fc_lport *, void *);
void ft_lport_del(struct fc_lport *, void *);
int ft_lport_notify(struct notifier_block *, unsigned long, void *);


int ft_check_stop_free(struct se_cmd *);
void ft_release_cmd(struct se_cmd *);
int ft_queue_status(struct se_cmd *);
int ft_queue_data_in(struct se_cmd *);
int ft_write_pending(struct se_cmd *);
void ft_queue_tm_resp(struct se_cmd *);
void ft_aborted_task(struct se_cmd *);


void ft_recv_req(struct ft_sess *, struct fc_frame *);
struct ft_tpg *ft_lport_find_tpg(struct fc_lport *);

void ft_recv_write_data(struct ft_cmd *, struct fc_frame *);
void ft_dump_cmd(struct ft_cmd *, const char *caller);

ssize_t ft_format_wwn(char *, size_t, u64);


void ft_invl_hw_context(struct ft_cmd *);

#endif 
