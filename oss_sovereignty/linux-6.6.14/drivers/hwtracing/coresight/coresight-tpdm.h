 
 

#ifndef _CORESIGHT_CORESIGHT_TPDM_H
#define _CORESIGHT_CORESIGHT_TPDM_H

 
#define TPDM_DATASETS       7

 
#define TPDM_DSB_CR		(0x780)
 
#define TPDM_DSB_CR_ENA		BIT(0)

 
#define TPDM_ITATBCNTRL		(0xEF0)
#define TPDM_ITCNTRL		(0xF00)

 
#define ATBCNTRL_VAL_32		0xC00F1409
#define ATBCNTRL_VAL_64		0xC01F1409

 
#define INTEGRATION_TEST_CYCLE	10

 

#define TPDM_PIDR0_DS_IMPDEF	BIT(0)
#define TPDM_PIDR0_DS_DSB	BIT(1)

 

struct tpdm_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	spinlock_t		spinlock;
	bool			enable;
	unsigned long		datasets;
};

#endif   
