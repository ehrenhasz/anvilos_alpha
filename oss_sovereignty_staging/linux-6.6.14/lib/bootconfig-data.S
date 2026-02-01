 
 
	.section .init.rodata, "aw"
	.global embedded_bootconfig_data
embedded_bootconfig_data:
	.incbin "lib/default.bconf"
	.global embedded_bootconfig_data_end
embedded_bootconfig_data_end:
