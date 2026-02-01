 
 
#ifndef __LINUX_CDNS3_HOST_EXPORT
#define __LINUX_CDNS3_HOST_EXPORT

#if IS_ENABLED(CONFIG_USB_CDNS_HOST)

int cdns_host_init(struct cdns *cdns);

#else

static inline int cdns_host_init(struct cdns *cdns)
{
	return -ENXIO;
}

static inline void cdns_host_exit(struct cdns *cdns) { }

#endif  

#endif  
