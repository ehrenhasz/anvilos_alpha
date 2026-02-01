

#include <linux/slab.h>

 
void **__memcat_p(void **a, void **b)
{
	void **p = a, **new;
	int nr;

	 
	for (nr = 0, p = a; *p; nr++, p++)
		;
	for (p = b; *p; nr++, p++)
		;
	 
	nr++;

	new = kmalloc_array(nr, sizeof(void *), GFP_KERNEL);
	if (!new)
		return NULL;

	 
	for (nr--; nr >= 0; nr--, p = p == b ? &a[nr] : p - 1)
		new[nr] = *p;

	return new;
}
EXPORT_SYMBOL_GPL(__memcat_p);

