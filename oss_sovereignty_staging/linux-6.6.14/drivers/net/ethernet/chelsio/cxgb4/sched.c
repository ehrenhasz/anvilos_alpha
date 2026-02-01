 

#include <linux/module.h>
#include <linux/netdevice.h>

#include "cxgb4.h"
#include "sched.h"

static int t4_sched_class_fw_cmd(struct port_info *pi,
				 struct ch_sched_params *p,
				 enum sched_fw_ops op)
{
	struct adapter *adap = pi->adapter;
	struct sched_table *s = pi->sched_tbl;
	struct sched_class *e;
	int err = 0;

	e = &s->tab[p->u.params.class];
	switch (op) {
	case SCHED_FW_OP_ADD:
	case SCHED_FW_OP_DEL:
		err = t4_sched_params(adap, p->type,
				      p->u.params.level, p->u.params.mode,
				      p->u.params.rateunit,
				      p->u.params.ratemode,
				      p->u.params.channel, e->idx,
				      p->u.params.minrate, p->u.params.maxrate,
				      p->u.params.weight, p->u.params.pktsize,
				      p->u.params.burstsize);
		break;
	default:
		err = -ENOTSUPP;
		break;
	}

	return err;
}

static int t4_sched_bind_unbind_op(struct port_info *pi, void *arg,
				   enum sched_bind_type type, bool bind)
{
	struct adapter *adap = pi->adapter;
	u32 fw_mnem, fw_class, fw_param;
	unsigned int pf = adap->pf;
	unsigned int vf = 0;
	int err = 0;

	switch (type) {
	case SCHED_QUEUE: {
		struct sched_queue_entry *qe;

		qe = (struct sched_queue_entry *)arg;

		 
		fw_mnem = (FW_PARAMS_MNEM_V(FW_PARAMS_MNEM_DMAQ) |
			   FW_PARAMS_PARAM_X_V(
				   FW_PARAMS_PARAM_DMAQ_EQ_SCHEDCLASS_ETH));
		fw_class = bind ? qe->param.class : FW_SCHED_CLS_NONE;
		fw_param = (fw_mnem | FW_PARAMS_PARAM_YZ_V(qe->cntxt_id));

		pf = adap->pf;
		vf = 0;

		err = t4_set_params(adap, adap->mbox, pf, vf, 1,
				    &fw_param, &fw_class);
		break;
	}
	case SCHED_FLOWC: {
		struct sched_flowc_entry *fe;

		fe = (struct sched_flowc_entry *)arg;

		fw_class = bind ? fe->param.class : FW_SCHED_CLS_NONE;
		err = cxgb4_ethofld_send_flowc(adap->port[pi->port_id],
					       fe->param.tid, fw_class);
		break;
	}
	default:
		err = -ENOTSUPP;
		break;
	}

	return err;
}

static void *t4_sched_entry_lookup(struct port_info *pi,
				   enum sched_bind_type type,
				   const u32 val)
{
	struct sched_table *s = pi->sched_tbl;
	struct sched_class *e, *end;
	void *found = NULL;

	 
	end = &s->tab[s->sched_size];
	for (e = &s->tab[0]; e != end; ++e) {
		if (e->state == SCHED_STATE_UNUSED ||
		    e->bind_type != type)
			continue;

		switch (type) {
		case SCHED_QUEUE: {
			struct sched_queue_entry *qe;

			list_for_each_entry(qe, &e->entry_list, list) {
				if (qe->cntxt_id == val) {
					found = qe;
					break;
				}
			}
			break;
		}
		case SCHED_FLOWC: {
			struct sched_flowc_entry *fe;

			list_for_each_entry(fe, &e->entry_list, list) {
				if (fe->param.tid == val) {
					found = fe;
					break;
				}
			}
			break;
		}
		default:
			return NULL;
		}

		if (found)
			break;
	}

	return found;
}

struct sched_class *cxgb4_sched_queue_lookup(struct net_device *dev,
					     struct ch_sched_queue *p)
{
	struct port_info *pi = netdev2pinfo(dev);
	struct sched_queue_entry *qe = NULL;
	struct adapter *adap = pi->adapter;
	struct sge_eth_txq *txq;

	if (p->queue < 0 || p->queue >= pi->nqsets)
		return NULL;

	txq = &adap->sge.ethtxq[pi->first_qset + p->queue];
	qe = t4_sched_entry_lookup(pi, SCHED_QUEUE, txq->q.cntxt_id);
	return qe ? &pi->sched_tbl->tab[qe->param.class] : NULL;
}

static int t4_sched_queue_unbind(struct port_info *pi, struct ch_sched_queue *p)
{
	struct sched_queue_entry *qe = NULL;
	struct adapter *adap = pi->adapter;
	struct sge_eth_txq *txq;
	struct sched_class *e;
	int err = 0;

	if (p->queue < 0 || p->queue >= pi->nqsets)
		return -ERANGE;

	txq = &adap->sge.ethtxq[pi->first_qset + p->queue];

	 
	qe = t4_sched_entry_lookup(pi, SCHED_QUEUE, txq->q.cntxt_id);
	if (qe) {
		err = t4_sched_bind_unbind_op(pi, (void *)qe, SCHED_QUEUE,
					      false);
		if (err)
			return err;

		e = &pi->sched_tbl->tab[qe->param.class];
		list_del(&qe->list);
		kvfree(qe);
		if (atomic_dec_and_test(&e->refcnt))
			cxgb4_sched_class_free(adap->port[pi->port_id], e->idx);
	}
	return err;
}

static int t4_sched_queue_bind(struct port_info *pi, struct ch_sched_queue *p)
{
	struct sched_table *s = pi->sched_tbl;
	struct sched_queue_entry *qe = NULL;
	struct adapter *adap = pi->adapter;
	struct sge_eth_txq *txq;
	struct sched_class *e;
	unsigned int qid;
	int err = 0;

	if (p->queue < 0 || p->queue >= pi->nqsets)
		return -ERANGE;

	qe = kvzalloc(sizeof(struct sched_queue_entry), GFP_KERNEL);
	if (!qe)
		return -ENOMEM;

	txq = &adap->sge.ethtxq[pi->first_qset + p->queue];
	qid = txq->q.cntxt_id;

	 
	err = t4_sched_queue_unbind(pi, p);
	if (err)
		goto out_err;

	 
	qe->cntxt_id = qid;
	memcpy(&qe->param, p, sizeof(qe->param));

	e = &s->tab[qe->param.class];
	err = t4_sched_bind_unbind_op(pi, (void *)qe, SCHED_QUEUE, true);
	if (err)
		goto out_err;

	list_add_tail(&qe->list, &e->entry_list);
	e->bind_type = SCHED_QUEUE;
	atomic_inc(&e->refcnt);
	return err;

out_err:
	kvfree(qe);
	return err;
}

static int t4_sched_flowc_unbind(struct port_info *pi, struct ch_sched_flowc *p)
{
	struct sched_flowc_entry *fe = NULL;
	struct adapter *adap = pi->adapter;
	struct sched_class *e;
	int err = 0;

	if (p->tid < 0 || p->tid >= adap->tids.neotids)
		return -ERANGE;

	 
	fe = t4_sched_entry_lookup(pi, SCHED_FLOWC, p->tid);
	if (fe) {
		err = t4_sched_bind_unbind_op(pi, (void *)fe, SCHED_FLOWC,
					      false);
		if (err)
			return err;

		e = &pi->sched_tbl->tab[fe->param.class];
		list_del(&fe->list);
		kvfree(fe);
		if (atomic_dec_and_test(&e->refcnt))
			cxgb4_sched_class_free(adap->port[pi->port_id], e->idx);
	}
	return err;
}

static int t4_sched_flowc_bind(struct port_info *pi, struct ch_sched_flowc *p)
{
	struct sched_table *s = pi->sched_tbl;
	struct sched_flowc_entry *fe = NULL;
	struct adapter *adap = pi->adapter;
	struct sched_class *e;
	int err = 0;

	if (p->tid < 0 || p->tid >= adap->tids.neotids)
		return -ERANGE;

	fe = kvzalloc(sizeof(*fe), GFP_KERNEL);
	if (!fe)
		return -ENOMEM;

	 
	err = t4_sched_flowc_unbind(pi, p);
	if (err)
		goto out_err;

	 
	memcpy(&fe->param, p, sizeof(fe->param));

	e = &s->tab[fe->param.class];
	err = t4_sched_bind_unbind_op(pi, (void *)fe, SCHED_FLOWC, true);
	if (err)
		goto out_err;

	list_add_tail(&fe->list, &e->entry_list);
	e->bind_type = SCHED_FLOWC;
	atomic_inc(&e->refcnt);
	return err;

out_err:
	kvfree(fe);
	return err;
}

static void t4_sched_class_unbind_all(struct port_info *pi,
				      struct sched_class *e,
				      enum sched_bind_type type)
{
	if (!e)
		return;

	switch (type) {
	case SCHED_QUEUE: {
		struct sched_queue_entry *qe;

		list_for_each_entry(qe, &e->entry_list, list)
			t4_sched_queue_unbind(pi, &qe->param);
		break;
	}
	case SCHED_FLOWC: {
		struct sched_flowc_entry *fe;

		list_for_each_entry(fe, &e->entry_list, list)
			t4_sched_flowc_unbind(pi, &fe->param);
		break;
	}
	default:
		break;
	}
}

static int t4_sched_class_bind_unbind_op(struct port_info *pi, void *arg,
					 enum sched_bind_type type, bool bind)
{
	int err = 0;

	if (!arg)
		return -EINVAL;

	switch (type) {
	case SCHED_QUEUE: {
		struct ch_sched_queue *qe = (struct ch_sched_queue *)arg;

		if (bind)
			err = t4_sched_queue_bind(pi, qe);
		else
			err = t4_sched_queue_unbind(pi, qe);
		break;
	}
	case SCHED_FLOWC: {
		struct ch_sched_flowc *fe = (struct ch_sched_flowc *)arg;

		if (bind)
			err = t4_sched_flowc_bind(pi, fe);
		else
			err = t4_sched_flowc_unbind(pi, fe);
		break;
	}
	default:
		err = -ENOTSUPP;
		break;
	}

	return err;
}

 
int cxgb4_sched_class_bind(struct net_device *dev, void *arg,
			   enum sched_bind_type type)
{
	struct port_info *pi = netdev2pinfo(dev);
	u8 class_id;

	if (!can_sched(dev))
		return -ENOTSUPP;

	if (!arg)
		return -EINVAL;

	switch (type) {
	case SCHED_QUEUE: {
		struct ch_sched_queue *qe = (struct ch_sched_queue *)arg;

		class_id = qe->class;
		break;
	}
	case SCHED_FLOWC: {
		struct ch_sched_flowc *fe = (struct ch_sched_flowc *)arg;

		class_id = fe->class;
		break;
	}
	default:
		return -ENOTSUPP;
	}

	if (!valid_class_id(dev, class_id))
		return -EINVAL;

	if (class_id == SCHED_CLS_NONE)
		return -ENOTSUPP;

	return t4_sched_class_bind_unbind_op(pi, arg, type, true);

}

 
int cxgb4_sched_class_unbind(struct net_device *dev, void *arg,
			     enum sched_bind_type type)
{
	struct port_info *pi = netdev2pinfo(dev);
	u8 class_id;

	if (!can_sched(dev))
		return -ENOTSUPP;

	if (!arg)
		return -EINVAL;

	switch (type) {
	case SCHED_QUEUE: {
		struct ch_sched_queue *qe = (struct ch_sched_queue *)arg;

		class_id = qe->class;
		break;
	}
	case SCHED_FLOWC: {
		struct ch_sched_flowc *fe = (struct ch_sched_flowc *)arg;

		class_id = fe->class;
		break;
	}
	default:
		return -ENOTSUPP;
	}

	if (!valid_class_id(dev, class_id))
		return -EINVAL;

	return t4_sched_class_bind_unbind_op(pi, arg, type, false);
}

 
static struct sched_class *t4_sched_class_lookup(struct port_info *pi,
						const struct ch_sched_params *p)
{
	struct sched_table *s = pi->sched_tbl;
	struct sched_class *found = NULL;
	struct sched_class *e, *end;

	if (!p) {
		 
		end = &s->tab[s->sched_size];
		for (e = &s->tab[0]; e != end; ++e) {
			if (e->state == SCHED_STATE_UNUSED) {
				found = e;
				break;
			}
		}
	} else {
		 
		struct ch_sched_params info;
		struct ch_sched_params tp;

		memcpy(&tp, p, sizeof(tp));
		 
		tp.u.params.class = SCHED_CLS_NONE;

		end = &s->tab[s->sched_size];
		for (e = &s->tab[0]; e != end; ++e) {
			if (e->state == SCHED_STATE_UNUSED)
				continue;

			memcpy(&info, &e->info, sizeof(info));
			 
			info.u.params.class = SCHED_CLS_NONE;

			if ((info.type == tp.type) &&
			    (!memcmp(&info.u.params, &tp.u.params,
				     sizeof(info.u.params)))) {
				found = e;
				break;
			}
		}
	}

	return found;
}

static struct sched_class *t4_sched_class_alloc(struct port_info *pi,
						struct ch_sched_params *p)
{
	struct sched_class *e = NULL;
	u8 class_id;
	int err;

	if (!p)
		return NULL;

	class_id = p->u.params.class;

	 
	if (class_id != SCHED_CLS_NONE)
		return NULL;

	 
	if (p->u.params.mode == SCHED_CLASS_MODE_FLOW)
		e = t4_sched_class_lookup(pi, p);

	if (!e) {
		struct ch_sched_params np;

		 
		e = t4_sched_class_lookup(pi, NULL);
		if (!e)
			return NULL;

		memcpy(&np, p, sizeof(np));
		np.u.params.class = e->idx;
		 
		err = t4_sched_class_fw_cmd(pi, &np, SCHED_FW_OP_ADD);
		if (err)
			return NULL;
		memcpy(&e->info, &np, sizeof(e->info));
		atomic_set(&e->refcnt, 0);
		e->state = SCHED_STATE_ACTIVE;
	}

	return e;
}

 
struct sched_class *cxgb4_sched_class_alloc(struct net_device *dev,
					    struct ch_sched_params *p)
{
	struct port_info *pi = netdev2pinfo(dev);
	u8 class_id;

	if (!can_sched(dev))
		return NULL;

	class_id = p->u.params.class;
	if (!valid_class_id(dev, class_id))
		return NULL;

	return t4_sched_class_alloc(pi, p);
}

 
void cxgb4_sched_class_free(struct net_device *dev, u8 classid)
{
	struct port_info *pi = netdev2pinfo(dev);
	struct sched_table *s = pi->sched_tbl;
	struct ch_sched_params p;
	struct sched_class *e;
	u32 speed;
	int ret;

	e = &s->tab[classid];
	if (!atomic_read(&e->refcnt) && e->state != SCHED_STATE_UNUSED) {
		 
		memcpy(&p, &e->info, sizeof(p));
		 
		p.u.params.mode = 0;
		p.u.params.minrate = 0;
		p.u.params.pktsize = 0;

		ret = t4_get_link_params(pi, NULL, &speed, NULL);
		if (!ret)
			p.u.params.maxrate = speed * 1000;  
		else
			p.u.params.maxrate = SCHED_MAX_RATE_KBPS;

		t4_sched_class_fw_cmd(pi, &p, SCHED_FW_OP_DEL);

		e->state = SCHED_STATE_UNUSED;
		memset(&e->info, 0, sizeof(e->info));
	}
}

static void t4_sched_class_free(struct net_device *dev, struct sched_class *e)
{
	struct port_info *pi = netdev2pinfo(dev);

	t4_sched_class_unbind_all(pi, e, e->bind_type);
	cxgb4_sched_class_free(dev, e->idx);
}

struct sched_table *t4_init_sched(unsigned int sched_size)
{
	struct sched_table *s;
	unsigned int i;

	s = kvzalloc(struct_size(s, tab, sched_size), GFP_KERNEL);
	if (!s)
		return NULL;

	s->sched_size = sched_size;

	for (i = 0; i < s->sched_size; i++) {
		memset(&s->tab[i], 0, sizeof(struct sched_class));
		s->tab[i].idx = i;
		s->tab[i].state = SCHED_STATE_UNUSED;
		INIT_LIST_HEAD(&s->tab[i].entry_list);
		atomic_set(&s->tab[i].refcnt, 0);
	}
	return s;
}

void t4_cleanup_sched(struct adapter *adap)
{
	struct sched_table *s;
	unsigned int j, i;

	for_each_port(adap, j) {
		struct port_info *pi = netdev2pinfo(adap->port[j]);

		s = pi->sched_tbl;
		if (!s)
			continue;

		for (i = 0; i < s->sched_size; i++) {
			struct sched_class *e;

			e = &s->tab[i];
			if (e->state == SCHED_STATE_ACTIVE)
				t4_sched_class_free(adap->port[j], e);
		}
		kvfree(s);
	}
}
