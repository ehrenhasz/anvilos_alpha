
 

 

#include "efc.h"
#include "efc_device.h"
#include "efc_fabric.h"

void
efc_d_send_prli_rsp(struct efc_node *node, u16 ox_id)
{
	int rc = EFC_SCSI_CALL_COMPLETE;
	struct efc *efc = node->efc;

	node->ls_acc_oxid = ox_id;
	node->send_ls_acc = EFC_NODE_SEND_LS_ACC_PRLI;

	 

	if (node->init) {
		efc_log_info(efc, "[%s] found(initiator) WWPN:%s WWNN:%s\n",
			     node->display_name, node->wwpn, node->wwnn);
		if (node->nport->enable_tgt)
			rc = efc->tt.scsi_new_node(efc, node);
	}

	if (rc < 0)
		efc_node_post_event(node, EFC_EVT_NODE_SESS_REG_FAIL, NULL);

	if (rc == EFC_SCSI_CALL_COMPLETE)
		efc_node_post_event(node, EFC_EVT_NODE_SESS_REG_OK, NULL);
}

static void
__efc_d_common(const char *funcname, struct efc_sm_ctx *ctx,
	       enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = NULL;
	struct efc *efc = NULL;

	node = ctx->app;
	efc = node->efc;

	switch (evt) {
	 
	case EFC_EVT_SHUTDOWN:
		efc_log_debug(efc, "[%s] %-20s %-20s\n", node->display_name,
			      funcname, efc_sm_event_name(evt));
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_node_transition(node, __efc_d_initiate_shutdown, NULL);
		break;
	case EFC_EVT_SHUTDOWN_EXPLICIT_LOGO:
		efc_log_debug(efc, "[%s] %-20s %-20s\n",
			      node->display_name, funcname,
				efc_sm_event_name(evt));
		node->shutdown_reason = EFC_NODE_SHUTDOWN_EXPLICIT_LOGO;
		efc_node_transition(node, __efc_d_initiate_shutdown, NULL);
		break;
	case EFC_EVT_SHUTDOWN_IMPLICIT_LOGO:
		efc_log_debug(efc, "[%s] %-20s %-20s\n", node->display_name,
			      funcname, efc_sm_event_name(evt));
		node->shutdown_reason = EFC_NODE_SHUTDOWN_IMPLICIT_LOGO;
		efc_node_transition(node, __efc_d_initiate_shutdown, NULL);
		break;

	default:
		 
		__efc_node_common(funcname, ctx, evt, arg);
	}
}

static void
__efc_d_wait_del_node(struct efc_sm_ctx *ctx,
		      enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	 
	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		efc_node_hold_frames(node);
		fallthrough;

	case EFC_EVT_NODE_ACTIVE_IO_LIST_EMPTY:
	case EFC_EVT_ALL_CHILD_NODES_FREE:
		 
		break;

	case EFC_EVT_NODE_DEL_INI_COMPLETE:
	case EFC_EVT_NODE_DEL_TGT_COMPLETE:
		 
		efc_node_initiate_cleanup(node);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	case EFC_EVT_SRRS_ELS_REQ_FAIL:
		 
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		break;

	 
	case EFC_EVT_SHUTDOWN:
		 
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		fallthrough;

	case EFC_EVT_SHUTDOWN_EXPLICIT_LOGO:
	case EFC_EVT_SHUTDOWN_IMPLICIT_LOGO:
		node_printf(node, "%s received\n", efc_sm_event_name(evt));
		break;
	case EFC_EVT_DOMAIN_ATTACH_OK:
		 
		break;
	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

static void
__efc_d_wait_del_ini_tgt(struct efc_sm_ctx *ctx,
			 enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		efc_node_hold_frames(node);
		fallthrough;

	case EFC_EVT_NODE_ACTIVE_IO_LIST_EMPTY:
	case EFC_EVT_ALL_CHILD_NODES_FREE:
		 
		break;

	case EFC_EVT_NODE_DEL_INI_COMPLETE:
	case EFC_EVT_NODE_DEL_TGT_COMPLETE:
		efc_node_transition(node, __efc_d_wait_del_node, NULL);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	case EFC_EVT_SRRS_ELS_REQ_FAIL:
		 
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		break;

	 
	case EFC_EVT_SHUTDOWN:
		 
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		fallthrough;

	case EFC_EVT_SHUTDOWN_EXPLICIT_LOGO:
	case EFC_EVT_SHUTDOWN_IMPLICIT_LOGO:
		node_printf(node, "%s received\n", efc_sm_event_name(evt));
		break;
	case EFC_EVT_DOMAIN_ATTACH_OK:
		 
		break;
	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_initiate_shutdown(struct efc_sm_ctx *ctx,
			  enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;
	struct efc *efc = node->efc;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER: {
		int rc = EFC_SCSI_CALL_COMPLETE;

		 
		node->els_io_enabled = false;

		 
		if (node->init && !node->targ) {
			efc_log_info(node->efc,
				     "[%s] delete (initiator) WWPN %s WWNN %s\n",
				node->display_name,
				node->wwpn, node->wwnn);
			efc_node_transition(node,
					    __efc_d_wait_del_node,
					     NULL);
			if (node->nport->enable_tgt)
				rc = efc->tt.scsi_del_node(efc, node,
					EFC_SCSI_INITIATOR_DELETED);

			if (rc == EFC_SCSI_CALL_COMPLETE || rc < 0)
				efc_node_post_event(node,
					EFC_EVT_NODE_DEL_INI_COMPLETE, NULL);

		} else if (node->targ && !node->init) {
			efc_log_info(node->efc,
				     "[%s] delete (target) WWPN %s WWNN %s\n",
				node->display_name,
				node->wwpn, node->wwnn);
			efc_node_transition(node,
					    __efc_d_wait_del_node,
					     NULL);
			if (node->nport->enable_ini)
				rc = efc->tt.scsi_del_node(efc, node,
					EFC_SCSI_TARGET_DELETED);

			if (rc == EFC_SCSI_CALL_COMPLETE)
				efc_node_post_event(node,
					EFC_EVT_NODE_DEL_TGT_COMPLETE, NULL);

		} else if (node->init && node->targ) {
			efc_log_info(node->efc,
				     "[%s] delete (I+T) WWPN %s WWNN %s\n",
				node->display_name, node->wwpn, node->wwnn);
			efc_node_transition(node, __efc_d_wait_del_ini_tgt,
					    NULL);
			if (node->nport->enable_tgt)
				rc = efc->tt.scsi_del_node(efc, node,
						EFC_SCSI_INITIATOR_DELETED);

			if (rc == EFC_SCSI_CALL_COMPLETE)
				efc_node_post_event(node,
					EFC_EVT_NODE_DEL_INI_COMPLETE, NULL);
			 
			rc = EFC_SCSI_CALL_COMPLETE;
			if (node->nport->enable_ini)
				rc = efc->tt.scsi_del_node(efc, node,
						EFC_SCSI_TARGET_DELETED);

			if (rc == EFC_SCSI_CALL_COMPLETE)
				efc_node_post_event(node,
					EFC_EVT_NODE_DEL_TGT_COMPLETE, NULL);
		}

		 
		if (node->attached) {
			 
			rc = efc_cmd_node_detach(efc, &node->rnode);
			if (rc < 0)
				node_printf(node,
					    "Failed freeing HW node, rc=%d\n",
					rc);
		}

		 
		if (!node->init && !node->targ) {
			 
			efc_node_initiate_cleanup(node);
		}
		break;
	}
	case EFC_EVT_ALL_CHILD_NODES_FREE:
		 
		break;
	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_wait_loop(struct efc_sm_ctx *ctx,
		  enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		efc_node_hold_frames(node);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	case EFC_EVT_DOMAIN_ATTACH_OK: {
		 
		efc_node_init_device(node, true);
		break;
	}
	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
efc_send_ls_acc_after_attach(struct efc_node *node,
			     struct fc_frame_header *hdr,
			     enum efc_node_send_ls_acc ls)
{
	u16 ox_id = be16_to_cpu(hdr->fh_ox_id);

	 
	WARN_ON(node->send_ls_acc != EFC_NODE_SEND_LS_ACC_NONE);

	node->ls_acc_oxid = ox_id;
	node->send_ls_acc = ls;
	node->ls_acc_did = ntoh24(hdr->fh_d_id);
}

void
efc_process_prli_payload(struct efc_node *node, void *prli)
{
	struct {
		struct fc_els_prli prli;
		struct fc_els_spp sp;
	} *pp;

	pp = prli;
	node->init = (pp->sp.spp_flags & FCP_SPPF_INIT_FCN) != 0;
	node->targ = (pp->sp.spp_flags & FCP_SPPF_TARG_FCN) != 0;
}

void
__efc_d_wait_plogi_acc_cmpl(struct efc_sm_ctx *ctx,
			    enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		efc_node_hold_frames(node);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	case EFC_EVT_SRRS_ELS_CMPL_FAIL:
		WARN_ON(!node->els_cmpl_cnt);
		node->els_cmpl_cnt--;
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_node_transition(node, __efc_d_initiate_shutdown, NULL);
		break;

	case EFC_EVT_SRRS_ELS_CMPL_OK:	 
		WARN_ON(!node->els_cmpl_cnt);
		node->els_cmpl_cnt--;
		efc_node_transition(node, __efc_d_port_logged_in, NULL);
		break;

	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_wait_logo_rsp(struct efc_sm_ctx *ctx,
		      enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		efc_node_hold_frames(node);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	case EFC_EVT_SRRS_ELS_REQ_OK:
	case EFC_EVT_SRRS_ELS_REQ_RJT:
	case EFC_EVT_SRRS_ELS_REQ_FAIL:
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_LOGO,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		node_printf(node,
			    "LOGO sent (evt=%s), shutdown node\n",
			efc_sm_event_name(evt));
		 
		efc_node_post_event(node, EFC_EVT_SHUTDOWN_EXPLICIT_LOGO,
				    NULL);
		break;

	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
efc_node_init_device(struct efc_node *node, bool send_plogi)
{
	node->send_plogi = send_plogi;
	if ((node->efc->nodedb_mask & EFC_NODEDB_PAUSE_NEW_NODES) &&
	    (node->rnode.fc_id != FC_FID_DOM_MGR)) {
		node->nodedb_state = __efc_d_init;
		efc_node_transition(node, __efc_node_paused, NULL);
	} else {
		efc_node_transition(node, __efc_d_init, NULL);
	}
}

static void
efc_d_check_plogi_topology(struct efc_node *node, u32 d_id)
{
	switch (node->nport->topology) {
	case EFC_NPORT_TOPO_P2P:
		 
		efc_domain_attach(node->nport->domain, d_id);
		efc_node_transition(node, __efc_d_wait_domain_attach, NULL);
		break;
	case EFC_NPORT_TOPO_FABRIC:
		 
		efc_node_transition(node, __efc_d_wait_domain_attach, NULL);
		break;
	case EFC_NPORT_TOPO_UNKNOWN:
		 
		node_printf(node, "received PLOGI, unknown topology did=0x%x\n",
			    d_id);
		efc_node_transition(node, __efc_d_wait_topology_notify, NULL);
		break;
	default:
		node_printf(node, "received PLOGI, unexpected topology %d\n",
			    node->nport->topology);
	}
}

void
__efc_d_init(struct efc_sm_ctx *ctx, enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	 
	switch (evt) {
	case EFC_EVT_ENTER:
		if (!node->send_plogi)
			break;
		 
		if (node->nport->enable_ini &&
		    node->nport->domain->attached) {
			efc_send_plogi(node);

			efc_node_transition(node, __efc_d_wait_plogi_rsp, NULL);
		} else {
			node_printf(node, "not sending plogi nport.ini=%d,",
				    node->nport->enable_ini);
			node_printf(node, "domain attached=%d\n",
				    node->nport->domain->attached);
		}
		break;
	case EFC_EVT_PLOGI_RCVD: {
		 
		struct fc_frame_header *hdr = cbdata->header->dma.virt;
		int rc;

		efc_node_save_sparms(node, cbdata->payload->dma.virt);
		efc_send_ls_acc_after_attach(node,
					     cbdata->header->dma.virt,
					     EFC_NODE_SEND_LS_ACC_PLOGI);

		 
		if (!node->nport->domain->attached) {
			efc_d_check_plogi_topology(node, ntoh24(hdr->fh_d_id));
			break;
		}

		 
		rc = efc_node_attach(node);
		efc_node_transition(node, __efc_d_wait_node_attach, NULL);
		if (rc < 0)
			efc_node_post_event(node, EFC_EVT_NODE_ATTACH_FAIL, NULL);

		break;
	}

	case EFC_EVT_FDISC_RCVD: {
		__efc_d_common(__func__, ctx, evt, arg);
		break;
	}

	case EFC_EVT_FLOGI_RCVD: {
		struct fc_frame_header *hdr = cbdata->header->dma.virt;
		u32 d_id = ntoh24(hdr->fh_d_id);

		 
		memcpy(node->nport->domain->flogi_service_params,
		       cbdata->payload->dma.virt,
		       sizeof(struct fc_els_flogi));

		 
		efc_fabric_set_topology(node, EFC_NPORT_TOPO_P2P);

		efc_send_flogi_p2p_acc(node, be16_to_cpu(hdr->fh_ox_id), d_id);

		if (efc_p2p_setup(node->nport)) {
			node_printf(node, "p2p failed, shutting down node\n");
			efc_node_post_event(node, EFC_EVT_SHUTDOWN, NULL);
			break;
		}

		efc_node_transition(node,  __efc_p2p_wait_flogi_acc_cmpl, NULL);
		break;
	}

	case EFC_EVT_LOGO_RCVD: {
		struct fc_frame_header *hdr = cbdata->header->dma.virt;

		if (!node->nport->domain->attached) {
			 
			node_printf(node, "%s domain not attached, dropping\n",
				    efc_sm_event_name(evt));
			efc_node_post_event(node,
					EFC_EVT_SHUTDOWN_EXPLICIT_LOGO, NULL);
			break;
		}

		efc_send_logo_acc(node, be16_to_cpu(hdr->fh_ox_id));
		efc_node_transition(node, __efc_d_wait_logo_acc_cmpl, NULL);
		break;
	}

	case EFC_EVT_PRLI_RCVD:
	case EFC_EVT_PRLO_RCVD:
	case EFC_EVT_PDISC_RCVD:
	case EFC_EVT_ADISC_RCVD:
	case EFC_EVT_RSCN_RCVD: {
		struct fc_frame_header *hdr = cbdata->header->dma.virt;

		if (!node->nport->domain->attached) {
			 
			node_printf(node, "%s domain not attached, dropping\n",
				    efc_sm_event_name(evt));

			efc_node_post_event(node,
					    EFC_EVT_SHUTDOWN_EXPLICIT_LOGO,
					    NULL);
			break;
		}
		node_printf(node, "%s received, sending reject\n",
			    efc_sm_event_name(evt));

		efc_send_ls_rjt(node, be16_to_cpu(hdr->fh_ox_id),
				ELS_RJT_UNAB, ELS_EXPL_PLOGI_REQD, 0);

		break;
	}

	case EFC_EVT_FCP_CMD_RCVD: {
		 
		if (!node->nport->domain->attached) {
			 
			node_printf(node, "%s domain not attached, dropping\n",
				    efc_sm_event_name(evt));
			efc_node_post_event(node,
					    EFC_EVT_SHUTDOWN_EXPLICIT_LOGO,
					    NULL);
			break;
		}

		 
		node_printf(node, "FCP_CMND received, send LOGO\n");
		if (efc_send_logo(node)) {
			 
			node_printf(node, "Failed to send LOGO\n");
			efc_node_post_event(node,
					    EFC_EVT_SHUTDOWN_EXPLICIT_LOGO,
					    NULL);
		} else {
			 
			efc_node_transition(node,
					    __efc_d_wait_logo_rsp, NULL);
		}
		break;
	}
	case EFC_EVT_DOMAIN_ATTACH_OK:
		 
		break;

	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_wait_plogi_rsp(struct efc_sm_ctx *ctx,
		       enum efc_sm_event evt, void *arg)
{
	int rc;
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_PLOGI_RCVD: {
		 
		 
		efc_node_save_sparms(node, cbdata->payload->dma.virt);
		efc_send_ls_acc_after_attach(node,
					     cbdata->header->dma.virt,
				EFC_NODE_SEND_LS_ACC_PLOGI);
		 
		rc = efc_node_attach(node);
		efc_node_transition(node, __efc_d_wait_node_attach, NULL);
		if (rc < 0)
			efc_node_post_event(node,
					    EFC_EVT_NODE_ATTACH_FAIL, NULL);

		break;
	}

	case EFC_EVT_PRLI_RCVD:
		 
		 
		efc_process_prli_payload(node, cbdata->payload->dma.virt);
		efc_send_ls_acc_after_attach(node,
					     cbdata->header->dma.virt,
				EFC_NODE_SEND_LS_ACC_PRLI);
		efc_node_transition(node, __efc_d_wait_plogi_rsp_recvd_prli,
				    NULL);
		break;

	case EFC_EVT_LOGO_RCVD:  
	case EFC_EVT_PRLO_RCVD:
	case EFC_EVT_PDISC_RCVD:
	case EFC_EVT_FDISC_RCVD:
	case EFC_EVT_ADISC_RCVD:
	case EFC_EVT_RSCN_RCVD:
	case EFC_EVT_SCR_RCVD: {
		struct fc_frame_header *hdr = cbdata->header->dma.virt;

		node_printf(node, "%s received, sending reject\n",
			    efc_sm_event_name(evt));

		efc_send_ls_rjt(node, be16_to_cpu(hdr->fh_ox_id),
				ELS_RJT_UNAB, ELS_EXPL_PLOGI_REQD, 0);

		break;
	}

	case EFC_EVT_SRRS_ELS_REQ_OK:	 
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PLOGI,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		 
		efc_node_save_sparms(node, cbdata->els_rsp.virt);
		rc = efc_node_attach(node);
		efc_node_transition(node, __efc_d_wait_node_attach, NULL);
		if (rc < 0)
			efc_node_post_event(node,
					    EFC_EVT_NODE_ATTACH_FAIL, NULL);

		break;

	case EFC_EVT_SRRS_ELS_REQ_FAIL:	 
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PLOGI,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		efc_node_post_event(node, EFC_EVT_SHUTDOWN, NULL);
		break;

	case EFC_EVT_SRRS_ELS_REQ_RJT:
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PLOGI,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		break;

	case EFC_EVT_FCP_CMD_RCVD: {
		 
		node_printf(node, "FCP_CMND received, drop\n");
		break;
	}

	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_wait_plogi_rsp_recvd_prli(struct efc_sm_ctx *ctx,
				  enum efc_sm_event evt, void *arg)
{
	int rc;
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		 
		efc_node_hold_frames(node);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	case EFC_EVT_SRRS_ELS_REQ_OK:	 
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PLOGI,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		 
		efc_node_save_sparms(node, cbdata->els_rsp.virt);
		rc = efc_node_attach(node);
		efc_node_transition(node, __efc_d_wait_node_attach, NULL);
		if (rc < 0)
			efc_node_post_event(node, EFC_EVT_NODE_ATTACH_FAIL,
					    NULL);

		break;

	case EFC_EVT_SRRS_ELS_REQ_FAIL:	 
	case EFC_EVT_SRRS_ELS_REQ_RJT:
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PLOGI,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		efc_node_post_event(node, EFC_EVT_SHUTDOWN, NULL);
		break;

	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_wait_domain_attach(struct efc_sm_ctx *ctx,
			   enum efc_sm_event evt, void *arg)
{
	int rc;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		efc_node_hold_frames(node);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	case EFC_EVT_DOMAIN_ATTACH_OK:
		WARN_ON(!node->nport->domain->attached);
		 
		rc = efc_node_attach(node);
		efc_node_transition(node, __efc_d_wait_node_attach, NULL);
		if (rc < 0)
			efc_node_post_event(node, EFC_EVT_NODE_ATTACH_FAIL,
					    NULL);

		break;

	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_wait_topology_notify(struct efc_sm_ctx *ctx,
			     enum efc_sm_event evt, void *arg)
{
	int rc;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		efc_node_hold_frames(node);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	case EFC_EVT_NPORT_TOPOLOGY_NOTIFY: {
		enum efc_nport_topology *topology = arg;

		WARN_ON(node->nport->domain->attached);

		WARN_ON(node->send_ls_acc != EFC_NODE_SEND_LS_ACC_PLOGI);

		node_printf(node, "topology notification, topology=%d\n",
			    *topology);

		 
		if (*topology == EFC_NPORT_TOPO_P2P) {
			 
			efc_domain_attach(node->nport->domain,
					  node->ls_acc_did);
		}
		 

		efc_node_transition(node, __efc_d_wait_domain_attach, NULL);
		break;
	}
	case EFC_EVT_DOMAIN_ATTACH_OK:
		WARN_ON(!node->nport->domain->attached);
		node_printf(node, "domain attach ok\n");
		 
		rc = efc_node_attach(node);
		efc_node_transition(node, __efc_d_wait_node_attach, NULL);
		if (rc < 0)
			efc_node_post_event(node,
					    EFC_EVT_NODE_ATTACH_FAIL, NULL);

		break;

	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_wait_node_attach(struct efc_sm_ctx *ctx,
			 enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		efc_node_hold_frames(node);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	case EFC_EVT_NODE_ATTACH_OK:
		node->attached = true;
		switch (node->send_ls_acc) {
		case EFC_NODE_SEND_LS_ACC_PLOGI: {
			 
			 
			efc_send_plogi_acc(node, node->ls_acc_oxid);
			efc_node_transition(node, __efc_d_wait_plogi_acc_cmpl,
					    NULL);
			node->send_ls_acc = EFC_NODE_SEND_LS_ACC_NONE;
			node->ls_acc_io = NULL;
			break;
		}
		case EFC_NODE_SEND_LS_ACC_PRLI: {
			efc_d_send_prli_rsp(node, node->ls_acc_oxid);
			node->send_ls_acc = EFC_NODE_SEND_LS_ACC_NONE;
			node->ls_acc_io = NULL;
			break;
		}
		case EFC_NODE_SEND_LS_ACC_NONE:
		default:
			 
			 
			efc_node_transition(node,
					    __efc_d_port_logged_in, NULL);
			break;
		}
		break;

	case EFC_EVT_NODE_ATTACH_FAIL:
		 
		node->attached = false;
		node_printf(node, "node attach failed\n");
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_node_transition(node, __efc_d_initiate_shutdown, NULL);
		break;

	 
	case EFC_EVT_SHUTDOWN:
		node_printf(node, "%s received\n", efc_sm_event_name(evt));
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_node_transition(node, __efc_d_wait_attach_evt_shutdown,
				    NULL);
		break;
	case EFC_EVT_SHUTDOWN_EXPLICIT_LOGO:
		node_printf(node, "%s received\n", efc_sm_event_name(evt));
		node->shutdown_reason = EFC_NODE_SHUTDOWN_EXPLICIT_LOGO;
		efc_node_transition(node, __efc_d_wait_attach_evt_shutdown,
				    NULL);
		break;
	case EFC_EVT_SHUTDOWN_IMPLICIT_LOGO:
		node_printf(node, "%s received\n", efc_sm_event_name(evt));
		node->shutdown_reason = EFC_NODE_SHUTDOWN_IMPLICIT_LOGO;
		efc_node_transition(node,
				    __efc_d_wait_attach_evt_shutdown, NULL);
		break;
	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_wait_attach_evt_shutdown(struct efc_sm_ctx *ctx,
				 enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		efc_node_hold_frames(node);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	 
	case EFC_EVT_NODE_ATTACH_OK:
		node->attached = true;
		node_printf(node, "Attach evt=%s, proceed to shutdown\n",
			    efc_sm_event_name(evt));
		efc_node_transition(node, __efc_d_initiate_shutdown, NULL);
		break;

	case EFC_EVT_NODE_ATTACH_FAIL:
		 
		node->attached = false;
		node_printf(node, "Attach evt=%s, proceed to shutdown\n",
			    efc_sm_event_name(evt));
		efc_node_transition(node, __efc_d_initiate_shutdown, NULL);
		break;

	 
	case EFC_EVT_SHUTDOWN:
		 
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		fallthrough;

	case EFC_EVT_SHUTDOWN_EXPLICIT_LOGO:
	case EFC_EVT_SHUTDOWN_IMPLICIT_LOGO:
		node_printf(node, "%s received\n", efc_sm_event_name(evt));
		break;

	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_port_logged_in(struct efc_sm_ctx *ctx,
		       enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		 
		if (node->nport->enable_ini &&
		    !(node->rnode.fc_id != FC_FID_DOM_MGR)) {
			 
			efc_send_prli(node);
			 
		}
		break;

	case EFC_EVT_FCP_CMD_RCVD: {
		break;
	}

	case EFC_EVT_PRLI_RCVD: {
		 
		struct fc_frame_header *hdr = cbdata->header->dma.virt;
		struct {
			struct fc_els_prli prli;
			struct fc_els_spp sp;
		} *pp;

		pp = cbdata->payload->dma.virt;
		if (pp->sp.spp_type != FC_TYPE_FCP) {
			 
			efc_send_ls_rjt(node, be16_to_cpu(hdr->fh_ox_id),
					ELS_RJT_UNAB, ELS_EXPL_UNSUPR, 0);
			break;
		}

		efc_process_prli_payload(node, cbdata->payload->dma.virt);
		efc_d_send_prli_rsp(node, be16_to_cpu(hdr->fh_ox_id));
		break;
	}

	case EFC_EVT_NODE_SESS_REG_OK:
		if (node->send_ls_acc == EFC_NODE_SEND_LS_ACC_PRLI)
			efc_send_prli_acc(node, node->ls_acc_oxid);

		node->send_ls_acc = EFC_NODE_SEND_LS_ACC_NONE;
		efc_node_transition(node, __efc_d_device_ready, NULL);
		break;

	case EFC_EVT_NODE_SESS_REG_FAIL:
		efc_send_ls_rjt(node, node->ls_acc_oxid, ELS_RJT_UNAB,
				ELS_EXPL_UNSUPR, 0);
		node->send_ls_acc = EFC_NODE_SEND_LS_ACC_NONE;
		break;

	case EFC_EVT_SRRS_ELS_REQ_OK: {	 
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PRLI,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		 
		efc_process_prli_payload(node, cbdata->els_rsp.virt);
		efc_node_transition(node, __efc_d_device_ready, NULL);
		break;
	}

	case EFC_EVT_SRRS_ELS_REQ_FAIL: {	 
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PRLI,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		efc_node_post_event(node, EFC_EVT_SHUTDOWN, NULL);
		break;
	}

	case EFC_EVT_SRRS_ELS_REQ_RJT: {
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PRLI,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		break;
	}

	case EFC_EVT_SRRS_ELS_CMPL_OK: {
		 
		 
		WARN_ON(!node->els_cmpl_cnt);
		node->els_cmpl_cnt--;
		break;
	}

	case EFC_EVT_PLOGI_RCVD: {
		 
		efc_node_save_sparms(node, cbdata->payload->dma.virt);
		efc_send_ls_acc_after_attach(node,
					     cbdata->header->dma.virt,
				EFC_NODE_SEND_LS_ACC_PLOGI);

		 
		efc_node_post_event(node, EFC_EVT_SHUTDOWN_IMPLICIT_LOGO,
				    NULL);
		break;
	}

	case EFC_EVT_LOGO_RCVD: {
		 
		struct fc_frame_header *hdr = cbdata->header->dma.virt;

		node_printf(node, "%s received attached=%d\n",
			    efc_sm_event_name(evt),
					node->attached);
		 
		efc_send_logo_acc(node, be16_to_cpu(hdr->fh_ox_id));
		efc_node_transition(node, __efc_d_wait_logo_acc_cmpl, NULL);
		break;
	}

	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_wait_logo_acc_cmpl(struct efc_sm_ctx *ctx,
			   enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		efc_node_hold_frames(node);
		break;

	case EFC_EVT_EXIT:
		efc_node_accept_frames(node);
		break;

	case EFC_EVT_SRRS_ELS_CMPL_OK:
	case EFC_EVT_SRRS_ELS_CMPL_FAIL:
		 
		WARN_ON(!node->els_cmpl_cnt);
		node->els_cmpl_cnt--;
		efc_node_post_event(node,
				    EFC_EVT_SHUTDOWN_EXPLICIT_LOGO, NULL);
		break;
	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_device_ready(struct efc_sm_ctx *ctx,
		     enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;
	struct efc *efc = node->efc;

	efc_node_evt_set(ctx, evt, __func__);

	if (evt != EFC_EVT_FCP_CMD_RCVD)
		node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		node->fcp_enabled = true;
		if (node->targ) {
			efc_log_info(efc,
				     "[%s] found (target) WWPN %s WWNN %s\n",
				node->display_name,
				node->wwpn, node->wwnn);
			if (node->nport->enable_ini)
				efc->tt.scsi_new_node(efc, node);
		}
		break;

	case EFC_EVT_EXIT:
		node->fcp_enabled = false;
		break;

	case EFC_EVT_PLOGI_RCVD: {
		 
		efc_node_save_sparms(node, cbdata->payload->dma.virt);
		efc_send_ls_acc_after_attach(node,
					     cbdata->header->dma.virt,
				EFC_NODE_SEND_LS_ACC_PLOGI);

		 
		efc_node_post_event(node,
				    EFC_EVT_SHUTDOWN_IMPLICIT_LOGO, NULL);
		break;
	}

	case EFC_EVT_PRLI_RCVD: {
		 
		struct fc_frame_header *hdr = cbdata->header->dma.virt;
		struct {
			struct fc_els_prli prli;
			struct fc_els_spp sp;
		} *pp;

		pp = cbdata->payload->dma.virt;
		if (pp->sp.spp_type != FC_TYPE_FCP) {
			 
			efc_send_ls_rjt(node, be16_to_cpu(hdr->fh_ox_id),
					ELS_RJT_UNAB, ELS_EXPL_UNSUPR, 0);
			break;
		}

		efc_process_prli_payload(node, cbdata->payload->dma.virt);
		efc_send_prli_acc(node, be16_to_cpu(hdr->fh_ox_id));
		break;
	}

	case EFC_EVT_PRLO_RCVD: {
		struct fc_frame_header *hdr = cbdata->header->dma.virt;
		 
		efc_send_prlo_acc(node, be16_to_cpu(hdr->fh_ox_id));
		 
		break;
	}

	case EFC_EVT_LOGO_RCVD: {
		struct fc_frame_header *hdr = cbdata->header->dma.virt;

		node_printf(node, "%s received attached=%d\n",
			    efc_sm_event_name(evt), node->attached);
		 
		efc_send_logo_acc(node, be16_to_cpu(hdr->fh_ox_id));
		efc_node_transition(node, __efc_d_wait_logo_acc_cmpl, NULL);
		break;
	}

	case EFC_EVT_ADISC_RCVD: {
		struct fc_frame_header *hdr = cbdata->header->dma.virt;
		 
		efc_send_adisc_acc(node, be16_to_cpu(hdr->fh_ox_id));
		break;
	}

	case EFC_EVT_ABTS_RCVD:
		 
		efc_log_err(efc, "Unexpected event:%s\n",
			    efc_sm_event_name(evt));
		break;

	case EFC_EVT_NODE_ACTIVE_IO_LIST_EMPTY:
		break;

	case EFC_EVT_NODE_REFOUND:
		break;

	case EFC_EVT_NODE_MISSING:
		if (node->nport->enable_rscn)
			efc_node_transition(node, __efc_d_device_gone, NULL);

		break;

	case EFC_EVT_SRRS_ELS_CMPL_OK:
		 
		WARN_ON(!node->els_cmpl_cnt);
		node->els_cmpl_cnt--;
		break;

	case EFC_EVT_SRRS_ELS_CMPL_FAIL:
		 
		WARN_ON(!node->els_cmpl_cnt);
		node->els_cmpl_cnt--;
		node_printf(node, "Failed to send PRLI LS_ACC\n");
		break;

	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_device_gone(struct efc_sm_ctx *ctx,
		    enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;
	struct efc *efc = node->efc;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER: {
		int rc = EFC_SCSI_CALL_COMPLETE;
		int rc_2 = EFC_SCSI_CALL_COMPLETE;
		static const char * const labels[] = {
			"none", "initiator", "target", "initiator+target"
		};

		efc_log_info(efc, "[%s] missing (%s)    WWPN %s WWNN %s\n",
			     node->display_name,
				labels[(node->targ << 1) | (node->init)],
						node->wwpn, node->wwnn);

		switch (efc_node_get_enable(node)) {
		case EFC_NODE_ENABLE_T_TO_T:
		case EFC_NODE_ENABLE_I_TO_T:
		case EFC_NODE_ENABLE_IT_TO_T:
			rc = efc->tt.scsi_del_node(efc, node,
				EFC_SCSI_TARGET_MISSING);
			break;

		case EFC_NODE_ENABLE_T_TO_I:
		case EFC_NODE_ENABLE_I_TO_I:
		case EFC_NODE_ENABLE_IT_TO_I:
			rc = efc->tt.scsi_del_node(efc, node,
				EFC_SCSI_INITIATOR_MISSING);
			break;

		case EFC_NODE_ENABLE_T_TO_IT:
			rc = efc->tt.scsi_del_node(efc, node,
				EFC_SCSI_INITIATOR_MISSING);
			break;

		case EFC_NODE_ENABLE_I_TO_IT:
			rc = efc->tt.scsi_del_node(efc, node,
						  EFC_SCSI_TARGET_MISSING);
			break;

		case EFC_NODE_ENABLE_IT_TO_IT:
			rc = efc->tt.scsi_del_node(efc, node,
				EFC_SCSI_INITIATOR_MISSING);
			rc_2 = efc->tt.scsi_del_node(efc, node,
				EFC_SCSI_TARGET_MISSING);
			break;

		default:
			rc = EFC_SCSI_CALL_COMPLETE;
			break;
		}

		if (rc == EFC_SCSI_CALL_COMPLETE &&
		    rc_2 == EFC_SCSI_CALL_COMPLETE)
			efc_node_post_event(node, EFC_EVT_SHUTDOWN, NULL);

		break;
	}
	case EFC_EVT_NODE_REFOUND:
		 

		 
		 

		 
		 
		efc_send_adisc(node);
		efc_node_transition(node, __efc_d_wait_adisc_rsp, NULL);
		break;

	case EFC_EVT_PLOGI_RCVD: {
		 
		efc_node_save_sparms(node, cbdata->payload->dma.virt);
		efc_send_ls_acc_after_attach(node,
					     cbdata->header->dma.virt,
				EFC_NODE_SEND_LS_ACC_PLOGI);

		 
		efc_node_post_event(node, EFC_EVT_SHUTDOWN_IMPLICIT_LOGO,
				    NULL);
		break;
	}

	case EFC_EVT_FCP_CMD_RCVD: {
		 
		node_printf(node, "FCP_CMND received, drop\n");
		break;
	}
	case EFC_EVT_LOGO_RCVD: {
		 
		struct fc_frame_header *hdr = cbdata->header->dma.virt;

		node_printf(node, "%s received attached=%d\n",
			    efc_sm_event_name(evt), node->attached);
		 
		efc_send_logo_acc(node, be16_to_cpu(hdr->fh_ox_id));
		efc_node_transition(node, __efc_d_wait_logo_acc_cmpl, NULL);
		break;
	}
	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}

void
__efc_d_wait_adisc_rsp(struct efc_sm_ctx *ctx,
		       enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_SRRS_ELS_REQ_OK:
		if (efc_node_check_els_req(ctx, evt, arg, ELS_ADISC,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		efc_node_transition(node, __efc_d_device_ready, NULL);
		break;

	case EFC_EVT_SRRS_ELS_REQ_RJT:
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_ADISC,
					   __efc_d_common, __func__))
			return;

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		 
		efc_node_post_event(node,
				    EFC_EVT_SHUTDOWN_EXPLICIT_LOGO,
				     NULL);
		break;

	case EFC_EVT_LOGO_RCVD: {
		 
		 
		struct fc_frame_header *hdr = cbdata->header->dma.virt;

		node_printf(node, "%s received attached=%d\n",
			    efc_sm_event_name(evt), node->attached);

		efc_send_logo_acc(node, be16_to_cpu(hdr->fh_ox_id));
		efc_node_transition(node, __efc_d_wait_logo_acc_cmpl, NULL);
		break;
	}
	default:
		__efc_d_common(__func__, ctx, evt, arg);
	}
}
