
 

#include <linux/mm.h>
#include <linux/module.h>
#include "trans_common.h"

 
void p9_release_pages(struct page **pages, int nr_pages)
{
	int i;

	for (i = 0; i < nr_pages; i++)
		if (pages[i])
			put_page(pages[i]);
}
EXPORT_SYMBOL(p9_release_pages);
