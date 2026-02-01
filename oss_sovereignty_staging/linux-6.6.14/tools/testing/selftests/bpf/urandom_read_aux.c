
 
#include "sdt.h"

void urand_read_without_sema(int iter_num, int iter_cnt, int read_sz)
{
	 
	STAP_PROBE3(urand, read_without_sema, iter_num, iter_cnt, read_sz);
}
