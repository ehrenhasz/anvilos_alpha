#ifndef COLOR_MOD_COLOR_TABLE_H_
#define COLOR_MOD_COLOR_TABLE_H_
#include "dc_types.h"
#define NUM_PTS_IN_REGION 16
#define NUM_REGIONS 32
#define MAX_HW_POINTS (NUM_PTS_IN_REGION*NUM_REGIONS)
enum table_type {
	type_pq_table,
	type_de_pq_table
};
bool mod_color_is_table_init(enum table_type type);
struct fixed31_32 *mod_color_get_table(enum table_type type);
void mod_color_set_table_init_state(enum table_type type, bool state);
#endif  
