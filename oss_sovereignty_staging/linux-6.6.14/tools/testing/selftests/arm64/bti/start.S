 
 

#include "assembler.h"

startfn _start
	mov	x0, sp
	b	start
endfn

emit_aarch64_feature_1_and
