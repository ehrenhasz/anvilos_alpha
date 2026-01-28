


.global pac_corruptor

.text

 pac_corruptor:
	paciasp

	
	eor lr, lr, #1 << 53

	autiasp
	ret
