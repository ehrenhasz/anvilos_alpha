#ifndef _CHACHA_S390_H
#define _CHACHA_S390_H
void chacha20_vx(u8 *out, const u8 *inp, size_t len, const u32 *key,
		 const u32 *counter);
#endif  
