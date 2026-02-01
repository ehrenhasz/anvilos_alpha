 

 

#ifndef _MTABLE_H
#define _MTABLE_H

#include "imalloc.h"

#ifdef MALLOC_REGISTER

 
#define MT_ALLOC	0x01
#define MT_FREE		0x02

 
typedef struct mr_table {
	PTR_T mem;
	size_t size;
	char flags;
	const char *func;
	const char *file;
	int line;
	int nalloc, nfree;
} mr_table_t;

#define REG_TABLE_SIZE	8192

extern mr_table_t *mr_table_entry PARAMS((PTR_T));
extern void mregister_alloc PARAMS((const char *, PTR_T, size_t, const char *, int));
extern void mregister_free PARAMS((PTR_T, int, const char *, int));
extern void mregister_describe_mem ();
extern void mregister_dump_table PARAMS((void));
extern void mregister_table_init PARAMS((void));

typedef struct ma_table {
	const char *file;
	int line;
	int nalloc;
} ma_table_t;

extern void mlocation_register_alloc PARAMS((const char *, int));
extern void mlocation_table_init PARAMS((void));
extern void mlocation_dump_table PARAMS((void));
extern void mlocation_write_table PARAMS((void));

 

 
#define HASH_MIX(a, b, c) \
 do { \
   a -= b; a -= c; a ^= (c >> 13); \
   b -= c; b -= a; b ^= (a << 8); \
   c -= a; c -= b; c ^= (b >> 13); \
   a -= b; a -= c; a ^= (c >> 12); \
   b -= c; b -= a; b ^= (a << 16); \
   c -= a; c -= b; c ^= (b >> 5); \
   a -= b; a -= c; a ^= (c >> 3); \
   b -= c; b -= a; b ^= (a << 10); \
   c -= a; c -= b; c ^= (b >> 15); \
 } while(0)

#endif  

#endif  
