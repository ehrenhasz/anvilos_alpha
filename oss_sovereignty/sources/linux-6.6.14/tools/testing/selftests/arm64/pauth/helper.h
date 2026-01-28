


#ifndef _HELPER_H_
#define _HELPER_H_

#include <stdlib.h>

#define NKEYS 5

struct signatures {
	size_t keyia;
	size_t keyib;
	size_t keyda;
	size_t keydb;
	size_t keyg;
};

void pac_corruptor(void);


size_t keyia_sign(size_t val);
size_t keyib_sign(size_t val);
size_t keyda_sign(size_t val);
size_t keydb_sign(size_t val);
size_t keyg_sign(size_t val);

#endif
