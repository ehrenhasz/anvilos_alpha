
 

#include "dlm_internal.h"
#include "member.h"
#include "lock.h"
#include "dir.h"
#include "config.h"
#include "requestqueue.h"
#include "util.h"

struct rq_entry {
	struct list_head list;
	uint32_t recover_seq;
	int nodeid;
	struct dlm_message request;
};

 

void dlm_add_requestqueue(struct dlm_ls *ls, int nodeid,
			  const struct dlm_message *ms)
{
	struct rq_entry *e;
	int length = le16_to_cpu(ms->m_header.h_length) -
		sizeof(struct dlm_message);

	e = kmalloc(sizeof(struct rq_entry) + length, GFP_NOFS);
	if (!e) {
		log_print("dlm_add_requestqueue: out of memory len %d", length);
		return;
	}

	e->recover_seq = ls->ls_recover_seq & 0xFFFFFFFF;
	e->nodeid = nodeid;
	memcpy(&e->request, ms, sizeof(*ms));
	memcpy(&e->request.m_extra, ms->m_extra, length);

	atomic_inc(&ls->ls_requestqueue_cnt);
	mutex_lock(&ls->ls_requestqueue_mutex);
	list_add_tail(&e->list, &ls->ls_requestqueue);
	mutex_unlock(&ls->ls_requestqueue_mutex);
}

 

int dlm_process_requestqueue(struct dlm_ls *ls)
{
	struct rq_entry *e;
	struct dlm_message *ms;
	int error = 0;

	mutex_lock(&ls->ls_requestqueue_mutex);

	for (;;) {
		if (list_empty(&ls->ls_requestqueue)) {
			mutex_unlock(&ls->ls_requestqueue_mutex);
			error = 0;
			break;
		}
		e = list_entry(ls->ls_requestqueue.next, struct rq_entry, list);
		mutex_unlock(&ls->ls_requestqueue_mutex);

		ms = &e->request;

		log_limit(ls, "dlm_process_requestqueue msg %d from %d "
			  "lkid %x remid %x result %d seq %u",
			  le32_to_cpu(ms->m_type),
			  le32_to_cpu(ms->m_header.h_nodeid),
			  le32_to_cpu(ms->m_lkid), le32_to_cpu(ms->m_remid),
			  from_dlm_errno(le32_to_cpu(ms->m_result)),
			  e->recover_seq);

		dlm_receive_message_saved(ls, &e->request, e->recover_seq);

		mutex_lock(&ls->ls_requestqueue_mutex);
		list_del(&e->list);
		if (atomic_dec_and_test(&ls->ls_requestqueue_cnt))
			wake_up(&ls->ls_requestqueue_wait);
		kfree(e);

		if (dlm_locking_stopped(ls)) {
			log_debug(ls, "process_requestqueue abort running");
			mutex_unlock(&ls->ls_requestqueue_mutex);
			error = -EINTR;
			break;
		}
		schedule();
	}

	return error;
}

 

void dlm_wait_requestqueue(struct dlm_ls *ls)
{
	wait_event(ls->ls_requestqueue_wait,
		   atomic_read(&ls->ls_requestqueue_cnt) == 0);
}

static int purge_request(struct dlm_ls *ls, struct dlm_message *ms, int nodeid)
{
	__le32 type = ms->m_type;

	 
	if (!atomic_read(&ls->ls_count))
		return 1;

	if (dlm_is_removed(ls, nodeid))
		return 1;

	 

	if (type == cpu_to_le32(DLM_MSG_REMOVE) ||
	    type == cpu_to_le32(DLM_MSG_LOOKUP) ||
	    type == cpu_to_le32(DLM_MSG_LOOKUP_REPLY))
		return 1;

	if (!dlm_no_directory(ls))
		return 0;

	return 1;
}

void dlm_purge_requestqueue(struct dlm_ls *ls)
{
	struct dlm_message *ms;
	struct rq_entry *e, *safe;

	mutex_lock(&ls->ls_requestqueue_mutex);
	list_for_each_entry_safe(e, safe, &ls->ls_requestqueue, list) {
		ms =  &e->request;

		if (purge_request(ls, ms, e->nodeid)) {
			list_del(&e->list);
			if (atomic_dec_and_test(&ls->ls_requestqueue_cnt))
				wake_up(&ls->ls_requestqueue_wait);
			kfree(e);
		}
	}
	mutex_unlock(&ls->ls_requestqueue_mutex);
}

