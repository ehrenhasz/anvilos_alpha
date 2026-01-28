





#ifndef _DES_DES_CRYPT_H
#define _DES_DES_CRYPT_H

#include <rpc/rpc.h>

#define DES_MAXDATA 8192	
#define DES_DIRMASK (1 << 0)
#define DES_ENCRYPT (0*DES_DIRMASK)	
#define DES_DECRYPT (1*DES_DIRMASK)	


#define DES_DEVMASK (1 << 1)
#define	DES_HW (0*DES_DEVMASK)	 
#define DES_SW (1*DES_DEVMASK)	


#define DESERR_NONE 0	
#define DESERR_NOHWDEVICE 1	
#define DESERR_HWERROR 2	
#define DESERR_BADPARAM 3	

#define DES_FAILED(err) \
	((err) > DESERR_NOHWDEVICE)





#ifdef __cplusplus
extern "C" {
#endif
int cbc_crypt( char *, char *, unsigned int, unsigned int, char *);
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
int ecb_crypt( char *, char *, unsigned int, unsigned int );
#ifdef __cplusplus
}
#endif


#ifdef __cplusplus
extern "C" {
#endif
void des_setparity( char *);
#ifdef __cplusplus
}
#endif

#endif  
