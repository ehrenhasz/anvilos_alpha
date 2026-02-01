 
#include <wchar.h>


static mbstate_t internal_state;

size_t
mbrlen (const char *s, size_t n, mbstate_t *ps)
{
  if (ps == NULL)
    ps = &internal_state;
  return mbrtowc (NULL, s, n, ps);
}
