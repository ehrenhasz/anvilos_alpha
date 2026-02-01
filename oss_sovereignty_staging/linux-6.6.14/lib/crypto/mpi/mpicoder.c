 

#include <linux/bitops.h>
#include <linux/count_zeros.h>
#include <linux/byteorder/generic.h>
#include <linux/scatterlist.h>
#include <linux/string.h>
#include "mpi-internal.h"

#define MAX_EXTERN_SCAN_BYTES (16*1024*1024)
#define MAX_EXTERN_MPI_BITS 16384

 
MPI mpi_read_raw_data(const void *xbuffer, size_t nbytes)
{
	const uint8_t *buffer = xbuffer;
	int i, j;
	unsigned nbits, nlimbs;
	mpi_limb_t a;
	MPI val = NULL;

	while (nbytes > 0 && buffer[0] == 0) {
		buffer++;
		nbytes--;
	}

	nbits = nbytes * 8;
	if (nbits > MAX_EXTERN_MPI_BITS) {
		pr_info("MPI: mpi too large (%u bits)\n", nbits);
		return NULL;
	}
	if (nbytes > 0)
		nbits -= count_leading_zeros(buffer[0]) - (BITS_PER_LONG - 8);

	nlimbs = DIV_ROUND_UP(nbytes, BYTES_PER_MPI_LIMB);
	val = mpi_alloc(nlimbs);
	if (!val)
		return NULL;
	val->nbits = nbits;
	val->sign = 0;
	val->nlimbs = nlimbs;

	if (nbytes > 0) {
		i = BYTES_PER_MPI_LIMB - nbytes % BYTES_PER_MPI_LIMB;
		i %= BYTES_PER_MPI_LIMB;
		for (j = nlimbs; j > 0; j--) {
			a = 0;
			for (; i < BYTES_PER_MPI_LIMB; i++) {
				a <<= 8;
				a |= *buffer++;
			}
			i = 0;
			val->d[j - 1] = a;
		}
	}
	return val;
}
EXPORT_SYMBOL_GPL(mpi_read_raw_data);

MPI mpi_read_from_buffer(const void *xbuffer, unsigned *ret_nread)
{
	const uint8_t *buffer = xbuffer;
	unsigned int nbits, nbytes;
	MPI val;

	if (*ret_nread < 2)
		return ERR_PTR(-EINVAL);
	nbits = buffer[0] << 8 | buffer[1];

	if (nbits > MAX_EXTERN_MPI_BITS) {
		pr_info("MPI: mpi too large (%u bits)\n", nbits);
		return ERR_PTR(-EINVAL);
	}

	nbytes = DIV_ROUND_UP(nbits, 8);
	if (nbytes + 2 > *ret_nread) {
		pr_info("MPI: mpi larger than buffer nbytes=%u ret_nread=%u\n",
				nbytes, *ret_nread);
		return ERR_PTR(-EINVAL);
	}

	val = mpi_read_raw_data(buffer + 2, nbytes);
	if (!val)
		return ERR_PTR(-ENOMEM);

	*ret_nread = nbytes + 2;
	return val;
}
EXPORT_SYMBOL_GPL(mpi_read_from_buffer);

 
int mpi_fromstr(MPI val, const char *str)
{
	int sign = 0;
	int prepend_zero = 0;
	int i, j, c, c1, c2;
	unsigned int nbits, nbytes, nlimbs;
	mpi_limb_t a;

	if (*str == '-') {
		sign = 1;
		str++;
	}

	 
	if (*str == '0' && str[1] == 'x')
		str += 2;

	nbits = strlen(str);
	if (nbits > MAX_EXTERN_SCAN_BYTES) {
		mpi_clear(val);
		return -EINVAL;
	}
	nbits *= 4;
	if ((nbits % 8))
		prepend_zero = 1;

	nbytes = (nbits+7) / 8;
	nlimbs = (nbytes+BYTES_PER_MPI_LIMB-1) / BYTES_PER_MPI_LIMB;

	if (val->alloced < nlimbs)
		mpi_resize(val, nlimbs);

	i = BYTES_PER_MPI_LIMB - (nbytes % BYTES_PER_MPI_LIMB);
	i %= BYTES_PER_MPI_LIMB;
	j = val->nlimbs = nlimbs;
	val->sign = sign;
	for (; j > 0; j--) {
		a = 0;
		for (; i < BYTES_PER_MPI_LIMB; i++) {
			if (prepend_zero) {
				c1 = '0';
				prepend_zero = 0;
			} else
				c1 = *str++;

			if (!c1) {
				mpi_clear(val);
				return -EINVAL;
			}
			c2 = *str++;
			if (!c2) {
				mpi_clear(val);
				return -EINVAL;
			}
			if (c1 >= '0' && c1 <= '9')
				c = c1 - '0';
			else if (c1 >= 'a' && c1 <= 'f')
				c = c1 - 'a' + 10;
			else if (c1 >= 'A' && c1 <= 'F')
				c = c1 - 'A' + 10;
			else {
				mpi_clear(val);
				return -EINVAL;
			}
			c <<= 4;
			if (c2 >= '0' && c2 <= '9')
				c |= c2 - '0';
			else if (c2 >= 'a' && c2 <= 'f')
				c |= c2 - 'a' + 10;
			else if (c2 >= 'A' && c2 <= 'F')
				c |= c2 - 'A' + 10;
			else {
				mpi_clear(val);
				return -EINVAL;
			}
			a <<= 8;
			a |= c;
		}
		i = 0;
		val->d[j-1] = a;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mpi_fromstr);

MPI mpi_scanval(const char *string)
{
	MPI a;

	a = mpi_alloc(0);
	if (!a)
		return NULL;

	if (mpi_fromstr(a, string)) {
		mpi_free(a);
		return NULL;
	}
	mpi_normalize(a);
	return a;
}
EXPORT_SYMBOL_GPL(mpi_scanval);

static int count_lzeros(MPI a)
{
	mpi_limb_t alimb;
	int i, lzeros = 0;

	for (i = a->nlimbs - 1; i >= 0; i--) {
		alimb = a->d[i];
		if (alimb == 0) {
			lzeros += sizeof(mpi_limb_t);
		} else {
			lzeros += count_leading_zeros(alimb) / 8;
			break;
		}
	}
	return lzeros;
}

 
int mpi_read_buffer(MPI a, uint8_t *buf, unsigned buf_len, unsigned *nbytes,
		    int *sign)
{
	uint8_t *p;
#if BYTES_PER_MPI_LIMB == 4
	__be32 alimb;
#elif BYTES_PER_MPI_LIMB == 8
	__be64 alimb;
#else
#error please implement for this limb size.
#endif
	unsigned int n = mpi_get_size(a);
	int i, lzeros;

	if (!buf || !nbytes)
		return -EINVAL;

	if (sign)
		*sign = a->sign;

	lzeros = count_lzeros(a);

	if (buf_len < n - lzeros) {
		*nbytes = n - lzeros;
		return -EOVERFLOW;
	}

	p = buf;
	*nbytes = n - lzeros;

	for (i = a->nlimbs - 1 - lzeros / BYTES_PER_MPI_LIMB,
			lzeros %= BYTES_PER_MPI_LIMB;
		i >= 0; i--) {
#if BYTES_PER_MPI_LIMB == 4
		alimb = cpu_to_be32(a->d[i]);
#elif BYTES_PER_MPI_LIMB == 8
		alimb = cpu_to_be64(a->d[i]);
#else
#error please implement for this limb size.
#endif
		memcpy(p, (u8 *)&alimb + lzeros, BYTES_PER_MPI_LIMB - lzeros);
		p += BYTES_PER_MPI_LIMB - lzeros;
		lzeros = 0;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mpi_read_buffer);

 
void *mpi_get_buffer(MPI a, unsigned *nbytes, int *sign)
{
	uint8_t *buf;
	unsigned int n;
	int ret;

	if (!nbytes)
		return NULL;

	n = mpi_get_size(a);

	if (!n)
		n++;

	buf = kmalloc(n, GFP_KERNEL);

	if (!buf)
		return NULL;

	ret = mpi_read_buffer(a, buf, n, nbytes, sign);

	if (ret) {
		kfree(buf);
		return NULL;
	}
	return buf;
}
EXPORT_SYMBOL_GPL(mpi_get_buffer);

 
int mpi_write_to_sgl(MPI a, struct scatterlist *sgl, unsigned nbytes,
		     int *sign)
{
	u8 *p, *p2;
#if BYTES_PER_MPI_LIMB == 4
	__be32 alimb;
#elif BYTES_PER_MPI_LIMB == 8
	__be64 alimb;
#else
#error please implement for this limb size.
#endif
	unsigned int n = mpi_get_size(a);
	struct sg_mapping_iter miter;
	int i, x, buf_len;
	int nents;

	if (sign)
		*sign = a->sign;

	if (nbytes < n)
		return -EOVERFLOW;

	nents = sg_nents_for_len(sgl, nbytes);
	if (nents < 0)
		return -EINVAL;

	sg_miter_start(&miter, sgl, nents, SG_MITER_ATOMIC | SG_MITER_TO_SG);
	sg_miter_next(&miter);
	buf_len = miter.length;
	p2 = miter.addr;

	while (nbytes > n) {
		i = min_t(unsigned, nbytes - n, buf_len);
		memset(p2, 0, i);
		p2 += i;
		nbytes -= i;

		buf_len -= i;
		if (!buf_len) {
			sg_miter_next(&miter);
			buf_len = miter.length;
			p2 = miter.addr;
		}
	}

	for (i = a->nlimbs - 1; i >= 0; i--) {
#if BYTES_PER_MPI_LIMB == 4
		alimb = a->d[i] ? cpu_to_be32(a->d[i]) : 0;
#elif BYTES_PER_MPI_LIMB == 8
		alimb = a->d[i] ? cpu_to_be64(a->d[i]) : 0;
#else
#error please implement for this limb size.
#endif
		p = (u8 *)&alimb;

		for (x = 0; x < sizeof(alimb); x++) {
			*p2++ = *p++;
			if (!--buf_len) {
				sg_miter_next(&miter);
				buf_len = miter.length;
				p2 = miter.addr;
			}
		}
	}

	sg_miter_stop(&miter);
	return 0;
}
EXPORT_SYMBOL_GPL(mpi_write_to_sgl);

 
MPI mpi_read_raw_from_sgl(struct scatterlist *sgl, unsigned int nbytes)
{
	struct sg_mapping_iter miter;
	unsigned int nbits, nlimbs;
	int x, j, z, lzeros, ents;
	unsigned int len;
	const u8 *buff;
	mpi_limb_t a;
	MPI val = NULL;

	ents = sg_nents_for_len(sgl, nbytes);
	if (ents < 0)
		return NULL;

	sg_miter_start(&miter, sgl, ents, SG_MITER_ATOMIC | SG_MITER_FROM_SG);

	lzeros = 0;
	len = 0;
	while (nbytes > 0) {
		while (len && !*buff) {
			lzeros++;
			len--;
			buff++;
		}

		if (len && *buff)
			break;

		sg_miter_next(&miter);
		buff = miter.addr;
		len = miter.length;

		nbytes -= lzeros;
		lzeros = 0;
	}

	miter.consumed = lzeros;

	nbytes -= lzeros;
	nbits = nbytes * 8;
	if (nbits > MAX_EXTERN_MPI_BITS) {
		sg_miter_stop(&miter);
		pr_info("MPI: mpi too large (%u bits)\n", nbits);
		return NULL;
	}

	if (nbytes > 0)
		nbits -= count_leading_zeros(*buff) - (BITS_PER_LONG - 8);

	sg_miter_stop(&miter);

	nlimbs = DIV_ROUND_UP(nbytes, BYTES_PER_MPI_LIMB);
	val = mpi_alloc(nlimbs);
	if (!val)
		return NULL;

	val->nbits = nbits;
	val->sign = 0;
	val->nlimbs = nlimbs;

	if (nbytes == 0)
		return val;

	j = nlimbs - 1;
	a = 0;
	z = BYTES_PER_MPI_LIMB - nbytes % BYTES_PER_MPI_LIMB;
	z %= BYTES_PER_MPI_LIMB;

	while (sg_miter_next(&miter)) {
		buff = miter.addr;
		len = min_t(unsigned, miter.length, nbytes);
		nbytes -= len;

		for (x = 0; x < len; x++) {
			a <<= 8;
			a |= *buff++;
			if (((z + x + 1) % BYTES_PER_MPI_LIMB) == 0) {
				val->d[j--] = a;
				a = 0;
			}
		}
		z += x;
	}

	return val;
}
EXPORT_SYMBOL_GPL(mpi_read_raw_from_sgl);

 
static void twocompl(unsigned char *p, unsigned int n)
{
	int i;

	for (i = n-1; i >= 0 && !p[i]; i--)
		;
	if (i >= 0) {
		if ((p[i] & 0x01))
			p[i] = (((p[i] ^ 0xfe) | 0x01) & 0xff);
		else if ((p[i] & 0x02))
			p[i] = (((p[i] ^ 0xfc) | 0x02) & 0xfe);
		else if ((p[i] & 0x04))
			p[i] = (((p[i] ^ 0xf8) | 0x04) & 0xfc);
		else if ((p[i] & 0x08))
			p[i] = (((p[i] ^ 0xf0) | 0x08) & 0xf8);
		else if ((p[i] & 0x10))
			p[i] = (((p[i] ^ 0xe0) | 0x10) & 0xf0);
		else if ((p[i] & 0x20))
			p[i] = (((p[i] ^ 0xc0) | 0x20) & 0xe0);
		else if ((p[i] & 0x40))
			p[i] = (((p[i] ^ 0x80) | 0x40) & 0xc0);
		else
			p[i] = 0x80;

		for (i--; i >= 0; i--)
			p[i] ^= 0xff;
	}
}

int mpi_print(enum gcry_mpi_format format, unsigned char *buffer,
			size_t buflen, size_t *nwritten, MPI a)
{
	unsigned int nbits = mpi_get_nbits(a);
	size_t len;
	size_t dummy_nwritten;
	int negative;

	if (!nwritten)
		nwritten = &dummy_nwritten;

	 
	if (a->sign && mpi_cmp_ui(a, 0))
		negative = 1;
	else
		negative = 0;

	len = buflen;
	*nwritten = 0;
	if (format == GCRYMPI_FMT_STD) {
		unsigned char *tmp;
		int extra = 0;
		unsigned int n;

		tmp = mpi_get_buffer(a, &n, NULL);
		if (!tmp)
			return -EINVAL;

		if (negative) {
			twocompl(tmp, n);
			if (!(*tmp & 0x80)) {
				 
				n++;
				extra = 2;
			}
		} else if (n && (*tmp & 0x80)) {
			 
			n++;
			extra = 1;
		}

		if (buffer && n > len) {
			 
			kfree(tmp);
			return -E2BIG;
		}
		if (buffer) {
			unsigned char *s = buffer;

			if (extra == 1)
				*s++ = 0;
			else if (extra)
				*s++ = 0xff;
			memcpy(s, tmp, n-!!extra);
		}
		kfree(tmp);
		*nwritten = n;
		return 0;
	} else if (format == GCRYMPI_FMT_USG) {
		unsigned int n = (nbits + 7)/8;

		 
		 

		if (buffer && n > len)
			return -E2BIG;
		if (buffer) {
			unsigned char *tmp;

			tmp = mpi_get_buffer(a, &n, NULL);
			if (!tmp)
				return -EINVAL;
			memcpy(buffer, tmp, n);
			kfree(tmp);
		}
		*nwritten = n;
		return 0;
	} else if (format == GCRYMPI_FMT_PGP) {
		unsigned int n = (nbits + 7)/8;

		 
		if (negative)
			return -EINVAL;

		if (buffer && n+2 > len)
			return -E2BIG;

		if (buffer) {
			unsigned char *tmp;
			unsigned char *s = buffer;

			s[0] = nbits >> 8;
			s[1] = nbits;

			tmp = mpi_get_buffer(a, &n, NULL);
			if (!tmp)
				return -EINVAL;
			memcpy(s+2, tmp, n);
			kfree(tmp);
		}
		*nwritten = n+2;
		return 0;
	} else if (format == GCRYMPI_FMT_SSH) {
		unsigned char *tmp;
		int extra = 0;
		unsigned int n;

		tmp = mpi_get_buffer(a, &n, NULL);
		if (!tmp)
			return -EINVAL;

		if (negative) {
			twocompl(tmp, n);
			if (!(*tmp & 0x80)) {
				 
				n++;
				extra = 2;
			}
		} else if (n && (*tmp & 0x80)) {
			n++;
			extra = 1;
		}

		if (buffer && n+4 > len) {
			kfree(tmp);
			return -E2BIG;
		}

		if (buffer) {
			unsigned char *s = buffer;

			*s++ = n >> 24;
			*s++ = n >> 16;
			*s++ = n >> 8;
			*s++ = n;
			if (extra == 1)
				*s++ = 0;
			else if (extra)
				*s++ = 0xff;
			memcpy(s, tmp, n-!!extra);
		}
		kfree(tmp);
		*nwritten = 4+n;
		return 0;
	} else if (format == GCRYMPI_FMT_HEX) {
		unsigned char *tmp;
		int i;
		int extra = 0;
		unsigned int n = 0;

		tmp = mpi_get_buffer(a, &n, NULL);
		if (!tmp)
			return -EINVAL;
		if (!n || (*tmp & 0x80))
			extra = 2;

		if (buffer && 2*n + extra + negative + 1 > len) {
			kfree(tmp);
			return -E2BIG;
		}
		if (buffer) {
			unsigned char *s = buffer;

			if (negative)
				*s++ = '-';
			if (extra) {
				*s++ = '0';
				*s++ = '0';
			}

			for (i = 0; i < n; i++) {
				unsigned int c = tmp[i];

				*s++ = (c >> 4) < 10 ? '0'+(c>>4) : 'A'+(c>>4)-10;
				c &= 15;
				*s++ = c < 10 ? '0'+c : 'A'+c-10;
			}
			*s++ = 0;
			*nwritten = s - buffer;
		} else {
			*nwritten = 2*n + extra + negative + 1;
		}
		kfree(tmp);
		return 0;
	} else
		return -EINVAL;
}
EXPORT_SYMBOL_GPL(mpi_print);
