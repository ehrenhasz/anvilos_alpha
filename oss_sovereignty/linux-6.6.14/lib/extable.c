
 

#include <linux/bsearch.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sort.h>
#include <linux/uaccess.h>
#include <linux/extable.h>

#ifndef ARCH_HAS_RELATIVE_EXTABLE
#define ex_to_insn(x)	((x)->insn)
#else
static inline unsigned long ex_to_insn(const struct exception_table_entry *x)
{
	return (unsigned long)&x->insn + x->insn;
}
#endif

#ifndef ARCH_HAS_RELATIVE_EXTABLE
#define swap_ex		NULL
#else
static void swap_ex(void *a, void *b, int size)
{
	struct exception_table_entry *x = a, *y = b, tmp;
	int delta = b - a;

	tmp = *x;
	x->insn = y->insn + delta;
	y->insn = tmp.insn - delta;

#ifdef swap_ex_entry_fixup
	swap_ex_entry_fixup(x, y, tmp, delta);
#else
	x->fixup = y->fixup + delta;
	y->fixup = tmp.fixup - delta;
#endif
}
#endif  

 
static int cmp_ex_sort(const void *a, const void *b)
{
	const struct exception_table_entry *x = a, *y = b;

	 
	if (ex_to_insn(x) > ex_to_insn(y))
		return 1;
	if (ex_to_insn(x) < ex_to_insn(y))
		return -1;
	return 0;
}

void sort_extable(struct exception_table_entry *start,
		  struct exception_table_entry *finish)
{
	sort(start, finish - start, sizeof(struct exception_table_entry),
	     cmp_ex_sort, swap_ex);
}

#ifdef CONFIG_MODULES
 
void trim_init_extable(struct module *m)
{
	 
	while (m->num_exentries &&
	       within_module_init(ex_to_insn(&m->extable[0]), m)) {
		m->extable++;
		m->num_exentries--;
	}
	 
	while (m->num_exentries &&
	       within_module_init(ex_to_insn(&m->extable[m->num_exentries - 1]),
				  m))
		m->num_exentries--;
}
#endif  

static int cmp_ex_search(const void *key, const void *elt)
{
	const struct exception_table_entry *_elt = elt;
	unsigned long _key = *(unsigned long *)key;

	 
	if (_key > ex_to_insn(_elt))
		return 1;
	if (_key < ex_to_insn(_elt))
		return -1;
	return 0;
}

 
const struct exception_table_entry *
search_extable(const struct exception_table_entry *base,
	       const size_t num,
	       unsigned long value)
{
	return bsearch(&value, base, num,
		       sizeof(struct exception_table_entry), cmp_ex_search);
}
