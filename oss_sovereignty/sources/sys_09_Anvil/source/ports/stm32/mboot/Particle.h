



#include "py/mphal.h"
#include "rng.h"

static inline uint32_t HAL_RNG_GetRandomNumber(void) {
    return rng_get();
}
