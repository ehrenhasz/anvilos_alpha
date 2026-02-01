 

#include <config.h>

 
#include "hash-triple.h"

#include "same.h"
#include "same-inode.h"

 
size_t
triple_hash_no_name (void const *x, size_t table_size)
{
  struct F_triple const *p = x;

   
  return p->st_ino % table_size;
}

 
bool
triple_compare (void const *x, void const *y)
{
  struct F_triple const *a = x;
  struct F_triple const *b = y;
  return (SAME_INODE (*a, *b) && same_name (a->name, b->name)) ? true : false;
}
