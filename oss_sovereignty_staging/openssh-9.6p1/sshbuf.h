 
 

#ifndef _SSHBUF_H
#define _SSHBUF_H

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef WITH_OPENSSL
# include <openssl/bn.h>
# ifdef OPENSSL_HAS_ECC
#  include <openssl/ec.h>
# endif  
#endif  

#define SSHBUF_SIZE_MAX		0x8000000	 
#define SSHBUF_REFS_MAX		0x100000	 
#define SSHBUF_MAX_BIGNUM	(16384 / 8)	 
#define SSHBUF_MAX_ECPOINT	((528 * 2 / 8) + 1)  

struct sshbuf;

 
struct sshbuf *sshbuf_new(void);

 
struct sshbuf *sshbuf_from(const void *blob, size_t len);

 
struct sshbuf *sshbuf_fromb(struct sshbuf *buf);

 
int	sshbuf_froms(struct sshbuf *buf, struct sshbuf **bufp);

 
void	sshbuf_free(struct sshbuf *buf);

 
void	sshbuf_reset(struct sshbuf *buf);

 
size_t	sshbuf_max_size(const struct sshbuf *buf);

 
int	sshbuf_set_max_size(struct sshbuf *buf, size_t max_size);

 
size_t	sshbuf_len(const struct sshbuf *buf);

 
size_t	sshbuf_avail(const struct sshbuf *buf);

 
const u_char *sshbuf_ptr(const struct sshbuf *buf);

 
u_char *sshbuf_mutable_ptr(const struct sshbuf *buf);

 
int	sshbuf_check_reserve(const struct sshbuf *buf, size_t len);

 
int	sshbuf_allocate(struct sshbuf *buf, size_t len);

 
int	sshbuf_reserve(struct sshbuf *buf, size_t len, u_char **dpp);

 
int	sshbuf_consume(struct sshbuf *buf, size_t len);

 
int	sshbuf_consume_end(struct sshbuf *buf, size_t len);

 
int	sshbuf_get(struct sshbuf *buf, void *v, size_t len);
int	sshbuf_put(struct sshbuf *buf, const void *v, size_t len);
int	sshbuf_putb(struct sshbuf *buf, const struct sshbuf *v);

 
int	sshbuf_putf(struct sshbuf *buf, const char *fmt, ...)
	    __attribute__((format(printf, 2, 3)));
int	sshbuf_putfv(struct sshbuf *buf, const char *fmt, va_list ap);

 
int	sshbuf_get_u64(struct sshbuf *buf, u_int64_t *valp);
int	sshbuf_get_u32(struct sshbuf *buf, u_int32_t *valp);
int	sshbuf_get_u16(struct sshbuf *buf, u_int16_t *valp);
int	sshbuf_get_u8(struct sshbuf *buf, u_char *valp);
int	sshbuf_put_u64(struct sshbuf *buf, u_int64_t val);
int	sshbuf_put_u32(struct sshbuf *buf, u_int32_t val);
int	sshbuf_put_u16(struct sshbuf *buf, u_int16_t val);
int	sshbuf_put_u8(struct sshbuf *buf, u_char val);

 
int	sshbuf_peek_u64(const struct sshbuf *buf, size_t offset,
    u_int64_t *valp);
int	sshbuf_peek_u32(const struct sshbuf *buf, size_t offset,
    u_int32_t *valp);
int	sshbuf_peek_u16(const struct sshbuf *buf, size_t offset,
    u_int16_t *valp);
int	sshbuf_peek_u8(const struct sshbuf *buf, size_t offset,
    u_char *valp);

 
int sshbuf_poke_u64(struct sshbuf *buf, size_t offset, u_int64_t val);
int sshbuf_poke_u32(struct sshbuf *buf, size_t offset, u_int32_t val);
int sshbuf_poke_u16(struct sshbuf *buf, size_t offset, u_int16_t val);
int sshbuf_poke_u8(struct sshbuf *buf, size_t offset, u_char val);
int sshbuf_poke(struct sshbuf *buf, size_t offset, void *v, size_t len);

 
int	sshbuf_get_string(struct sshbuf *buf, u_char **valp, size_t *lenp);
int	sshbuf_get_cstring(struct sshbuf *buf, char **valp, size_t *lenp);
int	sshbuf_get_stringb(struct sshbuf *buf, struct sshbuf *v);
int	sshbuf_put_string(struct sshbuf *buf, const void *v, size_t len);
int	sshbuf_put_cstring(struct sshbuf *buf, const char *v);
int	sshbuf_put_stringb(struct sshbuf *buf, const struct sshbuf *v);

 
int	sshbuf_get_string_direct(struct sshbuf *buf, const u_char **valp,
	    size_t *lenp);

 
#define sshbuf_skip_string(buf) sshbuf_get_string_direct(buf, NULL, NULL)

 
int	sshbuf_peek_string_direct(const struct sshbuf *buf, const u_char **valp,
	    size_t *lenp);

 
int	sshbuf_put_bignum2_bytes(struct sshbuf *buf, const void *v, size_t len);
int	sshbuf_get_bignum2_bytes_direct(struct sshbuf *buf,
	    const u_char **valp, size_t *lenp);
#ifdef WITH_OPENSSL
int	sshbuf_get_bignum2(struct sshbuf *buf, BIGNUM **valp);
int	sshbuf_put_bignum2(struct sshbuf *buf, const BIGNUM *v);
# ifdef OPENSSL_HAS_ECC
int	sshbuf_get_ec(struct sshbuf *buf, EC_POINT *v, const EC_GROUP *g);
int	sshbuf_get_eckey(struct sshbuf *buf, EC_KEY *v);
int	sshbuf_put_ec(struct sshbuf *buf, const EC_POINT *v, const EC_GROUP *g);
int	sshbuf_put_eckey(struct sshbuf *buf, const EC_KEY *v);
# endif  
#endif  

 
void	sshbuf_dump(const struct sshbuf *buf, FILE *f);

 
void	sshbuf_dump_data(const void *s, size_t len, FILE *f);

 
char	*sshbuf_dtob16(struct sshbuf *buf);

 
char	*sshbuf_dtob64_string(const struct sshbuf *buf, int wrap);
int	sshbuf_dtob64(const struct sshbuf *d, struct sshbuf *b64, int wrap);
 
int	sshbuf_dtourlb64(const struct sshbuf *d, struct sshbuf *b64, int wrap);

 
int	sshbuf_b64tod(struct sshbuf *buf, const char *b64);

 
int	sshbuf_cmp(const struct sshbuf *b, size_t offset,
    const void *s, size_t len);

 
int
sshbuf_find(const struct sshbuf *b, size_t start_offset,
    const void *s, size_t len, size_t *offsetp);

 
char *sshbuf_dup_string(struct sshbuf *buf);

 
int sshbuf_load_fd(int, struct sshbuf **)
    __attribute__((__nonnull__ (2)));
int sshbuf_load_file(const char *, struct sshbuf **)
    __attribute__((__nonnull__ (2)));

 
int sshbuf_write_file(const char *path, struct sshbuf *buf)
    __attribute__((__nonnull__ (2)));

 
int sshbuf_read(int, struct sshbuf *, size_t, size_t *)
    __attribute__((__nonnull__ (2)));

 
#define PEEK_U64(p) \
	(((u_int64_t)(((const u_char *)(p))[0]) << 56) | \
	 ((u_int64_t)(((const u_char *)(p))[1]) << 48) | \
	 ((u_int64_t)(((const u_char *)(p))[2]) << 40) | \
	 ((u_int64_t)(((const u_char *)(p))[3]) << 32) | \
	 ((u_int64_t)(((const u_char *)(p))[4]) << 24) | \
	 ((u_int64_t)(((const u_char *)(p))[5]) << 16) | \
	 ((u_int64_t)(((const u_char *)(p))[6]) << 8) | \
	  (u_int64_t)(((const u_char *)(p))[7]))
#define PEEK_U32(p) \
	(((u_int32_t)(((const u_char *)(p))[0]) << 24) | \
	 ((u_int32_t)(((const u_char *)(p))[1]) << 16) | \
	 ((u_int32_t)(((const u_char *)(p))[2]) << 8) | \
	  (u_int32_t)(((const u_char *)(p))[3]))
#define PEEK_U16(p) \
	(((u_int16_t)(((const u_char *)(p))[0]) << 8) | \
	  (u_int16_t)(((const u_char *)(p))[1]))

#define POKE_U64(p, v) \
	do { \
		const u_int64_t __v = (v); \
		((u_char *)(p))[0] = (__v >> 56) & 0xff; \
		((u_char *)(p))[1] = (__v >> 48) & 0xff; \
		((u_char *)(p))[2] = (__v >> 40) & 0xff; \
		((u_char *)(p))[3] = (__v >> 32) & 0xff; \
		((u_char *)(p))[4] = (__v >> 24) & 0xff; \
		((u_char *)(p))[5] = (__v >> 16) & 0xff; \
		((u_char *)(p))[6] = (__v >> 8) & 0xff; \
		((u_char *)(p))[7] = __v & 0xff; \
	} while (0)
#define POKE_U32(p, v) \
	do { \
		const u_int32_t __v = (v); \
		((u_char *)(p))[0] = (__v >> 24) & 0xff; \
		((u_char *)(p))[1] = (__v >> 16) & 0xff; \
		((u_char *)(p))[2] = (__v >> 8) & 0xff; \
		((u_char *)(p))[3] = __v & 0xff; \
	} while (0)
#define POKE_U16(p, v) \
	do { \
		const u_int16_t __v = (v); \
		((u_char *)(p))[0] = (__v >> 8) & 0xff; \
		((u_char *)(p))[1] = __v & 0xff; \
	} while (0)

 
#ifdef SSHBUF_INTERNAL

 
size_t	sshbuf_alloc(const struct sshbuf *buf);

 
int	sshbuf_set_parent(struct sshbuf *child, struct sshbuf *parent);

 
const struct sshbuf *sshbuf_parent(const struct sshbuf *buf);

 
u_int	sshbuf_refcount(const struct sshbuf *buf);

# define SSHBUF_SIZE_INIT	256		 
# define SSHBUF_SIZE_INC	256		 
# define SSHBUF_PACK_MIN	8192		 

 
 

# ifndef SSHBUF_ABORT
#  define SSHBUF_ABORT()
# endif

# ifdef SSHBUF_DEBUG
#  define SSHBUF_DBG(x) do { \
		printf("%s:%d %s: ", __FILE__, __LINE__, __func__); \
		printf x; \
		printf("\n"); \
		fflush(stdout); \
	} while (0)
# else
#  define SSHBUF_DBG(x)
# endif
#endif  

#endif  
