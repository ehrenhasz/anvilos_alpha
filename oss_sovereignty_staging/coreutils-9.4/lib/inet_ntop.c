 

#include <config.h>

 
#include <arpa/inet.h>

 
#ifndef IF_LINT
# if defined GCC_LINT || defined lint
#  define IF_LINT(Code) Code
# else
#  define IF_LINT(Code)  
# endif
#endif

#if HAVE_DECL_INET_NTOP

# undef inet_ntop

const char *
rpl_inet_ntop (int af, const void *restrict src,
               char *restrict dst, socklen_t cnt)
{
  return inet_ntop (af, src, dst, cnt);
}

#else

# include <stdio.h>
# include <string.h>
# include <errno.h>

# define NS_IN6ADDRSZ 16
# define NS_INT16SZ 2

 
typedef int verify_int_size[4 <= sizeof (int) ? 1 : -1];

static const char *inet_ntop4 (const unsigned char *src, char *dst, socklen_t size);
# if HAVE_IPV6
static const char *inet_ntop6 (const unsigned char *src, char *dst, socklen_t size);
# endif


 
const char *
inet_ntop (int af, const void *restrict src,
           char *restrict dst, socklen_t cnt)
{
  switch (af)
    {
# if HAVE_IPV4
    case AF_INET:
      return (inet_ntop4 (src, dst, cnt));
# endif

# if HAVE_IPV6
    case AF_INET6:
      return (inet_ntop6 (src, dst, cnt));
# endif

    default:
      errno = EAFNOSUPPORT;
      return (NULL);
    }
   
}

 
static const char *
inet_ntop4 (const unsigned char *src, char *dst, socklen_t size)
{
  char tmp[sizeof "255.255.255.255"];
  int len;

  len = sprintf (tmp, "%u.%u.%u.%u", src[0], src[1], src[2], src[3]);
  if (len < 0)
    return NULL;

  if (len > size)
    {
      errno = ENOSPC;
      return NULL;
    }

  return strcpy (dst, tmp);
}

# if HAVE_IPV6

 
static const char *
inet_ntop6 (const unsigned char *src, char *dst, socklen_t size)
{
   
  char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"], *tp;
  struct
  {
    int base, len;
  } best, cur;
  unsigned int words[NS_IN6ADDRSZ / NS_INT16SZ];
  int i;

   
  memset (words, '\0', sizeof words);
  for (i = 0; i < NS_IN6ADDRSZ; i += 2)
    words[i / 2] = (src[i] << 8) | src[i + 1];
  best.base = -1;
  cur.base = -1;
  IF_LINT(best.len = 0);
  IF_LINT(cur.len = 0);
  for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++)
    {
      if (words[i] == 0)
        {
          if (cur.base == -1)
            cur.base = i, cur.len = 1;
          else
            cur.len++;
        }
      else
        {
          if (cur.base != -1)
            {
              if (best.base == -1 || cur.len > best.len)
                best = cur;
              cur.base = -1;
            }
        }
    }
  if (cur.base != -1)
    {
      if (best.base == -1 || cur.len > best.len)
        best = cur;
    }
  if (best.base != -1 && best.len < 2)
    best.base = -1;

   
  tp = tmp;
  for (i = 0; i < (NS_IN6ADDRSZ / NS_INT16SZ); i++)
    {
       
      if (best.base != -1 && i >= best.base && i < (best.base + best.len))
        {
          if (i == best.base)
            *tp++ = ':';
          continue;
        }
       
      if (i != 0)
        *tp++ = ':';
       
      if (i == 6 && best.base == 0 &&
          (best.len == 6 || (best.len == 5 && words[5] == 0xffff)))
        {
          if (!inet_ntop4 (src + 12, tp, sizeof tmp - (tp - tmp)))
            return (NULL);
          tp += strlen (tp);
          break;
        }
      {
        int len = sprintf (tp, "%x", words[i]);
        if (len < 0)
          return NULL;
        tp += len;
      }
    }
   
  if (best.base != -1 && (best.base + best.len) ==
      (NS_IN6ADDRSZ / NS_INT16SZ))
    *tp++ = ':';
  *tp++ = '\0';

   
  if ((socklen_t) (tp - tmp) > size)
    {
      errno = ENOSPC;
      return NULL;
    }

  return strcpy (dst, tmp);
}

# endif

#endif
