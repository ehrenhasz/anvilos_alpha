

#ifndef SHELL_RANDOM_H
#define SHELL_RANDOM_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

typedef struct random_t {
	

	
	int32_t galois_LFSR; 

	
	uint32_t LCG;

	
	uint32_t xs64_x;
	uint32_t xs64_y;
} random_t;

#define UNINITED_RANDOM_T(rnd) \
	((rnd)->galois_LFSR == 0)

#define INIT_RANDOM_T(rnd, nonzero, v) \
	((rnd)->galois_LFSR = (rnd)->xs64_x = (nonzero), (rnd)->LCG = (rnd)->xs64_y = (v))

#define CLEAR_RANDOM_T(rnd) \
	((rnd)->galois_LFSR = 0)

uint32_t next_random(random_t *rnd) FAST_FUNC;

POP_SAVED_FUNCTION_VISIBILITY

#endif
