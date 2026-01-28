



#define ROUNDS(x) {unsigned int rcnt;			       \
		for (rcnt = 0; rcnt < x*1000; rcnt++) { \
			(void)(((int)(pow(rcnt, rcnt) * \
				      sqrt(rcnt*7230970)) ^ 7230716) ^ \
				      (int)atan2(rcnt, rcnt));	       \
		} }							\


void start_benchmark(struct config *config);
