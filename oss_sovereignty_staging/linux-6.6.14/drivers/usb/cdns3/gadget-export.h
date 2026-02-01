 
 
#ifndef __LINUX_CDNS3_GADGET_EXPORT
#define __LINUX_CDNS3_GADGET_EXPORT

#if IS_ENABLED(CONFIG_USB_CDNSP_GADGET)

int cdnsp_gadget_init(struct cdns *cdns);
#else

static inline int cdnsp_gadget_init(struct cdns *cdns)
{
	return -ENXIO;
}

#endif  

#if IS_ENABLED(CONFIG_USB_CDNS3_GADGET)

int cdns3_gadget_init(struct cdns *cdns);
#else

static inline int cdns3_gadget_init(struct cdns *cdns)
{
	return -ENXIO;
}

#endif  

#endif  
