#ifndef KFD_SMI_EVENTS_H_INCLUDED
#define KFD_SMI_EVENTS_H_INCLUDED
int kfd_smi_event_open(struct kfd_node *dev, uint32_t *fd);
void kfd_smi_event_update_vmfault(struct kfd_node *dev, uint16_t pasid);
void kfd_smi_event_update_thermal_throttling(struct kfd_node *dev,
					     uint64_t throttle_bitmask);
void kfd_smi_event_update_gpu_reset(struct kfd_node *dev, bool post_reset);
void kfd_smi_event_page_fault_start(struct kfd_node *node, pid_t pid,
				    unsigned long address, bool write_fault,
				    ktime_t ts);
void kfd_smi_event_page_fault_end(struct kfd_node *node, pid_t pid,
				  unsigned long address, bool migration);
void kfd_smi_event_migration_start(struct kfd_node *node, pid_t pid,
			     unsigned long start, unsigned long end,
			     uint32_t from, uint32_t to,
			     uint32_t prefetch_loc, uint32_t preferred_loc,
			     uint32_t trigger);
void kfd_smi_event_migration_end(struct kfd_node *node, pid_t pid,
			     unsigned long start, unsigned long end,
			     uint32_t from, uint32_t to, uint32_t trigger);
void kfd_smi_event_queue_eviction(struct kfd_node *node, pid_t pid,
				  uint32_t trigger);
void kfd_smi_event_queue_restore(struct kfd_node *node, pid_t pid);
void kfd_smi_event_queue_restore_rescheduled(struct mm_struct *mm);
void kfd_smi_event_unmap_from_gpu(struct kfd_node *node, pid_t pid,
				  unsigned long address, unsigned long last,
				  uint32_t trigger);
#endif
