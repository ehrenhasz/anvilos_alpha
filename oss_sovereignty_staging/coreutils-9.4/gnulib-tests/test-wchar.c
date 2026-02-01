 

#include <config.h>

#include <wchar.h>

 
wchar_t a = 'c';
wint_t b = 'x';

 
static_assert (sizeof NULL == sizeof (void *));

int
main (void)
{
  return 0;
}
