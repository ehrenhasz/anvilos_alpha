





#define MICROPY_CONFIG_ROM_LEVEL (MICROPY_CONFIG_ROM_LEVEL_EXTRA_FEATURES)


#include "../mpconfigvariant_common.h"


#define MICROPY_OBJ_REPR (MICROPY_OBJ_REPR_D)


#define MICROPY_EMIT_X86 (0)
#define MICROPY_EMIT_X64 (0)
#define MICROPY_EMIT_THUMB (0)
#define MICROPY_EMIT_ARM (0)

#include <stdint.h>

typedef int64_t mp_int_t;
typedef uint64_t mp_uint_t;
#define UINT_FMT "%llu"
#define INT_FMT "%lld"
