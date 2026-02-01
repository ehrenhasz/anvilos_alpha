
 

#ifndef SELFTEST_KVM_MEMSTRESS_H
#define SELFTEST_KVM_MEMSTRESS_H

#include <pthread.h>

#include "kvm_util.h"

 
#define DEFAULT_GUEST_TEST_MEM		0xc0000000

#define DEFAULT_PER_VCPU_MEM_SIZE	(1 << 30)  

#define MEMSTRESS_MEM_SLOT_INDEX	1

struct memstress_vcpu_args {
	uint64_t gpa;
	uint64_t gva;
	uint64_t pages;

	 
	struct kvm_vcpu *vcpu;
	int vcpu_idx;
};

struct memstress_args {
	struct kvm_vm *vm;
	 
	uint64_t gpa;
	uint64_t size;
	uint64_t guest_page_size;
	uint32_t random_seed;
	uint32_t write_percent;

	 
	bool nested;
	 
	bool random_access;
	 
	bool pin_vcpus;
	 
	uint32_t vcpu_to_pcpu[KVM_MAX_VCPUS];

 	 
 	bool stop_vcpus;

	struct memstress_vcpu_args vcpu_args[KVM_MAX_VCPUS];
};

extern struct memstress_args memstress_args;

struct kvm_vm *memstress_create_vm(enum vm_guest_mode mode, int nr_vcpus,
				   uint64_t vcpu_memory_bytes, int slots,
				   enum vm_mem_backing_src_type backing_src,
				   bool partition_vcpu_memory_access);
void memstress_destroy_vm(struct kvm_vm *vm);

void memstress_set_write_percent(struct kvm_vm *vm, uint32_t write_percent);
void memstress_set_random_seed(struct kvm_vm *vm, uint32_t random_seed);
void memstress_set_random_access(struct kvm_vm *vm, bool random_access);

void memstress_start_vcpu_threads(int vcpus, void (*vcpu_fn)(struct memstress_vcpu_args *));
void memstress_join_vcpu_threads(int vcpus);
void memstress_guest_code(uint32_t vcpu_id);

uint64_t memstress_nested_pages(int nr_vcpus);
void memstress_setup_nested(struct kvm_vm *vm, int nr_vcpus, struct kvm_vcpu *vcpus[]);

void memstress_enable_dirty_logging(struct kvm_vm *vm, int slots);
void memstress_disable_dirty_logging(struct kvm_vm *vm, int slots);
void memstress_get_dirty_log(struct kvm_vm *vm, unsigned long *bitmaps[], int slots);
void memstress_clear_dirty_log(struct kvm_vm *vm, unsigned long *bitmaps[],
			       int slots, uint64_t pages_per_slot);
unsigned long **memstress_alloc_bitmaps(int slots, uint64_t pages_per_slot);
void memstress_free_bitmaps(unsigned long *bitmaps[], int slots);

#endif  
