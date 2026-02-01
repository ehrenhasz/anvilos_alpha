 

#ifndef VERSION_ETC_H
# define VERSION_ETC_H 1

 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <stdarg.h>
# include <stdio.h>

# ifdef __cplusplus
extern "C"
{
# endif

extern const char version_etc_copyright[];

 

 
extern void version_etc_arn (FILE *stream,
                             const char *command_name, const char *package,
                             const char *version,
                             const char * const * authors, size_t n_authors);

 
extern void version_etc_ar (FILE *stream,
                            const char *command_name, const char *package,
                            const char *version, const char * const * authors);

 
extern void version_etc_va (FILE *stream,
                            const char *command_name, const char *package,
                            const char *version, va_list authors);

 
extern void version_etc (FILE *stream,
                         const char *command_name, const char *package,
                         const char *version,
                           ...)
  _GL_ATTRIBUTE_SENTINEL ((0));

 
extern void emit_bug_reporting_address (void);

# ifdef __cplusplus
}
# endif

#endif  
