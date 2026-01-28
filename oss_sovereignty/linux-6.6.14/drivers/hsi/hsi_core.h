#ifndef __LINUX_HSI_CORE_H__
#define __LINUX_HSI_CORE_H__
#include <linux/hsi/hsi.h>
struct hsi_cl_info {
	struct list_head	list;
	struct hsi_board_info	info;
};
extern struct list_head hsi_board_list;
#endif  
