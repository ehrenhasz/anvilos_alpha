 

#include <crypto/ecc_curve.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/swab.h>
#include <linux/fips.h>
#include <crypto/ecdh.h>
#include <crypto/rng.h>
#include <crypto/internal/ecc.h>
#include <asm/unaligned.h>
#include <linux/ratelimit.h>

#include "ecc_curve_defs.h"

typedef struct {
	u64 m_low;
	u64 m_high;
} uint128_t;

 
const struct ecc_curve *ecc_get_curve25519(void)
{
	return &ecc_25519;
}
EXPORT_SYMBOL(ecc_get_curve25519);

const struct ecc_curve *ecc_get_curve(unsigned int curve_id)
{
	switch (curve_id) {
	 
	case ECC_CURVE_NIST_P192:
		return fips_enabled ? NULL : &nist_p192;
	case ECC_CURVE_NIST_P256:
		return &nist_p256;
	case ECC_CURVE_NIST_P384:
		return &nist_p384;
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(ecc_get_curve);

static u64 *ecc_alloc_digits_space(unsigned int ndigits)
{
	size_t len = ndigits * sizeof(u64);

	if (!len)
		return NULL;

	return kmalloc(len, GFP_KERNEL);
}

static void ecc_free_digits_space(u64 *space)
{
	kfree_sensitive(space);
}

struct ecc_point *ecc_alloc_point(unsigned int ndigits)
{
	struct ecc_point *p = kmalloc(sizeof(*p), GFP_KERNEL);

	if (!p)
		return NULL;

	p->x = ecc_alloc_digits_space(ndigits);
	if (!p->x)
		goto err_alloc_x;

	p->y = ecc_alloc_digits_space(ndigits);
	if (!p->y)
		goto err_alloc_y;

	p->ndigits = ndigits;

	return p;

err_alloc_y:
	ecc_free_digits_space(p->x);
err_alloc_x:
	kfree(p);
	return NULL;
}
EXPORT_SYMBOL(ecc_alloc_point);

void ecc_free_point(struct ecc_point *p)
{
	if (!p)
		return;

	kfree_sensitive(p->x);
	kfree_sensitive(p->y);
	kfree_sensitive(p);
}
EXPORT_SYMBOL(ecc_free_point);

static void vli_clear(u64 *vli, unsigned int ndigits)
{
	int i;

	for (i = 0; i < ndigits; i++)
		vli[i] = 0;
}

 
bool vli_is_zero(const u64 *vli, unsigned int ndigits)
{
	int i;

	for (i = 0; i < ndigits; i++) {
		if (vli[i])
			return false;
	}

	return true;
}
EXPORT_SYMBOL(vli_is_zero);

 
static u64 vli_test_bit(const u64 *vli, unsigned int bit)
{
	return (vli[bit / 64] & ((u64)1 << (bit % 64)));
}

static bool vli_is_negative(const u64 *vli, unsigned int ndigits)
{
	return vli_test_bit(vli, ndigits * 64 - 1);
}

 
static unsigned int vli_num_digits(const u64 *vli, unsigned int ndigits)
{
	int i;

	 
	for (i = ndigits - 1; i >= 0 && vli[i] == 0; i--);

	return (i + 1);
}

 
unsigned int vli_num_bits(const u64 *vli, unsigned int ndigits)
{
	unsigned int i, num_digits;
	u64 digit;

	num_digits = vli_num_digits(vli, ndigits);
	if (num_digits == 0)
		return 0;

	digit = vli[num_digits - 1];
	for (i = 0; digit; i++)
		digit >>= 1;

	return ((num_digits - 1) * 64 + i);
}
EXPORT_SYMBOL(vli_num_bits);

 
void vli_from_be64(u64 *dest, const void *src, unsigned int ndigits)
{
	int i;
	const u64 *from = src;

	for (i = 0; i < ndigits; i++)
		dest[i] = get_unaligned_be64(&from[ndigits - 1 - i]);
}
EXPORT_SYMBOL(vli_from_be64);

void vli_from_le64(u64 *dest, const void *src, unsigned int ndigits)
{
	int i;
	const u64 *from = src;

	for (i = 0; i < ndigits; i++)
		dest[i] = get_unaligned_le64(&from[i]);
}
EXPORT_SYMBOL(vli_from_le64);

 
static void vli_set(u64 *dest, const u64 *src, unsigned int ndigits)
{
	int i;

	for (i = 0; i < ndigits; i++)
		dest[i] = src[i];
}

 
int vli_cmp(const u64 *left, const u64 *right, unsigned int ndigits)
{
	int i;

	for (i = ndigits - 1; i >= 0; i--) {
		if (left[i] > right[i])
			return 1;
		else if (left[i] < right[i])
			return -1;
	}

	return 0;
}
EXPORT_SYMBOL(vli_cmp);

 
static u64 vli_lshift(u64 *result, const u64 *in, unsigned int shift,
		      unsigned int ndigits)
{
	u64 carry = 0;
	int i;

	for (i = 0; i < ndigits; i++) {
		u64 temp = in[i];

		result[i] = (temp << shift) | carry;
		carry = temp >> (64 - shift);
	}

	return carry;
}

 
static void vli_rshift1(u64 *vli, unsigned int ndigits)
{
	u64 *end = vli;
	u64 carry = 0;

	vli += ndigits;

	while (vli-- > end) {
		u64 temp = *vli;
		*vli = (temp >> 1) | carry;
		carry = temp << 63;
	}
}

 
static u64 vli_add(u64 *result, const u64 *left, const u64 *right,
		   unsigned int ndigits)
{
	u64 carry = 0;
	int i;

	for (i = 0; i < ndigits; i++) {
		u64 sum;

		sum = left[i] + right[i] + carry;
		if (sum != left[i])
			carry = (sum < left[i]);

		result[i] = sum;
	}

	return carry;
}

 
static u64 vli_uadd(u64 *result, const u64 *left, u64 right,
		    unsigned int ndigits)
{
	u64 carry = right;
	int i;

	for (i = 0; i < ndigits; i++) {
		u64 sum;

		sum = left[i] + carry;
		if (sum != left[i])
			carry = (sum < left[i]);
		else
			carry = !!carry;

		result[i] = sum;
	}

	return carry;
}

 
u64 vli_sub(u64 *result, const u64 *left, const u64 *right,
		   unsigned int ndigits)
{
	u64 borrow = 0;
	int i;

	for (i = 0; i < ndigits; i++) {
		u64 diff;

		diff = left[i] - right[i] - borrow;
		if (diff != left[i])
			borrow = (diff > left[i]);

		result[i] = diff;
	}

	return borrow;
}
EXPORT_SYMBOL(vli_sub);

 
static u64 vli_usub(u64 *result, const u64 *left, u64 right,
	     unsigned int ndigits)
{
	u64 borrow = right;
	int i;

	for (i = 0; i < ndigits; i++) {
		u64 diff;

		diff = left[i] - borrow;
		if (diff != left[i])
			borrow = (diff > left[i]);

		result[i] = diff;
	}

	return borrow;
}

static uint128_t mul_64_64(u64 left, u64 right)
{
	uint128_t result;
#if defined(CONFIG_ARCH_SUPPORTS_INT128)
	unsigned __int128 m = (unsigned __int128)left * right;

	result.m_low  = m;
	result.m_high = m >> 64;
#else
	u64 a0 = left & 0xffffffffull;
	u64 a1 = left >> 32;
	u64 b0 = right & 0xffffffffull;
	u64 b1 = right >> 32;
	u64 m0 = a0 * b0;
	u64 m1 = a0 * b1;
	u64 m2 = a1 * b0;
	u64 m3 = a1 * b1;

	m2 += (m0 >> 32);
	m2 += m1;

	 
	if (m2 < m1)
		m3 += 0x100000000ull;

	result.m_low = (m0 & 0xffffffffull) | (m2 << 32);
	result.m_high = m3 + (m2 >> 32);
#endif
	return result;
}

static uint128_t add_128_128(uint128_t a, uint128_t b)
{
	uint128_t result;

	result.m_low = a.m_low + b.m_low;
	result.m_high = a.m_high + b.m_high + (result.m_low < a.m_low);

	return result;
}

static void vli_mult(u64 *result, const u64 *left, const u64 *right,
		     unsigned int ndigits)
{
	uint128_t r01 = { 0, 0 };
	u64 r2 = 0;
	unsigned int i, k;

	 
	for (k = 0; k < ndigits * 2 - 1; k++) {
		unsigned int min;

		if (k < ndigits)
			min = 0;
		else
			min = (k + 1) - ndigits;

		for (i = min; i <= k && i < ndigits; i++) {
			uint128_t product;

			product = mul_64_64(left[i], right[k - i]);

			r01 = add_128_128(r01, product);
			r2 += (r01.m_high < product.m_high);
		}

		result[k] = r01.m_low;
		r01.m_low = r01.m_high;
		r01.m_high = r2;
		r2 = 0;
	}

	result[ndigits * 2 - 1] = r01.m_low;
}

 
static void vli_umult(u64 *result, const u64 *left, u32 right,
		      unsigned int ndigits)
{
	uint128_t r01 = { 0 };
	unsigned int k;

	for (k = 0; k < ndigits; k++) {
		uint128_t product;

		product = mul_64_64(left[k], right);
		r01 = add_128_128(r01, product);
		 
		result[k] = r01.m_low;
		r01.m_low = r01.m_high;
		r01.m_high = 0;
	}
	result[k] = r01.m_low;
	for (++k; k < ndigits * 2; k++)
		result[k] = 0;
}

static void vli_square(u64 *result, const u64 *left, unsigned int ndigits)
{
	uint128_t r01 = { 0, 0 };
	u64 r2 = 0;
	int i, k;

	for (k = 0; k < ndigits * 2 - 1; k++) {
		unsigned int min;

		if (k < ndigits)
			min = 0;
		else
			min = (k + 1) - ndigits;

		for (i = min; i <= k && i <= k - i; i++) {
			uint128_t product;

			product = mul_64_64(left[i], left[k - i]);

			if (i < k - i) {
				r2 += product.m_high >> 63;
				product.m_high = (product.m_high << 1) |
						 (product.m_low >> 63);
				product.m_low <<= 1;
			}

			r01 = add_128_128(r01, product);
			r2 += (r01.m_high < product.m_high);
		}

		result[k] = r01.m_low;
		r01.m_low = r01.m_high;
		r01.m_high = r2;
		r2 = 0;
	}

	result[ndigits * 2 - 1] = r01.m_low;
}

 
static void vli_mod_add(u64 *result, const u64 *left, const u64 *right,
			const u64 *mod, unsigned int ndigits)
{
	u64 carry;

	carry = vli_add(result, left, right, ndigits);

	 
	if (carry || vli_cmp(result, mod, ndigits) >= 0)
		vli_sub(result, result, mod, ndigits);
}

 
static void vli_mod_sub(u64 *result, const u64 *left, const u64 *right,
			const u64 *mod, unsigned int ndigits)
{
	u64 borrow = vli_sub(result, left, right, ndigits);

	 
	if (borrow)
		vli_add(result, result, mod, ndigits);
}

 
static void vli_mmod_special(u64 *result, const u64 *product,
			      const u64 *mod, unsigned int ndigits)
{
	u64 c = -mod[0];
	u64 t[ECC_MAX_DIGITS * 2];
	u64 r[ECC_MAX_DIGITS * 2];

	vli_set(r, product, ndigits * 2);
	while (!vli_is_zero(r + ndigits, ndigits)) {
		vli_umult(t, r + ndigits, c, ndigits);
		vli_clear(r + ndigits, ndigits);
		vli_add(r, r, t, ndigits * 2);
	}
	vli_set(t, mod, ndigits);
	vli_clear(t + ndigits, ndigits);
	while (vli_cmp(r, t, ndigits * 2) >= 0)
		vli_sub(r, r, t, ndigits * 2);
	vli_set(result, r, ndigits);
}

 
static void vli_mmod_special2(u64 *result, const u64 *product,
			       const u64 *mod, unsigned int ndigits)
{
	u64 c2 = mod[0] * 2;
	u64 q[ECC_MAX_DIGITS];
	u64 r[ECC_MAX_DIGITS * 2];
	u64 m[ECC_MAX_DIGITS * 2];  
	int carry;  
	int i;

	vli_set(m, mod, ndigits);
	vli_clear(m + ndigits, ndigits);

	vli_set(r, product, ndigits);
	 
	vli_set(q, product + ndigits, ndigits);
	vli_clear(r + ndigits, ndigits);
	carry = vli_is_negative(r, ndigits);
	if (carry)
		r[ndigits - 1] &= (1ull << 63) - 1;
	for (i = 1; carry || !vli_is_zero(q, ndigits); i++) {
		u64 qc[ECC_MAX_DIGITS * 2];

		vli_umult(qc, q, c2, ndigits);
		if (carry)
			vli_uadd(qc, qc, mod[0], ndigits * 2);
		vli_set(q, qc + ndigits, ndigits);
		vli_clear(qc + ndigits, ndigits);
		carry = vli_is_negative(qc, ndigits);
		if (carry)
			qc[ndigits - 1] &= (1ull << 63) - 1;
		if (i & 1)
			vli_sub(r, r, qc, ndigits * 2);
		else
			vli_add(r, r, qc, ndigits * 2);
	}
	while (vli_is_negative(r, ndigits * 2))
		vli_add(r, r, m, ndigits * 2);
	while (vli_cmp(r, m, ndigits * 2) >= 0)
		vli_sub(r, r, m, ndigits * 2);

	vli_set(result, r, ndigits);
}

 
static void vli_mmod_slow(u64 *result, u64 *product, const u64 *mod,
			  unsigned int ndigits)
{
	u64 mod_m[2 * ECC_MAX_DIGITS];
	u64 tmp[2 * ECC_MAX_DIGITS];
	u64 *v[2] = { tmp, product };
	u64 carry = 0;
	unsigned int i;
	 
	int shift = (ndigits * 2 * 64) - vli_num_bits(mod, ndigits);
	int word_shift = shift / 64;
	int bit_shift = shift % 64;

	vli_clear(mod_m, word_shift);
	if (bit_shift > 0) {
		for (i = 0; i < ndigits; ++i) {
			mod_m[word_shift + i] = (mod[i] << bit_shift) | carry;
			carry = mod[i] >> (64 - bit_shift);
		}
	} else
		vli_set(mod_m + word_shift, mod, ndigits);

	for (i = 1; shift >= 0; --shift) {
		u64 borrow = 0;
		unsigned int j;

		for (j = 0; j < ndigits * 2; ++j) {
			u64 diff = v[i][j] - mod_m[j] - borrow;

			if (diff != v[i][j])
				borrow = (diff > v[i][j]);
			v[1 - i][j] = diff;
		}
		i = !(i ^ borrow);  
		vli_rshift1(mod_m, ndigits);
		mod_m[ndigits - 1] |= mod_m[ndigits] << (64 - 1);
		vli_rshift1(mod_m + ndigits, ndigits);
	}
	vli_set(result, v[i], ndigits);
}

 
static void vli_mmod_barrett(u64 *result, u64 *product, const u64 *mod,
			     unsigned int ndigits)
{
	u64 q[ECC_MAX_DIGITS * 2];
	u64 r[ECC_MAX_DIGITS * 2];
	const u64 *mu = mod + ndigits;

	vli_mult(q, product + ndigits, mu, ndigits);
	if (mu[ndigits])
		vli_add(q + ndigits, q + ndigits, product + ndigits, ndigits);
	vli_mult(r, mod, q + ndigits, ndigits);
	vli_sub(r, product, r, ndigits * 2);
	while (!vli_is_zero(r + ndigits, ndigits) ||
	       vli_cmp(r, mod, ndigits) != -1) {
		u64 carry;

		carry = vli_sub(r, r, mod, ndigits);
		vli_usub(r + ndigits, r + ndigits, carry, ndigits);
	}
	vli_set(result, r, ndigits);
}

 
static void vli_mmod_fast_192(u64 *result, const u64 *product,
			      const u64 *curve_prime, u64 *tmp)
{
	const unsigned int ndigits = 3;
	int carry;

	vli_set(result, product, ndigits);

	vli_set(tmp, &product[3], ndigits);
	carry = vli_add(result, result, tmp, ndigits);

	tmp[0] = 0;
	tmp[1] = product[3];
	tmp[2] = product[4];
	carry += vli_add(result, result, tmp, ndigits);

	tmp[0] = tmp[1] = product[5];
	tmp[2] = 0;
	carry += vli_add(result, result, tmp, ndigits);

	while (carry || vli_cmp(curve_prime, result, ndigits) != 1)
		carry -= vli_sub(result, result, curve_prime, ndigits);
}

 
static void vli_mmod_fast_256(u64 *result, const u64 *product,
			      const u64 *curve_prime, u64 *tmp)
{
	int carry;
	const unsigned int ndigits = 4;

	 
	vli_set(result, product, ndigits);

	 
	tmp[0] = 0;
	tmp[1] = product[5] & 0xffffffff00000000ull;
	tmp[2] = product[6];
	tmp[3] = product[7];
	carry = vli_lshift(tmp, tmp, 1, ndigits);
	carry += vli_add(result, result, tmp, ndigits);

	 
	tmp[1] = product[6] << 32;
	tmp[2] = (product[6] >> 32) | (product[7] << 32);
	tmp[3] = product[7] >> 32;
	carry += vli_lshift(tmp, tmp, 1, ndigits);
	carry += vli_add(result, result, tmp, ndigits);

	 
	tmp[0] = product[4];
	tmp[1] = product[5] & 0xffffffff;
	tmp[2] = 0;
	tmp[3] = product[7];
	carry += vli_add(result, result, tmp, ndigits);

	 
	tmp[0] = (product[4] >> 32) | (product[5] << 32);
	tmp[1] = (product[5] >> 32) | (product[6] & 0xffffffff00000000ull);
	tmp[2] = product[7];
	tmp[3] = (product[6] >> 32) | (product[4] << 32);
	carry += vli_add(result, result, tmp, ndigits);

	 
	tmp[0] = (product[5] >> 32) | (product[6] << 32);
	tmp[1] = (product[6] >> 32);
	tmp[2] = 0;
	tmp[3] = (product[4] & 0xffffffff) | (product[5] << 32);
	carry -= vli_sub(result, result, tmp, ndigits);

	 
	tmp[0] = product[6];
	tmp[1] = product[7];
	tmp[2] = 0;
	tmp[3] = (product[4] >> 32) | (product[5] & 0xffffffff00000000ull);
	carry -= vli_sub(result, result, tmp, ndigits);

	 
	tmp[0] = (product[6] >> 32) | (product[7] << 32);
	tmp[1] = (product[7] >> 32) | (product[4] << 32);
	tmp[2] = (product[4] >> 32) | (product[5] << 32);
	tmp[3] = (product[6] << 32);
	carry -= vli_sub(result, result, tmp, ndigits);

	 
	tmp[0] = product[7];
	tmp[1] = product[4] & 0xffffffff00000000ull;
	tmp[2] = product[5];
	tmp[3] = product[6] & 0xffffffff00000000ull;
	carry -= vli_sub(result, result, tmp, ndigits);

	if (carry < 0) {
		do {
			carry += vli_add(result, result, curve_prime, ndigits);
		} while (carry < 0);
	} else {
		while (carry || vli_cmp(curve_prime, result, ndigits) != 1)
			carry -= vli_sub(result, result, curve_prime, ndigits);
	}
}

#define SL32OR32(x32, y32) (((u64)x32 << 32) | y32)
#define AND64H(x64)  (x64 & 0xffFFffFF00000000ull)
#define AND64L(x64)  (x64 & 0x00000000ffFFffFFull)

 
static void vli_mmod_fast_384(u64 *result, const u64 *product,
				const u64 *curve_prime, u64 *tmp)
{
	int carry;
	const unsigned int ndigits = 6;

	 
	vli_set(result, product, ndigits);

	 
	tmp[0] = 0;		
	tmp[1] = 0;		
	tmp[2] = SL32OR32(product[11], (product[10]>>32));	
	tmp[3] = product[11]>>32;	
	tmp[4] = 0;		
	tmp[5] = 0;		
	carry = vli_lshift(tmp, tmp, 1, ndigits);
	carry += vli_add(result, result, tmp, ndigits);

	 
	tmp[0] = product[6];	
	tmp[1] = product[7];	
	tmp[2] = product[8];	
	tmp[3] = product[9];	
	tmp[4] = product[10];	
	tmp[5] = product[11];	
	carry += vli_add(result, result, tmp, ndigits);

	 
	tmp[0] = SL32OR32(product[11], (product[10]>>32));	
	tmp[1] = SL32OR32(product[6], (product[11]>>32));	
	tmp[2] = SL32OR32(product[7], (product[6])>>32);	
	tmp[3] = SL32OR32(product[8], (product[7]>>32));	
	tmp[4] = SL32OR32(product[9], (product[8]>>32));	
	tmp[5] = SL32OR32(product[10], (product[9]>>32));	
	carry += vli_add(result, result, tmp, ndigits);

	 
	tmp[0] = AND64H(product[11]);	
	tmp[1] = (product[10]<<32);	
	tmp[2] = product[6];	
	tmp[3] = product[7];	
	tmp[4] = product[8];	
	tmp[5] = product[9];	
	carry += vli_add(result, result, tmp, ndigits);

	 
	tmp[0] = 0;		
	tmp[1] = 0;		
	tmp[2] = product[10];	
	tmp[3] = product[11];	
	tmp[4] = 0;		
	tmp[5] = 0;		
	carry += vli_add(result, result, tmp, ndigits);

	 
	tmp[0] = AND64L(product[10]);	
	tmp[1] = AND64H(product[10]);	
	tmp[2] = product[11];	
	tmp[3] = 0;		
	tmp[4] = 0;		
	tmp[5] = 0;		
	carry += vli_add(result, result, tmp, ndigits);

	 
	tmp[0] = SL32OR32(product[6], (product[11]>>32));	
	tmp[1] = SL32OR32(product[7], (product[6]>>32));	
	tmp[2] = SL32OR32(product[8], (product[7]>>32));	
	tmp[3] = SL32OR32(product[9], (product[8]>>32));	
	tmp[4] = SL32OR32(product[10], (product[9]>>32));	
	tmp[5] = SL32OR32(product[11], (product[10]>>32));	
	carry -= vli_sub(result, result, tmp, ndigits);

	 
	tmp[0] = (product[10]<<32);	
	tmp[1] = SL32OR32(product[11], (product[10]>>32));	
	tmp[2] = (product[11]>>32);	
	tmp[3] = 0;		
	tmp[4] = 0;		
	tmp[5] = 0;		
	carry -= vli_sub(result, result, tmp, ndigits);

	 
	tmp[0] = 0;		
	tmp[1] = AND64H(product[11]);	
	tmp[2] = product[11]>>32;	
	tmp[3] = 0;		
	tmp[4] = 0;		
	tmp[5] = 0;		
	carry -= vli_sub(result, result, tmp, ndigits);

	if (carry < 0) {
		do {
			carry += vli_add(result, result, curve_prime, ndigits);
		} while (carry < 0);
	} else {
		while (carry || vli_cmp(curve_prime, result, ndigits) != 1)
			carry -= vli_sub(result, result, curve_prime, ndigits);
	}

}

#undef SL32OR32
#undef AND64H
#undef AND64L

 
static bool vli_mmod_fast(u64 *result, u64 *product,
			  const struct ecc_curve *curve)
{
	u64 tmp[2 * ECC_MAX_DIGITS];
	const u64 *curve_prime = curve->p;
	const unsigned int ndigits = curve->g.ndigits;

	 
	if (strncmp(curve->name, "nist_", 5) != 0) {
		 
		if (curve_prime[ndigits - 1] == -1ull) {
			vli_mmod_special(result, product, curve_prime,
					 ndigits);
			return true;
		} else if (curve_prime[ndigits - 1] == 1ull << 63 &&
			   curve_prime[ndigits - 2] == 0) {
			vli_mmod_special2(result, product, curve_prime,
					  ndigits);
			return true;
		}
		vli_mmod_barrett(result, product, curve_prime, ndigits);
		return true;
	}

	switch (ndigits) {
	case 3:
		vli_mmod_fast_192(result, product, curve_prime, tmp);
		break;
	case 4:
		vli_mmod_fast_256(result, product, curve_prime, tmp);
		break;
	case 6:
		vli_mmod_fast_384(result, product, curve_prime, tmp);
		break;
	default:
		pr_err_ratelimited("ecc: unsupported digits size!\n");
		return false;
	}

	return true;
}

 
void vli_mod_mult_slow(u64 *result, const u64 *left, const u64 *right,
		       const u64 *mod, unsigned int ndigits)
{
	u64 product[ECC_MAX_DIGITS * 2];

	vli_mult(product, left, right, ndigits);
	vli_mmod_slow(result, product, mod, ndigits);
}
EXPORT_SYMBOL(vli_mod_mult_slow);

 
static void vli_mod_mult_fast(u64 *result, const u64 *left, const u64 *right,
			      const struct ecc_curve *curve)
{
	u64 product[2 * ECC_MAX_DIGITS];

	vli_mult(product, left, right, curve->g.ndigits);
	vli_mmod_fast(result, product, curve);
}

 
static void vli_mod_square_fast(u64 *result, const u64 *left,
				const struct ecc_curve *curve)
{
	u64 product[2 * ECC_MAX_DIGITS];

	vli_square(product, left, curve->g.ndigits);
	vli_mmod_fast(result, product, curve);
}

#define EVEN(vli) (!(vli[0] & 1))
 
void vli_mod_inv(u64 *result, const u64 *input, const u64 *mod,
			unsigned int ndigits)
{
	u64 a[ECC_MAX_DIGITS], b[ECC_MAX_DIGITS];
	u64 u[ECC_MAX_DIGITS], v[ECC_MAX_DIGITS];
	u64 carry;
	int cmp_result;

	if (vli_is_zero(input, ndigits)) {
		vli_clear(result, ndigits);
		return;
	}

	vli_set(a, input, ndigits);
	vli_set(b, mod, ndigits);
	vli_clear(u, ndigits);
	u[0] = 1;
	vli_clear(v, ndigits);

	while ((cmp_result = vli_cmp(a, b, ndigits)) != 0) {
		carry = 0;

		if (EVEN(a)) {
			vli_rshift1(a, ndigits);

			if (!EVEN(u))
				carry = vli_add(u, u, mod, ndigits);

			vli_rshift1(u, ndigits);
			if (carry)
				u[ndigits - 1] |= 0x8000000000000000ull;
		} else if (EVEN(b)) {
			vli_rshift1(b, ndigits);

			if (!EVEN(v))
				carry = vli_add(v, v, mod, ndigits);

			vli_rshift1(v, ndigits);
			if (carry)
				v[ndigits - 1] |= 0x8000000000000000ull;
		} else if (cmp_result > 0) {
			vli_sub(a, a, b, ndigits);
			vli_rshift1(a, ndigits);

			if (vli_cmp(u, v, ndigits) < 0)
				vli_add(u, u, mod, ndigits);

			vli_sub(u, u, v, ndigits);
			if (!EVEN(u))
				carry = vli_add(u, u, mod, ndigits);

			vli_rshift1(u, ndigits);
			if (carry)
				u[ndigits - 1] |= 0x8000000000000000ull;
		} else {
			vli_sub(b, b, a, ndigits);
			vli_rshift1(b, ndigits);

			if (vli_cmp(v, u, ndigits) < 0)
				vli_add(v, v, mod, ndigits);

			vli_sub(v, v, u, ndigits);
			if (!EVEN(v))
				carry = vli_add(v, v, mod, ndigits);

			vli_rshift1(v, ndigits);
			if (carry)
				v[ndigits - 1] |= 0x8000000000000000ull;
		}
	}

	vli_set(result, u, ndigits);
}
EXPORT_SYMBOL(vli_mod_inv);

 

 
bool ecc_point_is_zero(const struct ecc_point *point)
{
	return (vli_is_zero(point->x, point->ndigits) &&
		vli_is_zero(point->y, point->ndigits));
}
EXPORT_SYMBOL(ecc_point_is_zero);

 

 
static void ecc_point_double_jacobian(u64 *x1, u64 *y1, u64 *z1,
					const struct ecc_curve *curve)
{
	 
	u64 t4[ECC_MAX_DIGITS];
	u64 t5[ECC_MAX_DIGITS];
	const u64 *curve_prime = curve->p;
	const unsigned int ndigits = curve->g.ndigits;

	if (vli_is_zero(z1, ndigits))
		return;

	 
	vli_mod_square_fast(t4, y1, curve);
	 
	vli_mod_mult_fast(t5, x1, t4, curve);
	 
	vli_mod_square_fast(t4, t4, curve);
	 
	vli_mod_mult_fast(y1, y1, z1, curve);
	 
	vli_mod_square_fast(z1, z1, curve);

	 
	vli_mod_add(x1, x1, z1, curve_prime, ndigits);
	 
	vli_mod_add(z1, z1, z1, curve_prime, ndigits);
	 
	vli_mod_sub(z1, x1, z1, curve_prime, ndigits);
	 
	vli_mod_mult_fast(x1, x1, z1, curve);

	 
	vli_mod_add(z1, x1, x1, curve_prime, ndigits);
	 
	vli_mod_add(x1, x1, z1, curve_prime, ndigits);
	if (vli_test_bit(x1, 0)) {
		u64 carry = vli_add(x1, x1, curve_prime, ndigits);

		vli_rshift1(x1, ndigits);
		x1[ndigits - 1] |= carry << 63;
	} else {
		vli_rshift1(x1, ndigits);
	}
	 

	 
	vli_mod_square_fast(z1, x1, curve);
	 
	vli_mod_sub(z1, z1, t5, curve_prime, ndigits);
	 
	vli_mod_sub(z1, z1, t5, curve_prime, ndigits);
	 
	vli_mod_sub(t5, t5, z1, curve_prime, ndigits);
	 
	vli_mod_mult_fast(x1, x1, t5, curve);
	 
	vli_mod_sub(t4, x1, t4, curve_prime, ndigits);

	vli_set(x1, z1, ndigits);
	vli_set(z1, y1, ndigits);
	vli_set(y1, t4, ndigits);
}

 
static void apply_z(u64 *x1, u64 *y1, u64 *z, const struct ecc_curve *curve)
{
	u64 t1[ECC_MAX_DIGITS];

	vli_mod_square_fast(t1, z, curve);		 
	vli_mod_mult_fast(x1, x1, t1, curve);	 
	vli_mod_mult_fast(t1, t1, z, curve);	 
	vli_mod_mult_fast(y1, y1, t1, curve);	 
}

 
static void xycz_initial_double(u64 *x1, u64 *y1, u64 *x2, u64 *y2,
				u64 *p_initial_z, const struct ecc_curve *curve)
{
	u64 z[ECC_MAX_DIGITS];
	const unsigned int ndigits = curve->g.ndigits;

	vli_set(x2, x1, ndigits);
	vli_set(y2, y1, ndigits);

	vli_clear(z, ndigits);
	z[0] = 1;

	if (p_initial_z)
		vli_set(z, p_initial_z, ndigits);

	apply_z(x1, y1, z, curve);

	ecc_point_double_jacobian(x1, y1, z, curve);

	apply_z(x2, y2, z, curve);
}

 
static void xycz_add(u64 *x1, u64 *y1, u64 *x2, u64 *y2,
			const struct ecc_curve *curve)
{
	 
	u64 t5[ECC_MAX_DIGITS];
	const u64 *curve_prime = curve->p;
	const unsigned int ndigits = curve->g.ndigits;

	 
	vli_mod_sub(t5, x2, x1, curve_prime, ndigits);
	 
	vli_mod_square_fast(t5, t5, curve);
	 
	vli_mod_mult_fast(x1, x1, t5, curve);
	 
	vli_mod_mult_fast(x2, x2, t5, curve);
	 
	vli_mod_sub(y2, y2, y1, curve_prime, ndigits);
	 
	vli_mod_square_fast(t5, y2, curve);

	 
	vli_mod_sub(t5, t5, x1, curve_prime, ndigits);
	 
	vli_mod_sub(t5, t5, x2, curve_prime, ndigits);
	 
	vli_mod_sub(x2, x2, x1, curve_prime, ndigits);
	 
	vli_mod_mult_fast(y1, y1, x2, curve);
	 
	vli_mod_sub(x2, x1, t5, curve_prime, ndigits);
	 
	vli_mod_mult_fast(y2, y2, x2, curve);
	 
	vli_mod_sub(y2, y2, y1, curve_prime, ndigits);

	vli_set(x2, t5, ndigits);
}

 
static void xycz_add_c(u64 *x1, u64 *y1, u64 *x2, u64 *y2,
			const struct ecc_curve *curve)
{
	 
	u64 t5[ECC_MAX_DIGITS];
	u64 t6[ECC_MAX_DIGITS];
	u64 t7[ECC_MAX_DIGITS];
	const u64 *curve_prime = curve->p;
	const unsigned int ndigits = curve->g.ndigits;

	 
	vli_mod_sub(t5, x2, x1, curve_prime, ndigits);
	 
	vli_mod_square_fast(t5, t5, curve);
	 
	vli_mod_mult_fast(x1, x1, t5, curve);
	 
	vli_mod_mult_fast(x2, x2, t5, curve);
	 
	vli_mod_add(t5, y2, y1, curve_prime, ndigits);
	 
	vli_mod_sub(y2, y2, y1, curve_prime, ndigits);

	 
	vli_mod_sub(t6, x2, x1, curve_prime, ndigits);
	 
	vli_mod_mult_fast(y1, y1, t6, curve);
	 
	vli_mod_add(t6, x1, x2, curve_prime, ndigits);
	 
	vli_mod_square_fast(x2, y2, curve);
	 
	vli_mod_sub(x2, x2, t6, curve_prime, ndigits);

	 
	vli_mod_sub(t7, x1, x2, curve_prime, ndigits);
	 
	vli_mod_mult_fast(y2, y2, t7, curve);
	 
	vli_mod_sub(y2, y2, y1, curve_prime, ndigits);

	 
	vli_mod_square_fast(t7, t5, curve);
	 
	vli_mod_sub(t7, t7, t6, curve_prime, ndigits);
	 
	vli_mod_sub(t6, t7, x1, curve_prime, ndigits);
	 
	vli_mod_mult_fast(t6, t6, t5, curve);
	 
	vli_mod_sub(y1, t6, y1, curve_prime, ndigits);

	vli_set(x1, t7, ndigits);
}

static void ecc_point_mult(struct ecc_point *result,
			   const struct ecc_point *point, const u64 *scalar,
			   u64 *initial_z, const struct ecc_curve *curve,
			   unsigned int ndigits)
{
	 
	u64 rx[2][ECC_MAX_DIGITS];
	u64 ry[2][ECC_MAX_DIGITS];
	u64 z[ECC_MAX_DIGITS];
	u64 sk[2][ECC_MAX_DIGITS];
	u64 *curve_prime = curve->p;
	int i, nb;
	int num_bits;
	int carry;

	carry = vli_add(sk[0], scalar, curve->n, ndigits);
	vli_add(sk[1], sk[0], curve->n, ndigits);
	scalar = sk[!carry];
	num_bits = sizeof(u64) * ndigits * 8 + 1;

	vli_set(rx[1], point->x, ndigits);
	vli_set(ry[1], point->y, ndigits);

	xycz_initial_double(rx[1], ry[1], rx[0], ry[0], initial_z, curve);

	for (i = num_bits - 2; i > 0; i--) {
		nb = !vli_test_bit(scalar, i);
		xycz_add_c(rx[1 - nb], ry[1 - nb], rx[nb], ry[nb], curve);
		xycz_add(rx[nb], ry[nb], rx[1 - nb], ry[1 - nb], curve);
	}

	nb = !vli_test_bit(scalar, 0);
	xycz_add_c(rx[1 - nb], ry[1 - nb], rx[nb], ry[nb], curve);

	 
	 
	vli_mod_sub(z, rx[1], rx[0], curve_prime, ndigits);
	 
	vli_mod_mult_fast(z, z, ry[1 - nb], curve);
	 
	vli_mod_mult_fast(z, z, point->x, curve);

	 
	vli_mod_inv(z, z, curve_prime, point->ndigits);

	 
	vli_mod_mult_fast(z, z, point->y, curve);
	 
	vli_mod_mult_fast(z, z, rx[1 - nb], curve);
	 

	xycz_add(rx[nb], ry[nb], rx[1 - nb], ry[1 - nb], curve);

	apply_z(rx[0], ry[0], z, curve);

	vli_set(result->x, rx[0], ndigits);
	vli_set(result->y, ry[0], ndigits);
}

 
static void ecc_point_add(const struct ecc_point *result,
		   const struct ecc_point *p, const struct ecc_point *q,
		   const struct ecc_curve *curve)
{
	u64 z[ECC_MAX_DIGITS];
	u64 px[ECC_MAX_DIGITS];
	u64 py[ECC_MAX_DIGITS];
	unsigned int ndigits = curve->g.ndigits;

	vli_set(result->x, q->x, ndigits);
	vli_set(result->y, q->y, ndigits);
	vli_mod_sub(z, result->x, p->x, curve->p, ndigits);
	vli_set(px, p->x, ndigits);
	vli_set(py, p->y, ndigits);
	xycz_add(px, py, result->x, result->y, curve);
	vli_mod_inv(z, z, curve->p, ndigits);
	apply_z(result->x, result->y, z, curve);
}

 
void ecc_point_mult_shamir(const struct ecc_point *result,
			   const u64 *u1, const struct ecc_point *p,
			   const u64 *u2, const struct ecc_point *q,
			   const struct ecc_curve *curve)
{
	u64 z[ECC_MAX_DIGITS];
	u64 sump[2][ECC_MAX_DIGITS];
	u64 *rx = result->x;
	u64 *ry = result->y;
	unsigned int ndigits = curve->g.ndigits;
	unsigned int num_bits;
	struct ecc_point sum = ECC_POINT_INIT(sump[0], sump[1], ndigits);
	const struct ecc_point *points[4];
	const struct ecc_point *point;
	unsigned int idx;
	int i;

	ecc_point_add(&sum, p, q, curve);
	points[0] = NULL;
	points[1] = p;
	points[2] = q;
	points[3] = &sum;

	num_bits = max(vli_num_bits(u1, ndigits), vli_num_bits(u2, ndigits));
	i = num_bits - 1;
	idx = !!vli_test_bit(u1, i);
	idx |= (!!vli_test_bit(u2, i)) << 1;
	point = points[idx];

	vli_set(rx, point->x, ndigits);
	vli_set(ry, point->y, ndigits);
	vli_clear(z + 1, ndigits - 1);
	z[0] = 1;

	for (--i; i >= 0; i--) {
		ecc_point_double_jacobian(rx, ry, z, curve);
		idx = !!vli_test_bit(u1, i);
		idx |= (!!vli_test_bit(u2, i)) << 1;
		point = points[idx];
		if (point) {
			u64 tx[ECC_MAX_DIGITS];
			u64 ty[ECC_MAX_DIGITS];
			u64 tz[ECC_MAX_DIGITS];

			vli_set(tx, point->x, ndigits);
			vli_set(ty, point->y, ndigits);
			apply_z(tx, ty, z, curve);
			vli_mod_sub(tz, rx, tx, curve->p, ndigits);
			xycz_add(tx, ty, rx, ry, curve);
			vli_mod_mult_fast(z, z, tz, curve);
		}
	}
	vli_mod_inv(z, z, curve->p, ndigits);
	apply_z(rx, ry, z, curve);
}
EXPORT_SYMBOL(ecc_point_mult_shamir);

static int __ecc_is_key_valid(const struct ecc_curve *curve,
			      const u64 *private_key, unsigned int ndigits)
{
	u64 one[ECC_MAX_DIGITS] = { 1, };
	u64 res[ECC_MAX_DIGITS];

	if (!private_key)
		return -EINVAL;

	if (curve->g.ndigits != ndigits)
		return -EINVAL;

	 
	if (vli_cmp(one, private_key, ndigits) != -1)
		return -EINVAL;
	vli_sub(res, curve->n, one, ndigits);
	vli_sub(res, res, one, ndigits);
	if (vli_cmp(res, private_key, ndigits) != 1)
		return -EINVAL;

	return 0;
}

int ecc_is_key_valid(unsigned int curve_id, unsigned int ndigits,
		     const u64 *private_key, unsigned int private_key_len)
{
	int nbytes;
	const struct ecc_curve *curve = ecc_get_curve(curve_id);

	nbytes = ndigits << ECC_DIGITS_TO_BYTES_SHIFT;

	if (private_key_len != nbytes)
		return -EINVAL;

	return __ecc_is_key_valid(curve, private_key, ndigits);
}
EXPORT_SYMBOL(ecc_is_key_valid);

 
int ecc_gen_privkey(unsigned int curve_id, unsigned int ndigits, u64 *privkey)
{
	const struct ecc_curve *curve = ecc_get_curve(curve_id);
	u64 priv[ECC_MAX_DIGITS];
	unsigned int nbytes = ndigits << ECC_DIGITS_TO_BYTES_SHIFT;
	unsigned int nbits = vli_num_bits(curve->n, ndigits);
	int err;

	 
	if (nbits < 160 || ndigits > ARRAY_SIZE(priv))
		return -EINVAL;

	 
	if (crypto_get_default_rng())
		return -EFAULT;

	err = crypto_rng_get_bytes(crypto_default_rng, (u8 *)priv, nbytes);
	crypto_put_default_rng();
	if (err)
		return err;

	 
	if (__ecc_is_key_valid(curve, priv, ndigits))
		return -EINVAL;

	ecc_swap_digits(priv, privkey, ndigits);

	return 0;
}
EXPORT_SYMBOL(ecc_gen_privkey);

int ecc_make_pub_key(unsigned int curve_id, unsigned int ndigits,
		     const u64 *private_key, u64 *public_key)
{
	int ret = 0;
	struct ecc_point *pk;
	u64 priv[ECC_MAX_DIGITS];
	const struct ecc_curve *curve = ecc_get_curve(curve_id);

	if (!private_key || !curve || ndigits > ARRAY_SIZE(priv)) {
		ret = -EINVAL;
		goto out;
	}

	ecc_swap_digits(private_key, priv, ndigits);

	pk = ecc_alloc_point(ndigits);
	if (!pk) {
		ret = -ENOMEM;
		goto out;
	}

	ecc_point_mult(pk, &curve->g, priv, NULL, curve, ndigits);

	 
	if (ecc_is_pubkey_valid_full(curve, pk)) {
		ret = -EAGAIN;
		goto err_free_point;
	}

	ecc_swap_digits(pk->x, public_key, ndigits);
	ecc_swap_digits(pk->y, &public_key[ndigits], ndigits);

err_free_point:
	ecc_free_point(pk);
out:
	return ret;
}
EXPORT_SYMBOL(ecc_make_pub_key);

 
int ecc_is_pubkey_valid_partial(const struct ecc_curve *curve,
				struct ecc_point *pk)
{
	u64 yy[ECC_MAX_DIGITS], xxx[ECC_MAX_DIGITS], w[ECC_MAX_DIGITS];

	if (WARN_ON(pk->ndigits != curve->g.ndigits))
		return -EINVAL;

	 
	if (ecc_point_is_zero(pk))
		return -EINVAL;

	 
	if (vli_cmp(curve->p, pk->x, pk->ndigits) != 1)
		return -EINVAL;
	if (vli_cmp(curve->p, pk->y, pk->ndigits) != 1)
		return -EINVAL;

	 
	vli_mod_square_fast(yy, pk->y, curve);  
	vli_mod_square_fast(xxx, pk->x, curve);  
	vli_mod_mult_fast(xxx, xxx, pk->x, curve);  
	vli_mod_mult_fast(w, curve->a, pk->x, curve);  
	vli_mod_add(w, w, curve->b, curve->p, pk->ndigits);  
	vli_mod_add(w, w, xxx, curve->p, pk->ndigits);  
	if (vli_cmp(yy, w, pk->ndigits) != 0)  
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(ecc_is_pubkey_valid_partial);

 
int ecc_is_pubkey_valid_full(const struct ecc_curve *curve,
			     struct ecc_point *pk)
{
	struct ecc_point *nQ;

	 
	int ret = ecc_is_pubkey_valid_partial(curve, pk);

	if (ret)
		return ret;

	 
	nQ = ecc_alloc_point(pk->ndigits);
	if (!nQ)
		return -ENOMEM;

	ecc_point_mult(nQ, pk, curve->n, NULL, curve, pk->ndigits);
	if (!ecc_point_is_zero(nQ))
		ret = -EINVAL;

	ecc_free_point(nQ);

	return ret;
}
EXPORT_SYMBOL(ecc_is_pubkey_valid_full);

int crypto_ecdh_shared_secret(unsigned int curve_id, unsigned int ndigits,
			      const u64 *private_key, const u64 *public_key,
			      u64 *secret)
{
	int ret = 0;
	struct ecc_point *product, *pk;
	u64 priv[ECC_MAX_DIGITS];
	u64 rand_z[ECC_MAX_DIGITS];
	unsigned int nbytes;
	const struct ecc_curve *curve = ecc_get_curve(curve_id);

	if (!private_key || !public_key || !curve ||
	    ndigits > ARRAY_SIZE(priv) || ndigits > ARRAY_SIZE(rand_z)) {
		ret = -EINVAL;
		goto out;
	}

	nbytes = ndigits << ECC_DIGITS_TO_BYTES_SHIFT;

	get_random_bytes(rand_z, nbytes);

	pk = ecc_alloc_point(ndigits);
	if (!pk) {
		ret = -ENOMEM;
		goto out;
	}

	ecc_swap_digits(public_key, pk->x, ndigits);
	ecc_swap_digits(&public_key[ndigits], pk->y, ndigits);
	ret = ecc_is_pubkey_valid_partial(curve, pk);
	if (ret)
		goto err_alloc_product;

	ecc_swap_digits(private_key, priv, ndigits);

	product = ecc_alloc_point(ndigits);
	if (!product) {
		ret = -ENOMEM;
		goto err_alloc_product;
	}

	ecc_point_mult(product, pk, priv, rand_z, curve, ndigits);

	if (ecc_point_is_zero(product)) {
		ret = -EFAULT;
		goto err_validity;
	}

	ecc_swap_digits(product->x, secret, ndigits);

err_validity:
	memzero_explicit(priv, sizeof(priv));
	memzero_explicit(rand_z, sizeof(rand_z));
	ecc_free_point(product);
err_alloc_product:
	ecc_free_point(pk);
out:
	return ret;
}
EXPORT_SYMBOL(crypto_ecdh_shared_secret);

MODULE_LICENSE("Dual BSD/GPL");
