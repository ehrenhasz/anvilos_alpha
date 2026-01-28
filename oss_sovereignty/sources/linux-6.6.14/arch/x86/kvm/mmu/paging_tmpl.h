




#if PTTYPE == 64
	#define pt_element_t u64
	#define guest_walker guest_walker64
	#define FNAME(name) paging##64_##name
	#define PT_LEVEL_BITS 9
	#define PT_GUEST_DIRTY_SHIFT PT_DIRTY_SHIFT
	#define PT_GUEST_ACCESSED_SHIFT PT_ACCESSED_SHIFT
	#define PT_HAVE_ACCESSED_DIRTY(mmu) true
	#ifdef CONFIG_X86_64
	#define PT_MAX_FULL_LEVELS PT64_ROOT_MAX_LEVEL
	#else
	#define PT_MAX_FULL_LEVELS 2
	#endif
#elif PTTYPE == 32
	#define pt_element_t u32
	#define guest_walker guest_walker32
	#define FNAME(name) paging##32_##name
	#define PT_LEVEL_BITS 10
	#define PT_MAX_FULL_LEVELS 2
	#define PT_GUEST_DIRTY_SHIFT PT_DIRTY_SHIFT
	#define PT_GUEST_ACCESSED_SHIFT PT_ACCESSED_SHIFT
	#define PT_HAVE_ACCESSED_DIRTY(mmu) true

	#define PT32_DIR_PSE36_SIZE 4
	#define PT32_DIR_PSE36_SHIFT 13
	#define PT32_DIR_PSE36_MASK \
		(((1ULL << PT32_DIR_PSE36_SIZE) - 1) << PT32_DIR_PSE36_SHIFT)
#elif PTTYPE == PTTYPE_EPT
	#define pt_element_t u64
	#define guest_walker guest_walkerEPT
	#define FNAME(name) ept_##name
	#define PT_LEVEL_BITS 9
	#define PT_GUEST_DIRTY_SHIFT 9
	#define PT_GUEST_ACCESSED_SHIFT 8
	#define PT_HAVE_ACCESSED_DIRTY(mmu) (!(mmu)->cpu_role.base.ad_disabled)
	#define PT_MAX_FULL_LEVELS PT64_ROOT_MAX_LEVEL
#else
	#error Invalid PTTYPE value
#endif


#define PT_BASE_ADDR_MASK	((pt_element_t)(((1ULL << 52) - 1) & ~(u64)(PAGE_SIZE-1)))
#define PT_LVL_ADDR_MASK(lvl)	__PT_LVL_ADDR_MASK(PT_BASE_ADDR_MASK, lvl, PT_LEVEL_BITS)
#define PT_LVL_OFFSET_MASK(lvl)	__PT_LVL_OFFSET_MASK(PT_BASE_ADDR_MASK, lvl, PT_LEVEL_BITS)
#define PT_INDEX(addr, lvl)	__PT_INDEX(addr, lvl, PT_LEVEL_BITS)

#define PT_GUEST_DIRTY_MASK    (1 << PT_GUEST_DIRTY_SHIFT)
#define PT_GUEST_ACCESSED_MASK (1 << PT_GUEST_ACCESSED_SHIFT)

#define gpte_to_gfn_lvl FNAME(gpte_to_gfn_lvl)
#define gpte_to_gfn(pte) gpte_to_gfn_lvl((pte), PG_LEVEL_4K)


struct guest_walker {
	int level;
	unsigned max_level;
	gfn_t table_gfn[PT_MAX_FULL_LEVELS];
	pt_element_t ptes[PT_MAX_FULL_LEVELS];
	pt_element_t prefetch_ptes[PTE_PREFETCH_NUM];
	gpa_t pte_gpa[PT_MAX_FULL_LEVELS];
	pt_element_t __user *ptep_user[PT_MAX_FULL_LEVELS];
	bool pte_writable[PT_MAX_FULL_LEVELS];
	unsigned int pt_access[PT_MAX_FULL_LEVELS];
	unsigned int pte_access;
	gfn_t gfn;
	struct x86_exception fault;
};

#if PTTYPE == 32
static inline gfn_t pse36_gfn_delta(u32 gpte)
{
	int shift = 32 - PT32_DIR_PSE36_SHIFT - PAGE_SHIFT;

	return (gpte & PT32_DIR_PSE36_MASK) << shift;
}
#endif

static gfn_t gpte_to_gfn_lvl(pt_element_t gpte, int lvl)
{
	return (gpte & PT_LVL_ADDR_MASK(lvl)) >> PAGE_SHIFT;
}

static inline void FNAME(protect_clean_gpte)(struct kvm_mmu *mmu, unsigned *access,
					     unsigned gpte)
{
	unsigned mask;

	
	if (!PT_HAVE_ACCESSED_DIRTY(mmu))
		return;

	BUILD_BUG_ON(PT_WRITABLE_MASK != ACC_WRITE_MASK);

	mask = (unsigned)~ACC_WRITE_MASK;
	
	mask |= (gpte >> (PT_GUEST_DIRTY_SHIFT - PT_WRITABLE_SHIFT)) &
		PT_WRITABLE_MASK;
	*access &= mask;
}

static inline int FNAME(is_present_gpte)(unsigned long pte)
{
#if PTTYPE != PTTYPE_EPT
	return pte & PT_PRESENT_MASK;
#else
	return pte & 7;
#endif
}

static bool FNAME(is_bad_mt_xwr)(struct rsvd_bits_validate *rsvd_check, u64 gpte)
{
#if PTTYPE != PTTYPE_EPT
	return false;
#else
	return __is_bad_mt_xwr(rsvd_check, gpte);
#endif
}

static bool FNAME(is_rsvd_bits_set)(struct kvm_mmu *mmu, u64 gpte, int level)
{
	return __is_rsvd_bits_set(&mmu->guest_rsvd_check, gpte, level) ||
	       FNAME(is_bad_mt_xwr)(&mmu->guest_rsvd_check, gpte);
}

static bool FNAME(prefetch_invalid_gpte)(struct kvm_vcpu *vcpu,
				  struct kvm_mmu_page *sp, u64 *spte,
				  u64 gpte)
{
	if (!FNAME(is_present_gpte)(gpte))
		goto no_present;

	
	if (PT_HAVE_ACCESSED_DIRTY(vcpu->arch.mmu) &&
	    !(gpte & PT_GUEST_ACCESSED_MASK))
		goto no_present;

	if (FNAME(is_rsvd_bits_set)(vcpu->arch.mmu, gpte, PG_LEVEL_4K))
		goto no_present;

	return false;

no_present:
	drop_spte(vcpu->kvm, spte);
	return true;
}


static inline unsigned FNAME(gpte_access)(u64 gpte)
{
	unsigned access;
#if PTTYPE == PTTYPE_EPT
	access = ((gpte & VMX_EPT_WRITABLE_MASK) ? ACC_WRITE_MASK : 0) |
		((gpte & VMX_EPT_EXECUTABLE_MASK) ? ACC_EXEC_MASK : 0) |
		((gpte & VMX_EPT_READABLE_MASK) ? ACC_USER_MASK : 0);
#else
	BUILD_BUG_ON(ACC_EXEC_MASK != PT_PRESENT_MASK);
	BUILD_BUG_ON(ACC_EXEC_MASK != 1);
	access = gpte & (PT_WRITABLE_MASK | PT_USER_MASK | PT_PRESENT_MASK);
	
	access ^= (gpte >> PT64_NX_SHIFT);
#endif

	return access;
}

static int FNAME(update_accessed_dirty_bits)(struct kvm_vcpu *vcpu,
					     struct kvm_mmu *mmu,
					     struct guest_walker *walker,
					     gpa_t addr, int write_fault)
{
	unsigned level, index;
	pt_element_t pte, orig_pte;
	pt_element_t __user *ptep_user;
	gfn_t table_gfn;
	int ret;

	
	if (!PT_HAVE_ACCESSED_DIRTY(mmu))
		return 0;

	for (level = walker->max_level; level >= walker->level; --level) {
		pte = orig_pte = walker->ptes[level - 1];
		table_gfn = walker->table_gfn[level - 1];
		ptep_user = walker->ptep_user[level - 1];
		index = offset_in_page(ptep_user) / sizeof(pt_element_t);
		if (!(pte & PT_GUEST_ACCESSED_MASK)) {
			trace_kvm_mmu_set_accessed_bit(table_gfn, index, sizeof(pte));
			pte |= PT_GUEST_ACCESSED_MASK;
		}
		if (level == walker->level && write_fault &&
				!(pte & PT_GUEST_DIRTY_MASK)) {
			trace_kvm_mmu_set_dirty_bit(table_gfn, index, sizeof(pte));
#if PTTYPE == PTTYPE_EPT
			if (kvm_x86_ops.nested_ops->write_log_dirty(vcpu, addr))
				return -EINVAL;
#endif
			pte |= PT_GUEST_DIRTY_MASK;
		}
		if (pte == orig_pte)
			continue;

		
		if (unlikely(!walker->pte_writable[level - 1]))
			continue;

		ret = __try_cmpxchg_user(ptep_user, &orig_pte, pte, fault);
		if (ret)
			return ret;

		kvm_vcpu_mark_page_dirty(vcpu, table_gfn);
		walker->ptes[level - 1] = pte;
	}
	return 0;
}

static inline unsigned FNAME(gpte_pkeys)(struct kvm_vcpu *vcpu, u64 gpte)
{
	unsigned pkeys = 0;
#if PTTYPE == 64
	pte_t pte = {.pte = gpte};

	pkeys = pte_flags_pkey(pte_flags(pte));
#endif
	return pkeys;
}

static inline bool FNAME(is_last_gpte)(struct kvm_mmu *mmu,
				       unsigned int level, unsigned int gpte)
{
	
#if PTTYPE == 32
	
	gpte &= level - (PT32_ROOT_LEVEL + mmu->cpu_role.ext.cr4_pse);
#endif
	
	gpte |= level - PG_LEVEL_4K - 1;

	return gpte & PT_PAGE_SIZE_MASK;
}

static int FNAME(walk_addr_generic)(struct guest_walker *walker,
				    struct kvm_vcpu *vcpu, struct kvm_mmu *mmu,
				    gpa_t addr, u64 access)
{
	int ret;
	pt_element_t pte;
	pt_element_t __user *ptep_user;
	gfn_t table_gfn;
	u64 pt_access, pte_access;
	unsigned index, accessed_dirty, pte_pkey;
	u64 nested_access;
	gpa_t pte_gpa;
	bool have_ad;
	int offset;
	u64 walk_nx_mask = 0;
	const int write_fault = access & PFERR_WRITE_MASK;
	const int user_fault  = access & PFERR_USER_MASK;
	const int fetch_fault = access & PFERR_FETCH_MASK;
	u16 errcode = 0;
	gpa_t real_gpa;
	gfn_t gfn;

	trace_kvm_mmu_pagetable_walk(addr, access);
retry_walk:
	walker->level = mmu->cpu_role.base.level;
	pte           = kvm_mmu_get_guest_pgd(vcpu, mmu);
	have_ad       = PT_HAVE_ACCESSED_DIRTY(mmu);

#if PTTYPE == 64
	walk_nx_mask = 1ULL << PT64_NX_SHIFT;
	if (walker->level == PT32E_ROOT_LEVEL) {
		pte = mmu->get_pdptr(vcpu, (addr >> 30) & 3);
		trace_kvm_mmu_paging_element(pte, walker->level);
		if (!FNAME(is_present_gpte)(pte))
			goto error;
		--walker->level;
	}
#endif
	walker->max_level = walker->level;

	
	nested_access = (have_ad ? PFERR_WRITE_MASK : 0) | PFERR_USER_MASK;

	pte_access = ~0;

	
	if (KVM_BUG_ON(is_long_mode(vcpu) && !is_pae(vcpu), vcpu->kvm))
		goto error;

	++walker->level;

	do {
		struct kvm_memory_slot *slot;
		unsigned long host_addr;

		pt_access = pte_access;
		--walker->level;

		index = PT_INDEX(addr, walker->level);
		table_gfn = gpte_to_gfn(pte);
		offset    = index * sizeof(pt_element_t);
		pte_gpa   = gfn_to_gpa(table_gfn) + offset;

		BUG_ON(walker->level < 1);
		walker->table_gfn[walker->level - 1] = table_gfn;
		walker->pte_gpa[walker->level - 1] = pte_gpa;

		real_gpa = kvm_translate_gpa(vcpu, mmu, gfn_to_gpa(table_gfn),
					     nested_access, &walker->fault);

		
		if (unlikely(real_gpa == INVALID_GPA))
			return 0;

		slot = kvm_vcpu_gfn_to_memslot(vcpu, gpa_to_gfn(real_gpa));
		if (!kvm_is_visible_memslot(slot))
			goto error;

		host_addr = gfn_to_hva_memslot_prot(slot, gpa_to_gfn(real_gpa),
					    &walker->pte_writable[walker->level - 1]);
		if (unlikely(kvm_is_error_hva(host_addr)))
			goto error;

		ptep_user = (pt_element_t __user *)((void *)host_addr + offset);
		if (unlikely(__get_user(pte, ptep_user)))
			goto error;
		walker->ptep_user[walker->level - 1] = ptep_user;

		trace_kvm_mmu_paging_element(pte, walker->level);

		
		pte_access = pt_access & (pte ^ walk_nx_mask);

		if (unlikely(!FNAME(is_present_gpte)(pte)))
			goto error;

		if (unlikely(FNAME(is_rsvd_bits_set)(mmu, pte, walker->level))) {
			errcode = PFERR_RSVD_MASK | PFERR_PRESENT_MASK;
			goto error;
		}

		walker->ptes[walker->level - 1] = pte;

		
		walker->pt_access[walker->level - 1] = FNAME(gpte_access)(pt_access ^ walk_nx_mask);
	} while (!FNAME(is_last_gpte)(mmu, walker->level, pte));

	pte_pkey = FNAME(gpte_pkeys)(vcpu, pte);
	accessed_dirty = have_ad ? pte_access & PT_GUEST_ACCESSED_MASK : 0;

	
	walker->pte_access = FNAME(gpte_access)(pte_access ^ walk_nx_mask);
	errcode = permission_fault(vcpu, mmu, walker->pte_access, pte_pkey, access);
	if (unlikely(errcode))
		goto error;

	gfn = gpte_to_gfn_lvl(pte, walker->level);
	gfn += (addr & PT_LVL_OFFSET_MASK(walker->level)) >> PAGE_SHIFT;

#if PTTYPE == 32
	if (walker->level > PG_LEVEL_4K && is_cpuid_PSE36())
		gfn += pse36_gfn_delta(pte);
#endif

	real_gpa = kvm_translate_gpa(vcpu, mmu, gfn_to_gpa(gfn), access, &walker->fault);
	if (real_gpa == INVALID_GPA)
		return 0;

	walker->gfn = real_gpa >> PAGE_SHIFT;

	if (!write_fault)
		FNAME(protect_clean_gpte)(mmu, &walker->pte_access, pte);
	else
		
		accessed_dirty &= pte >>
			(PT_GUEST_DIRTY_SHIFT - PT_GUEST_ACCESSED_SHIFT);

	if (unlikely(!accessed_dirty)) {
		ret = FNAME(update_accessed_dirty_bits)(vcpu, mmu, walker,
							addr, write_fault);
		if (unlikely(ret < 0))
			goto error;
		else if (ret)
			goto retry_walk;
	}

	return 1;

error:
	errcode |= write_fault | user_fault;
	if (fetch_fault && (is_efer_nx(mmu) || is_cr4_smep(mmu)))
		errcode |= PFERR_FETCH_MASK;

	walker->fault.vector = PF_VECTOR;
	walker->fault.error_code_valid = true;
	walker->fault.error_code = errcode;

#if PTTYPE == PTTYPE_EPT
	
	if (!(errcode & PFERR_RSVD_MASK)) {
		vcpu->arch.exit_qualification &= (EPT_VIOLATION_GVA_IS_VALID |
						  EPT_VIOLATION_GVA_TRANSLATED);
		if (write_fault)
			vcpu->arch.exit_qualification |= EPT_VIOLATION_ACC_WRITE;
		if (user_fault)
			vcpu->arch.exit_qualification |= EPT_VIOLATION_ACC_READ;
		if (fetch_fault)
			vcpu->arch.exit_qualification |= EPT_VIOLATION_ACC_INSTR;

		
		vcpu->arch.exit_qualification |= (pte_access & VMX_EPT_RWX_MASK) <<
						 EPT_VIOLATION_RWX_SHIFT;
	}
#endif
	walker->fault.address = addr;
	walker->fault.nested_page_fault = mmu != vcpu->arch.walk_mmu;
	walker->fault.async_page_fault = false;

	trace_kvm_mmu_walker_error(walker->fault.error_code);
	return 0;
}

static int FNAME(walk_addr)(struct guest_walker *walker,
			    struct kvm_vcpu *vcpu, gpa_t addr, u64 access)
{
	return FNAME(walk_addr_generic)(walker, vcpu, vcpu->arch.mmu, addr,
					access);
}

static bool
FNAME(prefetch_gpte)(struct kvm_vcpu *vcpu, struct kvm_mmu_page *sp,
		     u64 *spte, pt_element_t gpte)
{
	struct kvm_memory_slot *slot;
	unsigned pte_access;
	gfn_t gfn;
	kvm_pfn_t pfn;

	if (FNAME(prefetch_invalid_gpte)(vcpu, sp, spte, gpte))
		return false;

	gfn = gpte_to_gfn(gpte);
	pte_access = sp->role.access & FNAME(gpte_access)(gpte);
	FNAME(protect_clean_gpte)(vcpu->arch.mmu, &pte_access, gpte);

	slot = gfn_to_memslot_dirty_bitmap(vcpu, gfn, pte_access & ACC_WRITE_MASK);
	if (!slot)
		return false;

	pfn = gfn_to_pfn_memslot_atomic(slot, gfn);
	if (is_error_pfn(pfn))
		return false;

	mmu_set_spte(vcpu, slot, spte, pte_access, gfn, pfn, NULL);
	kvm_release_pfn_clean(pfn);
	return true;
}

static bool FNAME(gpte_changed)(struct kvm_vcpu *vcpu,
				struct guest_walker *gw, int level)
{
	pt_element_t curr_pte;
	gpa_t base_gpa, pte_gpa = gw->pte_gpa[level - 1];
	u64 mask;
	int r, index;

	if (level == PG_LEVEL_4K) {
		mask = PTE_PREFETCH_NUM * sizeof(pt_element_t) - 1;
		base_gpa = pte_gpa & ~mask;
		index = (pte_gpa - base_gpa) / sizeof(pt_element_t);

		r = kvm_vcpu_read_guest_atomic(vcpu, base_gpa,
				gw->prefetch_ptes, sizeof(gw->prefetch_ptes));
		curr_pte = gw->prefetch_ptes[index];
	} else
		r = kvm_vcpu_read_guest_atomic(vcpu, pte_gpa,
				  &curr_pte, sizeof(curr_pte));

	return r || curr_pte != gw->ptes[level - 1];
}

static void FNAME(pte_prefetch)(struct kvm_vcpu *vcpu, struct guest_walker *gw,
				u64 *sptep)
{
	struct kvm_mmu_page *sp;
	pt_element_t *gptep = gw->prefetch_ptes;
	u64 *spte;
	int i;

	sp = sptep_to_sp(sptep);

	if (sp->role.level > PG_LEVEL_4K)
		return;

	
	if (unlikely(vcpu->kvm->mmu_invalidate_in_progress))
		return;

	if (sp->role.direct)
		return __direct_pte_prefetch(vcpu, sp, sptep);

	i = spte_index(sptep) & ~(PTE_PREFETCH_NUM - 1);
	spte = sp->spt + i;

	for (i = 0; i < PTE_PREFETCH_NUM; i++, spte++) {
		if (spte == sptep)
			continue;

		if (is_shadow_present_pte(*spte))
			continue;

		if (!FNAME(prefetch_gpte)(vcpu, sp, spte, gptep[i]))
			break;
	}
}


static int FNAME(fetch)(struct kvm_vcpu *vcpu, struct kvm_page_fault *fault,
			 struct guest_walker *gw)
{
	struct kvm_mmu_page *sp = NULL;
	struct kvm_shadow_walk_iterator it;
	unsigned int direct_access, access;
	int top_level, ret;
	gfn_t base_gfn = fault->gfn;

	WARN_ON_ONCE(gw->gfn != base_gfn);
	direct_access = gw->pte_access;

	top_level = vcpu->arch.mmu->cpu_role.base.level;
	if (top_level == PT32E_ROOT_LEVEL)
		top_level = PT32_ROOT_LEVEL;
	
	if (FNAME(gpte_changed)(vcpu, gw, top_level))
		goto out_gpte_changed;

	if (WARN_ON_ONCE(!VALID_PAGE(vcpu->arch.mmu->root.hpa)))
		goto out_gpte_changed;

	
	if (unlikely(kvm_mmu_is_dummy_root(vcpu->arch.mmu->root.hpa))) {
		kvm_make_request(KVM_REQ_MMU_FREE_OBSOLETE_ROOTS, vcpu);
		goto out_gpte_changed;
	}

	for_each_shadow_entry(vcpu, fault->addr, it) {
		gfn_t table_gfn;

		clear_sp_write_flooding_count(it.sptep);
		if (it.level == gw->level)
			break;

		table_gfn = gw->table_gfn[it.level - 2];
		access = gw->pt_access[it.level - 2];
		sp = kvm_mmu_get_child_sp(vcpu, it.sptep, table_gfn,
					  false, access);

		if (sp != ERR_PTR(-EEXIST)) {
			
			if (sp->unsync_children &&
			    mmu_sync_children(vcpu, sp, false))
				return RET_PF_RETRY;
		}

		
		if (FNAME(gpte_changed)(vcpu, gw, it.level - 1))
			goto out_gpte_changed;

		if (sp != ERR_PTR(-EEXIST))
			link_shadow_page(vcpu, it.sptep, sp);

		if (fault->write && table_gfn == fault->gfn)
			fault->write_fault_to_shadow_pgtable = true;
	}

	
	kvm_mmu_hugepage_adjust(vcpu, fault);

	trace_kvm_mmu_spte_requested(fault);

	for (; shadow_walk_okay(&it); shadow_walk_next(&it)) {
		
		if (fault->nx_huge_page_workaround_enabled)
			disallowed_hugepage_adjust(fault, *it.sptep, it.level);

		base_gfn = gfn_round_for_level(fault->gfn, it.level);
		if (it.level == fault->goal_level)
			break;

		validate_direct_spte(vcpu, it.sptep, direct_access);

		sp = kvm_mmu_get_child_sp(vcpu, it.sptep, base_gfn,
					  true, direct_access);
		if (sp == ERR_PTR(-EEXIST))
			continue;

		link_shadow_page(vcpu, it.sptep, sp);
		if (fault->huge_page_disallowed)
			account_nx_huge_page(vcpu->kvm, sp,
					     fault->req_level >= it.level);
	}

	if (WARN_ON_ONCE(it.level != fault->goal_level))
		return -EFAULT;

	ret = mmu_set_spte(vcpu, fault->slot, it.sptep, gw->pte_access,
			   base_gfn, fault->pfn, fault);
	if (ret == RET_PF_SPURIOUS)
		return ret;

	FNAME(pte_prefetch)(vcpu, gw, it.sptep);
	return ret;

out_gpte_changed:
	return RET_PF_RETRY;
}


static int FNAME(page_fault)(struct kvm_vcpu *vcpu, struct kvm_page_fault *fault)
{
	struct guest_walker walker;
	int r;

	WARN_ON_ONCE(fault->is_tdp);

	
	r = FNAME(walk_addr)(&walker, vcpu, fault->addr,
			     fault->error_code & ~PFERR_RSVD_MASK);

	
	if (!r) {
		if (!fault->prefetch)
			kvm_inject_emulated_page_fault(vcpu, &walker.fault);

		return RET_PF_RETRY;
	}

	fault->gfn = walker.gfn;
	fault->max_level = walker.level;
	fault->slot = kvm_vcpu_gfn_to_memslot(vcpu, fault->gfn);

	if (page_fault_handle_page_track(vcpu, fault)) {
		shadow_page_table_clear_flood(vcpu, fault->addr);
		return RET_PF_EMULATE;
	}

	r = mmu_topup_memory_caches(vcpu, true);
	if (r)
		return r;

	r = kvm_faultin_pfn(vcpu, fault, walker.pte_access);
	if (r != RET_PF_CONTINUE)
		return r;

	
	if (fault->write && !(walker.pte_access & ACC_WRITE_MASK) &&
	    !is_cr0_wp(vcpu->arch.mmu) && !fault->user && fault->slot) {
		walker.pte_access |= ACC_WRITE_MASK;
		walker.pte_access &= ~ACC_USER_MASK;

		
		if (is_cr4_smep(vcpu->arch.mmu))
			walker.pte_access &= ~ACC_EXEC_MASK;
	}

	r = RET_PF_RETRY;
	write_lock(&vcpu->kvm->mmu_lock);

	if (is_page_fault_stale(vcpu, fault))
		goto out_unlock;

	r = make_mmu_pages_available(vcpu);
	if (r)
		goto out_unlock;
	r = FNAME(fetch)(vcpu, fault, &walker);

out_unlock:
	write_unlock(&vcpu->kvm->mmu_lock);
	kvm_release_pfn_clean(fault->pfn);
	return r;
}

static gpa_t FNAME(get_level1_sp_gpa)(struct kvm_mmu_page *sp)
{
	int offset = 0;

	WARN_ON_ONCE(sp->role.level != PG_LEVEL_4K);

	if (PTTYPE == 32)
		offset = sp->role.quadrant << SPTE_LEVEL_BITS;

	return gfn_to_gpa(sp->gfn) + offset * sizeof(pt_element_t);
}


static gpa_t FNAME(gva_to_gpa)(struct kvm_vcpu *vcpu, struct kvm_mmu *mmu,
			       gpa_t addr, u64 access,
			       struct x86_exception *exception)
{
	struct guest_walker walker;
	gpa_t gpa = INVALID_GPA;
	int r;

#ifndef CONFIG_X86_64
	
	WARN_ON_ONCE((addr >> 32) && mmu == vcpu->arch.walk_mmu);
#endif

	r = FNAME(walk_addr_generic)(&walker, vcpu, mmu, addr, access);

	if (r) {
		gpa = gfn_to_gpa(walker.gfn);
		gpa |= addr & ~PAGE_MASK;
	} else if (exception)
		*exception = walker.fault;

	return gpa;
}


static int FNAME(sync_spte)(struct kvm_vcpu *vcpu, struct kvm_mmu_page *sp, int i)
{
	bool host_writable;
	gpa_t first_pte_gpa;
	u64 *sptep, spte;
	struct kvm_memory_slot *slot;
	unsigned pte_access;
	pt_element_t gpte;
	gpa_t pte_gpa;
	gfn_t gfn;

	if (WARN_ON_ONCE(!sp->spt[i]))
		return 0;

	first_pte_gpa = FNAME(get_level1_sp_gpa)(sp);
	pte_gpa = first_pte_gpa + i * sizeof(pt_element_t);

	if (kvm_vcpu_read_guest_atomic(vcpu, pte_gpa, &gpte,
				       sizeof(pt_element_t)))
		return -1;

	if (FNAME(prefetch_invalid_gpte)(vcpu, sp, &sp->spt[i], gpte))
		return 1;

	gfn = gpte_to_gfn(gpte);
	pte_access = sp->role.access;
	pte_access &= FNAME(gpte_access)(gpte);
	FNAME(protect_clean_gpte)(vcpu->arch.mmu, &pte_access, gpte);

	if (sync_mmio_spte(vcpu, &sp->spt[i], gfn, pte_access))
		return 0;

	
	if ((!pte_access && !shadow_present_mask) ||
	    gfn != kvm_mmu_page_get_gfn(sp, i)) {
		drop_spte(vcpu->kvm, &sp->spt[i]);
		return 1;
	}
	
	if (kvm_mmu_page_get_access(sp, i) == pte_access)
		return 0;

	
	kvm_mmu_page_set_access(sp, i, pte_access);

	sptep = &sp->spt[i];
	spte = *sptep;
	host_writable = spte & shadow_host_writable_mask;
	slot = kvm_vcpu_gfn_to_memslot(vcpu, gfn);
	make_spte(vcpu, sp, slot, pte_access, gfn,
		  spte_to_pfn(spte), spte, true, false,
		  host_writable, &spte);

	return mmu_spte_update(sptep, spte);
}

#undef pt_element_t
#undef guest_walker
#undef FNAME
#undef PT_BASE_ADDR_MASK
#undef PT_INDEX
#undef PT_LVL_ADDR_MASK
#undef PT_LVL_OFFSET_MASK
#undef PT_LEVEL_BITS
#undef PT_MAX_FULL_LEVELS
#undef gpte_to_gfn
#undef gpte_to_gfn_lvl
#undef PT_GUEST_ACCESSED_MASK
#undef PT_GUEST_DIRTY_MASK
#undef PT_GUEST_DIRTY_SHIFT
#undef PT_GUEST_ACCESSED_SHIFT
#undef PT_HAVE_ACCESSED_DIRTY
