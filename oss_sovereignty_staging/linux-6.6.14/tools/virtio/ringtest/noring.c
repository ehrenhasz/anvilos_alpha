
#define _GNU_SOURCE
#include "main.h"
#include <assert.h>

 
void alloc_ring(void)
{
}

 
int add_inbuf(unsigned len, void *buf, void *datap)
{
	return 0;
}

 
void *get_buf(unsigned *lenp, void **bufp)
{
	return "Buffer";
}

bool used_empty()
{
	return false;
}

void disable_call()
{
	assert(0);
}

bool enable_call()
{
	assert(0);
}

void kick_available(void)
{
	assert(0);
}

 
void disable_kick()
{
	assert(0);
}

bool enable_kick()
{
	assert(0);
}

bool avail_empty()
{
	return false;
}

bool use_buf(unsigned *lenp, void **bufp)
{
	return true;
}

void call_used(void)
{
	assert(0);
}
