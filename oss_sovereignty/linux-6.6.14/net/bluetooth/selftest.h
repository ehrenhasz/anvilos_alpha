 

#if IS_ENABLED(CONFIG_BT_SELFTEST) && IS_MODULE(CONFIG_BT)

 
int bt_selftest(void);

#else

 
static inline int bt_selftest(void)
{
	return 0;
}

#endif
