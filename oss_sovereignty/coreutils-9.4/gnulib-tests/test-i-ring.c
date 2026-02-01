 

#include <config.h>

#include "i-ring.h"

#include "macros.h"

int
main (void)
{
  int o;
  I_ring ir;
  i_ring_init (&ir, -1);
  o = i_ring_push (&ir, 1);
  ASSERT (o == -1);
  o = i_ring_push (&ir, 2);
  ASSERT (o == -1);
  o = i_ring_push (&ir, 3);
  ASSERT (o == -1);
  o = i_ring_push (&ir, 4);
  ASSERT (o == -1);
  o = i_ring_push (&ir, 5);
  ASSERT (o == 1);
  o = i_ring_push (&ir, 6);
  ASSERT (o == 2);
  o = i_ring_push (&ir, 7);
  ASSERT (o == 3);

  o = i_ring_pop (&ir);
  ASSERT (o == 7);
  o = i_ring_pop (&ir);
  ASSERT (o == 6);
  o = i_ring_pop (&ir);
  ASSERT (o == 5);
  o = i_ring_pop (&ir);
  ASSERT (o == 4);
  ASSERT (i_ring_empty (&ir));

  o = i_ring_push (&ir, 8);
  ASSERT (o == -1);
  o = i_ring_pop (&ir);
  ASSERT (o == 8);
  ASSERT (i_ring_empty (&ir));

  return 0;
}
