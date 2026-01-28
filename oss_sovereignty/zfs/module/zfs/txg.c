#include <sys/zfs_context.h>
#include <sys/txg_impl.h>
#include <sys/dmu_impl.h>
#include <sys/spa_impl.h>
#include <sys/dmu_tx.h>
#include <sys/dsl_pool.h>
#include <sys/dsl_scan.h>
#include <sys/zil.h>
#include <sys/callb.h>
#include <sys/trace_zfs.h>
static __attribute__((noreturn)) void txg_sync_thread(void *arg);
static __attribute__((noreturn)) void txg_quiesce_thread(void *arg);
uint_t zfs_txg_timeout = 5;	 
void
txg_init(dsl_pool_t *dp, uint64_t txg)
{
	tx_state_t *tx = &dp->dp_tx;
	int c;
	memset(tx, 0, sizeof (tx_state_t));
	tx->tx_cpu = vmem_zalloc(max_ncpus * sizeof (tx_cpu_t), KM_SLEEP);
	for (c = 0; c < max_ncpus; c++) {
		int i;
		mutex_init(&tx->tx_cpu[c].tc_lock, NULL, MUTEX_DEFAULT, NULL);
		mutex_init(&tx->tx_cpu[c].tc_open_lock, NULL, MUTEX_NOLOCKDEP,
		    NULL);
		for (i = 0; i < TXG_SIZE; i++) {
			cv_init(&tx->tx_cpu[c].tc_cv[i], NULL, CV_DEFAULT,
			    NULL);
			list_create(&tx->tx_cpu[c].tc_callbacks[i],
			    sizeof (dmu_tx_callback_t),
			    offsetof(dmu_tx_callback_t, dcb_node));
		}
	}
	mutex_init(&tx->tx_sync_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&tx->tx_sync_more_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&tx->tx_sync_done_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&tx->tx_quiesce_more_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&tx->tx_quiesce_done_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&tx->tx_exit_cv, NULL, CV_DEFAULT, NULL);
	tx->tx_open_txg = txg;
}
void
txg_fini(dsl_pool_t *dp)
{
	tx_state_t *tx = &dp->dp_tx;
	int c;
	ASSERT0(tx->tx_threads);
	mutex_destroy(&tx->tx_sync_lock);
	cv_destroy(&tx->tx_sync_more_cv);
	cv_destroy(&tx->tx_sync_done_cv);
	cv_destroy(&tx->tx_quiesce_more_cv);
	cv_destroy(&tx->tx_quiesce_done_cv);
	cv_destroy(&tx->tx_exit_cv);
	for (c = 0; c < max_ncpus; c++) {
		int i;
		mutex_destroy(&tx->tx_cpu[c].tc_open_lock);
		mutex_destroy(&tx->tx_cpu[c].tc_lock);
		for (i = 0; i < TXG_SIZE; i++) {
			cv_destroy(&tx->tx_cpu[c].tc_cv[i]);
			list_destroy(&tx->tx_cpu[c].tc_callbacks[i]);
		}
	}
	if (tx->tx_commit_cb_taskq != NULL)
		taskq_destroy(tx->tx_commit_cb_taskq);
	vmem_free(tx->tx_cpu, max_ncpus * sizeof (tx_cpu_t));
	memset(tx, 0, sizeof (tx_state_t));
}
void
txg_sync_start(dsl_pool_t *dp)
{
	tx_state_t *tx = &dp->dp_tx;
	mutex_enter(&tx->tx_sync_lock);
	dprintf("pool %p\n", dp);
	ASSERT0(tx->tx_threads);
	tx->tx_threads = 2;
	tx->tx_quiesce_thread = thread_create(NULL, 0, txg_quiesce_thread,
	    dp, 0, &p0, TS_RUN, defclsyspri);
	tx->tx_sync_thread = thread_create(NULL, 0, txg_sync_thread,
	    dp, 0, &p0, TS_RUN, defclsyspri);
	mutex_exit(&tx->tx_sync_lock);
}
static void
txg_thread_enter(tx_state_t *tx, callb_cpr_t *cpr)
{
	CALLB_CPR_INIT(cpr, &tx->tx_sync_lock, callb_generic_cpr, FTAG);
	mutex_enter(&tx->tx_sync_lock);
}
static void
txg_thread_exit(tx_state_t *tx, callb_cpr_t *cpr, kthread_t **tpp)
{
	ASSERT(*tpp != NULL);
	*tpp = NULL;
	tx->tx_threads--;
	cv_broadcast(&tx->tx_exit_cv);
	CALLB_CPR_EXIT(cpr);		 
	thread_exit();
}
static void
txg_thread_wait(tx_state_t *tx, callb_cpr_t *cpr, kcondvar_t *cv, clock_t time)
{
	CALLB_CPR_SAFE_BEGIN(cpr);
	if (time) {
		(void) cv_timedwait_idle(cv, &tx->tx_sync_lock,
		    ddi_get_lbolt() + time);
	} else {
		cv_wait_idle(cv, &tx->tx_sync_lock);
	}
	CALLB_CPR_SAFE_END(cpr, &tx->tx_sync_lock);
}
void
txg_sync_stop(dsl_pool_t *dp)
{
	tx_state_t *tx = &dp->dp_tx;
	dprintf("pool %p\n", dp);
	ASSERT3U(tx->tx_threads, ==, 2);
	txg_wait_synced(dp, tx->tx_open_txg + TXG_DEFER_SIZE);
	mutex_enter(&tx->tx_sync_lock);
	ASSERT3U(tx->tx_threads, ==, 2);
	tx->tx_exiting = 1;
	cv_broadcast(&tx->tx_quiesce_more_cv);
	cv_broadcast(&tx->tx_quiesce_done_cv);
	cv_broadcast(&tx->tx_sync_more_cv);
	while (tx->tx_threads != 0)
		cv_wait(&tx->tx_exit_cv, &tx->tx_sync_lock);
	tx->tx_exiting = 0;
	mutex_exit(&tx->tx_sync_lock);
}
uint64_t
txg_hold_open(dsl_pool_t *dp, txg_handle_t *th)
{
	tx_state_t *tx = &dp->dp_tx;
	tx_cpu_t *tc;
	uint64_t txg;
	tc = &tx->tx_cpu[CPU_SEQID_UNSTABLE];
	mutex_enter(&tc->tc_open_lock);
	txg = tx->tx_open_txg;
	mutex_enter(&tc->tc_lock);
	tc->tc_count[txg & TXG_MASK]++;
	mutex_exit(&tc->tc_lock);
	th->th_cpu = tc;
	th->th_txg = txg;
	return (txg);
}
void
txg_rele_to_quiesce(txg_handle_t *th)
{
	tx_cpu_t *tc = th->th_cpu;
	ASSERT(!MUTEX_HELD(&tc->tc_lock));
	mutex_exit(&tc->tc_open_lock);
}
void
txg_register_callbacks(txg_handle_t *th, list_t *tx_callbacks)
{
	tx_cpu_t *tc = th->th_cpu;
	int g = th->th_txg & TXG_MASK;
	mutex_enter(&tc->tc_lock);
	list_move_tail(&tc->tc_callbacks[g], tx_callbacks);
	mutex_exit(&tc->tc_lock);
}
void
txg_rele_to_sync(txg_handle_t *th)
{
	tx_cpu_t *tc = th->th_cpu;
	int g = th->th_txg & TXG_MASK;
	mutex_enter(&tc->tc_lock);
	ASSERT(tc->tc_count[g] != 0);
	if (--tc->tc_count[g] == 0)
		cv_broadcast(&tc->tc_cv[g]);
	mutex_exit(&tc->tc_lock);
	th->th_cpu = NULL;	 
}
static void
txg_quiesce(dsl_pool_t *dp, uint64_t txg)
{
	tx_state_t *tx = &dp->dp_tx;
	uint64_t tx_open_time;
	int g = txg & TXG_MASK;
	int c;
	for (c = 0; c < max_ncpus; c++)
		mutex_enter(&tx->tx_cpu[c].tc_open_lock);
	ASSERT(txg == tx->tx_open_txg);
	tx->tx_open_txg++;
	tx->tx_open_time = tx_open_time = gethrtime();
	DTRACE_PROBE2(txg__quiescing, dsl_pool_t *, dp, uint64_t, txg);
	DTRACE_PROBE2(txg__opened, dsl_pool_t *, dp, uint64_t, tx->tx_open_txg);
	for (c = 0; c < max_ncpus; c++)
		mutex_exit(&tx->tx_cpu[c].tc_open_lock);
	spa_txg_history_set(dp->dp_spa, txg, TXG_STATE_OPEN, tx_open_time);
	spa_txg_history_add(dp->dp_spa, txg + 1, tx_open_time);
	for (c = 0; c < max_ncpus; c++) {
		tx_cpu_t *tc = &tx->tx_cpu[c];
		mutex_enter(&tc->tc_lock);
		while (tc->tc_count[g] != 0)
			cv_wait(&tc->tc_cv[g], &tc->tc_lock);
		mutex_exit(&tc->tc_lock);
	}
	spa_txg_history_set(dp->dp_spa, txg, TXG_STATE_QUIESCED, gethrtime());
}
static void
txg_do_callbacks(void *cb_list)
{
	dmu_tx_do_callbacks(cb_list, 0);
	list_destroy(cb_list);
	kmem_free(cb_list, sizeof (list_t));
}
static void
txg_dispatch_callbacks(dsl_pool_t *dp, uint64_t txg)
{
	int c;
	tx_state_t *tx = &dp->dp_tx;
	list_t *cb_list;
	for (c = 0; c < max_ncpus; c++) {
		tx_cpu_t *tc = &tx->tx_cpu[c];
		int g = txg & TXG_MASK;
		if (list_is_empty(&tc->tc_callbacks[g]))
			continue;
		if (tx->tx_commit_cb_taskq == NULL) {
			tx->tx_commit_cb_taskq = taskq_create("tx_commit_cb",
			    100, defclsyspri, boot_ncpus, boot_ncpus * 2,
			    TASKQ_PREPOPULATE | TASKQ_DYNAMIC |
			    TASKQ_THREADS_CPU_PCT);
		}
		cb_list = kmem_alloc(sizeof (list_t), KM_SLEEP);
		list_create(cb_list, sizeof (dmu_tx_callback_t),
		    offsetof(dmu_tx_callback_t, dcb_node));
		list_move_tail(cb_list, &tc->tc_callbacks[g]);
		(void) taskq_dispatch(tx->tx_commit_cb_taskq,
		    txg_do_callbacks, cb_list, TQ_SLEEP);
	}
}
void
txg_wait_callbacks(dsl_pool_t *dp)
{
	tx_state_t *tx = &dp->dp_tx;
	if (tx->tx_commit_cb_taskq != NULL)
		taskq_wait_outstanding(tx->tx_commit_cb_taskq, 0);
}
static boolean_t
txg_is_quiescing(dsl_pool_t *dp)
{
	tx_state_t *tx = &dp->dp_tx;
	ASSERT(MUTEX_HELD(&tx->tx_sync_lock));
	return (tx->tx_quiescing_txg != 0);
}
static boolean_t
txg_has_quiesced_to_sync(dsl_pool_t *dp)
{
	tx_state_t *tx = &dp->dp_tx;
	ASSERT(MUTEX_HELD(&tx->tx_sync_lock));
	return (tx->tx_quiesced_txg != 0);
}
static __attribute__((noreturn)) void
txg_sync_thread(void *arg)
{
	dsl_pool_t *dp = arg;
	spa_t *spa = dp->dp_spa;
	tx_state_t *tx = &dp->dp_tx;
	callb_cpr_t cpr;
	clock_t start, delta;
	(void) spl_fstrans_mark();
	txg_thread_enter(tx, &cpr);
	start = delta = 0;
	for (;;) {
		clock_t timeout = zfs_txg_timeout * hz;
		clock_t timer;
		uint64_t txg;
		timer = (delta >= timeout ? 0 : timeout - delta);
		while (!dsl_scan_active(dp->dp_scan) &&
		    !tx->tx_exiting && timer > 0 &&
		    tx->tx_synced_txg >= tx->tx_sync_txg_waiting &&
		    !txg_has_quiesced_to_sync(dp)) {
			dprintf("waiting; tx_synced=%llu waiting=%llu dp=%p\n",
			    (u_longlong_t)tx->tx_synced_txg,
			    (u_longlong_t)tx->tx_sync_txg_waiting, dp);
			txg_thread_wait(tx, &cpr, &tx->tx_sync_more_cv, timer);
			delta = ddi_get_lbolt() - start;
			timer = (delta > timeout ? 0 : timeout - delta);
		}
		while (!tx->tx_exiting && !txg_has_quiesced_to_sync(dp)) {
			if (txg_is_quiescing(dp)) {
				txg_thread_wait(tx, &cpr,
				    &tx->tx_quiesce_done_cv, 0);
				continue;
			}
			if (tx->tx_quiesce_txg_waiting < tx->tx_open_txg+1)
				tx->tx_quiesce_txg_waiting = tx->tx_open_txg+1;
			cv_broadcast(&tx->tx_quiesce_more_cv);
			txg_thread_wait(tx, &cpr, &tx->tx_quiesce_done_cv, 0);
		}
		if (tx->tx_exiting)
			txg_thread_exit(tx, &cpr, &tx->tx_sync_thread);
		ASSERT(tx->tx_quiesced_txg != 0);
		txg = tx->tx_quiesced_txg;
		tx->tx_quiesced_txg = 0;
		tx->tx_syncing_txg = txg;
		DTRACE_PROBE2(txg__syncing, dsl_pool_t *, dp, uint64_t, txg);
		cv_broadcast(&tx->tx_quiesce_more_cv);
		dprintf("txg=%llu quiesce_txg=%llu sync_txg=%llu\n",
		    (u_longlong_t)txg, (u_longlong_t)tx->tx_quiesce_txg_waiting,
		    (u_longlong_t)tx->tx_sync_txg_waiting);
		mutex_exit(&tx->tx_sync_lock);
		txg_stat_t *ts = spa_txg_history_init_io(spa, txg, dp);
		start = ddi_get_lbolt();
		spa_sync(spa, txg);
		delta = ddi_get_lbolt() - start;
		spa_txg_history_fini_io(spa, ts);
		mutex_enter(&tx->tx_sync_lock);
		tx->tx_synced_txg = txg;
		tx->tx_syncing_txg = 0;
		DTRACE_PROBE2(txg__synced, dsl_pool_t *, dp, uint64_t, txg);
		cv_broadcast(&tx->tx_sync_done_cv);
		txg_dispatch_callbacks(dp, txg);
	}
}
static __attribute__((noreturn)) void
txg_quiesce_thread(void *arg)
{
	dsl_pool_t *dp = arg;
	tx_state_t *tx = &dp->dp_tx;
	callb_cpr_t cpr;
	txg_thread_enter(tx, &cpr);
	for (;;) {
		uint64_t txg;
		while (!tx->tx_exiting &&
		    (tx->tx_open_txg >= tx->tx_quiesce_txg_waiting ||
		    txg_has_quiesced_to_sync(dp)))
			txg_thread_wait(tx, &cpr, &tx->tx_quiesce_more_cv, 0);
		if (tx->tx_exiting)
			txg_thread_exit(tx, &cpr, &tx->tx_quiesce_thread);
		txg = tx->tx_open_txg;
		dprintf("txg=%llu quiesce_txg=%llu sync_txg=%llu\n",
		    (u_longlong_t)txg,
		    (u_longlong_t)tx->tx_quiesce_txg_waiting,
		    (u_longlong_t)tx->tx_sync_txg_waiting);
		tx->tx_quiescing_txg = txg;
		mutex_exit(&tx->tx_sync_lock);
		txg_quiesce(dp, txg);
		mutex_enter(&tx->tx_sync_lock);
		dprintf("quiesce done, handing off txg %llu\n",
		    (u_longlong_t)txg);
		tx->tx_quiescing_txg = 0;
		tx->tx_quiesced_txg = txg;
		DTRACE_PROBE2(txg__quiesced, dsl_pool_t *, dp, uint64_t, txg);
		cv_broadcast(&tx->tx_sync_more_cv);
		cv_broadcast(&tx->tx_quiesce_done_cv);
	}
}
void
txg_delay(dsl_pool_t *dp, uint64_t txg, hrtime_t delay, hrtime_t resolution)
{
	tx_state_t *tx = &dp->dp_tx;
	hrtime_t start = gethrtime();
	if (tx->tx_open_txg > txg ||
	    tx->tx_syncing_txg == txg-1 || tx->tx_synced_txg == txg-1)
		return;
	mutex_enter(&tx->tx_sync_lock);
	if (tx->tx_open_txg > txg || tx->tx_synced_txg == txg-1) {
		mutex_exit(&tx->tx_sync_lock);
		return;
	}
	while (gethrtime() - start < delay &&
	    tx->tx_syncing_txg < txg-1 && !txg_stalled(dp)) {
		(void) cv_timedwait_hires(&tx->tx_quiesce_more_cv,
		    &tx->tx_sync_lock, delay, resolution, 0);
	}
	DMU_TX_STAT_BUMP(dmu_tx_delay);
	mutex_exit(&tx->tx_sync_lock);
}
static boolean_t
txg_wait_synced_impl(dsl_pool_t *dp, uint64_t txg, boolean_t wait_sig)
{
	tx_state_t *tx = &dp->dp_tx;
	ASSERT(!dsl_pool_config_held(dp));
	mutex_enter(&tx->tx_sync_lock);
	ASSERT3U(tx->tx_threads, ==, 2);
	if (txg == 0)
		txg = tx->tx_open_txg + TXG_DEFER_SIZE;
	if (tx->tx_sync_txg_waiting < txg)
		tx->tx_sync_txg_waiting = txg;
	dprintf("txg=%llu quiesce_txg=%llu sync_txg=%llu\n",
	    (u_longlong_t)txg, (u_longlong_t)tx->tx_quiesce_txg_waiting,
	    (u_longlong_t)tx->tx_sync_txg_waiting);
	while (tx->tx_synced_txg < txg) {
		dprintf("broadcasting sync more "
		    "tx_synced=%llu waiting=%llu dp=%px\n",
		    (u_longlong_t)tx->tx_synced_txg,
		    (u_longlong_t)tx->tx_sync_txg_waiting, dp);
		cv_broadcast(&tx->tx_sync_more_cv);
		if (wait_sig) {
			if (cv_wait_io_sig(&tx->tx_sync_done_cv,
			    &tx->tx_sync_lock) == 0) {
				mutex_exit(&tx->tx_sync_lock);
				return (B_TRUE);
			}
		} else {
			cv_wait_io(&tx->tx_sync_done_cv, &tx->tx_sync_lock);
		}
	}
	mutex_exit(&tx->tx_sync_lock);
	return (B_FALSE);
}
void
txg_wait_synced(dsl_pool_t *dp, uint64_t txg)
{
	VERIFY0(txg_wait_synced_impl(dp, txg, B_FALSE));
}
boolean_t
txg_wait_synced_sig(dsl_pool_t *dp, uint64_t txg)
{
	return (txg_wait_synced_impl(dp, txg, B_TRUE));
}
void
txg_wait_open(dsl_pool_t *dp, uint64_t txg, boolean_t should_quiesce)
{
	tx_state_t *tx = &dp->dp_tx;
	ASSERT(!dsl_pool_config_held(dp));
	mutex_enter(&tx->tx_sync_lock);
	ASSERT3U(tx->tx_threads, ==, 2);
	if (txg == 0)
		txg = tx->tx_open_txg + 1;
	if (tx->tx_quiesce_txg_waiting < txg && should_quiesce)
		tx->tx_quiesce_txg_waiting = txg;
	dprintf("txg=%llu quiesce_txg=%llu sync_txg=%llu\n",
	    (u_longlong_t)txg, (u_longlong_t)tx->tx_quiesce_txg_waiting,
	    (u_longlong_t)tx->tx_sync_txg_waiting);
	while (tx->tx_open_txg < txg) {
		cv_broadcast(&tx->tx_quiesce_more_cv);
		if (should_quiesce == B_TRUE) {
			cv_wait_io(&tx->tx_quiesce_done_cv, &tx->tx_sync_lock);
		} else {
			cv_wait_idle(&tx->tx_quiesce_done_cv,
			    &tx->tx_sync_lock);
		}
	}
	mutex_exit(&tx->tx_sync_lock);
}
void
txg_kick(dsl_pool_t *dp, uint64_t txg)
{
	tx_state_t *tx = &dp->dp_tx;
	ASSERT(!dsl_pool_config_held(dp));
	if (tx->tx_sync_txg_waiting >= txg)
		return;
	mutex_enter(&tx->tx_sync_lock);
	if (tx->tx_sync_txg_waiting < txg) {
		tx->tx_sync_txg_waiting = txg;
		cv_broadcast(&tx->tx_sync_more_cv);
	}
	mutex_exit(&tx->tx_sync_lock);
}
boolean_t
txg_stalled(dsl_pool_t *dp)
{
	tx_state_t *tx = &dp->dp_tx;
	return (tx->tx_quiesce_txg_waiting > tx->tx_open_txg);
}
boolean_t
txg_sync_waiting(dsl_pool_t *dp)
{
	tx_state_t *tx = &dp->dp_tx;
	return (tx->tx_syncing_txg <= tx->tx_sync_txg_waiting ||
	    tx->tx_quiesced_txg != 0);
}
#ifdef ZFS_DEBUG
void
txg_verify(spa_t *spa, uint64_t txg)
{
	dsl_pool_t *dp __maybe_unused = spa_get_dsl(spa);
	if (txg <= TXG_INITIAL || txg == ZILTEST_TXG)
		return;
	ASSERT3U(txg, <=, dp->dp_tx.tx_open_txg);
	ASSERT3U(txg, >=, dp->dp_tx.tx_synced_txg);
	ASSERT3U(txg, >=, dp->dp_tx.tx_open_txg - TXG_CONCURRENT_STATES);
}
#endif
void
txg_list_create(txg_list_t *tl, spa_t *spa, size_t offset)
{
	int t;
	mutex_init(&tl->tl_lock, NULL, MUTEX_DEFAULT, NULL);
	tl->tl_offset = offset;
	tl->tl_spa = spa;
	for (t = 0; t < TXG_SIZE; t++)
		tl->tl_head[t] = NULL;
}
static boolean_t
txg_list_empty_impl(txg_list_t *tl, uint64_t txg)
{
	ASSERT(MUTEX_HELD(&tl->tl_lock));
	TXG_VERIFY(tl->tl_spa, txg);
	return (tl->tl_head[txg & TXG_MASK] == NULL);
}
boolean_t
txg_list_empty(txg_list_t *tl, uint64_t txg)
{
	mutex_enter(&tl->tl_lock);
	boolean_t ret = txg_list_empty_impl(tl, txg);
	mutex_exit(&tl->tl_lock);
	return (ret);
}
void
txg_list_destroy(txg_list_t *tl)
{
	int t;
	mutex_enter(&tl->tl_lock);
	for (t = 0; t < TXG_SIZE; t++)
		ASSERT(txg_list_empty_impl(tl, t));
	mutex_exit(&tl->tl_lock);
	mutex_destroy(&tl->tl_lock);
}
boolean_t
txg_all_lists_empty(txg_list_t *tl)
{
	boolean_t res = B_TRUE;
	for (int i = 0; i < TXG_SIZE; i++)
		res &= (tl->tl_head[i] == NULL);
	return (res);
}
boolean_t
txg_list_add(txg_list_t *tl, void *p, uint64_t txg)
{
	int t = txg & TXG_MASK;
	txg_node_t *tn = (txg_node_t *)((char *)p + tl->tl_offset);
	boolean_t add;
	TXG_VERIFY(tl->tl_spa, txg);
	mutex_enter(&tl->tl_lock);
	add = (tn->tn_member[t] == 0);
	if (add) {
		tn->tn_member[t] = 1;
		tn->tn_next[t] = tl->tl_head[t];
		tl->tl_head[t] = tn;
	}
	mutex_exit(&tl->tl_lock);
	return (add);
}
boolean_t
txg_list_add_tail(txg_list_t *tl, void *p, uint64_t txg)
{
	int t = txg & TXG_MASK;
	txg_node_t *tn = (txg_node_t *)((char *)p + tl->tl_offset);
	boolean_t add;
	TXG_VERIFY(tl->tl_spa, txg);
	mutex_enter(&tl->tl_lock);
	add = (tn->tn_member[t] == 0);
	if (add) {
		txg_node_t **tp;
		for (tp = &tl->tl_head[t]; *tp != NULL; tp = &(*tp)->tn_next[t])
			continue;
		tn->tn_member[t] = 1;
		tn->tn_next[t] = NULL;
		*tp = tn;
	}
	mutex_exit(&tl->tl_lock);
	return (add);
}
void *
txg_list_remove(txg_list_t *tl, uint64_t txg)
{
	int t = txg & TXG_MASK;
	txg_node_t *tn;
	void *p = NULL;
	TXG_VERIFY(tl->tl_spa, txg);
	mutex_enter(&tl->tl_lock);
	if ((tn = tl->tl_head[t]) != NULL) {
		ASSERT(tn->tn_member[t]);
		ASSERT(tn->tn_next[t] == NULL || tn->tn_next[t]->tn_member[t]);
		p = (char *)tn - tl->tl_offset;
		tl->tl_head[t] = tn->tn_next[t];
		tn->tn_next[t] = NULL;
		tn->tn_member[t] = 0;
	}
	mutex_exit(&tl->tl_lock);
	return (p);
}
void *
txg_list_remove_this(txg_list_t *tl, void *p, uint64_t txg)
{
	int t = txg & TXG_MASK;
	txg_node_t *tn, **tp;
	TXG_VERIFY(tl->tl_spa, txg);
	mutex_enter(&tl->tl_lock);
	for (tp = &tl->tl_head[t]; (tn = *tp) != NULL; tp = &tn->tn_next[t]) {
		if ((char *)tn - tl->tl_offset == p) {
			*tp = tn->tn_next[t];
			tn->tn_next[t] = NULL;
			tn->tn_member[t] = 0;
			mutex_exit(&tl->tl_lock);
			return (p);
		}
	}
	mutex_exit(&tl->tl_lock);
	return (NULL);
}
boolean_t
txg_list_member(txg_list_t *tl, void *p, uint64_t txg)
{
	int t = txg & TXG_MASK;
	txg_node_t *tn = (txg_node_t *)((char *)p + tl->tl_offset);
	TXG_VERIFY(tl->tl_spa, txg);
	return (tn->tn_member[t] != 0);
}
void *
txg_list_head(txg_list_t *tl, uint64_t txg)
{
	int t = txg & TXG_MASK;
	txg_node_t *tn;
	mutex_enter(&tl->tl_lock);
	tn = tl->tl_head[t];
	mutex_exit(&tl->tl_lock);
	TXG_VERIFY(tl->tl_spa, txg);
	return (tn == NULL ? NULL : (char *)tn - tl->tl_offset);
}
void *
txg_list_next(txg_list_t *tl, void *p, uint64_t txg)
{
	int t = txg & TXG_MASK;
	txg_node_t *tn = (txg_node_t *)((char *)p + tl->tl_offset);
	TXG_VERIFY(tl->tl_spa, txg);
	mutex_enter(&tl->tl_lock);
	tn = tn->tn_next[t];
	mutex_exit(&tl->tl_lock);
	return (tn == NULL ? NULL : (char *)tn - tl->tl_offset);
}
EXPORT_SYMBOL(txg_init);
EXPORT_SYMBOL(txg_fini);
EXPORT_SYMBOL(txg_sync_start);
EXPORT_SYMBOL(txg_sync_stop);
EXPORT_SYMBOL(txg_hold_open);
EXPORT_SYMBOL(txg_rele_to_quiesce);
EXPORT_SYMBOL(txg_rele_to_sync);
EXPORT_SYMBOL(txg_register_callbacks);
EXPORT_SYMBOL(txg_delay);
EXPORT_SYMBOL(txg_wait_synced);
EXPORT_SYMBOL(txg_wait_open);
EXPORT_SYMBOL(txg_wait_callbacks);
EXPORT_SYMBOL(txg_stalled);
EXPORT_SYMBOL(txg_sync_waiting);
ZFS_MODULE_PARAM(zfs_txg, zfs_txg_, timeout, UINT, ZMOD_RW,
	"Max seconds worth of delta per txg");
