

#ifndef _SPL_DKIO_H
#define	_SPL_DKIO_H

#define	DFL_SZ(num_exts) \
	(sizeof (dkioc_free_list_t) + (num_exts - 1) * 16)

#define	DKIOC		(0x04 << 8)
#define	DKIOCFLUSHWRITECACHE	(DKIOC|34)	


#define	DKIOCFREE	(DKIOC|50)

#endif 
