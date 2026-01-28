#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/sched.h>
enum {
	dns_key_data,
	dns_key_error,
};
extern const struct cred *dns_resolver_cache;
extern unsigned int dns_resolver_debug;
#define	kdebug(FMT, ...)				\
do {							\
	if (unlikely(dns_resolver_debug))		\
		printk(KERN_DEBUG "[%-6.6s] "FMT"\n",	\
		       current->comm, ##__VA_ARGS__);	\
} while (0)
#define kenter(FMT, ...) kdebug("==> %s("FMT")", __func__, ##__VA_ARGS__)
#define kleave(FMT, ...) kdebug("<== %s()"FMT"", __func__, ##__VA_ARGS__)
