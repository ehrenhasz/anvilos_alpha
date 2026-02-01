 
 

#ifndef HVA_HW_H
#define HVA_HW_H

#include "hva-mem.h"

 
#define HVA_VERSION_UNKNOWN    0x000
#define HVA_VERSION_V400       0x400

 
enum hva_hw_cmd_type {
	 
	 
	H264_ENC = 0x02,
	 
	 
	 
	 
	 
	REMOVE_CLIENT = 0x08,
	FREEZE_CLIENT = 0x09,
	START_CLIENT = 0x0A,
	FREEZE_ALL = 0x0B,
	START_ALL = 0x0C,
	REMOVE_ALL = 0x0D
};

int hva_hw_probe(struct platform_device *pdev, struct hva_dev *hva);
void hva_hw_remove(struct hva_dev *hva);
int hva_hw_runtime_suspend(struct device *dev);
int hva_hw_runtime_resume(struct device *dev);
int hva_hw_execute_task(struct hva_ctx *ctx, enum hva_hw_cmd_type cmd,
			struct hva_buffer *task);
#ifdef CONFIG_VIDEO_STI_HVA_DEBUGFS
void hva_hw_dump_regs(struct hva_dev *hva, struct seq_file *s);
#endif

#endif  
