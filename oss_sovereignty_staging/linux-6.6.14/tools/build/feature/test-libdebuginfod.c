
#include <elfutils/debuginfod.h>

int main(void)
{
	debuginfod_client* c = debuginfod_begin();
	return (long)c;
}
