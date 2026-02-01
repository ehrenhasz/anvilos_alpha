 

#ifndef HASH_TRIPLE_H
#define HASH_TRIPLE_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <sys/types.h>
#include <sys/stat.h>

 
struct F_triple
{
  char *name;
  ino_t st_ino;
  dev_t st_dev;
};

 

extern size_t triple_hash (void const *x, size_t table_size) _GL_ATTRIBUTE_PURE;
extern bool triple_compare_ino_str (void const *x, void const *y)
  _GL_ATTRIBUTE_PURE;
extern void triple_free (void *x);

 
extern size_t triple_hash_no_name (void const *x, size_t table_size)
  _GL_ATTRIBUTE_PURE;
extern bool triple_compare (void const *x, void const *y);

#endif
