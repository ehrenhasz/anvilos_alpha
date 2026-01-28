#ifndef _ASM_X86_KVM_PAGE_TRACK_H
#define _ASM_X86_KVM_PAGE_TRACK_H
#include <linux/kvm_types.h>
#ifdef CONFIG_KVM_EXTERNAL_WRITE_TRACKING
struct kvm_page_track_notifier_head {
	struct srcu_struct track_srcu;
	struct hlist_head track_notifier_list;
};
struct kvm_page_track_notifier_node {
	struct hlist_node node;
	void (*track_write)(gpa_t gpa, const u8 *new, int bytes,
			    struct kvm_page_track_notifier_node *node);
	void (*track_remove_region)(gfn_t gfn, unsigned long nr_pages,
				    struct kvm_page_track_notifier_node *node);
};
int kvm_page_track_register_notifier(struct kvm *kvm,
				     struct kvm_page_track_notifier_node *n);
void kvm_page_track_unregister_notifier(struct kvm *kvm,
					struct kvm_page_track_notifier_node *n);
int kvm_write_track_add_gfn(struct kvm *kvm, gfn_t gfn);
int kvm_write_track_remove_gfn(struct kvm *kvm, gfn_t gfn);
#else
struct kvm_page_track_notifier_node {};
#endif  
#endif
