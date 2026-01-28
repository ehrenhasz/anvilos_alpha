

#ifndef _NLS_UCS2_UTILS_H
#define _NLS_UCS2_UTILS_H

#include <asm/byteorder.h>
#include <linux/types.h>
#include <linux/nls.h>
#include <linux/unicode.h>
#include "nls_ucs2_data.h"


#define UNI_ASTERISK    ((__u16)('*' + 0xF000))
#define UNI_QUESTION    ((__u16)('?' + 0xF000))
#define UNI_COLON       ((__u16)(':' + 0xF000))
#define UNI_GRTRTHAN    ((__u16)('>' + 0xF000))
#define UNI_LESSTHAN    ((__u16)('<' + 0xF000))
#define UNI_PIPE        ((__u16)('|' + 0xF000))
#define UNI_SLASH       ((__u16)('\\' + 0xF000))


static inline wchar_t *UniStrcat(wchar_t *ucs1, const wchar_t *ucs2)
{
	wchar_t *anchor = ucs1;	

	while (*ucs1++)
	;	
	ucs1--;			
	while ((*ucs1++ = *ucs2++))
	;	
	return anchor;
}


static inline wchar_t *UniStrchr(const wchar_t *ucs, wchar_t uc)
{
	while ((*ucs != uc) && *ucs)
		ucs++;

	if (*ucs == uc)
		return (wchar_t *)ucs;
	return NULL;
}


static inline int UniStrcmp(const wchar_t *ucs1, const wchar_t *ucs2)
{
	while ((*ucs1 == *ucs2) && *ucs1) {
		ucs1++;
		ucs2++;
	}
	return (int)*ucs1 - (int)*ucs2;
}


static inline wchar_t *UniStrcpy(wchar_t *ucs1, const wchar_t *ucs2)
{
	wchar_t *anchor = ucs1;	

	while ((*ucs1++ = *ucs2++))
	;
	return anchor;
}


static inline size_t UniStrlen(const wchar_t *ucs1)
{
	int i = 0;

	while (*ucs1++)
		i++;
	return i;
}


static inline size_t UniStrnlen(const wchar_t *ucs1, int maxlen)
{
	int i = 0;

	while (*ucs1++) {
		i++;
		if (i >= maxlen)
			break;
	}
	return i;
}


static inline wchar_t *UniStrncat(wchar_t *ucs1, const wchar_t *ucs2, size_t n)
{
	wchar_t *anchor = ucs1;	

	while (*ucs1++)
	;
	ucs1--;			
	while (n-- && (*ucs1 = *ucs2)) {	
		ucs1++;
		ucs2++;
	}
	*ucs1 = 0;		
	return anchor;
}


static inline int UniStrncmp(const wchar_t *ucs1, const wchar_t *ucs2, size_t n)
{
	if (!n)
		return 0;	
	while ((*ucs1 == *ucs2) && *ucs1 && --n) {
		ucs1++;
		ucs2++;
	}
	return (int)*ucs1 - (int)*ucs2;
}


static inline int
UniStrncmp_le(const wchar_t *ucs1, const wchar_t *ucs2, size_t n)
{
	if (!n)
		return 0;	
	while ((*ucs1 == __le16_to_cpu(*ucs2)) && *ucs1 && --n) {
		ucs1++;
		ucs2++;
	}
	return (int)*ucs1 - (int)__le16_to_cpu(*ucs2);
}


static inline wchar_t *UniStrncpy(wchar_t *ucs1, const wchar_t *ucs2, size_t n)
{
	wchar_t *anchor = ucs1;

	while (n-- && *ucs2)	
		*ucs1++ = *ucs2++;

	n++;
	while (n--)		
		*ucs1++ = 0;
	return anchor;
}


static inline wchar_t *UniStrncpy_le(wchar_t *ucs1, const wchar_t *ucs2, size_t n)
{
	wchar_t *anchor = ucs1;

	while (n-- && *ucs2)	
		*ucs1++ = __le16_to_cpu(*ucs2++);

	n++;
	while (n--)		
		*ucs1++ = 0;
	return anchor;
}


static inline wchar_t *UniStrstr(const wchar_t *ucs1, const wchar_t *ucs2)
{
	const wchar_t *anchor1 = ucs1;
	const wchar_t *anchor2 = ucs2;

	while (*ucs1) {
		if (*ucs1 == *ucs2) {
			
			ucs1++;
			ucs2++;
		} else {
			if (!*ucs2)	
				return (wchar_t *)anchor1;
			ucs1 = ++anchor1;	
			ucs2 = anchor2;
		}
	}

	if (!*ucs2)		
		return (wchar_t *)anchor1;	
	return NULL;		
}

#ifndef UNIUPR_NOUPPER

static inline wchar_t UniToupper(register wchar_t uc)
{
	register const struct UniCaseRange *rp;

	if (uc < sizeof(NlsUniUpperTable)) {
		
		return uc + NlsUniUpperTable[uc];	
	}

	rp = NlsUniUpperRange;	
	while (rp->start) {
		if (uc < rp->start)	
			return uc;	
		if (uc <= rp->end)	
			return uc + rp->table[uc - rp->start];
		rp++;	
	}
	return uc;		
}


static inline __le16 *UniStrupr(register __le16 *upin)
{
	register __le16 *up;

	up = upin;
	while (*up) {		
		*up = cpu_to_le16(UniToupper(le16_to_cpu(*up)));
		up++;
	}
	return upin;		
}
#endif				

#endif 
