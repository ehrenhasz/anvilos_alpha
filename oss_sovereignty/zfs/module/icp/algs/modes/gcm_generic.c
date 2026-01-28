#include <modes/gcm_impl.h>
struct aes_block {
	uint64_t a;
	uint64_t b;
};
static void
gcm_generic_mul(uint64_t *x_in, uint64_t *y, uint64_t *res)
{
	static const uint64_t R = 0xe100000000000000ULL;
	struct aes_block z = {0, 0};
	struct aes_block v;
	uint64_t x;
	int i, j;
	v.a = ntohll(y[0]);
	v.b = ntohll(y[1]);
	for (j = 0; j < 2; j++) {
		x = ntohll(x_in[j]);
		for (i = 0; i < 64; i++, x <<= 1) {
			if (x & 0x8000000000000000ULL) {
				z.a ^= v.a;
				z.b ^= v.b;
			}
			if (v.b & 1ULL) {
				v.b = (v.a << 63)|(v.b >> 1);
				v.a = (v.a >> 1) ^ R;
			} else {
				v.b = (v.a << 63)|(v.b >> 1);
				v.a = v.a >> 1;
			}
		}
	}
	res[0] = htonll(z.a);
	res[1] = htonll(z.b);
}
static boolean_t
gcm_generic_will_work(void)
{
	return (B_TRUE);
}
const gcm_impl_ops_t gcm_generic_impl = {
	.mul = &gcm_generic_mul,
	.is_supported = &gcm_generic_will_work,
	.name = "generic"
};
