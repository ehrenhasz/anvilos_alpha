#include <sys/zfs_context.h>
#include <sys/zio_checksum.h>
#include <sys/zfs_impl.h>
#define	IMPL_FASTEST	(UINT32_MAX)
#define	IMPL_CYCLE	(UINT32_MAX - 1)
#define	IMPL_READ(i)	(*(volatile uint32_t *) &(i))
static IMPL_OPS_T generic_fastest_impl = {
	.name = "fastest"
};
static const IMPL_OPS_T *generic_supp_impls[ARRAY_SIZE(IMPL_ARRAY)];
static uint32_t generic_supp_impls_cnt = 0;
static uint32_t generic_impl_chosen = IMPL_FASTEST;
static struct generic_impl_selector {
	const char *name;
	uint32_t sel;
} generic_impl_selectors[] = {
	{ "cycle",	IMPL_CYCLE },
	{ "fastest",	IMPL_FASTEST }
};
static void
generic_impl_init(void)
{
	int i, c;
	if (likely(generic_supp_impls_cnt != 0))
		return;
	for (i = 0, c = 0; i < ARRAY_SIZE(IMPL_ARRAY); i++) {
		const IMPL_OPS_T *impl = IMPL_ARRAY[i];
		if (impl->is_supported && impl->is_supported())
			generic_supp_impls[c++] = impl;
	}
	generic_supp_impls_cnt = c;
	memcpy(&generic_fastest_impl, generic_supp_impls[0],
	    sizeof (generic_fastest_impl));
}
static uint32_t
generic_impl_getcnt(void)
{
	generic_impl_init();
	return (generic_supp_impls_cnt);
}
static uint32_t
generic_impl_getid(void)
{
	generic_impl_init();
	return (IMPL_READ(generic_impl_chosen));
}
static const char *
generic_impl_getname(void)
{
	uint32_t impl = IMPL_READ(generic_impl_chosen);
	generic_impl_init();
	switch (impl) {
	case IMPL_FASTEST:
		return ("fastest");
	case IMPL_CYCLE:
		return ("cycle");
	default:
		return (generic_supp_impls[impl]->name);
	}
}
static void
generic_impl_setid(uint32_t id)
{
	generic_impl_init();
	switch (id) {
	case IMPL_FASTEST:
		atomic_swap_32(&generic_impl_chosen, IMPL_FASTEST);
		break;
	case IMPL_CYCLE:
		atomic_swap_32(&generic_impl_chosen, IMPL_CYCLE);
		break;
	default:
		ASSERT3U(id, <, generic_supp_impls_cnt);
		atomic_swap_32(&generic_impl_chosen, id);
		break;
	}
}
static int
generic_impl_setname(const char *val)
{
	uint32_t impl = IMPL_READ(generic_impl_chosen);
	size_t val_len;
	int i, err = -EINVAL;
	generic_impl_init();
	val_len = strlen(val);
	while ((val_len > 0) && !!isspace(val[val_len-1]))  
		val_len--;
	for (i = 0; i < ARRAY_SIZE(generic_impl_selectors); i++) {
		const char *name = generic_impl_selectors[i].name;
		if (val_len == strlen(name) &&
		    strncmp(val, name, val_len) == 0) {
			impl = generic_impl_selectors[i].sel;
			err = 0;
			break;
		}
	}
	if (err != 0) {
		for (i = 0; i < generic_supp_impls_cnt; i++) {
			const char *name = generic_supp_impls[i]->name;
			if (val_len == strlen(name) &&
			    strncmp(val, name, val_len) == 0) {
				impl = i;
				err = 0;
				break;
			}
		}
	}
	if (err == 0) {
		atomic_swap_32(&generic_impl_chosen, impl);
	}
	return (err);
}
static void
generic_impl_set_fastest(uint32_t id)
{
	generic_impl_init();
	memcpy(&generic_fastest_impl, generic_supp_impls[id],
	    sizeof (generic_fastest_impl));
}
const zfs_impl_t ZFS_IMPL_OPS = {
	.name = IMPL_NAME,
	.getcnt = generic_impl_getcnt,
	.getid = generic_impl_getid,
	.getname = generic_impl_getname,
	.set_fastest = generic_impl_set_fastest,
	.setid = generic_impl_setid,
	.setname = generic_impl_setname
};
const IMPL_OPS_T *
IMPL_GET_OPS(void)
{
	const IMPL_OPS_T *ops = NULL;
	uint32_t idx, impl = IMPL_READ(generic_impl_chosen);
	static uint32_t cycle_count = 0;
	generic_impl_init();
	switch (impl) {
	case IMPL_FASTEST:
		ops = &generic_fastest_impl;
		break;
	case IMPL_CYCLE:
		idx = (++cycle_count) % generic_supp_impls_cnt;
		ops = generic_supp_impls[idx];
		break;
	default:
		ASSERT3U(impl, <, generic_supp_impls_cnt);
		ops = generic_supp_impls[impl];
		break;
	}
	ASSERT3P(ops, !=, NULL);
	return (ops);
}
