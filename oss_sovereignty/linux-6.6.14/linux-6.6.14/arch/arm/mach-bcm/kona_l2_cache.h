#ifdef CONFIG_ARCH_BCM_MOBILE_L2_CACHE
void	kona_l2_cache_init(void);
#else
#define kona_l2_cache_init() ((void)0)
#endif
