SECTIONS {
	.IA_64.unwind_info : { *(.IA_64.unwind_info*) }
	.IA_64.unwind : { *(.IA_64.unwind*) }
	.core.plt : { BYTE(0) }
	.init.plt : { BYTE(0) }
	.got : { BYTE(0) }
	.opd : { BYTE(0) }
}
