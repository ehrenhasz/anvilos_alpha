




#ifndef _RPC_DES_H_
#define _RPC_DES_H_

#define DES_MAXLEN 	65536	
#define DES_QUICKLEN	16	

enum desdir { ENCRYPT, DECRYPT };
enum desmode { CBC, ECB };


struct desparams {
	u_char des_key[8];	
	enum desdir des_dir;	
	enum desmode des_mode;	
	u_char des_ivec[8];	
	unsigned des_len;	
	union {
		u_char UDES_data[DES_QUICKLEN];
		u_char *UDES_buf;
	} UDES;
#	define des_data UDES.UDES_data	
#	define des_buf	UDES.UDES_buf	
};

#ifdef notdef




#define	DESIOCBLOCK	_IOWR('d', 6, struct desparams)


#define DESIOCQUICK	_IOWR('d', 7, struct desparams) 

#endif


extern int _des_crypt( char *, unsigned, struct desparams * );

#endif
