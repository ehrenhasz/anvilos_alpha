 

#include <config.h>
#include "di-set.h"

#include "hash.h"
#include "ino-map.h"

#include <limits.h>
#include <stdlib.h>

 
typedef size_t hashint;
#define HASHINT_MAX ((hashint) -1)

 
#define LARGE_INO_MIN (HASHINT_MAX / 2)

 

 
struct di_ent
{
  dev_t dev;
  struct hash_table *ino_set;
};

 
struct di_set
{
   
  struct hash_table *dev_map;

   
  struct ino_map *ino_map;

   
  struct di_ent *probe;
};

 
static size_t
di_ent_hash (void const *x, size_t table_size)
{
  struct di_ent const *p = x;
  dev_t dev = p->dev;

   
  size_t h = dev;
  unsigned int i;
  unsigned int n_words = sizeof dev / sizeof h + (sizeof dev % sizeof h != 0);
  for (i = 1; i < n_words; i++)
    h ^= dev >> CHAR_BIT * sizeof h * i;

  return h % table_size;
}

 
static bool
di_ent_compare (void const *x, void const *y)
{
  struct di_ent const *a = x;
  struct di_ent const *b = y;
  return a->dev == b->dev;
}

 
static void
di_ent_free (void *v)
{
  struct di_ent *a = v;
  hash_free (a->ino_set);
  free (a);
}

 
struct di_set *
di_set_alloc (void)
{
  struct di_set *dis = malloc (sizeof *dis);
  if (dis)
    {
      enum { INITIAL_DEV_MAP_SIZE = 11 };
      dis->dev_map = hash_initialize (INITIAL_DEV_MAP_SIZE, NULL,
                                      di_ent_hash, di_ent_compare,
                                      di_ent_free);
      if (! dis->dev_map)
        {
          free (dis);
          return NULL;
        }
      dis->ino_map = NULL;
      dis->probe = NULL;
    }

  return dis;
}

 
void
di_set_free (struct di_set *dis)
{
  hash_free (dis->dev_map);
  if (dis->ino_map)
    ino_map_free (dis->ino_map);
  free (dis->probe);
  free (dis);
}

 
static size_t
di_ino_hash (void const *i, size_t table_size)
{
  return (hashint) i % table_size;
}

 
static struct hash_table *
map_device (struct di_set *dis, dev_t dev)
{
   
  struct di_ent *ent;
  struct di_ent *probe = dis->probe;
  if (probe)
    {
       
      if (probe->dev == dev)
        return probe->ino_set;
    }
  else
    {
      dis->probe = probe = malloc (sizeof *probe);
      if (! probe)
        return NULL;
    }

   
  probe->dev = dev;
  ent = hash_insert (dis->dev_map, probe);
  if (! ent)
    return NULL;

  if (ent != probe)
    {
       
      probe->ino_set = ent->ino_set;
    }
  else
    {
      enum { INITIAL_INO_SET_SIZE = 1021 };

       
      dis->probe = NULL;

       
      probe->ino_set = hash_initialize (INITIAL_INO_SET_SIZE, NULL,
                                        di_ino_hash, NULL, NULL);
    }

  return probe->ino_set;
}

 
static hashint
map_inode_number (struct di_set *dis, ino_t ino)
{
  if (0 < ino && ino < LARGE_INO_MIN)
    return ino;

  if (! dis->ino_map)
    {
      dis->ino_map = ino_map_alloc (LARGE_INO_MIN);
      if (! dis->ino_map)
        return INO_MAP_INSERT_FAILURE;
    }

  return ino_map_insert (dis->ino_map, ino);
}

 
int
di_set_insert (struct di_set *dis, dev_t dev, ino_t ino)
{
  hashint i;

   
  struct hash_table *ino_set = map_device (dis, dev);
  if (! ino_set)
    return -1;

   
  i = map_inode_number (dis, ino);
  if (i == INO_MAP_INSERT_FAILURE)
    return -1;

   
  return hash_insert_if_absent (ino_set, (void const *) i, NULL);
}

 
int
di_set_lookup (struct di_set *dis, dev_t dev, ino_t ino)
{
  hashint i;

   
  struct hash_table *ino_set = map_device (dis, dev);
  if (! ino_set)
    return -1;

   
  i = map_inode_number (dis, ino);
  if (i == INO_MAP_INSERT_FAILURE)
    return -1;

   
  return !!hash_lookup (ino_set, (void const *) i);
}
