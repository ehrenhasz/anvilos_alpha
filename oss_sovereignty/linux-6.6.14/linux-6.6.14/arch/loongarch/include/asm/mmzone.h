#ifndef _ASM_MMZONE_H_
#define _ASM_MMZONE_H_
#include <asm/page.h>
#include <asm/numa.h>
extern struct pglist_data *node_data[];
#define NODE_DATA(nid)	(node_data[(nid)])
#endif  
