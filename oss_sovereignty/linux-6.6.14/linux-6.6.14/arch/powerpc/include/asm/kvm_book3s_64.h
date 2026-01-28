#ifndef __ASM_KVM_BOOK3S_64_H__
#define __ASM_KVM_BOOK3S_64_H__
#include <linux/string.h>
#include <asm/bitops.h>
#include <asm/book3s/64/mmu-hash.h>
#include <asm/cpu_has_feature.h>
#include <asm/ppc-opcode.h>
#include <asm/pte-walk.h>
struct kvm_nested_guest {
	struct kvm *l1_host;		 
	int l1_lpid;			 
	int shadow_lpid;		 
	pgd_t *shadow_pgtable;		 
	u64 l1_gr_to_hr;		 
	u64 process_table;		 
	long refcnt;			 
	struct mutex tlb_lock;		 
	struct kvm_nested_guest *next;
	cpumask_t need_tlb_flush;
	short prev_cpu[NR_CPUS];
	u8 radix;			 
};
#define RMAP_NESTED_LPID_MASK		0xFFF0000000000000UL
#define RMAP_NESTED_LPID_SHIFT		(52)
#define RMAP_NESTED_GPA_MASK		0x000FFFFFFFFFF000UL
#define RMAP_NESTED_IS_SINGLE_ENTRY	0x0000000000000001UL
struct rmap_nested {
	struct llist_node list;
	u64 rmap;
};
#define for_each_nest_rmap_safe(pos, node, rmapp)			       \
	for ((pos) = llist_entry((node), typeof(*(pos)), list);		       \
	     (node) &&							       \
	     (*(rmapp) = ((RMAP_NESTED_IS_SINGLE_ENTRY & ((u64) (node))) ?     \
			  ((u64) (node)) : ((pos)->rmap))) &&		       \
	     (((node) = ((RMAP_NESTED_IS_SINGLE_ENTRY & ((u64) (node))) ?      \
			 ((struct llist_node *) ((pos) = NULL)) :	       \
			 (pos)->list.next)), true);			       \
	     (pos) = llist_entry((node), typeof(*(pos)), list))
struct kvm_nested_guest *kvmhv_get_nested(struct kvm *kvm, int l1_lpid,
					  bool create);
void kvmhv_put_nested(struct kvm_nested_guest *gp);
int kvmhv_nested_next_lpid(struct kvm *kvm, int lpid);
#define H_TLBIE_P1_ENC(ric, prs, r)	(___PPC_RIC(ric) | ___PPC_PRS(prs) | \
					 ___PPC_R(r))
#define PPC_MIN_HPT_ORDER	18
#define PPC_MAX_HPT_ORDER	46
#ifdef CONFIG_KVM_BOOK3S_PR_POSSIBLE
static inline struct kvmppc_book3s_shadow_vcpu *svcpu_get(struct kvm_vcpu *vcpu)
{
	preempt_disable();
	return &get_paca()->shadow_vcpu;
}
static inline void svcpu_put(struct kvmppc_book3s_shadow_vcpu *svcpu)
{
	preempt_enable();
}
#endif
#ifdef CONFIG_KVM_BOOK3S_HV_POSSIBLE
static inline bool kvm_is_radix(struct kvm *kvm)
{
	return kvm->arch.radix;
}
static inline bool kvmhv_vcpu_is_radix(struct kvm_vcpu *vcpu)
{
	bool radix;
	if (vcpu->arch.nested)
		radix = vcpu->arch.nested->radix;
	else
		radix = kvm_is_radix(vcpu->kvm);
	return radix;
}
unsigned long kvmppc_msr_hard_disable_set_facilities(struct kvm_vcpu *vcpu, unsigned long msr);
int kvmhv_vcpu_entry_p9(struct kvm_vcpu *vcpu, u64 time_limit, unsigned long lpcr, u64 *tb);
#define KVM_DEFAULT_HPT_ORDER	24	 
#endif
#define HDSISR_CANARY	0x7fff
#define HPTE_V_HVLOCK	0x40UL
#define HPTE_V_ABSENT	0x20UL
#define HPTE_GR_MODIFIED	(1ul << 62)
#define HPTE_GR_RESERVED	HPTE_GR_MODIFIED
static inline long try_lock_hpte(__be64 *hpte, unsigned long bits)
{
	unsigned long tmp, old;
	__be64 be_lockbit, be_bits;
	be_lockbit = cpu_to_be64(HPTE_V_HVLOCK);
	be_bits = cpu_to_be64(bits);
	asm volatile("	ldarx	%0,0,%2\n"
		     "	and.	%1,%0,%3\n"
		     "	bne	2f\n"
		     "	or	%0,%0,%4\n"
		     "  stdcx.	%0,0,%2\n"
		     "	beq+	2f\n"
		     "	mr	%1,%3\n"
		     "2:	isync"
		     : "=&r" (tmp), "=&r" (old)
		     : "r" (hpte), "r" (be_bits), "r" (be_lockbit)
		     : "cc", "memory");
	return old == 0;
}
static inline void unlock_hpte(__be64 *hpte, unsigned long hpte_v)
{
	hpte_v &= ~HPTE_V_HVLOCK;
	asm volatile(PPC_RELEASE_BARRIER "" : : : "memory");
	hpte[0] = cpu_to_be64(hpte_v);
}
static inline void __unlock_hpte(__be64 *hpte, unsigned long hpte_v)
{
	hpte_v &= ~HPTE_V_HVLOCK;
	hpte[0] = cpu_to_be64(hpte_v);
}
static inline int kvmppc_hpte_page_shifts(unsigned long h, unsigned long l)
{
	unsigned int lphi;
	if (!(h & HPTE_V_LARGE))
		return 12;	 
	lphi = (l >> 16) & 0xf;
	switch ((l >> 12) & 0xf) {
	case 0:
		return !lphi ? 24 : 0;		 
		break;
	case 1:
		return 16;			 
		break;
	case 3:
		return !lphi ? 34 : 0;		 
		break;
	case 7:
		return (16 << 8) + 12;		 
		break;
	case 8:
		if (!lphi)
			return (24 << 8) + 16;	 
		if (lphi == 3)
			return (24 << 8) + 12;	 
		break;
	}
	return 0;
}
static inline int kvmppc_hpte_base_page_shift(unsigned long h, unsigned long l)
{
	return kvmppc_hpte_page_shifts(h, l) & 0xff;
}
static inline int kvmppc_hpte_actual_page_shift(unsigned long h, unsigned long l)
{
	int tmp = kvmppc_hpte_page_shifts(h, l);
	if (tmp >= 0x100)
		tmp >>= 8;
	return tmp;
}
static inline unsigned long kvmppc_actual_pgsz(unsigned long v, unsigned long r)
{
	int shift = kvmppc_hpte_actual_page_shift(v, r);
	if (shift)
		return 1ul << shift;
	return 0;
}
static inline int kvmppc_pgsize_lp_encoding(int base_shift, int actual_shift)
{
	switch (base_shift) {
	case 12:
		switch (actual_shift) {
		case 12:
			return 0;
		case 16:
			return 7;
		case 24:
			return 0x38;
		}
		break;
	case 16:
		switch (actual_shift) {
		case 16:
			return 1;
		case 24:
			return 8;
		}
		break;
	case 24:
		return 0;
	}
	return -1;
}
static inline unsigned long compute_tlbie_rb(unsigned long v, unsigned long r,
					     unsigned long pte_index)
{
	int a_pgshift, b_pgshift;
	unsigned long rb = 0, va_low, sllp;
	b_pgshift = a_pgshift = kvmppc_hpte_page_shifts(v, r);
	if (a_pgshift >= 0x100) {
		b_pgshift &= 0xff;
		a_pgshift >>= 8;
	}
	rb = (v & ~0x7fUL) << 16;		 
	va_low = pte_index >> 3;
	if (v & HPTE_V_SECONDARY)
		va_low = ~va_low;
	if (!(v & HPTE_V_1TB_SEG))
		va_low ^= v >> (SID_SHIFT - 16);
	else
		va_low ^= v >> (SID_SHIFT_1T - 16);
	va_low &= 0x7ff;
	if (b_pgshift <= 12) {
		if (a_pgshift > 12) {
			sllp = (a_pgshift == 16) ? 5 : 4;
			rb |= sllp << 5;	 
		}
		rb |= (va_low & 0x7ff) << 12;	 
	} else {
		int aval_shift;
		rb |= (va_low << b_pgshift) & 0x7ff000;
		rb &= ~((1ul << a_pgshift) - 1);
		aval_shift = 64 - (77 - b_pgshift) + 1;
		rb |= ((va_low << aval_shift) & 0xfe);
		rb |= 1;		 
		rb |= r & 0xff000 & ((1ul << a_pgshift) - 1);  
	}
	rb |= (v >> HPTE_V_SSIZE_SHIFT) << 8;	 
	return rb;
}
static inline unsigned long hpte_rpn(unsigned long ptel, unsigned long psize)
{
	return ((ptel & HPTE_R_RPN) & ~(psize - 1)) >> PAGE_SHIFT;
}
static inline int hpte_is_writable(unsigned long ptel)
{
	unsigned long pp = ptel & (HPTE_R_PP0 | HPTE_R_PP);
	return pp != PP_RXRX && pp != PP_RXXX;
}
static inline unsigned long hpte_make_readonly(unsigned long ptel)
{
	if ((ptel & HPTE_R_PP0) || (ptel & HPTE_R_PP) == PP_RWXX)
		ptel = (ptel & ~HPTE_R_PP) | PP_RXXX;
	else
		ptel |= PP_RXRX;
	return ptel;
}
static inline bool hpte_cache_flags_ok(unsigned long hptel, bool is_ci)
{
	unsigned int wimg = hptel & HPTE_R_WIMG;
	if (wimg == (HPTE_R_W | HPTE_R_I | HPTE_R_M) &&
	    cpu_has_feature(CPU_FTR_ARCH_206))
		wimg = HPTE_R_M;
	if (!is_ci)
		return wimg == HPTE_R_M;
	if (wimg & HPTE_R_W)  
		return false;
	return !!(wimg & HPTE_R_I);
}
static inline pte_t kvmppc_read_update_linux_pte(pte_t *ptep, int writing)
{
	pte_t old_pte, new_pte = __pte(0);
	while (1) {
		old_pte = READ_ONCE(*ptep);
		if (unlikely(pte_val(old_pte) & H_PAGE_BUSY)) {
			cpu_relax();
			continue;
		}
		if (unlikely(!pte_present(old_pte)))
			return __pte(0);
		new_pte = pte_mkyoung(old_pte);
		if (writing && pte_write(old_pte))
			new_pte = pte_mkdirty(new_pte);
		if (pte_xchg(ptep, old_pte, new_pte))
			break;
	}
	return new_pte;
}
static inline bool hpte_read_permission(unsigned long pp, unsigned long key)
{
	if (key)
		return PP_RWRX <= pp && pp <= PP_RXRX;
	return true;
}
static inline bool hpte_write_permission(unsigned long pp, unsigned long key)
{
	if (key)
		return pp == PP_RWRW;
	return pp <= PP_RWRW;
}
static inline int hpte_get_skey_perm(unsigned long hpte_r, unsigned long amr)
{
	unsigned long skey;
	skey = ((hpte_r & HPTE_R_KEY_HI) >> 57) |
		((hpte_r & HPTE_R_KEY_LO) >> 9);
	return (amr >> (62 - 2 * skey)) & 3;
}
static inline void lock_rmap(unsigned long *rmap)
{
	do {
		while (test_bit(KVMPPC_RMAP_LOCK_BIT, rmap))
			cpu_relax();
	} while (test_and_set_bit_lock(KVMPPC_RMAP_LOCK_BIT, rmap));
}
static inline void unlock_rmap(unsigned long *rmap)
{
	__clear_bit_unlock(KVMPPC_RMAP_LOCK_BIT, rmap);
}
static inline bool slot_is_aligned(struct kvm_memory_slot *memslot,
				   unsigned long pagesize)
{
	unsigned long mask = (pagesize >> PAGE_SHIFT) - 1;
	if (pagesize <= PAGE_SIZE)
		return true;
	return !(memslot->base_gfn & mask) && !(memslot->npages & mask);
}
static inline unsigned long slb_pgsize_encoding(unsigned long psize)
{
	unsigned long senc = 0;
	if (psize > 0x1000) {
		senc = SLB_VSID_L;
		if (psize == 0x10000)
			senc |= SLB_VSID_LP_01;
	}
	return senc;
}
static inline int is_vrma_hpte(unsigned long hpte_v)
{
	return (hpte_v & ~0xffffffUL) ==
		(HPTE_V_1TB_SEG | (VRMA_VSID << (40 - 16)));
}
#ifdef CONFIG_KVM_BOOK3S_HV_POSSIBLE
static inline void note_hpte_modification(struct kvm *kvm,
					  struct revmap_entry *rev)
{
	if (atomic_read(&kvm->arch.hpte_mod_interest))
		rev->guest_rpte |= HPTE_GR_MODIFIED;
}
static inline struct kvm_memslots *kvm_memslots_raw(struct kvm *kvm)
{
	return rcu_dereference_raw_check(kvm->memslots[0]);
}
extern void kvmppc_mmu_debugfs_init(struct kvm *kvm);
extern void kvmhv_radix_debugfs_init(struct kvm *kvm);
extern void kvmhv_rm_send_ipi(int cpu);
static inline unsigned long kvmppc_hpt_npte(struct kvm_hpt_info *hpt)
{
	return 1UL << (hpt->order - 4);
}
static inline unsigned long kvmppc_hpt_mask(struct kvm_hpt_info *hpt)
{
	return (1UL << (hpt->order - 7)) - 1;
}
static inline void set_dirty_bits(unsigned long *map, unsigned long i,
				  unsigned long npages)
{
	if (npages >= 8)
		memset((char *)map + i / 8, 0xff, npages / 8);
	else
		for (; npages; ++i, --npages)
			__set_bit_le(i, map);
}
static inline void set_dirty_bits_atomic(unsigned long *map, unsigned long i,
					 unsigned long npages)
{
	if (npages >= 8)
		memset((char *)map + i / 8, 0xff, npages / 8);
	else
		for (; npages; ++i, --npages)
			set_bit_le(i, map);
}
static inline u64 sanitize_msr(u64 msr)
{
	msr &= ~MSR_HV;
	msr |= MSR_ME;
	return msr;
}
#ifdef CONFIG_PPC_TRANSACTIONAL_MEM
static inline void copy_from_checkpoint(struct kvm_vcpu *vcpu)
{
	vcpu->arch.regs.ccr  = vcpu->arch.cr_tm;
	vcpu->arch.regs.xer = vcpu->arch.xer_tm;
	vcpu->arch.regs.link  = vcpu->arch.lr_tm;
	vcpu->arch.regs.ctr = vcpu->arch.ctr_tm;
	vcpu->arch.amr = vcpu->arch.amr_tm;
	vcpu->arch.ppr = vcpu->arch.ppr_tm;
	vcpu->arch.dscr = vcpu->arch.dscr_tm;
	vcpu->arch.tar = vcpu->arch.tar_tm;
	memcpy(vcpu->arch.regs.gpr, vcpu->arch.gpr_tm,
	       sizeof(vcpu->arch.regs.gpr));
	vcpu->arch.fp  = vcpu->arch.fp_tm;
	vcpu->arch.vr  = vcpu->arch.vr_tm;
	vcpu->arch.vrsave = vcpu->arch.vrsave_tm;
}
static inline void copy_to_checkpoint(struct kvm_vcpu *vcpu)
{
	vcpu->arch.cr_tm  = vcpu->arch.regs.ccr;
	vcpu->arch.xer_tm = vcpu->arch.regs.xer;
	vcpu->arch.lr_tm  = vcpu->arch.regs.link;
	vcpu->arch.ctr_tm = vcpu->arch.regs.ctr;
	vcpu->arch.amr_tm = vcpu->arch.amr;
	vcpu->arch.ppr_tm = vcpu->arch.ppr;
	vcpu->arch.dscr_tm = vcpu->arch.dscr;
	vcpu->arch.tar_tm = vcpu->arch.tar;
	memcpy(vcpu->arch.gpr_tm, vcpu->arch.regs.gpr,
	       sizeof(vcpu->arch.regs.gpr));
	vcpu->arch.fp_tm  = vcpu->arch.fp;
	vcpu->arch.vr_tm  = vcpu->arch.vr;
	vcpu->arch.vrsave_tm = vcpu->arch.vrsave;
}
#endif  
extern int kvmppc_create_pte(struct kvm *kvm, pgd_t *pgtable, pte_t pte,
			     unsigned long gpa, unsigned int level,
			     unsigned long mmu_seq, unsigned int lpid,
			     unsigned long *rmapp, struct rmap_nested **n_rmap);
extern void kvmhv_insert_nest_rmap(struct kvm *kvm, unsigned long *rmapp,
				   struct rmap_nested **n_rmap);
extern void kvmhv_update_nest_rmap_rc_list(struct kvm *kvm, unsigned long *rmapp,
					   unsigned long clr, unsigned long set,
					   unsigned long hpa, unsigned long nbytes);
extern void kvmhv_remove_nest_rmap_range(struct kvm *kvm,
				const struct kvm_memory_slot *memslot,
				unsigned long gpa, unsigned long hpa,
				unsigned long nbytes);
static inline pte_t *
find_kvm_secondary_pte_unlocked(struct kvm *kvm, unsigned long ea,
				unsigned *hshift)
{
	pte_t *pte;
	pte = __find_linux_pte(kvm->arch.pgtable, ea, NULL, hshift);
	return pte;
}
static inline pte_t *find_kvm_secondary_pte(struct kvm *kvm, unsigned long ea,
					    unsigned *hshift)
{
	pte_t *pte;
	VM_WARN(!spin_is_locked(&kvm->mmu_lock),
		"%s called with kvm mmu_lock not held \n", __func__);
	pte = __find_linux_pte(kvm->arch.pgtable, ea, NULL, hshift);
	return pte;
}
static inline pte_t *find_kvm_host_pte(struct kvm *kvm, unsigned long mmu_seq,
				       unsigned long ea, unsigned *hshift)
{
	pte_t *pte;
	VM_WARN(!spin_is_locked(&kvm->mmu_lock),
		"%s called with kvm mmu_lock not held \n", __func__);
	if (mmu_invalidate_retry(kvm, mmu_seq))
		return NULL;
	pte = __find_linux_pte(kvm->mm->pgd, ea, NULL, hshift);
	return pte;
}
extern pte_t *find_kvm_nested_guest_pte(struct kvm *kvm, unsigned long lpid,
					unsigned long ea, unsigned *hshift);
#endif  
#endif  
