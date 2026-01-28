


#ifndef __DFL_FME_PR_H
#define __DFL_FME_PR_H

#include <linux/platform_device.h>


struct dfl_fme_region {
	struct platform_device *region;
	struct list_head node;
	int port_id;
};


struct dfl_fme_region_pdata {
	struct platform_device *mgr;
	struct platform_device *br;
	int region_id;
};


struct dfl_fme_bridge {
	struct platform_device *br;
	struct list_head node;
};


struct dfl_fme_br_pdata {
	struct dfl_fpga_cdev *cdev;
	int port_id;
};


struct dfl_fme_mgr_pdata {
	void __iomem *ioaddr;
};

#define DFL_FPGA_FME_MGR	"dfl-fme-mgr"
#define DFL_FPGA_FME_BRIDGE	"dfl-fme-bridge"
#define DFL_FPGA_FME_REGION	"dfl-fme-region"

#endif 
