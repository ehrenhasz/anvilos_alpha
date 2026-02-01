 
# include <libintl.h>
# define fprintf __fxprintf_nocancel
# define flockfile(fp) _IO_flockfile (fp)
# define funlockfile(fp) _IO_funlockfile (fp)
#else
# include "gettext.h"
# define _(msgid) gettext (msgid)
 
# if (!defined _POSIX_THREAD_SAFE_FUNCTIONS \
      || (defined _WIN32 && ! defined __CYGWIN__))
#  define flockfile(fp)  
#  define funlockfile(fp)  
# endif
 
# define __libc_use_alloca(size) 0
# undef alloca
# define alloca(size) (abort (), (void *)0)
#endif

 

#include "getopt_int.h"

 

char *optarg;

 

 
int optind = 1;

 

int opterr = 1;

 

int optopt = '?';

 

static struct _getopt_data getopt_data;

 

static void
exchange (char **argv, struct _getopt_data *d)
{
  int bottom = d->__first_nonopt;
  int middle = d->__last_nonopt;
  int top = d->optind;
  char *tem;

   

  while (top > middle && middle > bottom)
    {
      if (top - middle > middle - bottom)
	{
	   
	  int len = middle - bottom;
	  int i;

	   
	  for (i = 0; i < len; i++)
	    {
	      tem = argv[bottom + i];
	      argv[bottom + i] = argv[top - (middle - bottom) + i];
	      argv[top - (middle - bottom) + i] = tem;
	    }
	   
	  top -= len;
	}
      else
	{
	   
	  int len = top - middle;
	  int i;

	   
	  for (i = 0; i < len; i++)
	    {
	      tem = argv[bottom + i];
	      argv[bottom + i] = argv[middle + i];
	      argv[middle + i] = tem;
	    }
	   
	  bottom += len;
	}
    }

   

  d->__first_nonopt += (d->optind - d->__last_nonopt);
  d->__last_nonopt = d->optind;
}

 
static int
process_long_option (int argc, char **argv, const char *optstring,
		     const struct option *longopts, int *longind,
		     int long_only, struct _getopt_data *d,
		     int print_errors, const char *prefix)
{
  char *nameend;
  size_t namelen;
  const struct option *p;
  const struct option *pfound = NULL;
  int n_options;
  int option_index;

  for (nameend = d->__nextchar; *nameend && *nameend != '='; nameend++)
      ;
  namelen = nameend - d->__nextchar;

   
  for (p = longopts, n_options = 0; p->name; p++, n_options++)
    if (!strncmp (p->name, d->__nextchar, namelen)
	&& namelen == strlen (p->name))
      {
	 
	pfound = p;
	option_index = n_options;
	break;
      }

  if (pfound == NULL)
    {
       
      unsigned char *ambig_set = NULL;
      int ambig_malloced = 0;
      int ambig_fallback = 0;
      int indfound = -1;

      for (p = longopts, option_index = 0; p->name; p++, option_index++)
	if (!strncmp (p->name, d->__nextchar, namelen))
	  {
	    if (pfound == NULL)
	      {
		 
		pfound = p;
		indfound = option_index;
	      }
	    else if (long_only
		     || pfound->has_arg != p->has_arg
		     || pfound->flag != p->flag
		     || pfound->val != p->val)
	      {
		 
		if (!ambig_fallback)
		  {
		    if (!print_errors)
		       
		      ambig_fallback = 1;
		    else if (!ambig_set)
		      {
			if (__libc_use_alloca (n_options))
			  ambig_set = alloca (n_options);
			else if ((ambig_set = malloc (n_options)) == NULL)
			   
			  ambig_fallback = 1;
			else
			  ambig_malloced = 1;

			if (ambig_set)
			  {
			    memset (ambig_set, 0, n_options);
			    ambig_set[indfound] = 1;
			  }
		      }
		    if (ambig_set)
		      ambig_set[option_index] = 1;
		  }
	      }
	  }

      if (ambig_set || ambig_fallback)
	{
	  if (print_errors)
	    {
	      if (ambig_fallback)
		fprintf (stderr, _("%s: option '%s%s' is ambiguous\n"),
			 argv[0], prefix, d->__nextchar);
	      else
		{
		  flockfile (stderr);
		  fprintf (stderr,
			   _("%s: option '%s%s' is ambiguous; possibilities:"),
			   argv[0], prefix, d->__nextchar);

		  for (option_index = 0; option_index < n_options; option_index++)
		    if (ambig_set[option_index])
		      fprintf (stderr, " '%s%s'",
			       prefix, longopts[option_index].name);

		   
		  fprintf (stderr, "\n");
		  funlockfile (stderr);
		}
	    }
	  if (ambig_malloced)
	    free (ambig_set);
	  d->__nextchar += strlen (d->__nextchar);
	  d->optind++;
	  d->optopt = 0;
	  return '?';
	}

      option_index = indfound;
    }

  if (pfound == NULL)
    {
       
      if (!long_only || argv[d->optind][1] == '-'
	  || strchr (optstring, *d->__nextchar) == NULL)
	{
	  if (print_errors)
	    fprintf (stderr, _("%s: unrecognized option '%s%s'\n"),
		     argv[0], prefix, d->__nextchar);

	  d->__nextchar = NULL;
	  d->optind++;
	  d->optopt = 0;
	  return '?';
	}

       
      return -1;
    }

   
  d->optind++;
  d->__nextchar = NULL;
  if (*nameend)
    {
       
      if (pfound->has_arg)
	d->optarg = nameend + 1;
      else
	{
	  if (print_errors)
	    fprintf (stderr,
		     _("%s: option '%s%s' doesn't allow an argument\n"),
		     argv[0], prefix, pfound->name);

	  d->optopt = pfound->val;
	  return '?';
	}
    }
  else if (pfound->has_arg == 1)
    {
      if (d->optind < argc)
	d->optarg = argv[d->optind++];
      else
	{
	  if (print_errors)
	    fprintf (stderr,
		     _("%s: option '%s%s' requires an argument\n"),
		     argv[0], prefix, pfound->name);

	  d->optopt = pfound->val;
	  return optstring[0] == ':' ? ':' : '?';
	}
    }

  if (longind != NULL)
    *longind = option_index;
  if (pfound->flag)
    {
      *(pfound->flag) = pfound->val;
      return 0;
    }
  return pfound->val;
}

 

static const char *
_getopt_initialize (_GL_UNUSED int argc,
		    _GL_UNUSED char **argv, const char *optstring,
		    struct _getopt_data *d, int posixly_correct)
{
   
  if (d->optind == 0)
    d->optind = 1;

  d->__first_nonopt = d->__last_nonopt = d->optind;
  d->__nextchar = NULL;

   
  if (optstring[0] == '-')
    {
      d->__ordering = RETURN_IN_ORDER;
      ++optstring;
    }
  else if (optstring[0] == '+')
    {
      d->__ordering = REQUIRE_ORDER;
      ++optstring;
    }
  else if (posixly_correct || !!getenv ("POSIXLY_CORRECT"))
    d->__ordering = REQUIRE_ORDER;
  else
    d->__ordering = PERMUTE;

  d->__initialized = 1;
  return optstring;
}

 

int
_getopt_internal_r (int argc, char **argv, const char *optstring,
		    const struct option *longopts, int *longind,
		    int long_only, struct _getopt_data *d, int posixly_correct)
{
  int print_errors = d->opterr;

  if (argc < 1)
    return -1;

  d->optarg = NULL;

  if (d->optind == 0 || !d->__initialized)
    optstring = _getopt_initialize (argc, argv, optstring, d, posixly_correct);
  else if (optstring[0] == '-' || optstring[0] == '+')
    optstring++;

  if (optstring[0] == ':')
    print_errors = 0;

   
#define NONOPTION_P (argv[d->optind][0] != '-' || argv[d->optind][1] == '\0')

  if (d->__nextchar == NULL || *d->__nextchar == '\0')
    {
       

       
      if (d->__last_nonopt > d->optind)
	d->__last_nonopt = d->optind;
      if (d->__first_nonopt > d->optind)
	d->__first_nonopt = d->optind;

      if (d->__ordering == PERMUTE)
	{
	   

	  if (d->__first_nonopt != d->__last_nonopt
	      && d->__last_nonopt != d->optind)
	    exchange (argv, d);
	  else if (d->__last_nonopt != d->optind)
	    d->__first_nonopt = d->optind;

	   

	  while (d->optind < argc && NONOPTION_P)
	    d->optind++;
	  d->__last_nonopt = d->optind;
	}

       

      if (d->optind != argc && !strcmp (argv[d->optind], "--"))
	{
	  d->optind++;

	  if (d->__first_nonopt != d->__last_nonopt
	      && d->__last_nonopt != d->optind)
	    exchange (argv, d);
	  else if (d->__first_nonopt == d->__last_nonopt)
	    d->__first_nonopt = d->optind;
	  d->__last_nonopt = argc;

	  d->optind = argc;
	}

       

      if (d->optind == argc)
	{
	   
	  if (d->__first_nonopt != d->__last_nonopt)
	    d->optind = d->__first_nonopt;
	  return -1;
	}

       

      if (NONOPTION_P)
	{
	  if (d->__ordering == REQUIRE_ORDER)
	    return -1;
	  d->optarg = argv[d->optind++];
	  return 1;
	}

       
      if (longopts)
	{
	  if (argv[d->optind][1] == '-')
	    {
	       
	      d->__nextchar = argv[d->optind] + 2;
	      return process_long_option (argc, argv, optstring, longopts,
					  longind, long_only, d,
					  print_errors, "--");
	    }

	   
	  if (long_only && (argv[d->optind][2]
			    || !strchr (optstring, argv[d->optind][1])))
	    {
	      int code;
	      d->__nextchar = argv[d->optind] + 1;
	      code = process_long_option (argc, argv, optstring, longopts,
					  longind, long_only, d,
					  print_errors, "-");
	      if (code != -1)
		return code;
	    }
	}

       
      d->__nextchar = argv[d->optind] + 1;
    }

   

  {
    char c = *d->__nextchar++;
    const char *temp = strchr (optstring, c);

     
    if (*d->__nextchar == '\0')
      ++d->optind;

    if (temp == NULL || c == ':' || c == ';')
      {
	if (print_errors)
	  fprintf (stderr, _("%s: invalid option -- '%c'\n"), argv[0], c);
	d->optopt = c;
	return '?';
      }

     
    if (temp[0] == 'W' && temp[1] == ';' && longopts != NULL)
      {
	 
	if (*d->__nextchar != '\0')
	  d->optarg = d->__nextchar;
	else if (d->optind == argc)
	  {
	    if (print_errors)
	      fprintf (stderr,
		       _("%s: option requires an argument -- '%c'\n"),
		       argv[0], c);

	    d->optopt = c;
	    if (optstring[0] == ':')
	      c = ':';
	    else
	      c = '?';
	    return c;
	  }
	else
	  d->optarg = argv[d->optind];

	d->__nextchar = d->optarg;
	d->optarg = NULL;
	return process_long_option (argc, argv, optstring, longopts, longind,
				    0  , d, print_errors, "-W ");
      }
    if (temp[1] == ':')
      {
	if (temp[2] == ':')
	  {
	     
	    if (*d->__nextchar != '\0')
	      {
		d->optarg = d->__nextchar;
		d->optind++;
	      }
	    else
	      d->optarg = NULL;
	    d->__nextchar = NULL;
	  }
	else
	  {
	     
	    if (*d->__nextchar != '\0')
	      {
		d->optarg = d->__nextchar;
		 
		d->optind++;
	      }
	    else if (d->optind == argc)
	      {
		if (print_errors)
		  fprintf (stderr,
			   _("%s: option requires an argument -- '%c'\n"),
			   argv[0], c);

		d->optopt = c;
		if (optstring[0] == ':')
		  c = ':';
		else
		  c = '?';
	      }
	    else
	       
	      d->optarg = argv[d->optind++];
	    d->__nextchar = NULL;
	  }
      }
    return c;
  }
}

int
_getopt_internal (int argc, char **argv, const char *optstring,
		  const struct option *longopts, int *longind, int long_only,
		  int posixly_correct)
{
  int result;

  getopt_data.optind = optind;
  getopt_data.opterr = opterr;

  result = _getopt_internal_r (argc, argv, optstring, longopts,
			       longind, long_only, &getopt_data,
			       posixly_correct);

  optind = getopt_data.optind;
  optarg = getopt_data.optarg;
  optopt = getopt_data.optopt;

  return result;
}

 
#define GETOPT_ENTRY(NAME, POSIXLY_CORRECT)			\
  int								\
  NAME (int argc, char *const *argv, const char *optstring)	\
  {								\
    return _getopt_internal (argc, (char **)argv, optstring,	\
			     0, 0, 0, POSIXLY_CORRECT);		\
  }

#ifdef _LIBC
GETOPT_ENTRY(getopt, 0)
GETOPT_ENTRY(__posix_getopt, 1)
#else
GETOPT_ENTRY(getopt, 1)
#endif


#ifdef TEST

 

int
main (int argc, char **argv)
{
  int c;
  int digit_optind = 0;

  while (1)
    {
      int this_option_optind = optind ? optind : 1;

      c = getopt (argc, argv, "abc:d:0123456789");
      if (c == -1)
	break;

      switch (c)
	{
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	  if (digit_optind != 0 && digit_optind != this_option_optind)
	    printf ("digits occur in two different argv-elements.\n");
	  digit_optind = this_option_optind;
	  printf ("option %c\n", c);
	  break;

	case 'a':
	  printf ("option a\n");
	  break;

	case 'b':
	  printf ("option b\n");
	  break;

	case 'c':
	  printf ("option c with value '%s'\n", optarg);
	  break;

	case '?':
	  break;

	default:
	  printf ("?? getopt returned character code 0%o ??\n", c);
	}
    }

  if (optind < argc)
    {
      printf ("non-option ARGV-elements: ");
      while (optind < argc)
	printf ("%s ", argv[optind++]);
      printf ("\n");
    }

  exit (0);
}

#endif  
