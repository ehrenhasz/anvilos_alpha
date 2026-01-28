#ifndef ASM_POWERPC_EEH_EVENT_H
#define ASM_POWERPC_EEH_EVENT_H
#ifdef __KERNEL__
struct eeh_event {
	struct list_head	list;	 
	struct eeh_pe		*pe;	 
};
int eeh_event_init(void);
int eeh_send_failure_event(struct eeh_pe *pe);
int __eeh_send_failure_event(struct eeh_pe *pe);
void eeh_remove_event(struct eeh_pe *pe, bool force);
void eeh_handle_normal_event(struct eeh_pe *pe);
void eeh_handle_special_event(void);
#endif  
#endif  
