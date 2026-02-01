

 

#include <asm/xen/hypercall.h>

#include <xen/interface/memory.h>
#include <xen/mem-reservation.h>
#include <linux/moduleparam.h>

bool __read_mostly xen_scrub_pages = IS_ENABLED(CONFIG_XEN_SCRUB_PAGES_DEFAULT);
core_param(xen_scrub_pages, xen_scrub_pages, bool, 0);

 
#define EXTENT_ORDER (fls(XEN_PFN_PER_PAGE) - 1)

#ifdef CONFIG_XEN_HAVE_PVMMU
void __xenmem_reservation_va_mapping_update(unsigned long count,
					    struct page **pages,
					    xen_pfn_t *frames)
{
	int i;

	for (i = 0; i < count; i++) {
		struct page *page = pages[i];
		unsigned long pfn = page_to_pfn(page);
		int ret;

		BUG_ON(!page);

		 
		BUILD_BUG_ON(XEN_PAGE_SIZE != PAGE_SIZE);

		set_phys_to_machine(pfn, frames[i]);

		ret = HYPERVISOR_update_va_mapping(
				(unsigned long)__va(pfn << PAGE_SHIFT),
				mfn_pte(frames[i], PAGE_KERNEL), 0);
		BUG_ON(ret);
	}
}
EXPORT_SYMBOL_GPL(__xenmem_reservation_va_mapping_update);

void __xenmem_reservation_va_mapping_reset(unsigned long count,
					   struct page **pages)
{
	int i;

	for (i = 0; i < count; i++) {
		struct page *page = pages[i];
		unsigned long pfn = page_to_pfn(page);
		int ret;

		 
		BUILD_BUG_ON(XEN_PAGE_SIZE != PAGE_SIZE);

		ret = HYPERVISOR_update_va_mapping(
				(unsigned long)__va(pfn << PAGE_SHIFT),
				__pte_ma(0), 0);
		BUG_ON(ret);

		__set_phys_to_machine(pfn, INVALID_P2M_ENTRY);
	}
}
EXPORT_SYMBOL_GPL(__xenmem_reservation_va_mapping_reset);
#endif  

 
int xenmem_reservation_increase(int count, xen_pfn_t *frames)
{
	struct xen_memory_reservation reservation = {
		.address_bits = 0,
		.extent_order = EXTENT_ORDER,
		.domid        = DOMID_SELF
	};

	 
	set_xen_guest_handle(reservation.extent_start, frames);
	reservation.nr_extents = count;
	return HYPERVISOR_memory_op(XENMEM_populate_physmap, &reservation);
}
EXPORT_SYMBOL_GPL(xenmem_reservation_increase);

 
int xenmem_reservation_decrease(int count, xen_pfn_t *frames)
{
	struct xen_memory_reservation reservation = {
		.address_bits = 0,
		.extent_order = EXTENT_ORDER,
		.domid        = DOMID_SELF
	};

	 
	set_xen_guest_handle(reservation.extent_start, frames);
	reservation.nr_extents = count;
	return HYPERVISOR_memory_op(XENMEM_decrease_reservation, &reservation);
}
EXPORT_SYMBOL_GPL(xenmem_reservation_decrease);
