 

#include <config.h>

 
#include "hash-triple.h"

#include <stdlib.h>
#include <string.h>

#include "hash-pjw.h"
#include "same-inode.h"

#define STREQ(a, b) (strcmp (a, b) == 0)

 
size_t
triple_hash (void const *x, size_t table_size)
{
  struct F_triple const *p = x;
  size_t tmp = hash_pjw (p->name, table_size);

   
  return (tmp ^ p->st_ino) % table_size;
}

 
bool
triple_compare_ino_str (void const *x, void const *y)
{
  struct F_triple const *a = x;
  struct F_triple const *b = y;
  return (SAME_INODE (*a, *b) && STREQ (a->name, b->name)) ? true : false;
}

 
void
triple_free (void *x)
{
  struct F_triple *a = x;
  free (a->name);
  free (a);
}
