 
extern long unicode_to_mb (unsigned int code,
                           long (*success) (const char *buf, size_t buflen,
                                            void *callback_arg),
                           long (*failure) (unsigned int code, const char *msg,
                                            void *callback_arg),
                           void *callback_arg);

 
extern void print_unicode_char (FILE *stream, unsigned int code,
                                int exit_on_error);

 
extern long fwrite_success_callback (const char *buf, size_t buflen,
                                     void *callback_arg);

#endif
