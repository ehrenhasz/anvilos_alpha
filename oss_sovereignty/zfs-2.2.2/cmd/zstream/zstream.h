 

 

#ifndef	_ZSTREAM_H
#define	_ZSTREAM_H

#ifdef	__cplusplus
extern "C" {
#endif

extern void *safe_calloc(size_t n);
extern int sfread(void *buf, size_t size, FILE *fp);
extern void *safe_malloc(size_t size);
extern int zstream_do_redup(int, char *[]);
extern int zstream_do_dump(int, char *[]);
extern int zstream_do_decompress(int argc, char *argv[]);
extern int zstream_do_recompress(int argc, char *argv[]);
extern int zstream_do_token(int, char *[]);
extern void zstream_usage(void);

#ifdef	__cplusplus
}
#endif

#endif	 
