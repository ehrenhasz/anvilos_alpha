

#include <stdint.h>


#define MICROPY_OBJ_REPR (MICROPY_OBJ_REPR_D)


#define MICROPY_EMIT_THUMB (0)
#define MICROPY_EMIT_INLINE_THUMB (0)


#define UINT_FMT "%llu"
#define INT_FMT "%lld"
typedef int64_t mp_int_t;
typedef uint64_t mp_uint_t;


#include <mpconfigport.h>
