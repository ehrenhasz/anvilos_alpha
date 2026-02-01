 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#ifdef __cplusplus
extern "C" {
#endif


 
extern const char * gl_locale_name (int category, const char *categoryname);

 
extern const char * gl_locale_name_thread (int category, const char *categoryname);

 
extern const char * gl_locale_name_posix (int category, const char *categoryname);

 
extern const char * gl_locale_name_environ (int category, const char *categoryname);

 
extern const char * gl_locale_name_default (void)
#if !(HAVE_CFPREFERENCESCOPYAPPVALUE || defined _WIN32 || defined __CYGWIN__)
  _GL_ATTRIBUTE_CONST
#endif
  ;

#ifdef __cplusplus
}
#endif

#endif  
