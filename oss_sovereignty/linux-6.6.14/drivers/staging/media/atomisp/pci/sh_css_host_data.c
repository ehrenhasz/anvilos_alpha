
 

#include <linux/slab.h>
#include <ia_css_host_data.h>
#include <sh_css_internal.h>

struct ia_css_host_data *ia_css_host_data_allocate(size_t size)
{
	struct ia_css_host_data *me;

	me =  kmalloc(sizeof(struct ia_css_host_data), GFP_KERNEL);
	if (!me)
		return NULL;
	me->size = (uint32_t)size;
	me->address = kvmalloc(size, GFP_KERNEL);
	if (!me->address) {
		kfree(me);
		return NULL;
	}
	return me;
}

void ia_css_host_data_free(struct ia_css_host_data *me)
{
	if (me) {
		kvfree(me->address);
		me->address = NULL;
		kfree(me);
	}
}
