#ifndef _CIFS_UNICODE_H
#define _CIFS_UNICODE_H
#include <asm/byteorder.h>
#include <linux/types.h>
#include <linux/nls.h>
#include "../../nls/nls_ucs2_utils.h"
#define SFM_DOUBLEQUOTE ((__u16) 0xF020)
#define SFM_ASTERISK    ((__u16) 0xF021)
#define SFM_QUESTION    ((__u16) 0xF025)
#define SFM_COLON       ((__u16) 0xF022)
#define SFM_GRTRTHAN    ((__u16) 0xF024)
#define SFM_LESSTHAN    ((__u16) 0xF023)
#define SFM_PIPE        ((__u16) 0xF027)
#define SFM_SLASH       ((__u16) 0xF026)
#define SFM_SPACE	((__u16) 0xF028)
#define SFM_PERIOD	((__u16) 0xF029)
#define NO_MAP_UNI_RSVD		0
#define SFM_MAP_UNI_RSVD	1
#define SFU_MAP_UNI_RSVD	2
#ifdef __KERNEL__
int cifs_from_utf16(char *to, const __le16 *from, int tolen, int fromlen,
		    const struct nls_table *cp, int map_type);
int cifs_utf16_bytes(const __le16 *from, int maxbytes,
		     const struct nls_table *codepage);
int cifs_strtoUTF16(__le16 *, const char *, int, const struct nls_table *);
char *cifs_strndup_from_utf16(const char *src, const int maxlen,
			      const bool is_unicode,
			      const struct nls_table *codepage);
extern int cifsConvertToUTF16(__le16 *target, const char *source, int maxlen,
			      const struct nls_table *cp, int mapChars);
extern int cifs_remap(struct cifs_sb_info *cifs_sb);
extern __le16 *cifs_strndup_to_utf16(const char *src, const int maxlen,
				     int *utf16_len, const struct nls_table *cp,
				     int remap);
#endif
wchar_t cifs_toupper(wchar_t in);
#endif  
