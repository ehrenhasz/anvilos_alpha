

#include "lkdtm.h"
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/mmu.h>

 
static void insert_slb_entry(unsigned long p, int ssize, int page_size)
{
	unsigned long flags;

	flags = SLB_VSID_KERNEL | mmu_psize_defs[page_size].sllp;
	preempt_disable();

	asm volatile("slbmte %0,%1" :
		     : "r" (mk_vsid_data(p, ssize, flags)),
		       "r" (mk_esid_data(p, ssize, SLB_NUM_BOLTED))
		     : "memory");

	asm volatile("slbmte %0,%1" :
			: "r" (mk_vsid_data(p, ssize, flags)),
			  "r" (mk_esid_data(p, ssize, SLB_NUM_BOLTED + 1))
			: "memory");
	preempt_enable();
}

 
static int inject_vmalloc_slb_multihit(void)
{
	char *p;

	p = vmalloc(PAGE_SIZE);
	if (!p)
		return -ENOMEM;

	insert_slb_entry((unsigned long)p, MMU_SEGSIZE_1T, mmu_vmalloc_psize);
	 
	p[0] = '!';
	vfree(p);
	return 0;
}

 
static int inject_kmalloc_slb_multihit(void)
{
	char *p;

	p = kmalloc(2048, GFP_KERNEL);
	if (!p)
		return -ENOMEM;

	insert_slb_entry((unsigned long)p, MMU_SEGSIZE_1T, mmu_linear_psize);
	 
	p[0] = '!';
	kfree(p);
	return 0;
}

 
static void insert_dup_slb_entry_0(void)
{
	unsigned long test_address = PAGE_OFFSET, *test_ptr;
	unsigned long esid, vsid;
	unsigned long i = 0;

	test_ptr = (unsigned long *)test_address;
	preempt_disable();

	asm volatile("slbmfee  %0,%1" : "=r" (esid) : "r" (i));
	asm volatile("slbmfev  %0,%1" : "=r" (vsid) : "r" (i));

	 
	asm volatile("slbmte %0,%1" :
			: "r" (vsid),
			  "r" (esid | SLB_NUM_BOLTED)
			: "memory");

	asm volatile("slbmfee  %0,%1" : "=r" (esid) : "r" (i));
	asm volatile("slbmfev  %0,%1" : "=r" (vsid) : "r" (i));

	 
	asm volatile("slbmte %0,%1" :
			: "r" (vsid),
			  "r" (esid | (SLB_NUM_BOLTED + 1))
			: "memory");

	pr_info("%s accessing test address 0x%lx: 0x%lx\n",
		__func__, test_address, *test_ptr);

	preempt_enable();
}

static void lkdtm_PPC_SLB_MULTIHIT(void)
{
	if (!radix_enabled()) {
		pr_info("Injecting SLB multihit errors\n");
		 
		inject_vmalloc_slb_multihit();
		inject_kmalloc_slb_multihit();
		insert_dup_slb_entry_0();
		pr_info("Recovered from SLB multihit errors\n");
	} else {
		pr_err("XFAIL: This test is for ppc64 and with hash mode MMU only\n");
	}
}

static struct crashtype crashtypes[] = {
	CRASHTYPE(PPC_SLB_MULTIHIT),
};

struct crashtype_category powerpc_crashtypes = {
	.crashtypes = crashtypes,
	.len	    = ARRAY_SIZE(crashtypes),
};
