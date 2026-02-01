 

 

#ifndef _RL_TYPEDEFS_H_
#define _RL_TYPEDEFS_H_

#ifdef __cplusplus
extern "C" {
#endif

 

#if !defined (_FUNCTION_DEF)
#  define _FUNCTION_DEF

#if defined(__GNUC__) || defined(__clang__)
typedef int Function () __attribute__((deprecated));
typedef void VFunction () __attribute__((deprecated));
typedef char *CPFunction () __attribute__((deprecated));
typedef char **CPPFunction () __attribute__((deprecated));
#else
typedef int Function ();
typedef void VFunction ();
typedef char *CPFunction ();
typedef char **CPPFunction ();
#endif

#endif  

 

#if !defined (_RL_FUNCTION_TYPEDEF)
#  define _RL_FUNCTION_TYPEDEF

 
typedef int rl_command_func_t (int, int);

 
typedef char *rl_compentry_func_t (const char *, int);
typedef char **rl_completion_func_t (const char *, int, int);

typedef char *rl_quote_func_t (char *, int, char *);
typedef char *rl_dequote_func_t (char *, int);

typedef int rl_compignore_func_t (char **);

typedef void rl_compdisp_func_t (char **, int, int);

 
typedef int rl_hook_func_t (void);

 
typedef int rl_getc_func_t (FILE *);

 
typedef int rl_linebuf_func_t (char *, int);

 
typedef int rl_intfunc_t (int);
#define rl_ivoidfunc_t rl_hook_func_t
typedef int rl_icpfunc_t (char *);
typedef int rl_icppfunc_t (char **);

typedef void rl_voidfunc_t (void);
typedef void rl_vintfunc_t (int);
typedef void rl_vcpfunc_t (char *);
typedef void rl_vcppfunc_t (char **);

typedef char *rl_cpvfunc_t (void);
typedef char *rl_cpifunc_t (int);
typedef char *rl_cpcpfunc_t (char  *);
typedef char *rl_cpcppfunc_t (char  **);

#endif  

#ifdef __cplusplus
}
#endif

#endif  
