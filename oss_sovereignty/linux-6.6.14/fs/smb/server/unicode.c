
 
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include "glob.h"
#include "unicode.h"
#include "smb_common.h"

 
static int
cifs_mapchar(char *target, const __u16 *from, const struct nls_table *cp,
	     bool mapchar)
{
	int len = 1;
	__u16 src_char;

	src_char = *from;

	if (!mapchar)
		goto cp_convert;

	 
	switch (src_char) {
	case UNI_COLON:
		*target = ':';
		break;
	case UNI_ASTERISK:
		*target = '*';
		break;
	case UNI_QUESTION:
		*target = '?';
		break;
	case UNI_PIPE:
		*target = '|';
		break;
	case UNI_GRTRTHAN:
		*target = '>';
		break;
	case UNI_LESSTHAN:
		*target = '<';
		break;
	default:
		goto cp_convert;
	}

out:
	return len;

cp_convert:
	len = cp->uni2char(src_char, target, NLS_MAX_CHARSET_SIZE);
	if (len <= 0)
		goto surrogate_pair;

	goto out;

surrogate_pair:
	 
	if (strcmp(cp->charset, "utf8"))
		goto unknown;
	len = utf16s_to_utf8s(from, 3, UTF16_LITTLE_ENDIAN, target, 6);
	if (len <= 0)
		goto unknown;
	return len;

unknown:
	*target = '?';
	len = 1;
	goto out;
}

 
static int smb_utf16_bytes(const __le16 *from, int maxbytes,
			   const struct nls_table *codepage)
{
	int i, j;
	int charlen, outlen = 0;
	int maxwords = maxbytes / 2;
	char tmp[NLS_MAX_CHARSET_SIZE];
	__u16 ftmp[3];

	for (i = 0; i < maxwords; i++) {
		ftmp[0] = get_unaligned_le16(&from[i]);
		if (ftmp[0] == 0)
			break;
		for (j = 1; j <= 2; j++) {
			if (i + j < maxwords)
				ftmp[j] = get_unaligned_le16(&from[i + j]);
			else
				ftmp[j] = 0;
		}

		charlen = cifs_mapchar(tmp, ftmp, codepage, 0);
		if (charlen > 0)
			outlen += charlen;
		else
			outlen++;
	}

	return outlen;
}

 
static int smb_from_utf16(char *to, const __le16 *from, int tolen, int fromlen,
			  const struct nls_table *codepage, bool mapchar)
{
	int i, j, charlen, safelen;
	int outlen = 0;
	int nullsize = nls_nullsize(codepage);
	int fromwords = fromlen / 2;
	char tmp[NLS_MAX_CHARSET_SIZE];
	__u16 ftmp[3];	 

	 
	safelen = tolen - (NLS_MAX_CHARSET_SIZE + nullsize);

	for (i = 0; i < fromwords; i++) {
		ftmp[0] = get_unaligned_le16(&from[i]);
		if (ftmp[0] == 0)
			break;
		for (j = 1; j <= 2; j++) {
			if (i + j < fromwords)
				ftmp[j] = get_unaligned_le16(&from[i + j]);
			else
				ftmp[j] = 0;
		}

		 
		if (outlen >= safelen) {
			charlen = cifs_mapchar(tmp, ftmp, codepage, mapchar);
			if ((outlen + charlen) > (tolen - nullsize))
				break;
		}

		 
		charlen = cifs_mapchar(&to[outlen], ftmp, codepage, mapchar);
		outlen += charlen;

		 
		if (charlen == 4)
			i++;
		else if (charlen >= 5)
			 
			i += 2;
	}

	 
	for (i = 0; i < nullsize; i++)
		to[outlen++] = 0;

	return outlen;
}

 
int smb_strtoUTF16(__le16 *to, const char *from, int len,
		   const struct nls_table *codepage)
{
	int charlen;
	int i;
	wchar_t wchar_to;  

	 
	if (!strcmp(codepage->charset, "utf8")) {
		 
		i  = utf8s_to_utf16s(from, len, UTF16_LITTLE_ENDIAN,
				     (wchar_t *)to, len);

		 
		if (i >= 0)
			goto success;
		 
	}

	for (i = 0; len > 0 && *from; i++, from += charlen, len -= charlen) {
		charlen = codepage->char2uni(from, len, &wchar_to);
		if (charlen < 1) {
			 
			wchar_to = 0x003f;
			charlen = 1;
		}
		put_unaligned_le16(wchar_to, &to[i]);
	}

success:
	put_unaligned_le16(0, &to[i]);
	return i;
}

 
char *smb_strndup_from_utf16(const char *src, const int maxlen,
			     const bool is_unicode,
			     const struct nls_table *codepage)
{
	int len, ret;
	char *dst;

	if (is_unicode) {
		len = smb_utf16_bytes((__le16 *)src, maxlen, codepage);
		len += nls_nullsize(codepage);
		dst = kmalloc(len, GFP_KERNEL);
		if (!dst)
			return ERR_PTR(-ENOMEM);
		ret = smb_from_utf16(dst, (__le16 *)src, len, maxlen, codepage,
				     false);
		if (ret < 0) {
			kfree(dst);
			return ERR_PTR(-EINVAL);
		}
	} else {
		len = strnlen(src, maxlen);
		len++;
		dst = kmalloc(len, GFP_KERNEL);
		if (!dst)
			return ERR_PTR(-ENOMEM);
		strscpy(dst, src, len);
	}

	return dst;
}

 
 
int smbConvertToUTF16(__le16 *target, const char *source, int srclen,
		      const struct nls_table *cp, int mapchars)
{
	int i, j, charlen;
	char src_char;
	__le16 dst_char;
	wchar_t tmp;
	wchar_t wchar_to[6];	 
	int ret;
	unicode_t u;

	if (!mapchars)
		return smb_strtoUTF16(target, source, srclen, cp);

	for (i = 0, j = 0; i < srclen; j++) {
		src_char = source[i];
		charlen = 1;
		switch (src_char) {
		case 0:
			put_unaligned(0, &target[j]);
			return j;
		case ':':
			dst_char = cpu_to_le16(UNI_COLON);
			break;
		case '*':
			dst_char = cpu_to_le16(UNI_ASTERISK);
			break;
		case '?':
			dst_char = cpu_to_le16(UNI_QUESTION);
			break;
		case '<':
			dst_char = cpu_to_le16(UNI_LESSTHAN);
			break;
		case '>':
			dst_char = cpu_to_le16(UNI_GRTRTHAN);
			break;
		case '|':
			dst_char = cpu_to_le16(UNI_PIPE);
			break;
		 
		default:
			charlen = cp->char2uni(source + i, srclen - i, &tmp);
			dst_char = cpu_to_le16(tmp);

			 
			if (charlen > 0)
				goto ctoUTF16;

			 
			if (strcmp(cp->charset, "utf8"))
				goto unknown;
			if (*(source + i) & 0x80) {
				charlen = utf8_to_utf32(source + i, 6, &u);
				if (charlen < 0)
					goto unknown;
			} else
				goto unknown;
			ret  = utf8s_to_utf16s(source + i, charlen,
					UTF16_LITTLE_ENDIAN,
					wchar_to, 6);
			if (ret < 0)
				goto unknown;

			i += charlen;
			dst_char = cpu_to_le16(*wchar_to);
			if (charlen <= 3)
				 
				put_unaligned(dst_char, &target[j]);
			else if (charlen == 4) {
				 
				put_unaligned(dst_char, &target[j]);
				dst_char = cpu_to_le16(*(wchar_to + 1));
				j++;
				put_unaligned(dst_char, &target[j]);
			} else if (charlen >= 5) {
				 
				put_unaligned(dst_char, &target[j]);
				dst_char = cpu_to_le16(*(wchar_to + 1));
				j++;
				put_unaligned(dst_char, &target[j]);
				dst_char = cpu_to_le16(*(wchar_to + 2));
				j++;
				put_unaligned(dst_char, &target[j]);
			}
			continue;

unknown:
			dst_char = cpu_to_le16(0x003f);
			charlen = 1;
		}

ctoUTF16:
		 
		i += charlen;
		put_unaligned(dst_char, &target[j]);
	}

	return j;
}
