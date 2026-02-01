
#include <linux/slab.h>

enum slab_state slab_state;

bool slab_is_available(void)
{
	return slab_state >= UP;
}
