 
	.global test_mma
test_mma:
	 
	lxvh8x	4,0,3
	lxvh8x	5,0,4

	 
	.long	0x7c030162

	 
	 

	 
	.long	0xec042958

	 
	.long 0x7c000162

	 
	stxvw4x	0,0,5
	addi	5,5,16
	stxvw4x	1,0,5
	addi	5,5,16
	stxvw4x	2,0,5
	addi	5,5,16
	stxvw4x	3,0,5
	addi	5,5,16

	blr
