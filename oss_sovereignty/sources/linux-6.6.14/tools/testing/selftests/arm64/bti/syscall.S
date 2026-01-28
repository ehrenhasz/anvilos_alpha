


#include "assembler.h"

startfn syscall
	bti	c
	mov	w8, w0
	mov	x0, x1
	mov	x1, x2
	mov	x2, x3
	mov	x3, x4
	mov	x4, x5
	mov	x5, x6
	mov	x6, x7
	svc	#0
	ret
endfn

emit_aarch64_feature_1_and
