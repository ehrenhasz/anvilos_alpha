 

 

 
#ifdef HAVE_CONFIG_H
#include <config.h>

#ifdef HAVE_STDLIB_H 
#  include <stdlib.h>
#else
extern char *getenv ();
extern char *malloc ();
extern char *realloc ();
#endif

#if defined (HAVE_STRING_H)
#include <string.h>
#endif

#if !defined (HAVE_BCOPY) && (defined (HAVE_STRING_H) || defined (STDC_HEADERS))
#  define bcopy(s, d, n)	memcpy ((d), (s), (n))
#endif

#else  

#if defined(HAVE_STRING_H) || defined(STDC_HEADERS)
#define bcopy(s, d, n) memcpy ((d), (s), (n))
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#include <string.h>
#else
char *malloc ();
char *realloc ();
#endif

#endif  

#include "ltcap.h"

#ifndef NULL
#define NULL (char *) 0
#endif

#ifndef emacs
static void
memory_out ()
{
  write (2, "virtual memory exhausted\n", 25);
  exit (1);
}

static char *
xmalloc (size)
     unsigned size;
{
  register char *tem = malloc (size);

  if (!tem)
    memory_out ();
  return tem;
}

static char *
xrealloc (ptr, size)
     char *ptr;
     unsigned size;
{
  register char *tem = realloc (ptr, size);

  if (!tem)
    memory_out ();
  return tem;
}
#endif  

 

static char *tparam1 ();

 
char *
tparam (string, outstring, len, arg0, arg1, arg2, arg3)
     char *string;
     char *outstring;
     int len;
     int arg0, arg1, arg2, arg3;
{
  int arg[4];

  arg[0] = arg0;
  arg[1] = arg1;
  arg[2] = arg2;
  arg[3] = arg3;
  return tparam1 (string, outstring, len, NULL, NULL, arg);
}

__private_extern__ char *BC;
__private_extern__ char *UP;

static char tgoto_buf[50];

__private_extern__
char *
tgoto (cm, hpos, vpos)
     char *cm;
     int hpos, vpos;
{
  int args[2];
  if (!cm)
    return NULL;
  args[0] = vpos;
  args[1] = hpos;
  return tparam1 (cm, tgoto_buf, 50, UP, BC, args);
}

static char *
tparam1 (string, outstring, len, up, left, argp)
     char *string;
     char *outstring;
     int len;
     char *up, *left;
     register int *argp;
{
  register int c;
  register char *p = string;
  register char *op = outstring;
  char *outend;
  int outlen = 0;

  register int tem;
  int *old_argp = argp;
  int doleft = 0;
  int doup = 0;

  outend = outstring + len;

  while (1)
    {
       
      if (op + 5 >= outend)
	{
	  register char *new;
	  if (outlen == 0)
	    {
	      outlen = len + 40;
	      new = (char *) xmalloc (outlen);
	      outend += 40;
	      bcopy (outstring, new, op - outstring);
	    }
	  else
	    {
	      outend += outlen;
	      outlen *= 2;
	      new = (char *) xrealloc (outstring, outlen);
	    }
	  op += new - outstring;
	  outend += new - outstring;
	  outstring = new;
	}
      c = *p++;
      if (!c)
	break;
      if (c == '%')
	{
	  c = *p++;
	  tem = *argp;
	  switch (c)
	    {
	    case 'd':		 
	      if (tem < 10)
		goto onedigit;
	      if (tem < 100)
		goto twodigit;
	    case '3':		 
	      if (tem > 999)
		{
		  *op++ = tem / 1000 + '0';
		  tem %= 1000;
		}
	      *op++ = tem / 100 + '0';
	    case '2':		 
	    twodigit:
	      tem %= 100;
	      *op++ = tem / 10 + '0';
	    onedigit:
	      *op++ = tem % 10 + '0';
	      argp++;
	      break;

	    case 'C':
	       
	      if (tem >= 96)
		{
		  *op++ = tem / 96;
		  tem %= 96;
		}
	    case '+':		 
	      tem += *p++;
	    case '.':		 
	      if (left)
		{
		   
		  while (tem == 0 || tem == '\n' || tem == '\t')
		    {
		      tem++;
		      if (argp == old_argp)
			doup++, outend -= strlen (up);
		      else
			doleft++, outend -= strlen (left);
		    }
		}
	      *op++ = tem ? tem : 0200;
	    case 'f':		 
	      argp++;
	      break;

	    case 'b':		 
	      argp--;
	      break;

	    case 'r':		 
	      argp[0] = argp[1];
	      argp[1] = tem;
	      old_argp++;
	      break;

	    case '>':		 
	      if (argp[0] > *p++)  
		argp[0] += *p;	 
	      p++;		 
	      break;

	    case 'a':		 
	       
	       
	       
	      tem = p[2] & 0177;
	      if (p[1] == 'p')
		tem = argp[tem - 0100];
	      if (p[0] == '-')
		argp[0] -= tem;
	      else if (p[0] == '+')
		argp[0] += tem;
	      else if (p[0] == '*')
		argp[0] *= tem;
	      else if (p[0] == '/')
		argp[0] /= tem;
	      else
		argp[0] = tem;

	      p += 3;
	      break;

	    case 'i':		 
	      argp[0] ++;	 
	      argp[1] ++;	 
	      break;

	    case '%':		 
	      goto ordinary;

	    case 'n':		 
	      argp[0] ^= 0140;
	      argp[1] ^= 0140;
	      break;

	    case 'm':		 
	      argp[0] ^= 0177;
	      argp[1] ^= 0177;
	      break;

	    case 'B':		 
	      argp[0] += 6 * (tem / 10);
	      break;

	    case 'D':		 
	      argp[0] -= 2 * (tem % 16);
	      break;
	    }
	}
      else
	 
      ordinary:
	*op++ = c;
    }
  *op = 0;
  while (doup-- > 0)
    strcat (op, up);
  while (doleft-- > 0)
    strcat (op, left);
  return outstring;
}

#ifdef DEBUG

main (argc, argv)
     int argc;
     char **argv;
{
  char buf[50];
  int args[3];
  args[0] = atoi (argv[2]);
  args[1] = atoi (argv[3]);
  args[2] = atoi (argv[4]);
  tparam1 (argv[1], buf, "LEFT", "UP", args);
  printf ("%s\n", buf);
  return 0;
}

#endif  
