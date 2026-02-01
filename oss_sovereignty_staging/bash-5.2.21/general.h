 

 

#if !defined (_GENERAL_H_)
#define _GENERAL_H_

#include "stdc.h"

#include "bashtypes.h"
#include "chartypes.h"

#if defined (HAVE_SYS_RESOURCE_H) && defined (RLIMTYPE)
#  if defined (HAVE_SYS_TIME_H)
#    include <sys/time.h>
#  endif
#  include <sys/resource.h>
#endif

#if defined (HAVE_STRING_H)
#  include <string.h>
#else
#  include <strings.h>
#endif  

#if defined (HAVE_LIMITS_H)
#  include <limits.h>
#endif

#include "xmalloc.h"

 
#if !defined (NULL)
#  if defined (__STDC__)
#    define NULL ((void *) 0)
#  else
#    define NULL 0x0
#  endif  
#endif  

 
#define pointer_to_int(x)	(int)((char *)x - (char *)0)

#if defined (alpha) && defined (__GNUC__) && !defined (strchr) && !defined (__STDC__)
extern char *strchr (), *strrchr ();
#endif

#if !defined (strcpy) && (defined (HAVE_DECL_STRCPY) && !HAVE_DECL_STRCPY)
extern char *strcpy PARAMS((char *, const char *));
#endif

#if !defined (savestring)
#  define savestring(x) (char *)strcpy (xmalloc (1 + strlen (x)), (x))
#endif

#ifndef member
#  define member(c, s) ((c) ? ((char *)mbschr ((s), (c)) != (char *)NULL) : 0)
#endif

#ifndef whitespace
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))
#endif

#ifndef CHAR_MAX
#  ifdef __CHAR_UNSIGNED__
#    define CHAR_MAX	0xff
#  else
#    define CHAR_MAX	0x7f
#  endif
#endif

#ifndef CHAR_BIT
#  define CHAR_BIT 8
#endif

 
#define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))

 
#define TYPE_WIDTH(t) (sizeof (t) * CHAR_BIT)

 
#define INT_BITS_STRLEN_BOUND(b) (((b) * 146 + 484) / 485)

 
#define INT_STRLEN_BOUND(t) \
  ((TYPE_WIDTH (t) - TYPE_SIGNED (t)) * 302 / 1000 \
   + 1 + TYPE_SIGNED (t))

 
#if 0
#define INT_STRLEN_BOUND(t) \
  (INT_BITS_STRLEN_BOUND (TYPE_WIDTH (t) - TYPE_SIGNED (t)) + TYPE_SIGNED(t))
#endif

 
#define INT_BUFSIZE_BOUND(t) (INT_STRLEN_BOUND (t) + 1)

 
#define legal_variable_starter(c) (ISALPHA(c) || (c == '_'))
#define legal_variable_char(c)	(ISALNUM(c) || c == '_')

 
#define spctabnl(c)	((c) == ' ' || (c) == '\t' || (c) == '\n')

 
typedef struct g_list {
  struct g_list *next;
} GENERIC_LIST;

 
typedef struct {
  char *word;
  int token;
} STRING_INT_ALIST;

 
#define REVERSE_LIST(list, type) \
  ((list && list->next) ? (type)list_reverse ((GENERIC_LIST *)list) \
			: (type)(list))

#if __GNUC__ > 1
#  define FASTCOPY(s, d, n)  __builtin_memcpy ((d), (s), (n))
#else  
#  if !defined (HAVE_BCOPY)
#    if !defined (HAVE_MEMMOVE)
#      define FASTCOPY(s, d, n)  memcpy ((d), (s), (n))
#    else
#      define FASTCOPY(s, d, n)  memmove ((d), (s), (n))
#    endif  
#  else  
#    define FASTCOPY(s, d, n)  bcopy ((s), (d), (n))
#  endif  
#endif  

 
#define STREQ(a, b) ((a)[0] == (b)[0] && strcmp(a, b) == 0)
#define STREQN(a, b, n) ((n == 0) ? (1) \
				  : ((a)[0] == (b)[0] && strncmp(a, b, n) == 0))

 
#define STRLEN(s) (((s) && (s)[0]) ? ((s)[1] ? ((s)[2] ? strlen(s) : 2) : 1) : 0)
#define FREE(s)  do { if (s) free (s); } while (0)
#define MEMBER(c, s) (((c) && c == (s)[0] && !(s)[1]) || (member(c, s)))

 

#define RESIZE_MALLOCED_BUFFER(str, cind, room, csize, sincr) \
  do { \
    if ((cind) + (room) >= csize) \
      { \
	while ((cind) + (room) >= csize) \
	  csize += (sincr); \
	str = xrealloc (str, csize); \
      } \
  } while (0)

 
#if !defined (_FUNCTION_DEF)
#  define _FUNCTION_DEF
typedef int Function ();
typedef void VFunction ();
typedef char *CPFunction ();		 
typedef char **CPPFunction ();		 
#endif  

#ifndef SH_FUNCTION_TYPEDEF
#  define SH_FUNCTION_TYPEDEF

 
 

typedef int sh_intfunc_t PARAMS((int));
typedef int sh_ivoidfunc_t PARAMS((void));
typedef int sh_icpfunc_t PARAMS((char *));
typedef int sh_icppfunc_t PARAMS((char **));
typedef int sh_iptrfunc_t PARAMS((PTR_T));

typedef void sh_voidfunc_t PARAMS((void));
typedef void sh_vintfunc_t PARAMS((int));
typedef void sh_vcpfunc_t PARAMS((char *));
typedef void sh_vcppfunc_t PARAMS((char **));
typedef void sh_vptrfunc_t PARAMS((PTR_T));

typedef int sh_wdesc_func_t PARAMS((WORD_DESC *));
typedef int sh_wlist_func_t PARAMS((WORD_LIST *));

typedef int sh_glist_func_t PARAMS((GENERIC_LIST *));

typedef char *sh_string_func_t PARAMS((char *));	 

typedef int sh_msg_func_t PARAMS((const char *, ...));	 
typedef void sh_vmsg_func_t PARAMS((const char *, ...));	 

 
typedef void sh_sv_func_t PARAMS((char *));	 
typedef void sh_free_func_t PARAMS((PTR_T));	 
typedef void sh_resetsig_func_t PARAMS((int));	 

typedef int sh_ignore_func_t PARAMS((const char *));	 

typedef int sh_assign_func_t PARAMS((const char *));
typedef int sh_wassign_func_t PARAMS((WORD_DESC *, int));

typedef int sh_load_func_t PARAMS((char *));
typedef void sh_unload_func_t PARAMS((char *));

typedef int sh_builtin_func_t PARAMS((WORD_LIST *));  

#endif  

#define NOW	((time_t) time ((time_t *) 0))
#define GETTIME(tv)	gettimeofday(&(tv), NULL)

 
#define FS_EXISTS	  0x1
#define FS_EXECABLE	  0x2
#define FS_EXEC_PREFERRED 0x4
#define FS_EXEC_ONLY	  0x8
#define FS_DIRECTORY	  0x10
#define FS_NODIRS	  0x20
#define FS_READABLE	  0x40

 
#define HIGH_FD_MAX	256

 
#ifdef __STDC__
typedef int QSFUNC (const void *, const void *);
#else
typedef int QSFUNC ();
#endif 

 

#if !defined (__CYGWIN__)
#  define ABSPATH(x)	((x)[0] == '/')
#  define RELPATH(x)	((x)[0] != '/')
#else  
#  define ABSPATH(x)	(((x)[0] && ISALPHA((unsigned char)(x)[0]) && (x)[1] == ':') || ISDIRSEP((x)[0]))
#  define RELPATH(x)	(ABSPATH(x) == 0)
#endif  

#define ROOTEDPATH(x)	(ABSPATH(x))

#define DIRSEP	'/'
#if !defined (__CYGWIN__)
#  define ISDIRSEP(c)	((c) == '/')
#else
#  define ISDIRSEP(c)	((c) == '/' || (c) == '\\')
#endif  
#define PATHSEP(c)	(ISDIRSEP(c) || (c) == 0)

#define DOT_OR_DOTDOT(s)	(s[0] == '.' && (s[1] == 0 || (s[1] == '.' && s[2] == 0)))

#if defined (HANDLE_MULTIBYTE)
#define WDOT_OR_DOTDOT(w)	(w[0] == L'.' && (w[1] == L'\0' || (w[1] == L'.' && w[2] == L'\0')))
#endif

#if 0
 
extern PTR_T xmalloc PARAMS((size_t));
extern PTR_T xrealloc PARAMS((void *, size_t));
extern void xfree PARAMS((void *));
#endif

 
extern void posix_initialize PARAMS((int));

extern int num_posix_options PARAMS((void));
extern char *get_posix_options PARAMS((char *));
extern void set_posix_options PARAMS((const char *));

extern void save_posix_options PARAMS((void));

#if defined (RLIMTYPE)
extern RLIMTYPE string_to_rlimtype PARAMS((char *));
extern void print_rlimtype PARAMS((RLIMTYPE, int));
#endif

extern int all_digits PARAMS((const char *));
extern int legal_number PARAMS((const char *, intmax_t *));
extern int legal_identifier PARAMS((const char *));
extern int importable_function_name PARAMS((const char *, size_t));
extern int exportable_function_name PARAMS((const char *));
extern int check_identifier PARAMS((WORD_DESC *, int));
extern int valid_nameref_value PARAMS((const char *, int));
extern int check_selfref PARAMS((const char *, char *, int));
extern int legal_alias_name PARAMS((const char *, int));
extern int line_isblank PARAMS((const char *));
extern int assignment PARAMS((const char *, int));

extern int sh_unset_nodelay_mode PARAMS((int));
extern int sh_setclexec PARAMS((int));
extern int sh_validfd PARAMS((int));
extern int fd_ispipe PARAMS((int));
extern void check_dev_tty PARAMS((void));
extern int move_to_high_fd PARAMS((int, int, int));
extern int check_binary_file PARAMS((const char *, int));

#ifdef _POSIXSTAT_H_
extern int same_file PARAMS((const char *, const char *, struct stat *, struct stat *));
#endif

extern int sh_openpipe PARAMS((int *));
extern int sh_closepipe PARAMS((int *));

extern int file_exists PARAMS((const char *));
extern int file_isdir PARAMS((const char  *));
extern int file_iswdir PARAMS((const char  *));
extern int path_dot_or_dotdot PARAMS((const char *));
extern int absolute_pathname PARAMS((const char *));
extern int absolute_program PARAMS((const char *));

extern char *make_absolute PARAMS((const char *, const char *));
extern char *base_pathname PARAMS((char *));
extern char *full_pathname PARAMS((char *));
extern char *polite_directory_format PARAMS((char *));
extern char *trim_pathname PARAMS((char *, int));
extern char *printable_filename PARAMS((char *, int));

extern char *extract_colon_unit PARAMS((char *, int *));

extern void tilde_initialize PARAMS((void));
extern char *bash_tilde_find_word PARAMS((const char *, int, int *));
extern char *bash_tilde_expand PARAMS((const char *, int));

extern int group_member PARAMS((gid_t));
extern char **get_group_list PARAMS((int *));
extern int *get_group_array PARAMS((int *));

extern char *conf_standard_path PARAMS((void));
extern int default_columns PARAMS((void));

#endif	 
