

#ifndef _SMB_UNICODE_H
#define _SMB_UNICODE_H

#include <asm/byteorder.h>
#include <linux/types.h>
#include <linux/nls.h>
#include <linux/unicode.h>
#include "../../nls/nls_ucs2_utils.h"

#ifdef __KERNEL__
int smb_strtoUTF16(__le16 *to, const char *from, int len,
		   const struct nls_table *codepage);
char *smb_strndup_from_utf16(const char *src, const int maxlen,
			     const bool is_unicode,
			     const struct nls_table *codepage);
int smbConvertToUTF16(__le16 *target, const char *source, int srclen,
		      const struct nls_table *cp, int mapchars);
char *ksmbd_extract_sharename(struct unicode_map *um, const char *treename);
#endif

#endif 
