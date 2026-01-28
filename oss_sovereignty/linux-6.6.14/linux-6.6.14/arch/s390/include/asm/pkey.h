#ifndef _KAPI_PKEY_H
#define _KAPI_PKEY_H
#include <linux/ioctl.h>
#include <linux/types.h>
#include <uapi/asm/pkey.h>
int pkey_keyblob2pkey(const u8 *key, u32 keylen,
		      u8 *protkey, u32 *protkeylen, u32 *protkeytype);
#endif  
