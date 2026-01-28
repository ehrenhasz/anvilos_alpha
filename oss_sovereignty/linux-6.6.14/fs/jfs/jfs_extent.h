#ifndef	_H_JFS_EXTENT
#define _H_JFS_EXTENT
#define	INOHINT(ip)	\
	(addressPXD(&(JFS_IP(ip)->ixpxd)) + lengthPXD(&(JFS_IP(ip)->ixpxd)) - 1)
extern int	extAlloc(struct inode *, s64, s64, xad_t *, bool);
extern int	extHint(struct inode *, s64, xad_t *);
extern int	extRecord(struct inode *, xad_t *);
#endif	 
