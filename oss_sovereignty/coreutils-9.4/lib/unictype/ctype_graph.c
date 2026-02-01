 
#include "unictype.h"

#include "bitmap.h"

 
#include "ctype_graph.h"

bool
uc_is_graph (ucs4_t uc)
{
  return bitmap_lookup (&u_is_graph, uc);
}
