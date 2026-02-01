

#include <elfutils/libdwfl.h>

int main(void)
{
	 
	dwfl_thread_getframes((void *) 1, (void *) 1, NULL);
	return 0;
}
