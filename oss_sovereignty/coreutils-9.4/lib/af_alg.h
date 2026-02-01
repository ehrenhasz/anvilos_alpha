 

 

#ifndef AF_ALG_H
# define AF_ALG_H 1

# include <stdio.h>
# include <errno.h>

# ifdef __cplusplus
extern "C" {
# endif

# if USE_LINUX_CRYPTO_API

 
int
afalg_buffer (const char *buffer, size_t len, const char *alg,
              void *resblock, ssize_t hashlen);

 
int
afalg_stream (FILE *stream, const char *alg,
              void *resblock, ssize_t hashlen);

# else

static inline int
afalg_buffer (const char *buffer, size_t len, const char *alg,
              void *resblock, ssize_t hashlen)
{
  return -EAFNOSUPPORT;
}

static inline int
afalg_stream (FILE *stream, const char *alg,
              void *resblock, ssize_t hashlen)
{
  return -EAFNOSUPPORT;
}

# endif

# ifdef __cplusplus
}
# endif

#endif  
