 
 
SYM_FUNC_START(sev_verify_cbit)
#ifdef CONFIG_AMD_MEM_ENCRYPT
	 
	movq	sme_me_mask(%rip), %rsi
	testq	%rsi, %rsi
	jz	3f

	 
	movq	sev_status(%rip), %rsi
	testq	%rsi, %rsi
	jz	3f

	 
	movq	%cr4, %rsi

	 
	movq	%rsi, %rdx
	andq	$(~X86_CR4_PGE), %rdx
	movq	%rdx, %cr4

	 
1:	rdrand	%rdx
	jnc	1b

	 
	movq	%rdx, sev_check_data(%rip)

	 
	movq	%cr3, %rcx

	 
	movq	%rdi, %cr3

	 
	cmpq	%rdx, sev_check_data(%rip)

	 
	movq	%rcx, %cr3

	 
	movq	%rsi, %cr4

	 
	je	3f

	 
	xorq	%rsp, %rsp
	subq	$0x1000, %rsp
2:	hlt
	jmp 2b
3:
#endif
	 
	movq	%rdi, %rax
	RET
SYM_FUNC_END(sev_verify_cbit)
