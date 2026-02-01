 

 

 

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if defined _LIBC || defined HAVE_STRING_H
# include <string.h>
#endif
#include <sys/types.h>

typedef unsigned chartype;

#undef strstr

char *
strstr (const char *phaystack, const char *pneedle)
{
  register const unsigned char *haystack, *needle;
  register chartype b, c;

  haystack = (const unsigned char *) phaystack;
  needle = (const unsigned char *) pneedle;

  b = *needle;
  if (b != '\0')
    {
      haystack--;				 
      do
	{
	  c = *++haystack;
	  if (c == '\0')
	    goto ret0;
	}
      while (c != b);

      c = *++needle;
      if (c == '\0')
	goto foundneedle;
      ++needle;
      goto jin;

      for (;;)
        {
          register chartype a;
	  register const unsigned char *rhaystack, *rneedle;

	  do
	    {
	      a = *++haystack;
	      if (a == '\0')
		goto ret0;
	      if (a == b)
		break;
	      a = *++haystack;
	      if (a == '\0')
		goto ret0;
shloop:;    }
          while (a != b);

jin:	  a = *++haystack;
	  if (a == '\0')
	    goto ret0;

	  if (a != c)
	    goto shloop;

	  rhaystack = haystack-- + 1;
	  rneedle = needle;
	  a = *rneedle;

	  if (*rhaystack == a)
	    do
	      {
		if (a == '\0')
		  goto foundneedle;
		++rhaystack;
		a = *++needle;
		if (*rhaystack != a)
		  break;
		if (a == '\0')
		  goto foundneedle;
		++rhaystack;
		a = *++needle;
	      }
	    while (*rhaystack == a);

	  needle = rneedle;		    

	  if (a == '\0')
	    break;
        }
    }
foundneedle:
  return (char*) haystack;
ret0:
  return 0;
}
