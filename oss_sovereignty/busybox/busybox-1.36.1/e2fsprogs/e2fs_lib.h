#include "bb_e2fs_defs.h"
PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN
void print_e2flags_long(unsigned flags);
void print_e2flags(unsigned flags);
extern const uint32_t e2attr_flags_value[];
extern const char e2attr_flags_sname[];
#ifdef ENABLE_COMPRESSION
#define e2attr_flags_value_chattr (&e2attr_flags_value[5])
#define e2attr_flags_sname_chattr (&e2attr_flags_sname[5])
#else
#define e2attr_flags_value_chattr (&e2attr_flags_value[1])
#define e2attr_flags_sname_chattr (&e2attr_flags_sname[1])
#endif
POP_SAVED_FUNCTION_VISIBILITY
