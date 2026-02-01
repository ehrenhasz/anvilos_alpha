 


#ifdef __cplusplus
extern "C" {
#endif


 
extern const char *program_name;

 
extern void set_program_name (const char *argv0);

#if ENABLE_RELOCATABLE

 
extern void set_program_name_and_installdir (const char *argv0,
                                             const char *orig_installprefix,
                                             const char *orig_installdir);
#undef set_program_name
#define set_program_name(ARG0) \
  set_program_name_and_installdir (ARG0, INSTALLPREFIX, INSTALLDIR)

 
extern char *get_full_program_name (void);

#endif


#ifdef __cplusplus
}
#endif


#endif  
