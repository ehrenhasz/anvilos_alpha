
 

#include <crypto/curve25519.h>
#include <linux/module.h>
#include <linux/init.h>

static int __init curve25519_init(void)
{
	if (!IS_ENABLED(CONFIG_CRYPTO_MANAGER_DISABLE_TESTS) &&
	    WARN_ON(!curve25519_selftest()))
		return -ENODEV;
	return 0;
}

static void __exit curve25519_exit(void)
{
}

module_init(curve25519_init);
module_exit(curve25519_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Curve25519 scalar multiplication");
MODULE_AUTHOR("Jason A. Donenfeld <Jason@zx2c4.com>");
