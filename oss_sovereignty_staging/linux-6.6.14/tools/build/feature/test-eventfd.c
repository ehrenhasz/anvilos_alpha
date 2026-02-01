


#include <sys/eventfd.h>

int main(void)
{
	return eventfd(0, EFD_NONBLOCK);
}
