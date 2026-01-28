



#ifndef _LIBZFSBOOTENV_H
#define	_LIBZFSBOOTENV_H extern __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

typedef enum lzbe_flags {
	lzbe_add,	
	lzbe_replace	
} lzbe_flags_t;

_LIBZFSBOOTENV_H int lzbe_nvlist_get(const char *, const char *, void **);
_LIBZFSBOOTENV_H int lzbe_nvlist_set(const char *, const char *, void *);
_LIBZFSBOOTENV_H void lzbe_nvlist_free(void *);
_LIBZFSBOOTENV_H int lzbe_add_pair(void *, const char *, const char *, void *,
    size_t);
_LIBZFSBOOTENV_H int lzbe_remove_pair(void *, const char *);
_LIBZFSBOOTENV_H int lzbe_set_boot_device(const char *, lzbe_flags_t,
    const char *);
_LIBZFSBOOTENV_H int lzbe_get_boot_device(const char *, char **);
_LIBZFSBOOTENV_H int lzbe_bootenv_print(const char *, const char *, FILE *);

#ifdef __cplusplus
}
#endif

#endif 
