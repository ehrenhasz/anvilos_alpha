#ifndef _SYS_PATHNAME_H
#define	_SYS_PATHNAME_H
#ifdef	__cplusplus
extern "C" {
#endif
typedef struct pathname {
	char	*pn_buf;		 
	size_t	pn_bufsize;		 
} pathname_t;
extern void	pn_alloc(struct pathname *);
extern void	pn_alloc_sz(struct pathname *, size_t);
extern void	pn_free(struct pathname *);
#ifdef	__cplusplus
}
#endif
#endif	 
