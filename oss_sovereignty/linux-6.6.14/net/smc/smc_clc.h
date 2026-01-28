#ifndef _SMC_CLC_H
#define _SMC_CLC_H
#include <rdma/ib_verbs.h>
#include <linux/smc.h>
#include "smc.h"
#include "smc_netlink.h"
#define SMC_CLC_PROPOSAL	0x01
#define SMC_CLC_ACCEPT		0x02
#define SMC_CLC_CONFIRM		0x03
#define SMC_CLC_DECLINE		0x04
#define SMC_TYPE_R		0		 
#define SMC_TYPE_D		1		 
#define SMC_TYPE_N		2		 
#define SMC_TYPE_B		3		 
#define CLC_WAIT_TIME		(6 * HZ)	 
#define CLC_WAIT_TIME_SHORT	HZ		 
#define SMC_CLC_DECL_MEM	0x01010000   
#define SMC_CLC_DECL_TIMEOUT_CL	0x02010000   
#define SMC_CLC_DECL_TIMEOUT_AL	0x02020000   
#define SMC_CLC_DECL_CNFERR	0x03000000   
#define SMC_CLC_DECL_PEERNOSMC	0x03010000   
#define SMC_CLC_DECL_IPSEC	0x03020000   
#define SMC_CLC_DECL_NOSMCDEV	0x03030000   
#define SMC_CLC_DECL_NOSMCDDEV	0x03030001   
#define SMC_CLC_DECL_NOSMCRDEV	0x03030002   
#define SMC_CLC_DECL_NOISM2SUPP	0x03030003   
#define SMC_CLC_DECL_NOV2EXT	0x03030004   
#define SMC_CLC_DECL_NOV2DEXT	0x03030005   
#define SMC_CLC_DECL_NOSEID	0x03030006   
#define SMC_CLC_DECL_NOSMCD2DEV	0x03030007   
#define SMC_CLC_DECL_NOUEID	0x03030008   
#define SMC_CLC_DECL_RELEASEERR	0x03030009   
#define SMC_CLC_DECL_MAXCONNERR	0x0303000a   
#define SMC_CLC_DECL_MAXLINKERR	0x0303000b   
#define SMC_CLC_DECL_MODEUNSUPP	0x03040000   
#define SMC_CLC_DECL_RMBE_EC	0x03050000   
#define SMC_CLC_DECL_OPTUNSUPP	0x03060000   
#define SMC_CLC_DECL_DIFFPREFIX	0x03070000   
#define SMC_CLC_DECL_GETVLANERR	0x03080000   
#define SMC_CLC_DECL_ISMVLANERR	0x03090000   
#define SMC_CLC_DECL_NOACTLINK	0x030a0000   
#define SMC_CLC_DECL_NOSRVLINK	0x030b0000   
#define SMC_CLC_DECL_VERSMISMAT	0x030c0000   
#define SMC_CLC_DECL_MAX_DMB	0x030d0000   
#define SMC_CLC_DECL_NOROUTE	0x030e0000   
#define SMC_CLC_DECL_NOINDIRECT	0x030f0000   
#define SMC_CLC_DECL_SYNCERR	0x04000000   
#define SMC_CLC_DECL_PEERDECL	0x05000000   
#define SMC_CLC_DECL_INTERR	0x09990000   
#define SMC_CLC_DECL_ERR_RTOK	0x09990001   
#define SMC_CLC_DECL_ERR_RDYLNK	0x09990002   
#define SMC_CLC_DECL_ERR_REGBUF	0x09990003   
#define SMC_FIRST_CONTACT_MASK	0b10	 
struct smc_clc_msg_hdr {	 
	u8 eyecatcher[4];	 
	u8 type;		 
	__be16 length;
#if defined(__BIG_ENDIAN_BITFIELD)
	u8 version : 4,
	   typev2  : 2,
	   typev1  : 2;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	u8 typev1  : 2,
	   typev2  : 2,
	   version : 4;
#endif
} __packed;			 
struct smc_clc_msg_trail {	 
	u8 eyecatcher[4];
};
struct smc_clc_msg_local {	 
	u8 id_for_peer[SMC_SYSTEMID_LEN];  
	u8 gid[16];		 
	u8 mac[6];		 
};
struct smc_clc_ipv6_prefix {
	struct in6_addr prefix;
	u8 prefix_len;
} __packed;			 
#if defined(__BIG_ENDIAN_BITFIELD)
struct smc_clc_v2_flag {
	u8 release : 4,
	   rsvd    : 3,
	   seid    : 1;
};
#elif defined(__LITTLE_ENDIAN_BITFIELD)
struct smc_clc_v2_flag {
	u8 seid   : 1,
	rsvd      : 3,
	release   : 4;
};
#endif
struct smc_clnt_opts_area_hdr {
	u8 eid_cnt;		 
	u8 ism_gid_cnt;		 
	u8 reserved1;
	struct smc_clc_v2_flag flag;
	u8 reserved2[2];
	__be16 smcd_v2_ext_offset;  
};
struct smc_clc_smcd_gid_chid {
	__be64 gid;		 
	__be16 chid;		 
} __packed;		 
struct smc_clc_v2_extension {
	struct smc_clnt_opts_area_hdr hdr;
	u8 roce[16];		 
	u8 max_conns;
	u8 max_links;
	u8 reserved[14];
	u8 user_eids[][SMC_MAX_EID_LEN];
};
struct smc_clc_msg_proposal_prefix {	 
	__be32 outgoing_subnet;	 
	u8 prefix_len;		 
	u8 reserved[2];
	u8 ipv6_prefixes_cnt;	 
} __aligned(4);
struct smc_clc_msg_smcd {	 
	struct smc_clc_smcd_gid_chid ism;  
	__be16 v2_ext_offset;	 
	u8 vendor_oui[3];	 
	u8 vendor_exp_options[5];
	u8 reserved[20];
};
struct smc_clc_smcd_v2_extension {
	u8 system_eid[SMC_MAX_EID_LEN];
	u8 reserved[16];
	struct smc_clc_smcd_gid_chid gidchid[];
};
struct smc_clc_msg_proposal {	 
	struct smc_clc_msg_hdr hdr;
	struct smc_clc_msg_local lcl;
	__be16 iparea_offset;	 
} __aligned(4);
#define SMC_CLC_MAX_V6_PREFIX		8
#define SMC_CLC_MAX_UEID		8
struct smc_clc_msg_proposal_area {
	struct smc_clc_msg_proposal		pclc_base;
	struct smc_clc_msg_smcd			pclc_smcd;
	struct smc_clc_msg_proposal_prefix	pclc_prfx;
	struct smc_clc_ipv6_prefix	pclc_prfx_ipv6[SMC_CLC_MAX_V6_PREFIX];
	struct smc_clc_v2_extension		pclc_v2_ext;
	u8			user_eids[SMC_CLC_MAX_UEID][SMC_MAX_EID_LEN];
	struct smc_clc_smcd_v2_extension	pclc_smcd_v2_ext;
	struct smc_clc_smcd_gid_chid		pclc_gidchids[SMC_MAX_ISM_DEVS];
	struct smc_clc_msg_trail		pclc_trl;
};
struct smcr_clc_msg_accept_confirm {	 
	struct smc_clc_msg_local lcl;
	u8 qpn[3];			 
	__be32 rmb_rkey;		 
	u8 rmbe_idx;			 
	__be32 rmbe_alert_token;	 
 #if defined(__BIG_ENDIAN_BITFIELD)
	u8 rmbe_size : 4,		 
	   qp_mtu   : 4;		 
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	u8 qp_mtu   : 4,
	   rmbe_size : 4;
#endif
	u8 reserved;
	__be64 rmb_dma_addr;	 
	u8 reserved2;
	u8 psn[3];		 
} __packed;
struct smcd_clc_msg_accept_confirm_common {	 
	__be64 gid;		 
	__be64 token;		 
	u8 dmbe_idx;		 
#if defined(__BIG_ENDIAN_BITFIELD)
	u8 dmbe_size : 4,	 
	   reserved3 : 4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	u8 reserved3 : 4,
	   dmbe_size : 4;
#endif
	u16 reserved4;
	__be32 linkid;		 
} __packed;
#define SMC_CLC_OS_ZOS		1
#define SMC_CLC_OS_LINUX	2
#define SMC_CLC_OS_AIX		3
struct smc_clc_first_contact_ext {
#if defined(__BIG_ENDIAN_BITFIELD)
	u8 v2_direct : 1,
	   reserved  : 7;
	u8 os_type : 4,
	   release : 4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	u8 reserved  : 7,
	   v2_direct : 1;
	u8 release : 4,
	   os_type : 4;
#endif
	u8 reserved2[2];
	u8 hostname[SMC_MAX_HOSTNAME_LEN];
};
struct smc_clc_first_contact_ext_v2x {
	struct smc_clc_first_contact_ext fce_v2_base;
	u8 max_conns;  
	u8 max_links;  
	u8 reserved3[2];
	__be32 vendor_exp_options;
	u8 reserved4[8];
} __packed;		 
struct smc_clc_fce_gid_ext {
	u8 gid_cnt;
	u8 reserved2[3];
	u8 gid[][SMC_GID_SIZE];
};
struct smc_clc_msg_accept_confirm {	 
	struct smc_clc_msg_hdr hdr;
	union {
		struct smcr_clc_msg_accept_confirm r0;  
		struct {  
			struct smcd_clc_msg_accept_confirm_common d0;
			u32 reserved5[3];
		};
	};
} __packed;			 
struct smc_clc_msg_accept_confirm_v2 {	 
	struct smc_clc_msg_hdr hdr;
	union {
		struct {  
			struct smcr_clc_msg_accept_confirm r0;
			u8 eid[SMC_MAX_EID_LEN];
			u8 reserved6[8];
		} r1;
		struct {  
			struct smcd_clc_msg_accept_confirm_common d0;
			__be16 chid;
			u8 eid[SMC_MAX_EID_LEN];
			u8 reserved5[8];
		} d1;
	};
};
struct smc_clc_msg_decline {	 
	struct smc_clc_msg_hdr hdr;
	u8 id_for_peer[SMC_SYSTEMID_LEN];  
	__be32 peer_diagnosis;	 
#if defined(__BIG_ENDIAN_BITFIELD)
	u8 os_type  : 4,
	   reserved : 4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	u8 reserved : 4,
	   os_type  : 4;
#endif
	u8 reserved2[3];
	struct smc_clc_msg_trail trl;  
} __aligned(4);
#define SMC_DECL_DIAG_COUNT_V2	4  
struct smc_clc_msg_decline_v2 {	 
	struct smc_clc_msg_hdr hdr;
	u8 id_for_peer[SMC_SYSTEMID_LEN];  
	__be32 peer_diagnosis;	 
#if defined(__BIG_ENDIAN_BITFIELD)
	u8 os_type  : 4,
	   reserved : 4;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	u8 reserved : 4,
	   os_type  : 4;
#endif
	u8 reserved2[3];
	__be32 peer_diagnosis_v2[SMC_DECL_DIAG_COUNT_V2];
	struct smc_clc_msg_trail trl;  
} __aligned(4);
static inline struct smc_clc_msg_proposal_prefix *
smc_clc_proposal_get_prefix(struct smc_clc_msg_proposal *pclc)
{
	return (struct smc_clc_msg_proposal_prefix *)
	       ((u8 *)pclc + sizeof(*pclc) + ntohs(pclc->iparea_offset));
}
static inline bool smcr_indicated(int smc_type)
{
	return smc_type == SMC_TYPE_R || smc_type == SMC_TYPE_B;
}
static inline bool smcd_indicated(int smc_type)
{
	return smc_type == SMC_TYPE_D || smc_type == SMC_TYPE_B;
}
static inline u8 smc_indicated_type(int is_smcd, int is_smcr)
{
	if (is_smcd && is_smcr)
		return SMC_TYPE_B;
	if (is_smcd)
		return SMC_TYPE_D;
	if (is_smcr)
		return SMC_TYPE_R;
	return SMC_TYPE_N;
}
static inline struct smc_clc_msg_smcd *
smc_get_clc_msg_smcd(struct smc_clc_msg_proposal *prop)
{
	if (smcd_indicated(prop->hdr.typev1) &&
	    ntohs(prop->iparea_offset) != sizeof(struct smc_clc_msg_smcd))
		return NULL;
	return (struct smc_clc_msg_smcd *)(prop + 1);
}
static inline struct smc_clc_v2_extension *
smc_get_clc_v2_ext(struct smc_clc_msg_proposal *prop)
{
	struct smc_clc_msg_smcd *prop_smcd = smc_get_clc_msg_smcd(prop);
	if (!prop_smcd || !ntohs(prop_smcd->v2_ext_offset))
		return NULL;
	return (struct smc_clc_v2_extension *)
	       ((u8 *)prop_smcd +
	       offsetof(struct smc_clc_msg_smcd, v2_ext_offset) +
	       sizeof(prop_smcd->v2_ext_offset) +
	       ntohs(prop_smcd->v2_ext_offset));
}
static inline struct smc_clc_smcd_v2_extension *
smc_get_clc_smcd_v2_ext(struct smc_clc_v2_extension *prop_v2ext)
{
	if (!prop_v2ext)
		return NULL;
	if (!ntohs(prop_v2ext->hdr.smcd_v2_ext_offset))
		return NULL;
	return (struct smc_clc_smcd_v2_extension *)
		((u8 *)prop_v2ext +
		 offsetof(struct smc_clc_v2_extension, hdr) +
		 offsetof(struct smc_clnt_opts_area_hdr, smcd_v2_ext_offset) +
		 sizeof(prop_v2ext->hdr.smcd_v2_ext_offset) +
		 ntohs(prop_v2ext->hdr.smcd_v2_ext_offset));
}
static inline struct smc_clc_first_contact_ext *
smc_get_clc_first_contact_ext(struct smc_clc_msg_accept_confirm_v2 *clc_v2,
			      bool is_smcd)
{
	int clc_v2_len;
	if (clc_v2->hdr.version == SMC_V1 ||
	    !(clc_v2->hdr.typev2 & SMC_FIRST_CONTACT_MASK))
		return NULL;
	if (is_smcd)
		clc_v2_len =
			offsetofend(struct smc_clc_msg_accept_confirm_v2, d1);
	else
		clc_v2_len =
			offsetofend(struct smc_clc_msg_accept_confirm_v2, r1);
	return (struct smc_clc_first_contact_ext *)(((u8 *)clc_v2) +
						    clc_v2_len);
}
struct smcd_dev;
struct smc_init_info;
int smc_clc_prfx_match(struct socket *clcsock,
		       struct smc_clc_msg_proposal_prefix *prop);
int smc_clc_wait_msg(struct smc_sock *smc, void *buf, int buflen,
		     u8 expected_type, unsigned long timeout);
int smc_clc_send_decline(struct smc_sock *smc, u32 peer_diag_info, u8 version);
int smc_clc_send_proposal(struct smc_sock *smc, struct smc_init_info *ini);
int smc_clc_send_confirm(struct smc_sock *smc, bool clnt_first_contact,
			 u8 version, u8 *eid, struct smc_init_info *ini);
int smc_clc_send_accept(struct smc_sock *smc, bool srv_first_contact,
			u8 version, u8 *negotiated_eid, struct smc_init_info *ini);
int smc_clc_srv_v2x_features_validate(struct smc_clc_msg_proposal *pclc,
				      struct smc_init_info *ini);
int smc_clc_clnt_v2x_features_validate(struct smc_clc_first_contact_ext *fce,
				       struct smc_init_info *ini);
int smc_clc_v2x_features_confirm_check(struct smc_clc_msg_accept_confirm *cclc,
				       struct smc_init_info *ini);
void smc_clc_init(void) __init;
void smc_clc_exit(void);
void smc_clc_get_hostname(u8 **host);
bool smc_clc_match_eid(u8 *negotiated_eid,
		       struct smc_clc_v2_extension *smc_v2_ext,
		       u8 *peer_eid, u8 *local_eid);
int smc_clc_ueid_count(void);
int smc_nl_dump_ueid(struct sk_buff *skb, struct netlink_callback *cb);
int smc_nl_add_ueid(struct sk_buff *skb, struct genl_info *info);
int smc_nl_remove_ueid(struct sk_buff *skb, struct genl_info *info);
int smc_nl_flush_ueid(struct sk_buff *skb, struct genl_info *info);
int smc_nl_dump_seid(struct sk_buff *skb, struct netlink_callback *cb);
int smc_nl_enable_seid(struct sk_buff *skb, struct genl_info *info);
int smc_nl_disable_seid(struct sk_buff *skb, struct genl_info *info);
#endif
