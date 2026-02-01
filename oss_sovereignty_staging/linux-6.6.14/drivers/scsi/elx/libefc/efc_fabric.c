
 

 

 

#include "efc.h"

static void
efc_fabric_initiate_shutdown(struct efc_node *node)
{
	struct efc *efc = node->efc;

	node->els_io_enabled = false;

	if (node->attached) {
		int rc;

		 
		rc = efc_cmd_node_detach(efc, &node->rnode);
		if (rc < 0) {
			node_printf(node, "Failed freeing HW node, rc=%d\n",
				    rc);
		}
	}
	 
	efc_node_initiate_cleanup(node);
}

static void
__efc_fabric_common(const char *funcname, struct efc_sm_ctx *ctx,
		    enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = NULL;

	node = ctx->app;

	switch (evt) {
	case EFC_EVT_DOMAIN_ATTACH_OK:
		break;
	case EFC_EVT_SHUTDOWN:
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_fabric_initiate_shutdown(node);
		break;

	default:
		 
		__efc_node_common(funcname, ctx, evt, arg);
	}
}

void
__efc_fabric_init(struct efc_sm_ctx *ctx, enum efc_sm_event evt,
		  void *arg)
{
	struct efc_node *node = ctx->app;
	struct efc *efc = node->efc;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_REENTER:
		efc_log_debug(efc, ">>> reenter !!\n");
		fallthrough;

	case EFC_EVT_ENTER:
		 
		efc_send_flogi(node);
		efc_node_transition(node, __efc_fabric_flogi_wait_rsp, NULL);
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
efc_fabric_set_topology(struct efc_node *node,
			enum efc_nport_topology topology)
{
	node->nport->topology = topology;
}

void
efc_fabric_notify_topology(struct efc_node *node)
{
	struct efc_node *tmp_node;
	unsigned long index;

	 
	xa_for_each(&node->nport->lookup, index, tmp_node) {
		if (tmp_node != node) {
			efc_node_post_event(tmp_node,
					    EFC_EVT_NPORT_TOPOLOGY_NOTIFY,
					    &node->nport->topology);
		}
	}
}

static bool efc_rnode_is_nport(struct fc_els_flogi *rsp)
{
	return !(ntohs(rsp->fl_csp.sp_features) & FC_SP_FT_FPORT);
}

void
__efc_fabric_flogi_wait_rsp(struct efc_sm_ctx *ctx,
			    enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_SRRS_ELS_REQ_OK: {
		if (efc_node_check_els_req(ctx, evt, arg, ELS_FLOGI,
					   __efc_fabric_common, __func__)) {
			return;
		}
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;

		memcpy(node->nport->domain->flogi_service_params,
		       cbdata->els_rsp.virt,
		       sizeof(struct fc_els_flogi));

		 
		if (!efc_rnode_is_nport(cbdata->els_rsp.virt)) {
			 
			 
			efc_fabric_set_topology(node, EFC_NPORT_TOPO_FABRIC);
			efc_fabric_notify_topology(node);
			WARN_ON(node->nport->domain->attached);
			efc_domain_attach(node->nport->domain,
					  cbdata->ext_status);
			efc_node_transition(node,
					    __efc_fabric_wait_domain_attach,
					    NULL);
			break;
		}

		 
		efc_fabric_set_topology(node, EFC_NPORT_TOPO_P2P);
		if (efc_p2p_setup(node->nport)) {
			node_printf(node,
				    "p2p setup failed, shutting down node\n");
			node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
			efc_fabric_initiate_shutdown(node);
			break;
		}

		if (node->nport->p2p_winner) {
			efc_node_transition(node,
					    __efc_p2p_wait_domain_attach,
					     NULL);
			if (node->nport->domain->attached &&
			    !node->nport->domain->domain_notify_pend) {
				 
				node_printf(node,
					    "p2p winner, domain already attached\n");
				efc_node_post_event(node,
						    EFC_EVT_DOMAIN_ATTACH_OK,
						    NULL);
			}
		} else {
			 
			node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
			efc_fabric_initiate_shutdown(node);
		}

		break;
	}

	case EFC_EVT_ELS_REQ_ABORTED:
	case EFC_EVT_SRRS_ELS_REQ_RJT:
	case EFC_EVT_SRRS_ELS_REQ_FAIL: {
		struct efc_nport *nport = node->nport;
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_FLOGI,
					   __efc_fabric_common, __func__)) {
			return;
		}
		node_printf(node,
			    "FLOGI failed evt=%s, shutting down nport [%s]\n",
			    efc_sm_event_name(evt), nport->display_name);
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		efc_sm_post_event(&nport->sm, EFC_EVT_SHUTDOWN, NULL);
		break;
	}

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_vport_fabric_init(struct efc_sm_ctx *ctx,
			enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		 
		efc_send_fdisc(node);
		efc_node_transition(node, __efc_fabric_fdisc_wait_rsp, NULL);
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_fabric_fdisc_wait_rsp(struct efc_sm_ctx *ctx,
			    enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_SRRS_ELS_REQ_OK: {
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_FDISC,
					   __efc_fabric_common, __func__)) {
			return;
		}

		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		 
		efc_nport_attach(node->nport, cbdata->ext_status);
		efc_node_transition(node, __efc_fabric_wait_domain_attach,
				    NULL);
		break;
	}

	case EFC_EVT_SRRS_ELS_REQ_RJT:
	case EFC_EVT_SRRS_ELS_REQ_FAIL: {
		if (efc_node_check_els_req(ctx, evt, arg, ELS_FDISC,
					   __efc_fabric_common, __func__)) {
			return;
		}
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		efc_log_err(node->efc, "FDISC failed, shutting down nport\n");
		 
		efc_sm_post_event(&node->nport->sm, EFC_EVT_SHUTDOWN, NULL);
		break;
	}

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

static int
efc_start_ns_node(struct efc_nport *nport)
{
	struct efc_node *ns;

	 
	ns = efc_node_find(nport, FC_FID_DIR_SERV);
	if (!ns) {
		ns = efc_node_alloc(nport, FC_FID_DIR_SERV, false, false);
		if (!ns)
			return -EIO;
	}
	 
	if (ns->efc->nodedb_mask & EFC_NODEDB_PAUSE_NAMESERVER)
		efc_node_pause(ns, __efc_ns_init);
	else
		efc_node_transition(ns, __efc_ns_init, NULL);
	return 0;
}

static int
efc_start_fabctl_node(struct efc_nport *nport)
{
	struct efc_node *fabctl;

	fabctl = efc_node_find(nport, FC_FID_FCTRL);
	if (!fabctl) {
		fabctl = efc_node_alloc(nport, FC_FID_FCTRL,
					false, false);
		if (!fabctl)
			return -EIO;
	}
	 
	efc_node_transition(fabctl, __efc_fabctl_init, NULL);
	return 0;
}

void
__efc_fabric_wait_domain_attach(struct efc_sm_ctx *ctx,
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
	case EFC_EVT_DOMAIN_ATTACH_OK:
	case EFC_EVT_NPORT_ATTACH_OK: {
		int rc;

		rc = efc_start_ns_node(node->nport);
		if (rc)
			return;

		 
		 
		if (node->nport->enable_rscn) {
			rc = efc_start_fabctl_node(node->nport);
			if (rc)
				return;
		}
		efc_node_transition(node, __efc_fabric_idle, NULL);
		break;
	}
	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_fabric_idle(struct efc_sm_ctx *ctx, enum efc_sm_event evt,
		  void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_DOMAIN_ATTACH_OK:
		break;
	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_ns_init(struct efc_sm_ctx *ctx, enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		 
		efc_send_plogi(node);
		efc_node_transition(node, __efc_ns_plogi_wait_rsp, NULL);
		break;
	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_ns_plogi_wait_rsp(struct efc_sm_ctx *ctx,
			enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_SRRS_ELS_REQ_OK: {
		int rc;

		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PLOGI,
					   __efc_fabric_common, __func__)) {
			return;
		}
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		 
		efc_node_save_sparms(node, cbdata->els_rsp.virt);
		rc = efc_node_attach(node);
		efc_node_transition(node, __efc_ns_wait_node_attach, NULL);
		if (rc < 0)
			efc_node_post_event(node, EFC_EVT_NODE_ATTACH_FAIL,
					    NULL);
		break;
	}
	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_ns_wait_node_attach(struct efc_sm_ctx *ctx,
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
		 
		efc_ns_send_rftid(node);
		efc_node_transition(node, __efc_ns_rftid_wait_rsp, NULL);
		break;

	case EFC_EVT_NODE_ATTACH_FAIL:
		 
		node->attached = false;
		node_printf(node, "Node attach failed\n");
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_fabric_initiate_shutdown(node);
		break;

	case EFC_EVT_SHUTDOWN:
		node_printf(node, "Shutdown event received\n");
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_node_transition(node,
				    __efc_fabric_wait_attach_evt_shutdown,
				     NULL);
		break;

	 
	case EFC_EVT_RSCN_RCVD:
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_fabric_wait_attach_evt_shutdown(struct efc_sm_ctx *ctx,
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
		efc_fabric_initiate_shutdown(node);
		break;

	case EFC_EVT_NODE_ATTACH_FAIL:
		node->attached = false;
		node_printf(node, "Attach evt=%s, proceed to shutdown\n",
			    efc_sm_event_name(evt));
		efc_fabric_initiate_shutdown(node);
		break;

	 
	case EFC_EVT_SHUTDOWN:
		node_printf(node, "Shutdown event received\n");
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_ns_rftid_wait_rsp(struct efc_sm_ctx *ctx,
			enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_SRRS_ELS_REQ_OK:
		if (efc_node_check_ns_req(ctx, evt, arg, FC_NS_RFT_ID,
					  __efc_fabric_common, __func__)) {
			return;
		}
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		 
		efc_ns_send_rffid(node);
		efc_node_transition(node, __efc_ns_rffid_wait_rsp, NULL);
		break;

	 
	case EFC_EVT_RSCN_RCVD:
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_ns_rffid_wait_rsp(struct efc_sm_ctx *ctx,
			enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	 
	switch (evt) {
	case EFC_EVT_SRRS_ELS_REQ_OK:	{
		if (efc_node_check_ns_req(ctx, evt, arg, FC_NS_RFF_ID,
					  __efc_fabric_common, __func__)) {
			return;
		}
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		if (node->nport->enable_rscn) {
			 
			efc_ns_send_gidpt(node);

			efc_node_transition(node, __efc_ns_gidpt_wait_rsp,
					    NULL);
		} else {
			 
			efc_node_transition(node, __efc_ns_idle, NULL);
		}
		break;
	}
	 
	case EFC_EVT_RSCN_RCVD:
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

static int
efc_process_gidpt_payload(struct efc_node *node,
			  void *data, u32 gidpt_len)
{
	u32 i, j;
	struct efc_node *newnode;
	struct efc_nport *nport = node->nport;
	struct efc *efc = node->efc;
	u32 port_id = 0, port_count, plist_count;
	struct efc_node *n;
	struct efc_node **active_nodes;
	int residual;
	struct {
		struct fc_ct_hdr hdr;
		struct fc_gid_pn_resp pn_rsp;
	} *rsp;
	struct fc_gid_pn_resp *gidpt;
	unsigned long index;

	rsp = data;
	gidpt = &rsp->pn_rsp;
	residual = be16_to_cpu(rsp->hdr.ct_mr_size);

	if (residual != 0)
		efc_log_debug(node->efc, "residual is %u words\n", residual);

	if (be16_to_cpu(rsp->hdr.ct_cmd) == FC_FS_RJT) {
		node_printf(node,
			    "GIDPT request failed: rsn x%x rsn_expl x%x\n",
			    rsp->hdr.ct_reason, rsp->hdr.ct_explan);
		return -EIO;
	}

	plist_count = (gidpt_len - sizeof(struct fc_ct_hdr)) / sizeof(*gidpt);

	 
	port_count = 0;
	xa_for_each(&nport->lookup, index, n) {
		port_count++;
	}

	 
	active_nodes = kcalloc(port_count, sizeof(*active_nodes), GFP_ATOMIC);
	if (!active_nodes) {
		node_printf(node, "efc_malloc failed\n");
		return -EIO;
	}

	 
	i = 0;
	xa_for_each(&nport->lookup, index, n) {
		port_id = n->rnode.fc_id;
		switch (port_id) {
		case FC_FID_FLOGI:
		case FC_FID_FCTRL:
		case FC_FID_DIR_SERV:
			break;
		default:
			if (port_id != FC_FID_DOM_MGR)
				active_nodes[i++] = n;
			break;
		}
	}

	 
	for (i = 0; i < plist_count; i++) {
		hton24(gidpt[i].fp_fid, port_id);

		for (j = 0; j < port_count; j++) {
			if (active_nodes[j] &&
			    port_id == active_nodes[j]->rnode.fc_id) {
				active_nodes[j] = NULL;
			}
		}

		if (gidpt[i].fp_resvd & FC_NS_FID_LAST)
			break;
	}

	 
	for (i = 0; i < port_count; i++) {
		 
		if (!active_nodes[i])
			continue;

		if ((node->nport->enable_ini && active_nodes[i]->targ) ||
		    (node->nport->enable_tgt && enable_target_rscn(efc))) {
			efc_node_post_event(active_nodes[i],
					    EFC_EVT_NODE_MISSING, NULL);
		} else {
			node_printf(node,
				    "GID_PT: skipping non-tgt port_id x%06x\n",
				    active_nodes[i]->rnode.fc_id);
		}
	}
	kfree(active_nodes);

	for (i = 0; i < plist_count; i++) {
		hton24(gidpt[i].fp_fid, port_id);

		 
		if (port_id == node->rnode.nport->fc_id) {
			if (gidpt[i].fp_resvd & FC_NS_FID_LAST)
				break;
			continue;
		}

		newnode = efc_node_find(nport, port_id);
		if (!newnode) {
			if (!node->nport->enable_ini)
				continue;

			newnode = efc_node_alloc(nport, port_id, false, false);
			if (!newnode) {
				efc_log_err(efc, "efc_node_alloc() failed\n");
				return -EIO;
			}
			 
			efc_node_init_device(newnode, true);
		}

		if (node->nport->enable_ini && newnode->targ) {
			efc_node_post_event(newnode, EFC_EVT_NODE_REFOUND,
					    NULL);
		}

		if (gidpt[i].fp_resvd & FC_NS_FID_LAST)
			break;
	}
	return 0;
}

void
__efc_ns_gidpt_wait_rsp(struct efc_sm_ctx *ctx,
			enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();
	 

	switch (evt) {
	case EFC_EVT_SRRS_ELS_REQ_OK:	{
		if (efc_node_check_ns_req(ctx, evt, arg, FC_NS_GID_PT,
					  __efc_fabric_common, __func__)) {
			return;
		}
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		 
		efc_process_gidpt_payload(node, cbdata->els_rsp.virt,
					  cbdata->els_rsp.len);
		efc_node_transition(node, __efc_ns_idle, NULL);
		break;
	}

	case EFC_EVT_SRRS_ELS_REQ_FAIL:	{
		 
		node_printf(node, "GID_PT failed to complete\n");
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		efc_node_transition(node, __efc_ns_idle, NULL);
		break;
	}

	 
	case EFC_EVT_RSCN_RCVD: {
		node_printf(node, "RSCN received during GID_PT processing\n");
		node->rscn_pending = true;
		break;
	}

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_ns_idle(struct efc_sm_ctx *ctx, enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;
	struct efc *efc = node->efc;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	 

	switch (evt) {
	case EFC_EVT_ENTER:
		if (!node->rscn_pending)
			break;

		node_printf(node, "RSCN pending, restart discovery\n");
		node->rscn_pending = false;
		fallthrough;

	case EFC_EVT_RSCN_RCVD: {
		 
		 
		if (efc->tgt_rscn_delay_msec != 0 &&
		    !node->nport->enable_ini && node->nport->enable_tgt &&
		    enable_target_rscn(efc)) {
			efc_node_transition(node, __efc_ns_gidpt_delay, NULL);
		} else {
			efc_ns_send_gidpt(node);
			efc_node_transition(node, __efc_ns_gidpt_wait_rsp,
					    NULL);
		}
		break;
	}

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

static void
gidpt_delay_timer_cb(struct timer_list *t)
{
	struct efc_node *node = from_timer(node, t, gidpt_delay_timer);

	del_timer(&node->gidpt_delay_timer);

	efc_node_post_event(node, EFC_EVT_GIDPT_DELAY_EXPIRED, NULL);
}

void
__efc_ns_gidpt_delay(struct efc_sm_ctx *ctx,
		     enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;
	struct efc *efc = node->efc;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER: {
		u64 delay_msec, tmp;

		 
		delay_msec = efc->tgt_rscn_delay_msec;
		tmp = jiffies_to_msecs(jiffies) - node->time_last_gidpt_msec;
		if (tmp < efc->tgt_rscn_period_msec)
			delay_msec = efc->tgt_rscn_period_msec;

		timer_setup(&node->gidpt_delay_timer, &gidpt_delay_timer_cb,
			    0);
		mod_timer(&node->gidpt_delay_timer,
			  jiffies + msecs_to_jiffies(delay_msec));

		break;
	}

	case EFC_EVT_GIDPT_DELAY_EXPIRED:
		node->time_last_gidpt_msec = jiffies_to_msecs(jiffies);

		efc_ns_send_gidpt(node);
		efc_node_transition(node, __efc_ns_gidpt_wait_rsp, NULL);
		break;

	case EFC_EVT_RSCN_RCVD: {
		efc_log_debug(efc,
			      "RSCN received while in GIDPT delay - no action\n");
		break;
	}

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_fabctl_init(struct efc_sm_ctx *ctx,
		  enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		 
		efc_send_scr(node);
		efc_node_transition(node, __efc_fabctl_wait_scr_rsp, NULL);
		break;

	case EFC_EVT_NODE_ATTACH_OK:
		node->attached = true;
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_fabctl_wait_scr_rsp(struct efc_sm_ctx *ctx,
			  enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	 
	switch (evt) {
	case EFC_EVT_SRRS_ELS_REQ_OK:
		if (efc_node_check_els_req(ctx, evt, arg, ELS_SCR,
					   __efc_fabric_common, __func__)) {
			return;
		}
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		efc_node_transition(node, __efc_fabctl_ready, NULL);
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

static void
efc_process_rscn(struct efc_node *node, struct efc_node_cb *cbdata)
{
	struct efc *efc = node->efc;
	struct efc_nport *nport = node->nport;
	struct efc_node *ns;

	 
	ns = efc_node_find(nport, FC_FID_DIR_SERV);
	if (ns)
		efc_node_post_event(ns, EFC_EVT_RSCN_RCVD, cbdata);
	else
		efc_log_warn(efc, "can't find name server node\n");
}

void
__efc_fabctl_ready(struct efc_sm_ctx *ctx,
		   enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	 
	switch (evt) {
	case EFC_EVT_RSCN_RCVD: {
		struct fc_frame_header *hdr = cbdata->header->dma.virt;

		 
		efc_process_rscn(node, cbdata);
		efc_send_ls_acc(node, be16_to_cpu(hdr->fh_ox_id));
		efc_node_transition(node, __efc_fabctl_wait_ls_acc_cmpl,
				    NULL);
		break;
	}

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_fabctl_wait_ls_acc_cmpl(struct efc_sm_ctx *ctx,
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
		WARN_ON(!node->els_cmpl_cnt);
		node->els_cmpl_cnt--;
		efc_node_transition(node, __efc_fabctl_ready, NULL);
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

static uint64_t
efc_get_wwpn(struct fc_els_flogi *sp)
{
	return be64_to_cpu(sp->fl_wwnn);
}

static int
efc_rnode_is_winner(struct efc_nport *nport)
{
	struct fc_els_flogi *remote_sp;
	u64 remote_wwpn;
	u64 local_wwpn = nport->wwpn;
	u64 wwn_bump = 0;

	remote_sp = (struct fc_els_flogi *)nport->domain->flogi_service_params;
	remote_wwpn = efc_get_wwpn(remote_sp);

	local_wwpn ^= wwn_bump;

	efc_log_debug(nport->efc, "r: %llx\n",
		      be64_to_cpu(remote_sp->fl_wwpn));
	efc_log_debug(nport->efc, "l: %llx\n", local_wwpn);

	if (remote_wwpn == local_wwpn) {
		efc_log_warn(nport->efc,
			     "WWPN of remote node [%08x %08x] matches local WWPN\n",
			     (u32)(local_wwpn >> 32ll),
			     (u32)local_wwpn);
		return -1;
	}

	return (remote_wwpn > local_wwpn);
}

void
__efc_p2p_wait_domain_attach(struct efc_sm_ctx *ctx,
			     enum efc_sm_event evt, void *arg)
{
	struct efc_node *node = ctx->app;
	struct efc *efc = node->efc;

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
		struct efc_nport *nport = node->nport;
		struct efc_node *rnode;

		 
		WARN_ON(!node->nport->p2p_winner);

		rnode = efc_node_find(nport, node->nport->p2p_remote_port_id);
		if (rnode) {
			 
			node_printf(node,
				    "Node with fc_id x%x already exists\n",
				    rnode->rnode.fc_id);
		} else {
			 
			rnode = efc_node_alloc(nport,
					       nport->p2p_remote_port_id,
						false, false);
			if (!rnode) {
				efc_log_err(efc, "node alloc failed\n");
				return;
			}

			efc_fabric_notify_topology(node);
			 
			efc_node_transition(rnode, __efc_p2p_rnode_init,
					    NULL);
		}

		 
		if (node->rnode.fc_id == 0) {
			 
			 
			efc_node_init_device(node, false);
		} else {
			 
			node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
			efc_fabric_initiate_shutdown(node);
		}
		break;
	}

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_p2p_rnode_init(struct efc_sm_ctx *ctx,
		     enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_ENTER:
		 
		efc_send_plogi(node);
		efc_node_transition(node, __efc_p2p_wait_plogi_rsp, NULL);
		break;

	case EFC_EVT_ABTS_RCVD:
		 
		efc_send_bls_acc(node, cbdata->header->dma.virt);

		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_p2p_wait_flogi_acc_cmpl(struct efc_sm_ctx *ctx,
			      enum efc_sm_event evt, void *arg)
{
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

	case EFC_EVT_SRRS_ELS_CMPL_OK:
		WARN_ON(!node->els_cmpl_cnt);
		node->els_cmpl_cnt--;

		 
		if (node->nport->p2p_winner) {
			efc_node_transition(node,
					    __efc_p2p_wait_domain_attach,
					NULL);
			if (!node->nport->domain->attached) {
				node_printf(node, "Domain not attached\n");
				efc_domain_attach(node->nport->domain,
						  node->nport->p2p_port_id);
			} else {
				node_printf(node, "Domain already attached\n");
				efc_node_post_event(node,
						    EFC_EVT_DOMAIN_ATTACH_OK,
						    NULL);
			}
		} else {
			 
			 
			efc_node_init_device(node, false);
		}
		break;

	case EFC_EVT_SRRS_ELS_CMPL_FAIL:
		 
		node_printf(node, "FLOGI LS_ACC failed, shutting down\n");
		WARN_ON(!node->els_cmpl_cnt);
		node->els_cmpl_cnt--;
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_fabric_initiate_shutdown(node);
		break;

	case EFC_EVT_ABTS_RCVD: {
		 
		efc_send_bls_acc(node, cbdata->header->dma.virt);
		break;
	}

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_p2p_wait_plogi_rsp(struct efc_sm_ctx *ctx,
			 enum efc_sm_event evt, void *arg)
{
	struct efc_node_cb *cbdata = arg;
	struct efc_node *node = ctx->app;

	efc_node_evt_set(ctx, evt, __func__);

	node_sm_trace();

	switch (evt) {
	case EFC_EVT_SRRS_ELS_REQ_OK: {
		int rc;

		if (efc_node_check_els_req(ctx, evt, arg, ELS_PLOGI,
					   __efc_fabric_common, __func__)) {
			return;
		}
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		 
		efc_node_save_sparms(node, cbdata->els_rsp.virt);
		rc = efc_node_attach(node);
		efc_node_transition(node, __efc_p2p_wait_node_attach, NULL);
		if (rc < 0)
			efc_node_post_event(node, EFC_EVT_NODE_ATTACH_FAIL,
					    NULL);
		break;
	}
	case EFC_EVT_SRRS_ELS_REQ_FAIL: {
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PLOGI,
					   __efc_fabric_common, __func__)) {
			return;
		}
		node_printf(node, "PLOGI failed, shutting down\n");
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_fabric_initiate_shutdown(node);
		break;
	}

	case EFC_EVT_PLOGI_RCVD: {
		struct fc_frame_header *hdr = cbdata->header->dma.virt;
		 
		if (node->efc->external_loopback) {
			efc_send_plogi_acc(node, be16_to_cpu(hdr->fh_ox_id));
		} else {
			 
			__efc_fabric_common(__func__, ctx, evt, arg);
		}
		break;
	}
	case EFC_EVT_PRLI_RCVD:
		 
		 
		efc_process_prli_payload(node, cbdata->payload->dma.virt);
		efc_send_ls_acc_after_attach(node,
					     cbdata->header->dma.virt,
					     EFC_NODE_SEND_LS_ACC_PRLI);
		efc_node_transition(node, __efc_p2p_wait_plogi_rsp_recvd_prli,
				    NULL);
		break;
	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_p2p_wait_plogi_rsp_recvd_prli(struct efc_sm_ctx *ctx,
				    enum efc_sm_event evt, void *arg)
{
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

	case EFC_EVT_SRRS_ELS_REQ_OK: {	 
		int rc;

		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PLOGI,
					   __efc_fabric_common, __func__)) {
			return;
		}
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		 
		efc_node_save_sparms(node, cbdata->els_rsp.virt);
		rc = efc_node_attach(node);
		efc_node_transition(node, __efc_p2p_wait_node_attach, NULL);
		if (rc < 0)
			efc_node_post_event(node, EFC_EVT_NODE_ATTACH_FAIL,
					    NULL);
		break;
	}
	case EFC_EVT_SRRS_ELS_REQ_FAIL:	 
	case EFC_EVT_SRRS_ELS_REQ_RJT:
		 
		if (efc_node_check_els_req(ctx, evt, arg, ELS_PLOGI,
					   __efc_fabric_common, __func__)) {
			return;
		}
		WARN_ON(!node->els_req_cnt);
		node->els_req_cnt--;
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_fabric_initiate_shutdown(node);
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

void
__efc_p2p_wait_node_attach(struct efc_sm_ctx *ctx,
			   enum efc_sm_event evt, void *arg)
{
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

	case EFC_EVT_NODE_ATTACH_OK:
		node->attached = true;
		switch (node->send_ls_acc) {
		case EFC_NODE_SEND_LS_ACC_PRLI: {
			efc_d_send_prli_rsp(node->ls_acc_io,
					    node->ls_acc_oxid);
			node->send_ls_acc = EFC_NODE_SEND_LS_ACC_NONE;
			node->ls_acc_io = NULL;
			break;
		}
		case EFC_NODE_SEND_LS_ACC_PLOGI:  
		case EFC_NODE_SEND_LS_ACC_NONE:
		default:
			 
			 
			efc_node_transition(node, __efc_d_port_logged_in,
					    NULL);
			break;
		}
		break;

	case EFC_EVT_NODE_ATTACH_FAIL:
		 
		node->attached = false;
		node_printf(node, "Node attach failed\n");
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_fabric_initiate_shutdown(node);
		break;

	case EFC_EVT_SHUTDOWN:
		node_printf(node, "%s received\n", efc_sm_event_name(evt));
		node->shutdown_reason = EFC_NODE_SHUTDOWN_DEFAULT;
		efc_node_transition(node,
				    __efc_fabric_wait_attach_evt_shutdown,
				     NULL);
		break;
	case EFC_EVT_PRLI_RCVD:
		node_printf(node, "%s: PRLI received before node is attached\n",
			    efc_sm_event_name(evt));
		efc_process_prli_payload(node, cbdata->payload->dma.virt);
		efc_send_ls_acc_after_attach(node,
					     cbdata->header->dma.virt,
				EFC_NODE_SEND_LS_ACC_PRLI);
		break;

	default:
		__efc_fabric_common(__func__, ctx, evt, arg);
	}
}

int
efc_p2p_setup(struct efc_nport *nport)
{
	struct efc *efc = nport->efc;
	int rnode_winner;

	rnode_winner = efc_rnode_is_winner(nport);

	 
	if (rnode_winner == 1) {
		nport->p2p_remote_port_id = 0;
		nport->p2p_port_id = 0;
		nport->p2p_winner = false;
	} else if (rnode_winner == 0) {
		nport->p2p_remote_port_id = 2;
		nport->p2p_port_id = 1;
		nport->p2p_winner = true;
	} else {
		 
		if (nport->efc->external_loopback) {
			 
			efc_log_debug(efc,
				      "External loopback mode enabled\n");
			nport->p2p_remote_port_id = 1;
			nport->p2p_port_id = 1;
			nport->p2p_winner = true;
		} else {
			efc_log_warn(efc,
				     "failed to determine p2p winner\n");
			return rnode_winner;
		}
	}
	return 0;
}
