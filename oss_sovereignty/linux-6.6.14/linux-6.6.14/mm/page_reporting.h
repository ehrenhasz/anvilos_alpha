#ifndef _MM_PAGE_REPORTING_H
#define _MM_PAGE_REPORTING_H
#include <linux/mmzone.h>
#include <linux/pageblock-flags.h>
#include <linux/page-isolation.h>
#include <linux/jump_label.h>
#include <linux/slab.h>
#include <linux/pgtable.h>
#include <linux/scatterlist.h>
#ifdef CONFIG_PAGE_REPORTING
DECLARE_STATIC_KEY_FALSE(page_reporting_enabled);
extern unsigned int page_reporting_order;
void __page_reporting_notify(void);
static inline bool page_reported(struct page *page)
{
	return static_branch_unlikely(&page_reporting_enabled) &&
	       PageReported(page);
}
static inline void page_reporting_notify_free(unsigned int order)
{
	if (!static_branch_unlikely(&page_reporting_enabled))
		return;
	if (order < page_reporting_order)
		return;
	__page_reporting_notify();
}
#else  
#define page_reported(_page)	false
static inline void page_reporting_notify_free(unsigned int order)
{
}
#endif  
#endif  
