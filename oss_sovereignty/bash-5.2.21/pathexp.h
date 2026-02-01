 

 

#if !defined (_PATHEXP_H_)
#define _PATHEXP_H_

#define GLOB_FAILED(glist)	(glist) == (char **)&glob_error_return

extern int noglob_dot_filenames;
extern char *glob_error_return;

 
#define QGLOB_CVTNULL	0x01	 
#define QGLOB_FILENAME	0x02	 
#define QGLOB_REGEXP	0x04	 
#define QGLOB_CTLESC	0x08	 
#define QGLOB_DEQUOTE	0x10	 

#if defined (EXTENDED_GLOB)
 
#  define FNMATCH_EXTFLAG	(extended_glob ? FNM_EXTMATCH : 0)
#else
#  define FNMATCH_EXTFLAG	0
#endif  

#define FNMATCH_IGNCASE		(match_ignore_case ? FNM_CASEFOLD : 0)
#define FNMATCH_NOCASEGLOB	(glob_ignore_case ? FNM_CASEFOLD : 0)

extern int glob_dot_filenames;
extern int extended_glob;
extern int glob_star;
extern int match_ignore_case;	 

extern int unquoted_glob_pattern_p PARAMS((char *));

 
extern char *quote_string_for_globbing PARAMS((const char *, int));

extern int glob_char_p PARAMS((const char *));
extern char *quote_globbing_chars PARAMS((const char *));

 
extern char **shell_glob_filename PARAMS((const char *, int));

 

struct ign {
  char *val;
  int len, flags;
};

typedef int sh_iv_item_func_t PARAMS((struct ign *));

struct ignorevar {
  char *varname;	 
  struct ign *ignores;	 
  int num_ignores;	 
  char *last_ignoreval;	 
  sh_iv_item_func_t *item_func;  
};

extern void setup_ignore_patterns PARAMS((struct ignorevar *));

extern void setup_glob_ignore PARAMS((char *));
extern int should_ignore_glob_matches PARAMS((void));
extern void ignore_glob_matches PARAMS((char **));

#endif
