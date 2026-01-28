





#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_EVERYTHING)


#include "../mpconfigvariant_common.h"


#define MICROPY_GC_SPLIT_HEAP          (1)
#define MICROPY_GC_SPLIT_HEAP_N_HEAPS  (4)


#define MICROPY_DEBUG_PARSE_RULE_NAME  (1)
#define MICROPY_TRACKED_ALLOC          (1)
#define MICROPY_WARNINGS_CATEGORY      (1)
#define MICROPY_PY_CRYPTOLIB_CTR       (1)
