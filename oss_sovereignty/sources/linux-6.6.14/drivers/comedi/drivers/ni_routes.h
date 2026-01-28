


#ifndef _COMEDI_DRIVERS_NI_ROUTES_H
#define _COMEDI_DRIVERS_NI_ROUTES_H

#include <linux/types.h>
#include <linux/errno.h>

#ifndef NI_ROUTE_VALUE_EXTERNAL_CONVERSION
#include <linux/bitops.h>
#endif

#include <linux/comedi.h>


struct ni_route_set {
	int dest;
	int n_src;
	int *src;
};


struct ni_device_routes {
	const char *device;
	int n_route_sets;
	struct ni_route_set *routes;
};


struct ni_route_tables {
	const struct ni_device_routes *valid_routes;
	const u8 *route_values;
};


int ni_assign_device_routes(const char *device_family,
			    const char *board_name,
			    const char *alt_board_name,
			    struct ni_route_tables *tables);


const struct ni_route_set *
ni_find_route_set(const int destination,
		  const struct ni_device_routes *valid_routes);


bool ni_route_set_has_source(const struct ni_route_set *routes, const int src);


s8 ni_route_to_register(const int src, const int dest,
			const struct ni_route_tables *tables);

static inline bool ni_rtsi_route_requires_mux(s8 value)
{
	return value & BIT(6);
}


s8 ni_lookup_route_register(int src, int dest,
			    const struct ni_route_tables *tables);


static inline bool route_is_valid(const int src, const int dest,
				  const struct ni_route_tables *tables)
{
	return ni_route_to_register(src, dest, tables) >= 0;
}


bool ni_is_cmd_dest(int dest);

static inline bool channel_is_pfi(int channel)
{
	return NI_PFI(0) <= channel && channel <= NI_PFI(-1);
}

static inline bool channel_is_rtsi(int channel)
{
	return TRIGGER_LINE(0) <= channel && channel <= TRIGGER_LINE(-1);
}

static inline bool channel_is_ctr(int channel)
{
	return channel >= NI_COUNTER_NAMES_BASE &&
	       channel <= NI_COUNTER_NAMES_MAX;
}


unsigned int ni_count_valid_routes(const struct ni_route_tables *tables);


unsigned int ni_get_valid_routes(const struct ni_route_tables *tables,
				 unsigned int n_pairs,
				 unsigned int *pair_data);


void ni_sort_device_routes(struct ni_device_routes *valid_routes);


int ni_find_route_source(const u8 src_sel_reg_value, const int dest,
			 const struct ni_route_tables *tables);


static inline bool route_register_is_valid(const u8 src_sel_reg_value,
					   const int dest,
					   const struct ni_route_tables *tables)
{
	return ni_find_route_source(src_sel_reg_value, dest, tables) >= 0;
}


static inline s8 ni_get_reg_value_roffs(int src, const int dest,
					const struct ni_route_tables *tables,
					const int direct_reg_offset)
{
	if (src < NI_NAMES_BASE) {
		src += direct_reg_offset;
		
		if (route_register_is_valid(src, dest, tables))
			return src;
		return -1;
	}

	
	return ni_route_to_register(src, dest, tables);
}

static inline int ni_get_reg_value(const int src, const int dest,
				   const struct ni_route_tables *tables)
{
	return ni_get_reg_value_roffs(src, dest, tables, 0);
}


static inline
int ni_check_trigger_arg_roffs(int src, const int dest,
			       const struct ni_route_tables *tables,
			       const int direct_reg_offset)
{
	if (ni_get_reg_value_roffs(src, dest, tables, direct_reg_offset) < 0)
		return -EINVAL;
	return 0;
}

static inline int ni_check_trigger_arg(const int src, const int dest,
				       const struct ni_route_tables *tables)
{
	return ni_check_trigger_arg_roffs(src, dest, tables, 0);
}

#endif 
