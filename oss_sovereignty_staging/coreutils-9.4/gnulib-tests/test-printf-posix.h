 

#include "infinity.h"

static void
test_function (int (*my_printf) (const char *, ...))
{
   

   

  my_printf ("%ju %d\n", (uintmax_t) 12345671, 33, 44, 55);

  my_printf ("%zu %d\n", (size_t) 12345672, 33, 44, 55);

  my_printf ("%tu %d\n", (ptrdiff_t) 12345673, 33, 44, 55);

   

   
  my_printf ("%a %d\n", 0.0, 33, 44, 55);

   
  my_printf ("%a %d\n", Infinityd (), 33, 44, 55);

   
  my_printf ("%a %d\n", - Infinityd (), 33, 44, 55);

   
   

   
  my_printf ("%f %d\n", 12.75, 33, 44, 55);

   
  my_printf ("%f %d\n", 1234567.0, 33, 44, 55);

   
  my_printf ("%f %d\n", -0.03125, 33, 44, 55);

   
  my_printf ("%f %d\n", 0.0, 33, 44, 55);

   
  my_printf ("%015f %d\n", 1234.0, 33, 44, 55);

   
  my_printf ("%.f %d\n", 1234.0, 33, 44, 55);

   
  my_printf ("%.2f %d\n", 999.95, 33, 44, 55);

   
  my_printf ("%.2f %d\n", 999.996, 33, 44, 55);

   
  my_printf ("%Lf %d\n", 12.75L, 33, 44, 55);

   
  my_printf ("%Lf %d\n", 1234567.0L, 33, 44, 55);

   
  my_printf ("%Lf %d\n", -0.03125L, 33, 44, 55);

   
  my_printf ("%Lf %d\n", 0.0L, 33, 44, 55);

   
  my_printf ("%015Lf %d\n", 1234.0L, 33, 44, 55);

   
  my_printf ("%.Lf %d\n", 1234.0L, 33, 44, 55);

   
  my_printf ("%.2Lf %d\n", 999.95L, 33, 44, 55);

   
  my_printf ("%.2Lf %d\n", 999.996L, 33, 44, 55);

   

   
  my_printf ("%F %d\n", 12.75, 33, 44, 55);

   
  my_printf ("%F %d\n", 1234567.0, 33, 44, 55);

   
  my_printf ("%F %d\n", -0.03125, 33, 44, 55);

   
  my_printf ("%F %d\n", 0.0, 33, 44, 55);

   
  my_printf ("%015F %d\n", 1234.0, 33, 44, 55);

   
  my_printf ("%.F %d\n", 1234.0, 33, 44, 55);

   
  my_printf ("%.2F %d\n", 999.95, 33, 44, 55);

   
  my_printf ("%.2F %d\n", 999.996, 33, 44, 55);

   
  my_printf ("%LF %d\n", 12.75L, 33, 44, 55);

   
  my_printf ("%LF %d\n", 1234567.0L, 33, 44, 55);

   
  my_printf ("%LF %d\n", -0.03125L, 33, 44, 55);

   
  my_printf ("%LF %d\n", 0.0L, 33, 44, 55);

   
  my_printf ("%015LF %d\n", 1234.0L, 33, 44, 55);

   
  my_printf ("%.LF %d\n", 1234.0L, 33, 44, 55);

   
  my_printf ("%.2LF %d\n", 999.95L, 33, 44, 55);

   
  my_printf ("%.2LF %d\n", 999.996L, 33, 44, 55);

   

  my_printf ("%2$d %1$d\n", 33, 55);
}
