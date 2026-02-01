 
#include <target/target_core_base.h>
#include <linux/btree.h>

 
#define TCM_QLA2XXX_NAMELEN	32
 
#define TCM_QLA2XXX_DEFAULT_TAGS 2088

#include "qla_target.h"

struct tcm_qla2xxx_nacl {
	struct se_node_acl se_node_acl;

	 
	u32 nport_id;
	 
	u64 nport_wwnn;
	 
	char nport_name[TCM_QLA2XXX_NAMELEN];
	 
	struct fc_port *fc_port;
	 
	struct se_session *nport_nexus;
};

struct tcm_qla2xxx_tpg_attrib {
	int generate_node_acls;
	int cache_dynamic_acls;
	int demo_mode_write_protect;
	int prod_mode_write_protect;
	int demo_mode_login_only;
	int fabric_prot_type;
	int jam_host;
};

struct tcm_qla2xxx_tpg {
	 
	u16 lport_tpgt;
	 
	atomic_t lport_tpg_enabled;
	 
	struct tcm_qla2xxx_lport *lport;
	 
	struct tcm_qla2xxx_tpg_attrib tpg_attrib;
	 
	struct se_portal_group se_tpg;
};

struct tcm_qla2xxx_fc_loopid {
	struct se_node_acl *se_nacl;
};

struct tcm_qla2xxx_lport {
	 
	u64 lport_wwpn;
	 
	u64 lport_npiv_wwpn;
	 
	u64 lport_npiv_wwnn;
	 
	char lport_name[TCM_QLA2XXX_NAMELEN];
	 
	char lport_naa_name[TCM_QLA2XXX_NAMELEN];
	 
	struct btree_head32 lport_fcport_map;
	 
	struct tcm_qla2xxx_fc_loopid *lport_loopid_map;
	 
	struct scsi_qla_host *qla_vha;
	 
	struct qla_tgt lport_qla_tgt;
	 
	struct tcm_qla2xxx_tpg *tpg_1;
	 
	struct se_wwn lport_wwn;
};
