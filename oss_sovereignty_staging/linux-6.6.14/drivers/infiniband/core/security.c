 

#include <linux/security.h>
#include <linux/completion.h>
#include <linux/list.h>

#include <rdma/ib_verbs.h>
#include <rdma/ib_cache.h>
#include "core_priv.h"
#include "mad_priv.h"

static LIST_HEAD(mad_agent_list);
 
static DEFINE_SPINLOCK(mad_agent_list_lock);

static struct pkey_index_qp_list *get_pkey_idx_qp_list(struct ib_port_pkey *pp)
{
	struct pkey_index_qp_list *pkey = NULL;
	struct pkey_index_qp_list *tmp_pkey;
	struct ib_device *dev = pp->sec->dev;

	spin_lock(&dev->port_data[pp->port_num].pkey_list_lock);
	list_for_each_entry (tmp_pkey, &dev->port_data[pp->port_num].pkey_list,
			     pkey_index_list) {
		if (tmp_pkey->pkey_index == pp->pkey_index) {
			pkey = tmp_pkey;
			break;
		}
	}
	spin_unlock(&dev->port_data[pp->port_num].pkey_list_lock);
	return pkey;
}

static int get_pkey_and_subnet_prefix(struct ib_port_pkey *pp,
				      u16 *pkey,
				      u64 *subnet_prefix)
{
	struct ib_device *dev = pp->sec->dev;
	int ret;

	ret = ib_get_cached_pkey(dev, pp->port_num, pp->pkey_index, pkey);
	if (ret)
		return ret;

	ib_get_cached_subnet_prefix(dev, pp->port_num, subnet_prefix);

	return ret;
}

static int enforce_qp_pkey_security(u16 pkey,
				    u64 subnet_prefix,
				    struct ib_qp_security *qp_sec)
{
	struct ib_qp_security *shared_qp_sec;
	int ret;

	ret = security_ib_pkey_access(qp_sec->security, subnet_prefix, pkey);
	if (ret)
		return ret;

	list_for_each_entry(shared_qp_sec,
			    &qp_sec->shared_qp_list,
			    shared_qp_list) {
		ret = security_ib_pkey_access(shared_qp_sec->security,
					      subnet_prefix,
					      pkey);
		if (ret)
			return ret;
	}
	return 0;
}

 
static int check_qp_port_pkey_settings(struct ib_ports_pkeys *pps,
				       struct ib_qp_security *sec)
{
	u64 subnet_prefix;
	u16 pkey;
	int ret = 0;

	if (!pps)
		return 0;

	if (pps->main.state != IB_PORT_PKEY_NOT_VALID) {
		ret = get_pkey_and_subnet_prefix(&pps->main,
						 &pkey,
						 &subnet_prefix);
		if (ret)
			return ret;

		ret = enforce_qp_pkey_security(pkey,
					       subnet_prefix,
					       sec);
		if (ret)
			return ret;
	}

	if (pps->alt.state != IB_PORT_PKEY_NOT_VALID) {
		ret = get_pkey_and_subnet_prefix(&pps->alt,
						 &pkey,
						 &subnet_prefix);
		if (ret)
			return ret;

		ret = enforce_qp_pkey_security(pkey,
					       subnet_prefix,
					       sec);
	}

	return ret;
}

 
static void qp_to_error(struct ib_qp_security *sec)
{
	struct ib_qp_security *shared_qp_sec;
	struct ib_qp_attr attr = {
		.qp_state = IB_QPS_ERR
	};
	struct ib_event event = {
		.event = IB_EVENT_QP_FATAL
	};

	 
	if (sec->destroying)
		return;

	ib_modify_qp(sec->qp,
		     &attr,
		     IB_QP_STATE);

	if (sec->qp->event_handler && sec->qp->qp_context) {
		event.element.qp = sec->qp;
		sec->qp->event_handler(&event,
				       sec->qp->qp_context);
	}

	list_for_each_entry(shared_qp_sec,
			    &sec->shared_qp_list,
			    shared_qp_list) {
		struct ib_qp *qp = shared_qp_sec->qp;

		if (qp->event_handler && qp->qp_context) {
			event.element.qp = qp;
			event.device = qp->device;
			qp->event_handler(&event,
					  qp->qp_context);
		}
	}
}

static inline void check_pkey_qps(struct pkey_index_qp_list *pkey,
				  struct ib_device *device,
				  u32 port_num,
				  u64 subnet_prefix)
{
	struct ib_port_pkey *pp, *tmp_pp;
	bool comp;
	LIST_HEAD(to_error_list);
	u16 pkey_val;

	if (!ib_get_cached_pkey(device,
				port_num,
				pkey->pkey_index,
				&pkey_val)) {
		spin_lock(&pkey->qp_list_lock);
		list_for_each_entry(pp, &pkey->qp_list, qp_list) {
			if (atomic_read(&pp->sec->error_list_count))
				continue;

			if (enforce_qp_pkey_security(pkey_val,
						     subnet_prefix,
						     pp->sec)) {
				atomic_inc(&pp->sec->error_list_count);
				list_add(&pp->to_error_list,
					 &to_error_list);
			}
		}
		spin_unlock(&pkey->qp_list_lock);
	}

	list_for_each_entry_safe(pp,
				 tmp_pp,
				 &to_error_list,
				 to_error_list) {
		mutex_lock(&pp->sec->mutex);
		qp_to_error(pp->sec);
		list_del(&pp->to_error_list);
		atomic_dec(&pp->sec->error_list_count);
		comp = pp->sec->destroying;
		mutex_unlock(&pp->sec->mutex);

		if (comp)
			complete(&pp->sec->error_complete);
	}
}

 
static int port_pkey_list_insert(struct ib_port_pkey *pp)
{
	struct pkey_index_qp_list *tmp_pkey;
	struct pkey_index_qp_list *pkey;
	struct ib_device *dev;
	u32 port_num = pp->port_num;
	int ret = 0;

	if (pp->state != IB_PORT_PKEY_VALID)
		return 0;

	dev = pp->sec->dev;

	pkey = get_pkey_idx_qp_list(pp);

	if (!pkey) {
		bool found = false;

		pkey = kzalloc(sizeof(*pkey), GFP_KERNEL);
		if (!pkey)
			return -ENOMEM;

		spin_lock(&dev->port_data[port_num].pkey_list_lock);
		 
		list_for_each_entry(tmp_pkey,
				    &dev->port_data[port_num].pkey_list,
				    pkey_index_list) {
			if (tmp_pkey->pkey_index == pp->pkey_index) {
				kfree(pkey);
				pkey = tmp_pkey;
				found = true;
				break;
			}
		}

		if (!found) {
			pkey->pkey_index = pp->pkey_index;
			spin_lock_init(&pkey->qp_list_lock);
			INIT_LIST_HEAD(&pkey->qp_list);
			list_add(&pkey->pkey_index_list,
				 &dev->port_data[port_num].pkey_list);
		}
		spin_unlock(&dev->port_data[port_num].pkey_list_lock);
	}

	spin_lock(&pkey->qp_list_lock);
	list_add(&pp->qp_list, &pkey->qp_list);
	spin_unlock(&pkey->qp_list_lock);

	pp->state = IB_PORT_PKEY_LISTED;

	return ret;
}

 
static void port_pkey_list_remove(struct ib_port_pkey *pp)
{
	struct pkey_index_qp_list *pkey;

	if (pp->state != IB_PORT_PKEY_LISTED)
		return;

	pkey = get_pkey_idx_qp_list(pp);

	spin_lock(&pkey->qp_list_lock);
	list_del(&pp->qp_list);
	spin_unlock(&pkey->qp_list_lock);

	 
	pp->state = IB_PORT_PKEY_VALID;
}

static void destroy_qp_security(struct ib_qp_security *sec)
{
	security_ib_free_security(sec->security);
	kfree(sec->ports_pkeys);
	kfree(sec);
}

 
static struct ib_ports_pkeys *get_new_pps(const struct ib_qp *qp,
					  const struct ib_qp_attr *qp_attr,
					  int qp_attr_mask)
{
	struct ib_ports_pkeys *new_pps;
	struct ib_ports_pkeys *qp_pps = qp->qp_sec->ports_pkeys;

	new_pps = kzalloc(sizeof(*new_pps), GFP_KERNEL);
	if (!new_pps)
		return NULL;

	if (qp_attr_mask & IB_QP_PORT)
		new_pps->main.port_num = qp_attr->port_num;
	else if (qp_pps)
		new_pps->main.port_num = qp_pps->main.port_num;

	if (qp_attr_mask & IB_QP_PKEY_INDEX)
		new_pps->main.pkey_index = qp_attr->pkey_index;
	else if (qp_pps)
		new_pps->main.pkey_index = qp_pps->main.pkey_index;

	if (((qp_attr_mask & IB_QP_PKEY_INDEX) &&
	     (qp_attr_mask & IB_QP_PORT)) ||
	    (qp_pps && qp_pps->main.state != IB_PORT_PKEY_NOT_VALID))
		new_pps->main.state = IB_PORT_PKEY_VALID;

	if (qp_attr_mask & IB_QP_ALT_PATH) {
		new_pps->alt.port_num = qp_attr->alt_port_num;
		new_pps->alt.pkey_index = qp_attr->alt_pkey_index;
		new_pps->alt.state = IB_PORT_PKEY_VALID;
	} else if (qp_pps) {
		new_pps->alt.port_num = qp_pps->alt.port_num;
		new_pps->alt.pkey_index = qp_pps->alt.pkey_index;
		if (qp_pps->alt.state != IB_PORT_PKEY_NOT_VALID)
			new_pps->alt.state = IB_PORT_PKEY_VALID;
	}

	new_pps->main.sec = qp->qp_sec;
	new_pps->alt.sec = qp->qp_sec;
	return new_pps;
}

int ib_open_shared_qp_security(struct ib_qp *qp, struct ib_device *dev)
{
	struct ib_qp *real_qp = qp->real_qp;
	int ret;

	ret = ib_create_qp_security(qp, dev);

	if (ret)
		return ret;

	if (!qp->qp_sec)
		return 0;

	mutex_lock(&real_qp->qp_sec->mutex);
	ret = check_qp_port_pkey_settings(real_qp->qp_sec->ports_pkeys,
					  qp->qp_sec);

	if (ret)
		goto ret;

	if (qp != real_qp)
		list_add(&qp->qp_sec->shared_qp_list,
			 &real_qp->qp_sec->shared_qp_list);
ret:
	mutex_unlock(&real_qp->qp_sec->mutex);
	if (ret)
		destroy_qp_security(qp->qp_sec);

	return ret;
}

void ib_close_shared_qp_security(struct ib_qp_security *sec)
{
	struct ib_qp *real_qp = sec->qp->real_qp;

	mutex_lock(&real_qp->qp_sec->mutex);
	list_del(&sec->shared_qp_list);
	mutex_unlock(&real_qp->qp_sec->mutex);

	destroy_qp_security(sec);
}

int ib_create_qp_security(struct ib_qp *qp, struct ib_device *dev)
{
	unsigned int i;
	bool is_ib = false;
	int ret;

	rdma_for_each_port (dev, i) {
		is_ib = rdma_protocol_ib(dev, i);
		if (is_ib)
			break;
	}

	 
	if (!is_ib)
		return 0;

	qp->qp_sec = kzalloc(sizeof(*qp->qp_sec), GFP_KERNEL);
	if (!qp->qp_sec)
		return -ENOMEM;

	qp->qp_sec->qp = qp;
	qp->qp_sec->dev = dev;
	mutex_init(&qp->qp_sec->mutex);
	INIT_LIST_HEAD(&qp->qp_sec->shared_qp_list);
	atomic_set(&qp->qp_sec->error_list_count, 0);
	init_completion(&qp->qp_sec->error_complete);
	ret = security_ib_alloc_security(&qp->qp_sec->security);
	if (ret) {
		kfree(qp->qp_sec);
		qp->qp_sec = NULL;
	}

	return ret;
}
EXPORT_SYMBOL(ib_create_qp_security);

void ib_destroy_qp_security_begin(struct ib_qp_security *sec)
{
	 
	if (!sec)
		return;

	mutex_lock(&sec->mutex);

	 
	if (sec->ports_pkeys) {
		port_pkey_list_remove(&sec->ports_pkeys->main);
		port_pkey_list_remove(&sec->ports_pkeys->alt);
	}

	 
	sec->destroying = true;

	 
	sec->error_comps_pending = atomic_read(&sec->error_list_count);

	mutex_unlock(&sec->mutex);
}

void ib_destroy_qp_security_abort(struct ib_qp_security *sec)
{
	int ret;
	int i;

	 
	if (!sec)
		return;

	 
	for (i = 0; i < sec->error_comps_pending; i++)
		wait_for_completion(&sec->error_complete);

	mutex_lock(&sec->mutex);
	sec->destroying = false;

	 
	if (sec->ports_pkeys) {
		port_pkey_list_insert(&sec->ports_pkeys->main);
		port_pkey_list_insert(&sec->ports_pkeys->alt);
	}

	ret = check_qp_port_pkey_settings(sec->ports_pkeys, sec);
	if (ret)
		qp_to_error(sec);

	mutex_unlock(&sec->mutex);
}

void ib_destroy_qp_security_end(struct ib_qp_security *sec)
{
	int i;

	 
	if (!sec)
		return;

	 
	for (i = 0; i < sec->error_comps_pending; i++)
		wait_for_completion(&sec->error_complete);

	destroy_qp_security(sec);
}

void ib_security_cache_change(struct ib_device *device,
			      u32 port_num,
			      u64 subnet_prefix)
{
	struct pkey_index_qp_list *pkey;

	list_for_each_entry (pkey, &device->port_data[port_num].pkey_list,
			     pkey_index_list) {
		check_pkey_qps(pkey,
			       device,
			       port_num,
			       subnet_prefix);
	}
}

void ib_security_release_port_pkey_list(struct ib_device *device)
{
	struct pkey_index_qp_list *pkey, *tmp_pkey;
	unsigned int i;

	rdma_for_each_port (device, i) {
		list_for_each_entry_safe(pkey,
					 tmp_pkey,
					 &device->port_data[i].pkey_list,
					 pkey_index_list) {
			list_del(&pkey->pkey_index_list);
			kfree(pkey);
		}
	}
}

int ib_security_modify_qp(struct ib_qp *qp,
			  struct ib_qp_attr *qp_attr,
			  int qp_attr_mask,
			  struct ib_udata *udata)
{
	int ret = 0;
	struct ib_ports_pkeys *tmp_pps;
	struct ib_ports_pkeys *new_pps = NULL;
	struct ib_qp *real_qp = qp->real_qp;
	bool special_qp = (real_qp->qp_type == IB_QPT_SMI ||
			   real_qp->qp_type == IB_QPT_GSI ||
			   real_qp->qp_type >= IB_QPT_RESERVED1);
	bool pps_change = ((qp_attr_mask & (IB_QP_PKEY_INDEX | IB_QP_PORT)) ||
			   (qp_attr_mask & IB_QP_ALT_PATH));

	WARN_ONCE((qp_attr_mask & IB_QP_PORT &&
		   rdma_protocol_ib(real_qp->device, qp_attr->port_num) &&
		   !real_qp->qp_sec),
		   "%s: QP security is not initialized for IB QP: %u\n",
		   __func__, real_qp->qp_num);

	 

	if (pps_change && !special_qp && real_qp->qp_sec) {
		mutex_lock(&real_qp->qp_sec->mutex);
		new_pps = get_new_pps(real_qp,
				      qp_attr,
				      qp_attr_mask);
		if (!new_pps) {
			mutex_unlock(&real_qp->qp_sec->mutex);
			return -ENOMEM;
		}
		 
		ret = port_pkey_list_insert(&new_pps->main);

		if (!ret)
			ret = port_pkey_list_insert(&new_pps->alt);

		if (!ret)
			ret = check_qp_port_pkey_settings(new_pps,
							  real_qp->qp_sec);
	}

	if (!ret)
		ret = real_qp->device->ops.modify_qp(real_qp,
						     qp_attr,
						     qp_attr_mask,
						     udata);

	if (new_pps) {
		 
		if (ret) {
			tmp_pps = new_pps;
		} else {
			tmp_pps = real_qp->qp_sec->ports_pkeys;
			real_qp->qp_sec->ports_pkeys = new_pps;
		}

		if (tmp_pps) {
			port_pkey_list_remove(&tmp_pps->main);
			port_pkey_list_remove(&tmp_pps->alt);
		}
		kfree(tmp_pps);
		mutex_unlock(&real_qp->qp_sec->mutex);
	}
	return ret;
}

static int ib_security_pkey_access(struct ib_device *dev,
				   u32 port_num,
				   u16 pkey_index,
				   void *sec)
{
	u64 subnet_prefix;
	u16 pkey;
	int ret;

	if (!rdma_protocol_ib(dev, port_num))
		return 0;

	ret = ib_get_cached_pkey(dev, port_num, pkey_index, &pkey);
	if (ret)
		return ret;

	ib_get_cached_subnet_prefix(dev, port_num, &subnet_prefix);

	return security_ib_pkey_access(sec, subnet_prefix, pkey);
}

void ib_mad_agent_security_change(void)
{
	struct ib_mad_agent *ag;

	spin_lock(&mad_agent_list_lock);
	list_for_each_entry(ag,
			    &mad_agent_list,
			    mad_agent_sec_list)
		WRITE_ONCE(ag->smp_allowed,
			   !security_ib_endport_manage_subnet(ag->security,
				dev_name(&ag->device->dev), ag->port_num));
	spin_unlock(&mad_agent_list_lock);
}

int ib_mad_agent_security_setup(struct ib_mad_agent *agent,
				enum ib_qp_type qp_type)
{
	int ret;

	if (!rdma_protocol_ib(agent->device, agent->port_num))
		return 0;

	INIT_LIST_HEAD(&agent->mad_agent_sec_list);

	ret = security_ib_alloc_security(&agent->security);
	if (ret)
		return ret;

	if (qp_type != IB_QPT_SMI)
		return 0;

	spin_lock(&mad_agent_list_lock);
	ret = security_ib_endport_manage_subnet(agent->security,
						dev_name(&agent->device->dev),
						agent->port_num);
	if (ret)
		goto free_security;

	WRITE_ONCE(agent->smp_allowed, true);
	list_add(&agent->mad_agent_sec_list, &mad_agent_list);
	spin_unlock(&mad_agent_list_lock);
	return 0;

free_security:
	spin_unlock(&mad_agent_list_lock);
	security_ib_free_security(agent->security);
	return ret;
}

void ib_mad_agent_security_cleanup(struct ib_mad_agent *agent)
{
	if (!rdma_protocol_ib(agent->device, agent->port_num))
		return;

	if (agent->qp->qp_type == IB_QPT_SMI) {
		spin_lock(&mad_agent_list_lock);
		list_del(&agent->mad_agent_sec_list);
		spin_unlock(&mad_agent_list_lock);
	}

	security_ib_free_security(agent->security);
}

int ib_mad_enforce_security(struct ib_mad_agent_private *map, u16 pkey_index)
{
	if (!rdma_protocol_ib(map->agent.device, map->agent.port_num))
		return 0;

	if (map->agent.qp->qp_type == IB_QPT_SMI) {
		if (!READ_ONCE(map->agent.smp_allowed))
			return -EACCES;
		return 0;
	}

	return ib_security_pkey_access(map->agent.device,
				       map->agent.port_num,
				       pkey_index,
				       map->agent.security);
}
