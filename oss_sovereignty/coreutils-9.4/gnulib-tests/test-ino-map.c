 

#include <config.h>

#include "ino-map.h"

#include "macros.h"

int
main ()
{
  enum { INO_MAP_INIT = 123 };
  struct ino_map *ino_map = ino_map_alloc (INO_MAP_INIT);
  ASSERT (ino_map != NULL);

  ASSERT (ino_map_insert (ino_map, 42) == INO_MAP_INIT);
  ASSERT (ino_map_insert (ino_map, 42) == INO_MAP_INIT);
  ASSERT (ino_map_insert (ino_map, 398) == INO_MAP_INIT + 1);
  ASSERT (ino_map_insert (ino_map, 398) == INO_MAP_INIT + 1);
  ASSERT (ino_map_insert (ino_map, 0) == INO_MAP_INIT + 2);
  ASSERT (ino_map_insert (ino_map, 0) == INO_MAP_INIT + 2);

  {
    int i;
    for (i = 0; i < 100; i++)
      {
        ASSERT (ino_map_insert (ino_map, 10000 + i) == INO_MAP_INIT + 3 + i);
      }
  }

  ino_map_free (ino_map);

  return 0;
}
