
 

#include <crypto/curve25519.h>
#include <linux/module.h>

const u8 curve25519_null_point[CURVE25519_KEY_SIZE] __aligned(32) = { 0 };
const u8 curve25519_base_point[CURVE25519_KEY_SIZE] __aligned(32) = { 9 };

EXPORT_SYMBOL(curve25519_null_point);
EXPORT_SYMBOL(curve25519_base_point);
EXPORT_SYMBOL(curve25519_generic);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Curve25519 scalar multiplication");
MODULE_AUTHOR("Jason A. Donenfeld <Jason@zx2c4.com>");
