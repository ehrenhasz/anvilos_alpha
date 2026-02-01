 

#include <config.h>
#include "ino-map.h"

#include "hash.h"

#include <limits.h>
#include <stdlib.h>

 
struct ino_map_ent
{
  ino_t ino;
  size_t mapped_ino;
};

 
struct ino_map
{
   
  struct hash_table *map;

   
  size_t next_mapped_ino;

   
  struct ino_map_ent *probe;
};

 
static size_t
ino_hash (void const *x, size_t table_size)
{
  struct ino_map_ent const *p = x;
  ino_t ino = p->ino;

   
  size_t h = ino;
  unsigned int i;
  unsigned int n_words = sizeof ino / sizeof h + (sizeof ino % sizeof h != 0);
  for (i = 1; i < n_words; i++)
    h ^= ino >> CHAR_BIT * sizeof h * i;

  return h % table_size;
}

 
static bool
ino_compare (void const *x, void const *y)
{
  struct ino_map_ent const *a = x;
  struct ino_map_ent const *b = y;
  return a->ino == b->ino;
}

 
struct ino_map *
ino_map_alloc (size_t next_mapped_ino)
{
  struct ino_map *im = malloc (sizeof *im);

  if (im)
    {
      enum { INITIAL_INO_MAP_TABLE_SIZE = 1021 };
      im->map = hash_initialize (INITIAL_INO_MAP_TABLE_SIZE, NULL,
                                 ino_hash, ino_compare, free);
      if (! im->map)
        {
          free (im);
          return NULL;
        }
      im->next_mapped_ino = next_mapped_ino;
      im->probe = NULL;
    }

  return im;
}

 
void
ino_map_free (struct ino_map *map)
{
  hash_free (map->map);
  free (map->probe);
  free (map);
}


 
size_t
ino_map_insert (struct ino_map *im, ino_t ino)
{
  struct ino_map_ent *ent;

   
  struct ino_map_ent *probe = im->probe;
  if (probe)
    {
       
      if (probe->ino == ino)
        return probe->mapped_ino;
    }
  else
    {
      im->probe = probe = malloc (sizeof *probe);
      if (! probe)
        return INO_MAP_INSERT_FAILURE;
    }

  probe->ino = ino;
  ent = hash_insert (im->map, probe);
  if (! ent)
    return INO_MAP_INSERT_FAILURE;

  if (ent != probe)
    {
       
      probe->mapped_ino = ent->mapped_ino;
    }
  else
    {
       
      static_assert (INO_MAP_INSERT_FAILURE + 1 == 0);

       
      im->probe = NULL;

       
      probe->mapped_ino = im->next_mapped_ino++;
    }

  return probe->mapped_ino;
}
