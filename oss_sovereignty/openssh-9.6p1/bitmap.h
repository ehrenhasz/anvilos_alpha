 
 

#ifndef _BITMAP_H
#define _BITMAP_H

#include <sys/types.h>

 

struct bitmap;

 
struct bitmap *bitmap_new(void);

 
void bitmap_free(struct bitmap *b);

 
void bitmap_zero(struct bitmap *b);

 
int bitmap_test_bit(struct bitmap *b, u_int n);

 
int bitmap_set_bit(struct bitmap *b, u_int n);

 
void bitmap_clear_bit(struct bitmap *b, u_int n);

 
size_t bitmap_nbits(struct bitmap *b);

 
size_t bitmap_nbytes(struct bitmap *b);

 
int bitmap_to_string(struct bitmap *b, void *p, size_t l);

 
int bitmap_from_string(struct bitmap *b, const void *p, size_t l);

#endif  
