 

 

#include <config.h>

#if !defined (HAVE_WCSDUP) && defined (HANDLE_MULTIBYTE)

#include <stdc.h>
#include <wchar.h>
#include <bashansi.h>
#include <xmalloc.h>

wchar_t *
wcsdup (ws)
     const wchar_t *ws;
{
  wchar_t *ret;
  size_t len;

  len = wcslen (ws);
  ret = xmalloc ((len + 1) * sizeof (wchar_t));
  if (ret == 0)
    return ret;
  
  return (wcscpy (ret, ws));
}
#endif  
