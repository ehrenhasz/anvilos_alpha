 
# if !_GL_CONFIG_H_INCLUDED
#  error "Please include config.h first."
# endif

# include <errno.h>
# include <stddef.h>

 
# define STACKBUF_LEN 256

 
# if REPLACE_STRERROR_0 \
     || GNULIB_defined_ESOCK \
     || GNULIB_defined_ESTREAMS \
     || GNULIB_defined_EWINSOCK \
     || GNULIB_defined_ENOMSG \
     || GNULIB_defined_EIDRM \
     || GNULIB_defined_ENOLINK \
     || GNULIB_defined_EPROTO \
     || GNULIB_defined_EMULTIHOP \
     || GNULIB_defined_EBADMSG \
     || GNULIB_defined_EOVERFLOW \
     || GNULIB_defined_ENOTSUP \
     || GNULIB_defined_ENETRESET \
     || GNULIB_defined_ECONNABORTED \
     || GNULIB_defined_ESTALE \
     || GNULIB_defined_EDQUOT \
     || GNULIB_defined_ECANCELED \
     || GNULIB_defined_EOWNERDEAD \
     || GNULIB_defined_ENOTRECOVERABLE \
     || GNULIB_defined_EILSEQ
extern const char *strerror_override (int errnum) _GL_ATTRIBUTE_CONST;
# else
#  define strerror_override(ignored) NULL
#  define GNULIB_defined_strerror_override_macro 1
# endif

#endif  
