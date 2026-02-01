
 

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bsearch.h>
#include <linux/sort.h>
#include <linux/comedi.h>

#include "ni_routes.h"
#include "ni_routing/ni_route_values.h"
#include "ni_routing/ni_device_routes.h"

 

 

 
#define RVi(table, src, dest)	((table)[(dest) * NI_NUM_NAMES + (src)])

 
static const u8 *ni_find_route_values(const char *device_family)
{
	const u8 *rv = NULL;
	int i;

	for (i = 0; ni_all_route_values[i]; ++i) {
		if (!strcmp(ni_all_route_values[i]->family, device_family)) {
			rv = &ni_all_route_values[i]->register_values[0][0];
			break;
		}
	}
	return rv;
}

 
static const struct ni_device_routes *
ni_find_valid_routes(const char *board_name)
{
	const struct ni_device_routes *dr = NULL;
	int i;

	for (i = 0; ni_device_routes_list[i]; ++i) {
		if (!strcmp(ni_device_routes_list[i]->device, board_name)) {
			dr = ni_device_routes_list[i];
			break;
		}
	}
	return dr;
}

 
static int ni_find_device_routes(const char *device_family,
				 const char *board_name,
				 const char *alt_board_name,
				 struct ni_route_tables *tables)
{
	const struct ni_device_routes *dr;
	const u8 *rv;

	 
	rv = ni_find_route_values(device_family);

	 
	dr = ni_find_valid_routes(board_name);
	if (!dr && alt_board_name)
		dr = ni_find_valid_routes(alt_board_name);

	tables->route_values = rv;
	tables->valid_routes = dr;

	if (!rv || !dr)
		return -ENODATA;

	return 0;
}

 
int ni_assign_device_routes(const char *device_family,
			    const char *board_name,
			    const char *alt_board_name,
			    struct ni_route_tables *tables)
{
	memset(tables, 0, sizeof(struct ni_route_tables));
	return ni_find_device_routes(device_family, board_name, alt_board_name,
				     tables);
}
EXPORT_SYMBOL_GPL(ni_assign_device_routes);

 
unsigned int ni_count_valid_routes(const struct ni_route_tables *tables)
{
	int total = 0;
	int i;

	for (i = 0; i < tables->valid_routes->n_route_sets; ++i) {
		const struct ni_route_set *R = &tables->valid_routes->routes[i];
		int j;

		for (j = 0; j < R->n_src; ++j) {
			const int src  = R->src[j];
			const int dest = R->dest;
			const u8 *rv = tables->route_values;

			if (RVi(rv, B(src), B(dest)))
				 
				++total;
			else if (channel_is_rtsi(dest) &&
				 (RVi(rv, B(src), B(NI_RGOUT0)) ||
				  RVi(rv, B(src), B(NI_RTSI_BRD(0))) ||
				  RVi(rv, B(src), B(NI_RTSI_BRD(1))) ||
				  RVi(rv, B(src), B(NI_RTSI_BRD(2))) ||
				  RVi(rv, B(src), B(NI_RTSI_BRD(3))))) {
				++total;
			}
		}
	}
	return total;
}
EXPORT_SYMBOL_GPL(ni_count_valid_routes);

 
unsigned int ni_get_valid_routes(const struct ni_route_tables *tables,
				 unsigned int n_pairs,
				 unsigned int *pair_data)
{
	unsigned int n_valid = ni_count_valid_routes(tables);
	int i;

	if (n_pairs == 0 || n_valid == 0)
		return n_valid;

	if (!pair_data)
		return 0;

	n_valid = 0;

	for (i = 0; i < tables->valid_routes->n_route_sets; ++i) {
		const struct ni_route_set *R = &tables->valid_routes->routes[i];
		int j;

		for (j = 0; j < R->n_src; ++j) {
			const int src  = R->src[j];
			const int dest = R->dest;
			bool valid = false;
			const u8 *rv = tables->route_values;

			if (RVi(rv, B(src), B(dest)))
				 
				valid = true;
			else if (channel_is_rtsi(dest) &&
				 (RVi(rv, B(src), B(NI_RGOUT0)) ||
				  RVi(rv, B(src), B(NI_RTSI_BRD(0))) ||
				  RVi(rv, B(src), B(NI_RTSI_BRD(1))) ||
				  RVi(rv, B(src), B(NI_RTSI_BRD(2))) ||
				  RVi(rv, B(src), B(NI_RTSI_BRD(3))))) {
				 
				valid = true;
			}

			if (valid) {
				pair_data[2 * n_valid] = src;
				pair_data[2 * n_valid + 1] = dest;
				++n_valid;
			}

			if (n_valid >= n_pairs)
				return n_valid;
		}
	}
	return n_valid;
}
EXPORT_SYMBOL_GPL(ni_get_valid_routes);

 
static const int NI_CMD_DESTS[] = {
	NI_AI_SampleClock,
	NI_AI_StartTrigger,
	NI_AI_ConvertClock,
	NI_AO_SampleClock,
	NI_AO_StartTrigger,
	NI_DI_SampleClock,
	NI_DO_SampleClock,
};

 
bool ni_is_cmd_dest(int dest)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(NI_CMD_DESTS); ++i)
		if (NI_CMD_DESTS[i] == dest)
			return true;
	return false;
}
EXPORT_SYMBOL_GPL(ni_is_cmd_dest);

 
static int _ni_sort_destcmp(const void *va, const void *vb)
{
	const struct ni_route_set *a = va;
	const struct ni_route_set *b = vb;

	if (a->dest < b->dest)
		return -1;
	else if (a->dest > b->dest)
		return 1;
	return 0;
}

static int _ni_sort_srccmp(const void *vsrc0, const void *vsrc1)
{
	const int *src0 = vsrc0;
	const int *src1 = vsrc1;

	if (*src0 < *src1)
		return -1;
	else if (*src0 > *src1)
		return 1;
	return 0;
}

 
void ni_sort_device_routes(struct ni_device_routes *valid_routes)
{
	unsigned int n;

	 
	valid_routes->n_route_sets = 0;
	while (valid_routes->routes[valid_routes->n_route_sets].dest != 0)
		++valid_routes->n_route_sets;

	 
	sort(valid_routes->routes, valid_routes->n_route_sets,
	     sizeof(struct ni_route_set), _ni_sort_destcmp, NULL);

	 
	for (n = 0; n < valid_routes->n_route_sets; ++n) {
		struct ni_route_set *rs = &valid_routes->routes[n];

		 
		rs->n_src = 0;
		while (rs->src[rs->n_src])
			++rs->n_src;

		 
		sort(valid_routes->routes[n].src, valid_routes->routes[n].n_src,
		     sizeof(int), _ni_sort_srccmp, NULL);
	}
}
EXPORT_SYMBOL_GPL(ni_sort_device_routes);

 
static void ni_sort_all_device_routes(void)
{
	unsigned int i;

	for (i = 0; ni_device_routes_list[i]; ++i)
		ni_sort_device_routes(ni_device_routes_list[i]);
}

 
static int _ni_bsearch_destcmp(const void *vkey, const void *velt)
{
	const int *key = vkey;
	const struct ni_route_set *elt = velt;

	if (*key < elt->dest)
		return -1;
	else if (*key > elt->dest)
		return 1;
	return 0;
}

static int _ni_bsearch_srccmp(const void *vkey, const void *velt)
{
	const int *key = vkey;
	const int *elt = velt;

	if (*key < *elt)
		return -1;
	else if (*key > *elt)
		return 1;
	return 0;
}

 
const struct ni_route_set *
ni_find_route_set(const int destination,
		  const struct ni_device_routes *valid_routes)
{
	return bsearch(&destination, valid_routes->routes,
		       valid_routes->n_route_sets, sizeof(struct ni_route_set),
		       _ni_bsearch_destcmp);
}
EXPORT_SYMBOL_GPL(ni_find_route_set);

 
bool ni_route_set_has_source(const struct ni_route_set *routes,
			     const int source)
{
	if (!bsearch(&source, routes->src, routes->n_src, sizeof(int),
		     _ni_bsearch_srccmp))
		return false;
	return true;
}
EXPORT_SYMBOL_GPL(ni_route_set_has_source);

 
s8 ni_lookup_route_register(int src, int dest,
			    const struct ni_route_tables *tables)
{
	s8 regval;

	 
	src = B(src);
	dest = B(dest);
	if (src < 0 || src >= NI_NUM_NAMES || dest < 0 || dest >= NI_NUM_NAMES)
		return -EINVAL;
	regval = RVi(tables->route_values, src, dest);
	if (!regval)
		return -EINVAL;
	 
	return UNMARK(regval);
}
EXPORT_SYMBOL_GPL(ni_lookup_route_register);

 
s8 ni_route_to_register(const int src, const int dest,
			const struct ni_route_tables *tables)
{
	const struct ni_route_set *routes =
		ni_find_route_set(dest, tables->valid_routes);
	const u8 *rv;
	s8 regval;

	 
	if (!routes)
		return -1;
	 
	if (!ni_route_set_has_source(routes, src))
		return -1;
	 
	rv = tables->route_values;
	regval = RVi(rv, B(src), B(dest));

	 
	if (!regval && channel_is_rtsi(dest)) {
		regval = RVi(rv, B(src), B(NI_RGOUT0));
		if (!regval && (RVi(rv, B(src), B(NI_RTSI_BRD(0))) ||
				RVi(rv, B(src), B(NI_RTSI_BRD(1))) ||
				RVi(rv, B(src), B(NI_RTSI_BRD(2))) ||
				RVi(rv, B(src), B(NI_RTSI_BRD(3)))))
			regval = BIT(6);
	}

	if (!regval)
		return -1;
	 
	return UNMARK(regval);
}
EXPORT_SYMBOL_GPL(ni_route_to_register);

 
int ni_find_route_source(const u8 src_sel_reg_value, int dest,
			 const struct ni_route_tables *tables)
{
	int src;

	if (!tables->route_values)
		return -EINVAL;

	dest = B(dest);  
	 
	if (dest < 0 || dest >= NI_NUM_NAMES)
		return -EINVAL;
	for (src = 0; src < NI_NUM_NAMES; ++src)
		if (RVi(tables->route_values, src, dest) ==
		    V(src_sel_reg_value))
			return src + NI_NAMES_BASE;
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(ni_find_route_source);

 

 
static int __init ni_routes_module_init(void)
{
	ni_sort_all_device_routes();
	return 0;
}

static void __exit ni_routes_module_exit(void)
{
}

module_init(ni_routes_module_init);
module_exit(ni_routes_module_exit);

MODULE_AUTHOR("Comedi https://www.comedi.org");
MODULE_DESCRIPTION("Comedi helper for routing signals-->terminals for NI");
MODULE_LICENSE("GPL");
 
