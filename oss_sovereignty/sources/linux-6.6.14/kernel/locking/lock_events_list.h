


#ifndef LOCK_EVENT
#define LOCK_EVENT(name)	LOCKEVENT_ ## name,
#endif

#ifdef CONFIG_QUEUED_SPINLOCKS
#ifdef CONFIG_PARAVIRT_SPINLOCKS

LOCK_EVENT(pv_hash_hops)	
LOCK_EVENT(pv_kick_unlock)	
LOCK_EVENT(pv_kick_wake)	
LOCK_EVENT(pv_latency_kick)	
LOCK_EVENT(pv_latency_wake)	
LOCK_EVENT(pv_lock_stealing)	
LOCK_EVENT(pv_spurious_wakeup)	
LOCK_EVENT(pv_wait_again)	
LOCK_EVENT(pv_wait_early)	
LOCK_EVENT(pv_wait_head)	
LOCK_EVENT(pv_wait_node)	
#endif 


LOCK_EVENT(lock_pending)	
LOCK_EVENT(lock_slowpath)	
LOCK_EVENT(lock_use_node2)	
LOCK_EVENT(lock_use_node3)	
LOCK_EVENT(lock_use_node4)	
LOCK_EVENT(lock_no_node)	
#endif 


LOCK_EVENT(rwsem_sleep_reader)	
LOCK_EVENT(rwsem_sleep_writer)	
LOCK_EVENT(rwsem_wake_reader)	
LOCK_EVENT(rwsem_wake_writer)	
LOCK_EVENT(rwsem_opt_lock)	
LOCK_EVENT(rwsem_opt_fail)	
LOCK_EVENT(rwsem_opt_nospin)	
LOCK_EVENT(rwsem_rlock)		
LOCK_EVENT(rwsem_rlock_steal)	
LOCK_EVENT(rwsem_rlock_fast)	
LOCK_EVENT(rwsem_rlock_fail)	
LOCK_EVENT(rwsem_rlock_handoff)	
LOCK_EVENT(rwsem_wlock)		
LOCK_EVENT(rwsem_wlock_fail)	
LOCK_EVENT(rwsem_wlock_handoff)	
