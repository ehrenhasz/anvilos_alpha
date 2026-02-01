
 

#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/io.h>

 
int devmem_is_allowed(unsigned long pfn)
{
	if (iomem_is_exclusive(PFN_PHYS(pfn)))
		return 0;
	if (!page_is_ram(pfn))
		return 1;
	return 0;
}
