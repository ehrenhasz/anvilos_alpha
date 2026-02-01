
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dirent.h>

int main(void)
{
	
	return scandirat(  0,   (void *)1,   (void *)2,   (void *)3,   (void *)4);
}

#undef _GNU_SOURCE
