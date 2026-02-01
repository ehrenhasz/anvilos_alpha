 
 

#ifndef __BPF_TOOL_CFG_H
#define __BPF_TOOL_CFG_H

#include "xlated_dumper.h"

void dump_xlated_cfg(struct dump_data *dd, void *buf, unsigned int len,
		     bool opcodes, bool linum);

#endif  
