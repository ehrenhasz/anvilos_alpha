


#ifndef _NOLIBC_CTYPE_H
#define _NOLIBC_CTYPE_H

#include "std.h"



static __attribute__((unused))
int isascii(int c)
{
	
	return (unsigned int)c <= 0x7f;
}

static __attribute__((unused))
int isblank(int c)
{
	return c == '\t' || c == ' ';
}

static __attribute__((unused))
int iscntrl(int c)
{
	
	return (unsigned int)c < 0x20 || c == 0x7f;
}

static __attribute__((unused))
int isdigit(int c)
{
	return (unsigned int)(c - '0') < 10;
}

static __attribute__((unused))
int isgraph(int c)
{
	
	return (unsigned int)(c - 0x21) < 0x5e;
}

static __attribute__((unused))
int islower(int c)
{
	return (unsigned int)(c - 'a') < 26;
}

static __attribute__((unused))
int isprint(int c)
{
	
	return (unsigned int)(c - 0x20) < 0x5f;
}

static __attribute__((unused))
int isspace(int c)
{
	
	return ((unsigned int)c == ' ') || (unsigned int)(c - 0x09) < 5;
}

static __attribute__((unused))
int isupper(int c)
{
	return (unsigned int)(c - 'A') < 26;
}

static __attribute__((unused))
int isxdigit(int c)
{
	return isdigit(c) || (unsigned int)(c - 'A') < 6 || (unsigned int)(c - 'a') < 6;
}

static __attribute__((unused))
int isalpha(int c)
{
	return islower(c) || isupper(c);
}

static __attribute__((unused))
int isalnum(int c)
{
	return isalpha(c) || isdigit(c);
}

static __attribute__((unused))
int ispunct(int c)
{
	return isgraph(c) && !isalnum(c);
}


#include "nolibc.h"

#endif 
