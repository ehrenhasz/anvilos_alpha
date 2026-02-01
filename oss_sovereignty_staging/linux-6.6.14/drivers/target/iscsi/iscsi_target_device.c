
 

#include <target/target_core_base.h>
#include <target/target_core_fabric.h>

#include <target/iscsi/iscsi_target_core.h>
#include "iscsi_target_device.h"
#include "iscsi_target_tpg.h"
#include "iscsi_target_util.h"

void iscsit_determine_maxcmdsn(struct iscsit_session *sess)
{
	struct se_node_acl *se_nacl;

	 
	if (sess->sess_ops->SessionType)
		return;

	se_nacl = sess->se_sess->se_node_acl;

	 
	sess->cmdsn_window = se_nacl->queue_depth;
	atomic_add(se_nacl->queue_depth - 1, &sess->max_cmd_sn);
}

void iscsit_increment_maxcmdsn(struct iscsit_cmd *cmd, struct iscsit_session *sess)
{
	u32 max_cmd_sn;

	if (cmd->immediate_cmd || cmd->maxcmdsn_inc)
		return;

	cmd->maxcmdsn_inc = 1;

	max_cmd_sn = atomic_inc_return(&sess->max_cmd_sn);
	pr_debug("Updated MaxCmdSN to 0x%08x\n", max_cmd_sn);
}
EXPORT_SYMBOL(iscsit_increment_maxcmdsn);
