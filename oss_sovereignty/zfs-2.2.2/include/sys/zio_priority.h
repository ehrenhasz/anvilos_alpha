 
 
#ifndef	_ZIO_PRIORITY_H
#define	_ZIO_PRIORITY_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum zio_priority {
	ZIO_PRIORITY_SYNC_READ,
	ZIO_PRIORITY_SYNC_WRITE,	 
	ZIO_PRIORITY_ASYNC_READ,	 
	ZIO_PRIORITY_ASYNC_WRITE,	 
	ZIO_PRIORITY_SCRUB,		 
	ZIO_PRIORITY_REMOVAL,		 
	ZIO_PRIORITY_INITIALIZING,	 
	ZIO_PRIORITY_TRIM,		 
	ZIO_PRIORITY_REBUILD,		 
	ZIO_PRIORITY_NUM_QUEUEABLE,
	ZIO_PRIORITY_NOW,		 
} zio_priority_t;

#ifdef	__cplusplus
}
#endif

#endif	 
