 
#if defined ENABLE_NLS && ENABLE_NLS

 
# include <libintl.h>

 
# ifdef DEFAULT_TEXT_DOMAIN
#  undef gettext
#  define gettext(Msgid) \
     dgettext (DEFAULT_TEXT_DOMAIN, Msgid)
#  undef ngettext
#  define ngettext(Msgid1, Msgid2, N) \
     dngettext (DEFAULT_TEXT_DOMAIN, Msgid1, Msgid2, N)
# endif

#else

 
#if defined(__sun)
# include <locale.h>
#endif

 
#if defined(__cplusplus) && defined(__GNUG__) && (__GNUC__ >= 3)
# include <cstdlib>
# if (__GLIBC__ >= 2 && !defined __UCLIBC__) || _GLIBCXX_HAVE_LIBINTL_H
#  include <libintl.h>
# endif
#endif

 
# undef gettext
# define gettext(Msgid) ((const char *) (Msgid))
# undef dgettext
# define dgettext(Domainname, Msgid) ((void) (Domainname), gettext (Msgid))
# undef dcgettext
# define dcgettext(Domainname, Msgid, Category) \
    ((void) (Category), dgettext (Domainname, Msgid))
# undef ngettext
# define ngettext(Msgid1, Msgid2, N) \
    ((N) == 1 \
     ? ((void) (Msgid2), (const char *) (Msgid1)) \
     : ((void) (Msgid1), (const char *) (Msgid2)))
# undef dngettext
# define dngettext(Domainname, Msgid1, Msgid2, N) \
    ((void) (Domainname), ngettext (Msgid1, Msgid2, N))
# undef dcngettext
# define dcngettext(Domainname, Msgid1, Msgid2, N, Category) \
    ((void) (Category), dngettext (Domainname, Msgid1, Msgid2, N))
# undef textdomain
# define textdomain(Domainname) ((const char *) (Domainname))
# undef bindtextdomain
# define bindtextdomain(Domainname, Dirname) \
    ((void) (Domainname), (const char *) (Dirname))
# undef bind_textdomain_codeset
# define bind_textdomain_codeset(Domainname, Codeset) \
    ((void) (Domainname), (const char *) (Codeset))

#endif

 
#ifdef GNULIB_defined_setlocale
# undef setlocale
# define setlocale rpl_setlocale
#endif

 
#define gettext_noop(String) String

 
#define GETTEXT_CONTEXT_GLUE "\004"

 
#ifdef DEFAULT_TEXT_DOMAIN
# define pgettext(Msgctxt, Msgid) \
   pgettext_aux (DEFAULT_TEXT_DOMAIN, Msgctxt GETTEXT_CONTEXT_GLUE Msgid, Msgid, LC_MESSAGES)
#else
# define pgettext(Msgctxt, Msgid) \
   pgettext_aux (NULL, Msgctxt GETTEXT_CONTEXT_GLUE Msgid, Msgid, LC_MESSAGES)
#endif
#define dpgettext(Domainname, Msgctxt, Msgid) \
  pgettext_aux (Domainname, Msgctxt GETTEXT_CONTEXT_GLUE Msgid, Msgid, LC_MESSAGES)
#define dcpgettext(Domainname, Msgctxt, Msgid, Category) \
  pgettext_aux (Domainname, Msgctxt GETTEXT_CONTEXT_GLUE Msgid, Msgid, Category)
#ifdef DEFAULT_TEXT_DOMAIN
# define npgettext(Msgctxt, Msgid, MsgidPlural, N) \
   npgettext_aux (DEFAULT_TEXT_DOMAIN, Msgctxt GETTEXT_CONTEXT_GLUE Msgid, Msgid, MsgidPlural, N, LC_MESSAGES)
#else
# define npgettext(Msgctxt, Msgid, MsgidPlural, N) \
   npgettext_aux (NULL, Msgctxt GETTEXT_CONTEXT_GLUE Msgid, Msgid, MsgidPlural, N, LC_MESSAGES)
#endif
#define dnpgettext(Domainname, Msgctxt, Msgid, MsgidPlural, N) \
  npgettext_aux (Domainname, Msgctxt GETTEXT_CONTEXT_GLUE Msgid, Msgid, MsgidPlural, N, LC_MESSAGES)
#define dcnpgettext(Domainname, Msgctxt, Msgid, MsgidPlural, N, Category) \
  npgettext_aux (Domainname, Msgctxt GETTEXT_CONTEXT_GLUE Msgid, Msgid, MsgidPlural, N, Category)

#if defined __GNUC__ || defined __clang__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static const char *
pgettext_aux (const char *domain,
              const char *msg_ctxt_id, const char *msgid,
              int category)
{
  const char *translation = dcgettext (domain, msg_ctxt_id, category);
  if (translation == msg_ctxt_id)
    return msgid;
  else
    return translation;
}

#if defined __GNUC__ || defined __clang__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static const char *
npgettext_aux (const char *domain,
               const char *msg_ctxt_id, const char *msgid,
               const char *msgid_plural, unsigned long int n,
               int category)
{
  const char *translation =
    dcngettext (domain, msg_ctxt_id, msgid_plural, n, category);
  if (translation == msg_ctxt_id || translation == msgid_plural)
    return (n == 1 ? msgid : msgid_plural);
  else
    return translation;
}

 

#include <string.h>

 

#if (!defined GNULIB_NO_VLA \
     && defined __STDC_VERSION__ && 199901L <= __STDC_VERSION__ \
     && !defined __STDC_NO_VLA__)
# define _LIBGETTEXT_HAVE_VARIABLE_SIZE_ARRAYS 1
#else
# define _LIBGETTEXT_HAVE_VARIABLE_SIZE_ARRAYS 0
#endif

#if !_LIBGETTEXT_HAVE_VARIABLE_SIZE_ARRAYS
#include <stdlib.h>
#endif

#define pgettext_expr(Msgctxt, Msgid) \
  dcpgettext_expr (NULL, Msgctxt, Msgid, LC_MESSAGES)
#define dpgettext_expr(Domainname, Msgctxt, Msgid) \
  dcpgettext_expr (Domainname, Msgctxt, Msgid, LC_MESSAGES)

#if defined __GNUC__ || defined __clang__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static const char *
dcpgettext_expr (const char *domain,
                 const char *msgctxt, const char *msgid,
                 int category)
{
  size_t msgctxt_len = strlen (msgctxt) + 1;
  size_t msgid_len = strlen (msgid) + 1;
  const char *translation;
#if _LIBGETTEXT_HAVE_VARIABLE_SIZE_ARRAYS
  char msg_ctxt_id[msgctxt_len + msgid_len];
#else
  char buf[1024];
  char *msg_ctxt_id =
    (msgctxt_len + msgid_len <= sizeof (buf)
     ? buf
     : (char *) malloc (msgctxt_len + msgid_len));
  if (msg_ctxt_id != NULL)
#endif
    {
      int found_translation;
      memcpy (msg_ctxt_id, msgctxt, msgctxt_len - 1);
      msg_ctxt_id[msgctxt_len - 1] = '\004';
      memcpy (msg_ctxt_id + msgctxt_len, msgid, msgid_len);
      translation = dcgettext (domain, msg_ctxt_id, category);
      found_translation = (translation != msg_ctxt_id);
#if !_LIBGETTEXT_HAVE_VARIABLE_SIZE_ARRAYS
      if (msg_ctxt_id != buf)
        free (msg_ctxt_id);
#endif
      if (found_translation)
        return translation;
    }
  return msgid;
}

#define npgettext_expr(Msgctxt, Msgid, MsgidPlural, N) \
  dcnpgettext_expr (NULL, Msgctxt, Msgid, MsgidPlural, N, LC_MESSAGES)
#define dnpgettext_expr(Domainname, Msgctxt, Msgid, MsgidPlural, N) \
  dcnpgettext_expr (Domainname, Msgctxt, Msgid, MsgidPlural, N, LC_MESSAGES)

#if defined __GNUC__ || defined __clang__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static const char *
dcnpgettext_expr (const char *domain,
                  const char *msgctxt, const char *msgid,
                  const char *msgid_plural, unsigned long int n,
                  int category)
{
  size_t msgctxt_len = strlen (msgctxt) + 1;
  size_t msgid_len = strlen (msgid) + 1;
  const char *translation;
#if _LIBGETTEXT_HAVE_VARIABLE_SIZE_ARRAYS
  char msg_ctxt_id[msgctxt_len + msgid_len];
#else
  char buf[1024];
  char *msg_ctxt_id =
    (msgctxt_len + msgid_len <= sizeof (buf)
     ? buf
     : (char *) malloc (msgctxt_len + msgid_len));
  if (msg_ctxt_id != NULL)
#endif
    {
      int found_translation;
      memcpy (msg_ctxt_id, msgctxt, msgctxt_len - 1);
      msg_ctxt_id[msgctxt_len - 1] = '\004';
      memcpy (msg_ctxt_id + msgctxt_len, msgid, msgid_len);
      translation = dcngettext (domain, msg_ctxt_id, msgid_plural, n, category);
      found_translation = !(translation == msg_ctxt_id || translation == msgid_plural);
#if !_LIBGETTEXT_HAVE_VARIABLE_SIZE_ARRAYS
      if (msg_ctxt_id != buf)
        free (msg_ctxt_id);
#endif
      if (found_translation)
        return translation;
    }
  return (n == 1 ? msgid : msgid_plural);
}

#endif  
