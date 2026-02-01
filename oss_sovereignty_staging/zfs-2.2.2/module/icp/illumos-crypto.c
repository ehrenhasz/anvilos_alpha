 
 

#ifdef _KERNEL
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#else
#define	__exit
#define	__init
#endif

#include <sys/crypto/common.h>
#include <sys/crypto/api.h>
#include <sys/crypto/impl.h>
#include <sys/crypto/sched_impl.h>
#include <sys/crypto/icp.h>

 

void
icp_fini(void)
{
	skein_mod_fini();
	sha2_mod_fini();
	aes_mod_fini();
	kcf_sched_destroy();
	kcf_prov_tab_destroy();
	kcf_destroy_mech_tabs();
}

 
int __init
icp_init(void)
{
	 
	kcf_init_mech_tabs();

	 
	kcf_prov_tab_init();

	 
	kcf_sched_init();

	 
	aes_mod_init();
	sha2_mod_init();
	skein_mod_init();

	return (0);
}

#if defined(_KERNEL) && defined(__FreeBSD__)
module_exit(icp_fini);
module_init(icp_init);
#endif
