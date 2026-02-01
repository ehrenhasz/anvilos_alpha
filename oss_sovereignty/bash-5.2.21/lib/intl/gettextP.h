 

 

#ifndef _GETTEXTP_H
#define _GETTEXTP_H

#include <stddef.h>		 

#ifdef _LIBC
# include "../iconv/gconv_int.h"
#else
# if HAVE_ICONV
#  include <iconv.h>
# endif
#endif

#include "loadinfo.h"

#include "gmo.h"		 

 

#ifndef PARAMS
# if __STDC__ || defined __GNUC__ || defined __SUNPRO_C || defined __cplusplus || __PROTOTYPES
#  define PARAMS(args) args
# else
#  define PARAMS(args) ()
# endif
#endif

#ifndef internal_function
# define internal_function
#endif

#ifndef attribute_hidden
# define attribute_hidden
#endif

 
#ifndef HAVE_BUILTIN_EXPECT
# define __builtin_expect(expr, val) (expr)
#endif

#ifndef W
# define W(flag, data) ((flag) ? SWAP (data) : (data))
#endif


#ifdef _LIBC
# include <byteswap.h>
# define SWAP(i) bswap_32 (i)
#else
static inline nls_uint32
SWAP (i)
     nls_uint32 i;
{
  return (i << 24) | ((i & 0xff00) << 8) | ((i >> 8) & 0xff00) | (i >> 24);
}
#endif


 
struct sysdep_string_desc
{
   
  size_t length;
   
  const char *pointer;
};

 
struct loaded_domain
{
   
  const char *data;
   
  int use_mmap;
   
  size_t mmap_size;
   
  int must_swap;
   
  void *malloced;

   
  nls_uint32 nstrings;
   
  const struct string_desc *orig_tab;
   
  const struct string_desc *trans_tab;

   
  nls_uint32 n_sysdep_strings;
   
  const struct sysdep_string_desc *orig_sysdep_tab;
   
  const struct sysdep_string_desc *trans_sysdep_tab;

   
  nls_uint32 hash_size;
   
  const nls_uint32 *hash_tab;
   
  int must_swap_hash_tab;

  int codeset_cntr;
#ifdef _LIBC
  __gconv_t conv;
#else
# if HAVE_ICONV
  iconv_t conv;
# endif
#endif
  char **conv_tab;

  struct expression *plural;
  unsigned long int nplurals;
};

 
#ifdef __GNUC__
# define ZERO 0
#else
# define ZERO 1
#endif

 
struct binding
{
  struct binding *next;
  char *dirname;
  int codeset_cntr;	 
  char *codeset;
  char domainname[ZERO];
};

 
extern int _nl_msg_cat_cntr;

#ifndef _LIBC
const char *_nl_locale_name PARAMS ((int category, const char *categoryname));
#endif

struct loaded_l10nfile *_nl_find_domain PARAMS ((const char *__dirname,
						 char *__locale,
						 const char *__domainname,
					      struct binding *__domainbinding))
     internal_function;
void _nl_load_domain PARAMS ((struct loaded_l10nfile *__domain,
			      struct binding *__domainbinding))
     internal_function;
void _nl_unload_domain PARAMS ((struct loaded_domain *__domain))
     internal_function;
const char *_nl_init_domain_conv PARAMS ((struct loaded_l10nfile *__domain_file,
					  struct loaded_domain *__domain,
					  struct binding *__domainbinding))
     internal_function;
void _nl_free_domain_conv PARAMS ((struct loaded_domain *__domain))
     internal_function;

char *_nl_find_msg PARAMS ((struct loaded_l10nfile *domain_file,
			    struct binding *domainbinding,
			    const char *msgid, size_t *lengthp))
     internal_function;

#ifdef _LIBC
extern char *__gettext PARAMS ((const char *__msgid));
extern char *__dgettext PARAMS ((const char *__domainname,
				 const char *__msgid));
extern char *__dcgettext PARAMS ((const char *__domainname,
				  const char *__msgid, int __category));
extern char *__ngettext PARAMS ((const char *__msgid1, const char *__msgid2,
				 unsigned long int __n));
extern char *__dngettext PARAMS ((const char *__domainname,
				  const char *__msgid1, const char *__msgid2,
				  unsigned long int n));
extern char *__dcngettext PARAMS ((const char *__domainname,
				   const char *__msgid1, const char *__msgid2,
				   unsigned long int __n, int __category));
extern char *__dcigettext PARAMS ((const char *__domainname,
				   const char *__msgid1, const char *__msgid2,
				   int __plural, unsigned long int __n,
				   int __category));
extern char *__textdomain PARAMS ((const char *__domainname));
extern char *__bindtextdomain PARAMS ((const char *__domainname,
				       const char *__dirname));
extern char *__bind_textdomain_codeset PARAMS ((const char *__domainname,
						const char *__codeset));
#else
 
# define _INTL_REDIRECT_MACROS
# include "libgnuintl.h"
extern char *libintl_dcigettext PARAMS ((const char *__domainname,
					 const char *__msgid1,
					 const char *__msgid2,
					 int __plural, unsigned long int __n,
					 int __category));
#endif

 

#endif  
