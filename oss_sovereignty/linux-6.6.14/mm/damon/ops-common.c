
 

#include <linux/mmu_notifier.h>
#include <linux/page_idle.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>

#include "ops-common.h"

 
struct folio *damon_get_folio(unsigned long pfn)
{
	struct page *page = pfn_to_online_page(pfn);
	struct folio *folio;

	if (!page || PageTail(page))
		return NULL;

	folio = page_folio(page);
	if (!folio_test_lru(folio) || !folio_try_get(folio))
		return NULL;
	if (unlikely(page_folio(page) != folio || !folio_test_lru(folio))) {
		folio_put(folio);
		folio = NULL;
	}
	return folio;
}

void damon_ptep_mkold(pte_t *pte, struct vm_area_struct *vma, unsigned long addr)
{
	struct folio *folio = damon_get_folio(pte_pfn(ptep_get(pte)));

	if (!folio)
		return;

	if (ptep_clear_young_notify(vma, addr, pte))
		folio_set_young(folio);

	folio_set_idle(folio);
	folio_put(folio);
}

void damon_pmdp_mkold(pmd_t *pmd, struct vm_area_struct *vma, unsigned long addr)
{
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	struct folio *folio = damon_get_folio(pmd_pfn(pmdp_get(pmd)));

	if (!folio)
		return;

	if (pmdp_clear_young_notify(vma, addr, pmd))
		folio_set_young(folio);

	folio_set_idle(folio);
	folio_put(folio);
#endif  
}

#define DAMON_MAX_SUBSCORE	(100)
#define DAMON_MAX_AGE_IN_LOG	(32)

int damon_hot_score(struct damon_ctx *c, struct damon_region *r,
			struct damos *s)
{
	int freq_subscore;
	unsigned int age_in_sec;
	int age_in_log, age_subscore;
	unsigned int freq_weight = s->quota.weight_nr_accesses;
	unsigned int age_weight = s->quota.weight_age;
	int hotness;

	freq_subscore = r->nr_accesses * DAMON_MAX_SUBSCORE /
		damon_max_nr_accesses(&c->attrs);

	age_in_sec = (unsigned long)r->age * c->attrs.aggr_interval / 1000000;
	for (age_in_log = 0; age_in_log < DAMON_MAX_AGE_IN_LOG && age_in_sec;
			age_in_log++, age_in_sec >>= 1)
		;

	 
	if (freq_subscore == 0)
		age_in_log *= -1;

	 
	age_in_log += DAMON_MAX_AGE_IN_LOG;
	age_subscore = age_in_log * DAMON_MAX_SUBSCORE /
		DAMON_MAX_AGE_IN_LOG / 2;

	hotness = (freq_weight * freq_subscore + age_weight * age_subscore);
	if (freq_weight + age_weight)
		hotness /= freq_weight + age_weight;
	 
	hotness = hotness * DAMOS_MAX_SCORE / DAMON_MAX_SUBSCORE;

	return hotness;
}

int damon_cold_score(struct damon_ctx *c, struct damon_region *r,
			struct damos *s)
{
	int hotness = damon_hot_score(c, r, s);

	 
	return DAMOS_MAX_SCORE - hotness;
}
