SECTIONS {
	.plt 0 : { BYTE(0) }
	.init.plt 0 : { BYTE(0) }
	.text.ftrace_trampoline 0 : { BYTE(0) }
#ifdef CONFIG_KASAN_SW_TAGS
	.text.hot : { *(.text.hot) }
#endif
#ifdef CONFIG_UNWIND_TABLES
	.init.eh_frame : { *(.eh_frame) }
#endif
}
