


#ifndef _PRIVATE_RIJNDAEL_H
#define _PRIVATE_RIJNDAEL_H

#define AES_MAXKEYBITS	(256)
#define AES_MAXKEYBYTES	(AES_MAXKEYBITS/8)

#define AES_MAXROUNDS	14

typedef unsigned char	u8;
typedef unsigned short	u16;
typedef unsigned int	u32;

int	rijndaelKeySetupEnc(unsigned int [], const unsigned char [], int);
void	rijndaelEncrypt(const unsigned int [], int, const u8 [16], u8 [16]);


typedef struct {
	int	decrypt;
	int	Nr;		
	u32	ek[4*(AES_MAXROUNDS + 1)];	
	u32	dk[4*(AES_MAXROUNDS + 1)];	
} rijndael_ctx;

void	 rijndael_set_key(rijndael_ctx *, u_char *, int, int);
void	 rijndael_decrypt(rijndael_ctx *, u_char *, u_char *);
void	 rijndael_encrypt(rijndael_ctx *, u_char *, u_char *);

#endif 
