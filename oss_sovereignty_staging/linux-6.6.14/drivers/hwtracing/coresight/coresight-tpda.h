 
 

#ifndef _CORESIGHT_CORESIGHT_TPDA_H
#define _CORESIGHT_CORESIGHT_TPDA_H

#define TPDA_CR			(0x000)
#define TPDA_Pn_CR(n)		(0x004 + (n * 4))
 
#define TPDA_Pn_CR_ENA		BIT(0)

#define TPDA_MAX_INPORTS	32

 
#define TPDA_CR_ATID		GENMASK(12, 6)

 
struct tpda_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	spinlock_t		spinlock;
	u8			atid;
};

#endif   
