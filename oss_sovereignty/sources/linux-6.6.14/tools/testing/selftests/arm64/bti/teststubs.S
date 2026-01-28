


#include "assembler.h"

startfn bti_none_func
	bti
	ret
endfn

startfn bti_c_func
	bti	c
	ret
endfn

startfn bti_j_func
	bti	j
	ret
endfn

startfn bti_jc_func
	bti	jc
	ret
endfn

startfn paciasp_func
	paciasp
	autiasp
	ret
endfn

startfn nohint_func
	ret
endfn

emit_aarch64_feature_1_and
