#ifndef __HCLGE_DCB_H__
#define __HCLGE_DCB_H__
#include "hclge_main.h"
#ifdef CONFIG_HNS3_DCB
void hclge_dcb_ops_set(struct hclge_dev *hdev);
#else
static inline void hclge_dcb_ops_set(struct hclge_dev *hdev) {}
#endif
#endif  
