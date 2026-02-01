


#include "helper.h"

size_t keyia_sign(size_t ptr)
{
	asm volatile("paciza %0" : "+r" (ptr));
	return ptr;
}

size_t keyib_sign(size_t ptr)
{
	asm volatile("pacizb %0" : "+r" (ptr));
	return ptr;
}

size_t keyda_sign(size_t ptr)
{
	asm volatile("pacdza %0" : "+r" (ptr));
	return ptr;
}

size_t keydb_sign(size_t ptr)
{
	asm volatile("pacdzb %0" : "+r" (ptr));
	return ptr;
}

size_t keyg_sign(size_t ptr)
{
	 
	size_t dest = 0;
	size_t modifier = 0;

	asm volatile("pacga %0, %1, %2" : "=r" (dest) : "r" (ptr), "r" (modifier));

	return dest;
}
