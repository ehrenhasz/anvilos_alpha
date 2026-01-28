#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/zio_compress.h>
size_t
zle_compress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	uchar_t *src = s_start;
	uchar_t *dst = d_start;
	uchar_t *s_end = src + s_len;
	uchar_t *d_end = dst + d_len;
	while (src < s_end && dst < d_end - 1) {
		uchar_t *first = src;
		uchar_t *len = dst++;
		if (src[0] == 0) {
			uchar_t *last = src + (256 - n);
			while (src < MIN(last, s_end) && src[0] == 0)
				src++;
			*len = src - first - 1 + n;
		} else {
			uchar_t *last = src + n;
			if (d_end - dst < n)
				break;
			while (src < MIN(last, s_end) - 1 && (src[0] | src[1]))
				*dst++ = *src++;
			if (src[0])
				*dst++ = *src++;
			*len = src - first - 1;
		}
	}
	return (src == s_end ? dst - (uchar_t *)d_start : s_len);
}
int
zle_decompress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	uchar_t *src = s_start;
	uchar_t *dst = d_start;
	uchar_t *s_end = src + s_len;
	uchar_t *d_end = dst + d_len;
	while (src < s_end && dst < d_end) {
		int len = 1 + *src++;
		if (len <= n) {
			if (src + len > s_end || dst + len > d_end)
				return (-1);
			while (len-- != 0)
				*dst++ = *src++;
		} else {
			len -= n;
			if (dst + len > d_end)
				return (-1);
			while (len-- != 0)
				*dst++ = 0;
		}
	}
	return (dst == d_end ? 0 : -1);
}
