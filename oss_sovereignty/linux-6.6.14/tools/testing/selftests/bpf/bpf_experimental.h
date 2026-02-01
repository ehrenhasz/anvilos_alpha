#ifndef __BPF_EXPERIMENTAL__
#define __BPF_EXPERIMENTAL__

#include <vmlinux.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

#define __contains(name, node) __attribute__((btf_decl_tag("contains:" #name ":" #node)))

 
extern void *bpf_obj_new_impl(__u64 local_type_id, void *meta) __ksym;

 
#define bpf_obj_new(type) ((type *)bpf_obj_new_impl(bpf_core_type_id_local(type), NULL))

 
extern void bpf_obj_drop_impl(void *kptr, void *meta) __ksym;

 
#define bpf_obj_drop(kptr) bpf_obj_drop_impl(kptr, NULL)

 
extern void *bpf_refcount_acquire_impl(void *kptr, void *meta) __ksym;

 
#define bpf_refcount_acquire(kptr) bpf_refcount_acquire_impl(kptr, NULL)

 
extern int bpf_list_push_front_impl(struct bpf_list_head *head,
				    struct bpf_list_node *node,
				    void *meta, __u64 off) __ksym;

 
#define bpf_list_push_front(head, node) bpf_list_push_front_impl(head, node, NULL, 0)

 
extern int bpf_list_push_back_impl(struct bpf_list_head *head,
				   struct bpf_list_node *node,
				   void *meta, __u64 off) __ksym;

 
#define bpf_list_push_back(head, node) bpf_list_push_back_impl(head, node, NULL, 0)

 
extern struct bpf_list_node *bpf_list_pop_front(struct bpf_list_head *head) __ksym;

 
extern struct bpf_list_node *bpf_list_pop_back(struct bpf_list_head *head) __ksym;

 
extern struct bpf_rb_node *bpf_rbtree_remove(struct bpf_rb_root *root,
					     struct bpf_rb_node *node) __ksym;

 
extern int bpf_rbtree_add_impl(struct bpf_rb_root *root, struct bpf_rb_node *node,
			       bool (less)(struct bpf_rb_node *a, const struct bpf_rb_node *b),
			       void *meta, __u64 off) __ksym;

 
#define bpf_rbtree_add(head, node, less) bpf_rbtree_add_impl(head, node, less, NULL, 0)

 
extern struct bpf_rb_node *bpf_rbtree_first(struct bpf_rb_root *root) __ksym;

#endif
