#ifdef CONFIG_MODULE_SECTIONS
SECTIONS {
	.plt : { BYTE(0) }
	.got : { BYTE(0) }
	.got.plt : { BYTE(0) }
}
#endif
