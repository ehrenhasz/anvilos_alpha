$flavour = shift;
$output  = shift;
if ($flavour =~ /\./) { $output = $flavour; undef $flavour; }
$win64=0; $win64=1 if ($flavour =~ /[nm]asm|mingw64/ || $output =~ /\.asm$/);
$kernel=0; $kernel=1 if (!$flavour && !$output);
if (!$kernel) {
	$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
	( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
	( $xlate="${dir}../../perlasm/x86_64-xlate.pl" and -f $xlate) or
	die "can't locate x86_64-xlate.pl";
	open OUT,"| \"$^X\" \"$xlate\" $flavour \"$output\"";
	*STDOUT=*OUT;
	if (`$ENV{CC} -Wa,-v -c -o /dev/null -x assembler /dev/null 2>&1`
	    =~ /GNU assembler version ([2-9]\.[0-9]+)/) {
		$avx = ($1>=2.19) + ($1>=2.22) + ($1>=2.25);
	}
	if (!$avx && $win64 && ($flavour =~ /nasm/ || $ENV{ASM} =~ /nasm/) &&
	    `nasm -v 2>&1` =~ /NASM version ([2-9]\.[0-9]+)(?:\.([0-9]+))?/) {
		$avx = ($1>=2.09) + ($1>=2.10) + ($1>=2.12);
		$avx += 1 if ($1==2.11 && $2>=8);
	}
	if (!$avx && $win64 && ($flavour =~ /masm/ || $ENV{ASM} =~ /ml64/) &&
	    `ml64 2>&1` =~ /Version ([0-9]+)\./) {
		$avx = ($1>=10) + ($1>=11);
	}
	if (!$avx && `$ENV{CC} -v 2>&1` =~ /((?:^clang|LLVM) version|.*based on LLVM) ([3-9]\.[0-9]+)/) {
		$avx = ($2>=3.0) + ($2>3.0);
	}
} else {
	$avx = 4; 
}
sub declare_function() {
	my ($name, $align, $nargs) = @_;
	if($kernel) {
		$code .= "SYM_FUNC_START($name)\n";
		$code .= ".L$name:\n";
	} else {
		$code .= ".globl	$name\n";
		$code .= ".type	$name,\@function,$nargs\n";
		$code .= ".align	$align\n";
		$code .= "$name:\n";
	}
}
sub end_function() {
	my ($name) = @_;
	if($kernel) {
		$code .= "SYM_FUNC_END($name)\n";
	} else {
		$code .= ".size   $name,.-$name\n";
	}
}
$code.=<<___ if $kernel;
___
if ($avx) {
$code.=<<___ if $kernel;
.section .rodata
___
$code.=<<___;
.align	64
.Lconst:
.Lmask24:
.long	0x0ffffff,0,0x0ffffff,0,0x0ffffff,0,0x0ffffff,0
.L129:
.long	`1<<24`,0,`1<<24`,0,`1<<24`,0,`1<<24`,0
.Lmask26:
.long	0x3ffffff,0,0x3ffffff,0,0x3ffffff,0,0x3ffffff,0
.Lpermd_avx2:
.long	2,2,2,3,2,0,2,1
.Lpermd_avx512:
.long	0,0,0,1, 0,2,0,3, 0,4,0,5, 0,6,0,7
.L2_44_inp_permd:
.long	0,1,1,2,2,3,7,7
.L2_44_inp_shift:
.quad	0,12,24,64
.L2_44_mask:
.quad	0xfffffffffff,0xfffffffffff,0x3ffffffffff,0xffffffffffffffff
.L2_44_shift_rgt:
.quad	44,44,42,64
.L2_44_shift_lft:
.quad	8,8,10,64
.align	64
.Lx_mask44:
.quad	0xfffffffffff,0xfffffffffff,0xfffffffffff,0xfffffffffff
.quad	0xfffffffffff,0xfffffffffff,0xfffffffffff,0xfffffffffff
.Lx_mask42:
.quad	0x3ffffffffff,0x3ffffffffff,0x3ffffffffff,0x3ffffffffff
.quad	0x3ffffffffff,0x3ffffffffff,0x3ffffffffff,0x3ffffffffff
___
}
$code.=<<___ if (!$kernel);
.asciz	"Poly1305 for x86_64, CRYPTOGAMS by <appro\@openssl.org>"
.align	16
___
my ($ctx,$inp,$len,$padbit)=("%rdi","%rsi","%rdx","%rcx");
my ($mac,$nonce)=($inp,$len);	
my ($d1,$d2,$d3, $r0,$r1,$s1)=("%r8","%r9","%rdi","%r11","%r12","%r13");
my ($h0,$h1,$h2)=("%r14","%rbx","%r10");
sub poly1305_iteration {
$code.=<<___;
	mulq	$h0			
	mov	%rax,$d2
	 mov	$r0,%rax
	mov	%rdx,$d3
	mulq	$h0			
	mov	%rax,$h0		
	 mov	$r0,%rax
	mov	%rdx,$d1
	mulq	$h1			
	add	%rax,$d2
	 mov	$s1,%rax
	adc	%rdx,$d3
	mulq	$h1			
	 mov	$h2,$h1			
	add	%rax,$h0
	adc	%rdx,$d1
	imulq	$s1,$h1			
	add	$h1,$d2
	 mov	$d1,$h1
	adc	\$0,$d3
	imulq	$r0,$h2			
	add	$d2,$h1
	mov	\$-4,%rax		
	adc	$h2,$d3
	and	$d3,%rax		
	mov	$d3,$h2
	shr	\$2,$d3
	and	\$3,$h2
	add	$d3,%rax
	add	%rax,$h0
	adc	\$0,$h1
	adc	\$0,$h2
___
}
$code.=<<___;
.text
___
$code.=<<___ if (!$kernel);
.extern	OPENSSL_ia32cap_P
.globl	poly1305_init_x86_64
.hidden	poly1305_init_x86_64
.globl	poly1305_blocks_x86_64
.hidden	poly1305_blocks_x86_64
.globl	poly1305_emit_x86_64
.hidden	poly1305_emit_x86_64
___
&declare_function("poly1305_init_x86_64", 32, 3);
$code.=<<___;
	xor	%eax,%eax
	mov	%rax,0($ctx)		
	mov	%rax,8($ctx)
	mov	%rax,16($ctx)
	test	$inp,$inp
	je	.Lno_key
___
$code.=<<___ if (!$kernel);
	lea	poly1305_blocks_x86_64(%rip),%r10
	lea	poly1305_emit_x86_64(%rip),%r11
___
$code.=<<___	if (!$kernel && $avx);
	mov	OPENSSL_ia32cap_P+4(%rip),%r9
	lea	poly1305_blocks_avx(%rip),%rax
	lea	poly1305_emit_avx(%rip),%rcx
	bt	\$`60-32`,%r9		
	cmovc	%rax,%r10
	cmovc	%rcx,%r11
___
$code.=<<___	if (!$kernel && $avx>1);
	lea	poly1305_blocks_avx2(%rip),%rax
	bt	\$`5+32`,%r9		
	cmovc	%rax,%r10
___
$code.=<<___	if (!$kernel && $avx>3);
	mov	\$`(1<<31|1<<21|1<<16)`,%rax
	shr	\$32,%r9
	and	%rax,%r9
	cmp	%rax,%r9
	je	.Linit_base2_44
___
$code.=<<___;
	mov	\$0x0ffffffc0fffffff,%rax
	mov	\$0x0ffffffc0ffffffc,%rcx
	and	0($inp),%rax
	and	8($inp),%rcx
	mov	%rax,24($ctx)
	mov	%rcx,32($ctx)
___
$code.=<<___	if (!$kernel && $flavour !~ /elf32/);
	mov	%r10,0(%rdx)
	mov	%r11,8(%rdx)
___
$code.=<<___	if (!$kernel && $flavour =~ /elf32/);
	mov	%r10d,0(%rdx)
	mov	%r11d,4(%rdx)
___
$code.=<<___;
	mov	\$1,%eax
.Lno_key:
	RET
___
&end_function("poly1305_init_x86_64");
&declare_function("poly1305_blocks_x86_64", 32, 4);
$code.=<<___;
.cfi_startproc
.Lblocks:
	shr	\$4,$len
	jz	.Lno_data		
	push	%rbx
.cfi_push	%rbx
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
	push	$ctx
.cfi_push	$ctx
.Lblocks_body:
	mov	$len,%r15		
	mov	24($ctx),$r0		
	mov	32($ctx),$s1
	mov	0($ctx),$h0		
	mov	8($ctx),$h1
	mov	16($ctx),$h2
	mov	$s1,$r1
	shr	\$2,$s1
	mov	$r1,%rax
	add	$r1,$s1			
	jmp	.Loop
.align	32
.Loop:
	add	0($inp),$h0		
	adc	8($inp),$h1
	lea	16($inp),$inp
	adc	$padbit,$h2
___
	&poly1305_iteration();
$code.=<<___;
	mov	$r1,%rax
	dec	%r15			
	jnz	.Loop
	mov	0(%rsp),$ctx
.cfi_restore	$ctx
	mov	$h0,0($ctx)		
	mov	$h1,8($ctx)
	mov	$h2,16($ctx)
	mov	8(%rsp),%r15
.cfi_restore	%r15
	mov	16(%rsp),%r14
.cfi_restore	%r14
	mov	24(%rsp),%r13
.cfi_restore	%r13
	mov	32(%rsp),%r12
.cfi_restore	%r12
	mov	40(%rsp),%rbx
.cfi_restore	%rbx
	lea	48(%rsp),%rsp
.cfi_adjust_cfa_offset	-48
.Lno_data:
.Lblocks_epilogue:
	RET
.cfi_endproc
___
&end_function("poly1305_blocks_x86_64");
&declare_function("poly1305_emit_x86_64", 32, 3);
$code.=<<___;
.Lemit:
	mov	0($ctx),%r8	
	mov	8($ctx),%r9
	mov	16($ctx),%r10
	mov	%r8,%rax
	add	\$5,%r8		
	mov	%r9,%rcx
	adc	\$0,%r9
	adc	\$0,%r10
	shr	\$2,%r10	
	cmovnz	%r8,%rax
	cmovnz	%r9,%rcx
	add	0($nonce),%rax	
	adc	8($nonce),%rcx
	mov	%rax,0($mac)	
	mov	%rcx,8($mac)
	RET
___
&end_function("poly1305_emit_x86_64");
if ($avx) {
my ($H0,$H1,$H2,$H3,$H4, $T0,$T1,$T2,$T3,$T4, $D0,$D1,$D2,$D3,$D4, $MASK) =
    map("%xmm$_",(0..15));
$code.=<<___;
.type	__poly1305_block,\@abi-omnipotent
.align	32
__poly1305_block:
	push $ctx
___
	&poly1305_iteration();
$code.=<<___;
	pop $ctx
	RET
.size	__poly1305_block,.-__poly1305_block
.type	__poly1305_init_avx,\@abi-omnipotent
.align	32
__poly1305_init_avx:
	push %rbp
	mov %rsp,%rbp
	mov	$r0,$h0
	mov	$r1,$h1
	xor	$h2,$h2
	lea	48+64($ctx),$ctx	
	mov	$r1,%rax
	call	__poly1305_block	
	mov	\$0x3ffffff,%eax	
	mov	\$0x3ffffff,%edx
	mov	$h0,$d1
	and	$h0
	mov	$r0,$d2
	and	$r0
	mov	%eax,`16*0+0-64`($ctx)
	shr	\$26,$d1
	mov	%edx,`16*0+4-64`($ctx)
	shr	\$26,$d2
	mov	\$0x3ffffff,%eax
	mov	\$0x3ffffff,%edx
	and	$d1
	and	$d2
	mov	%eax,`16*1+0-64`($ctx)
	lea	(%rax,%rax,4),%eax	
	mov	%edx,`16*1+4-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	
	mov	%eax,`16*2+0-64`($ctx)
	shr	\$26,$d1
	mov	%edx,`16*2+4-64`($ctx)
	shr	\$26,$d2
	mov	$h1,%rax
	mov	$r1,%rdx
	shl	\$12,%rax
	shl	\$12,%rdx
	or	$d1,%rax
	or	$d2,%rdx
	and	\$0x3ffffff,%eax
	and	\$0x3ffffff,%edx
	mov	%eax,`16*3+0-64`($ctx)
	lea	(%rax,%rax,4),%eax	
	mov	%edx,`16*3+4-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	
	mov	%eax,`16*4+0-64`($ctx)
	mov	$h1,$d1
	mov	%edx,`16*4+4-64`($ctx)
	mov	$r1,$d2
	mov	\$0x3ffffff,%eax
	mov	\$0x3ffffff,%edx
	shr	\$14,$d1
	shr	\$14,$d2
	and	$d1
	and	$d2
	mov	%eax,`16*5+0-64`($ctx)
	lea	(%rax,%rax,4),%eax	
	mov	%edx,`16*5+4-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	
	mov	%eax,`16*6+0-64`($ctx)
	shr	\$26,$d1
	mov	%edx,`16*6+4-64`($ctx)
	shr	\$26,$d2
	mov	$h2,%rax
	shl	\$24,%rax
	or	%rax,$d1
	mov	$d1
	lea	($d1,$d1,4),$d1		
	mov	$d2
	lea	($d2,$d2,4),$d2		
	mov	$d1
	mov	$d2
	mov	$r1,%rax
	call	__poly1305_block	
	mov	\$0x3ffffff,%eax	
	mov	$h0,$d1
	and	$h0
	shr	\$26,$d1
	mov	%eax,`16*0+12-64`($ctx)
	mov	\$0x3ffffff,%edx
	and	$d1
	mov	%edx,`16*1+12-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	
	shr	\$26,$d1
	mov	%edx,`16*2+12-64`($ctx)
	mov	$h1,%rax
	shl	\$12,%rax
	or	$d1,%rax
	and	\$0x3ffffff,%eax
	mov	%eax,`16*3+12-64`($ctx)
	lea	(%rax,%rax,4),%eax	
	mov	$h1,$d1
	mov	%eax,`16*4+12-64`($ctx)
	mov	\$0x3ffffff,%edx
	shr	\$14,$d1
	and	$d1
	mov	%edx,`16*5+12-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	
	shr	\$26,$d1
	mov	%edx,`16*6+12-64`($ctx)
	mov	$h2,%rax
	shl	\$24,%rax
	or	%rax,$d1
	mov	$d1
	lea	($d1,$d1,4),$d1		
	mov	$d1
	mov	$r1,%rax
	call	__poly1305_block	
	mov	\$0x3ffffff,%eax	
	mov	$h0,$d1
	and	$h0
	shr	\$26,$d1
	mov	%eax,`16*0+8-64`($ctx)
	mov	\$0x3ffffff,%edx
	and	$d1
	mov	%edx,`16*1+8-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	
	shr	\$26,$d1
	mov	%edx,`16*2+8-64`($ctx)
	mov	$h1,%rax
	shl	\$12,%rax
	or	$d1,%rax
	and	\$0x3ffffff,%eax
	mov	%eax,`16*3+8-64`($ctx)
	lea	(%rax,%rax,4),%eax	
	mov	$h1,$d1
	mov	%eax,`16*4+8-64`($ctx)
	mov	\$0x3ffffff,%edx
	shr	\$14,$d1
	and	$d1
	mov	%edx,`16*5+8-64`($ctx)
	lea	(%rdx,%rdx,4),%edx	
	shr	\$26,$d1
	mov	%edx,`16*6+8-64`($ctx)
	mov	$h2,%rax
	shl	\$24,%rax
	or	%rax,$d1
	mov	$d1
	lea	($d1,$d1,4),$d1		
	mov	$d1
	lea	-48-64($ctx),$ctx	
	pop %rbp
	RET
.size	__poly1305_init_avx,.-__poly1305_init_avx
___
&declare_function("poly1305_blocks_avx", 32, 4);
$code.=<<___;
.cfi_startproc
	mov	20($ctx),%r8d		
	cmp	\$128,$len
	jae	.Lblocks_avx
	test	%r8d,%r8d
	jz	.Lblocks
.Lblocks_avx:
	and	\$-16,$len
	jz	.Lno_data_avx
	vzeroupper
	test	%r8d,%r8d
	jz	.Lbase2_64_avx
	test	\$31,$len
	jz	.Leven_avx
	push	%rbp
.cfi_push	%rbp
	mov 	%rsp,%rbp
	push	%rbx
.cfi_push	%rbx
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
.Lblocks_avx_body:
	mov	$len,%r15		
	mov	0($ctx),$d1		
	mov	8($ctx),$d2
	mov	16($ctx),$h2
	mov	24($ctx),$r0		
	mov	32($ctx),$s1
	mov	$d1
	and	\$`-1*(1<<31)`,$d1
	mov	$d2,$r1			
	mov	$d2
	and	\$`-1*(1<<31)`,$d2
	shr	\$6,$d1
	shl	\$52,$r1
	add	$d1,$h0
	shr	\$12,$h1
	shr	\$18,$d2
	add	$r1,$h0
	adc	$d2,$h1
	mov	$h2,$d1
	shl	\$40,$d1
	shr	\$24,$h2
	add	$d1,$h1
	adc	\$0,$h2			
	mov	\$-4,$d2		
	mov	$h2,$d1
	and	$h2,$d2
	shr	\$2,$d1
	and	\$3,$h2
	add	$d2,$d1			
	add	$d1,$h0
	adc	\$0,$h1
	adc	\$0,$h2
	mov	$s1,$r1
	mov	$s1,%rax
	shr	\$2,$s1
	add	$r1,$s1			
	add	0($inp),$h0		
	adc	8($inp),$h1
	lea	16($inp),$inp
	adc	$padbit,$h2
	call	__poly1305_block
	test	$padbit,$padbit		
	jz	.Lstore_base2_64_avx	
	mov	$h0,%rax
	mov	$h0,%rdx
	shr	\$52,$h0
	mov	$h1,$r0
	mov	$h1,$r1
	shr	\$26,%rdx
	and	\$0x3ffffff,%rax	
	shl	\$12,$r0
	and	\$0x3ffffff,%rdx	
	shr	\$14,$h1
	or	$r0,$h0
	shl	\$24,$h2
	and	\$0x3ffffff,$h0		
	shr	\$40,$r1
	and	\$0x3ffffff,$h1		
	or	$r1,$h2			
	sub	\$16,%r15
	jz	.Lstore_base2_26_avx
	vmovd	%rax
	vmovd	%rdx
	vmovd	$h0
	vmovd	$h1
	vmovd	$h2
	jmp	.Lproceed_avx
.align	32
.Lstore_base2_64_avx:
	mov	$h0,0($ctx)
	mov	$h1,8($ctx)
	mov	$h2,16($ctx)		
	jmp	.Ldone_avx
.align	16
.Lstore_base2_26_avx:
	mov	%rax
	mov	%rdx
	mov	$h0
	mov	$h1
	mov	$h2
.align	16
.Ldone_avx:
	pop 		%r15
.cfi_restore	%r15
	pop 		%r14
.cfi_restore	%r14
	pop 		%r13
.cfi_restore	%r13
	pop 		%r12
.cfi_restore	%r12
	pop 		%rbx
.cfi_restore	%rbx
	pop 		%rbp
.cfi_restore	%rbp
.Lno_data_avx:
.Lblocks_avx_epilogue:
	RET
.cfi_endproc
.align	32
.Lbase2_64_avx:
.cfi_startproc
	push	%rbp
.cfi_push	%rbp
	mov 	%rsp,%rbp
	push	%rbx
.cfi_push	%rbx
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
.Lbase2_64_avx_body:
	mov	$len,%r15		
	mov	24($ctx),$r0		
	mov	32($ctx),$s1
	mov	0($ctx),$h0		
	mov	8($ctx),$h1
	mov	16($ctx),$h2
	mov	$s1,$r1
	mov	$s1,%rax
	shr	\$2,$s1
	add	$r1,$s1			
	test	\$31,$len
	jz	.Linit_avx
	add	0($inp),$h0		
	adc	8($inp),$h1
	lea	16($inp),$inp
	adc	$padbit,$h2
	sub	\$16,%r15
	call	__poly1305_block
.Linit_avx:
	mov	$h0,%rax
	mov	$h0,%rdx
	shr	\$52,$h0
	mov	$h1,$d1
	mov	$h1,$d2
	shr	\$26,%rdx
	and	\$0x3ffffff,%rax	
	shl	\$12,$d1
	and	\$0x3ffffff,%rdx	
	shr	\$14,$h1
	or	$d1,$h0
	shl	\$24,$h2
	and	\$0x3ffffff,$h0		
	shr	\$40,$d2
	and	\$0x3ffffff,$h1		
	or	$d2,$h2			
	vmovd	%rax
	vmovd	%rdx
	vmovd	$h0
	vmovd	$h1
	vmovd	$h2
	movl	\$1,20($ctx)		
	call	__poly1305_init_avx
.Lproceed_avx:
	mov	%r15,$len
	pop 		%r15
.cfi_restore	%r15
	pop 		%r14
.cfi_restore	%r14
	pop 		%r13
.cfi_restore	%r13
	pop 		%r12
.cfi_restore	%r12
	pop 		%rbx
.cfi_restore	%rbx
	pop 		%rbp
.cfi_restore	%rbp
.Lbase2_64_avx_epilogue:
	jmp	.Ldo_avx
.cfi_endproc
.align	32
.Leven_avx:
.cfi_startproc
	vmovd		4*0($ctx),$H0		
	vmovd		4*1($ctx),$H1
	vmovd		4*2($ctx),$H2
	vmovd		4*3($ctx),$H3
	vmovd		4*4($ctx),$H4
.Ldo_avx:
___
$code.=<<___	if (!$win64);
	lea		8(%rsp),%r10
.cfi_def_cfa_register	%r10
	and		\$-32,%rsp
	sub		\$-8,%rsp
	lea		-0x58(%rsp),%r11
	sub		\$0x178,%rsp
___
$code.=<<___	if ($win64);
	lea		-0xf8(%rsp),%r11
	sub		\$0x218,%rsp
	vmovdqa		%xmm6,0x50(%r11)
	vmovdqa		%xmm7,0x60(%r11)
	vmovdqa		%xmm8,0x70(%r11)
	vmovdqa		%xmm9,0x80(%r11)
	vmovdqa		%xmm10,0x90(%r11)
	vmovdqa		%xmm11,0xa0(%r11)
	vmovdqa		%xmm12,0xb0(%r11)
	vmovdqa		%xmm13,0xc0(%r11)
	vmovdqa		%xmm14,0xd0(%r11)
	vmovdqa		%xmm15,0xe0(%r11)
.Ldo_avx_body:
___
$code.=<<___;
	sub		\$64,$len
	lea		-32($inp),%rax
	cmovc		%rax,$inp
	vmovdqu		`16*3`($ctx),$D4	
	lea		`16*3+64`($ctx),$ctx	
	lea		.Lconst(%rip),%rcx
	vmovdqu		16*2($inp),$T0
	vmovdqu		16*3($inp),$T1
	vmovdqa		64(%rcx),$MASK		
	vpsrldq		\$6,$T0,$T2		
	vpsrldq		\$6,$T1,$T3
	vpunpckhqdq	$T1,$T0,$T4		
	vpunpcklqdq	$T1,$T0,$T0		
	vpunpcklqdq	$T3,$T2,$T3		
	vpsrlq		\$40,$T4,$T4		
	vpsrlq		\$26,$T0,$T1
	vpand		$MASK,$T0,$T0		
	vpsrlq		\$4,$T3,$T2
	vpand		$MASK,$T1,$T1		
	vpsrlq		\$30,$T3,$T3
	vpand		$MASK,$T2,$T2		
	vpand		$MASK,$T3,$T3		
	vpor		32(%rcx),$T4,$T4	
	jbe		.Lskip_loop_avx
	vmovdqu		`16*1-64`($ctx),$D1
	vmovdqu		`16*2-64`($ctx),$D2
	vpshufd		\$0xEE,$D4,$D3		
	vpshufd		\$0x44,$D4,$D0		
	vmovdqa		$D3,-0x90(%r11)
	vmovdqa		$D0,0x00(%rsp)
	vpshufd		\$0xEE,$D1,$D4
	vmovdqu		`16*3-64`($ctx),$D0
	vpshufd		\$0x44,$D1,$D1
	vmovdqa		$D4,-0x80(%r11)
	vmovdqa		$D1,0x10(%rsp)
	vpshufd		\$0xEE,$D2,$D3
	vmovdqu		`16*4-64`($ctx),$D1
	vpshufd		\$0x44,$D2,$D2
	vmovdqa		$D3,-0x70(%r11)
	vmovdqa		$D2,0x20(%rsp)
	vpshufd		\$0xEE,$D0,$D4
	vmovdqu		`16*5-64`($ctx),$D2
	vpshufd		\$0x44,$D0,$D0
	vmovdqa		$D4,-0x60(%r11)
	vmovdqa		$D0,0x30(%rsp)
	vpshufd		\$0xEE,$D1,$D3
	vmovdqu		`16*6-64`($ctx),$D0
	vpshufd		\$0x44,$D1,$D1
	vmovdqa		$D3,-0x50(%r11)
	vmovdqa		$D1,0x40(%rsp)
	vpshufd		\$0xEE,$D2,$D4
	vmovdqu		`16*7-64`($ctx),$D1
	vpshufd		\$0x44,$D2,$D2
	vmovdqa		$D4,-0x40(%r11)
	vmovdqa		$D2,0x50(%rsp)
	vpshufd		\$0xEE,$D0,$D3
	vmovdqu		`16*8-64`($ctx),$D2
	vpshufd		\$0x44,$D0,$D0
	vmovdqa		$D3,-0x30(%r11)
	vmovdqa		$D0,0x60(%rsp)
	vpshufd		\$0xEE,$D1,$D4
	vpshufd		\$0x44,$D1,$D1
	vmovdqa		$D4,-0x20(%r11)
	vmovdqa		$D1,0x70(%rsp)
	vpshufd		\$0xEE,$D2,$D3
	 vmovdqa	0x00(%rsp),$D4		
	vpshufd		\$0x44,$D2,$D2
	vmovdqa		$D3,-0x10(%r11)
	vmovdqa		$D2,0x80(%rsp)
	jmp		.Loop_avx
.align	32
.Loop_avx:
	vpmuludq	$T0,$D4,$D0		
	vpmuludq	$T1,$D4,$D1		
	  vmovdqa	$H2,0x20(%r11)				
	vpmuludq	$T2,$D4,$D2		
	 vmovdqa	0x10(%rsp),$H2		
	vpmuludq	$T3,$D4,$D3		
	vpmuludq	$T4,$D4,$D4		
	  vmovdqa	$H0,0x00(%r11)				
	vpmuludq	0x20(%rsp),$T4,$H0	
	  vmovdqa	$H1,0x10(%r11)				
	vpmuludq	$T3,$H2,$H1		
	vpaddq		$H0,$D0,$D0		
	vpaddq		$H1,$D4,$D4		
	  vmovdqa	$H3,0x30(%r11)				
	vpmuludq	$T2,$H2,$H0		
	vpmuludq	$T1,$H2,$H1		
	vpaddq		$H0,$D3,$D3		
	 vmovdqa	0x30(%rsp),$H3		
	vpaddq		$H1,$D2,$D2		
	  vmovdqa	$H4,0x40(%r11)				
	vpmuludq	$T0,$H2,$H2		
	 vpmuludq	$T2,$H3,$H0		
	vpaddq		$H2,$D1,$D1		
	 vmovdqa	0x40(%rsp),$H4		
	vpaddq		$H0,$D4,$D4		
	vpmuludq	$T1,$H3,$H1		
	vpmuludq	$T0,$H3,$H3		
	vpaddq		$H1,$D3,$D3		
	 vmovdqa	0x50(%rsp),$H2		
	vpaddq		$H3,$D2,$D2		
	vpmuludq	$T4,$H4,$H0		
	vpmuludq	$T3,$H4,$H4		
	vpaddq		$H0,$D1,$D1		
	 vmovdqa	0x60(%rsp),$H3		
	vpaddq		$H4,$D0,$D0		
	 vmovdqa	0x80(%rsp),$H4		
	vpmuludq	$T1,$H2,$H1		
	vpmuludq	$T0,$H2,$H2		
	vpaddq		$H1,$D4,$D4		
	vpaddq		$H2,$D3,$D3		
	vpmuludq	$T4,$H3,$H0		
	vpmuludq	$T3,$H3,$H1		
	vpaddq		$H0,$D2,$D2		
	 vmovdqu	16*0($inp),$H0				
	vpaddq		$H1,$D1,$D1		
	vpmuludq	$T2,$H3,$H3		
	 vpmuludq	$T2,$H4,$T2		
	vpaddq		$H3,$D0,$D0		
	 vmovdqu	16*1($inp),$H1				
	vpaddq		$T2,$D1,$D1		
	vpmuludq	$T3,$H4,$T3		
	vpmuludq	$T4,$H4,$T4		
	 vpsrldq	\$6,$H0,$H2				
	vpaddq		$T3,$D2,$D2		
	vpaddq		$T4,$D3,$D3		
	 vpsrldq	\$6,$H1,$H3				
	vpmuludq	0x70(%rsp),$T0,$T4	
	vpmuludq	$T1,$H4,$T0		
	 vpunpckhqdq	$H1,$H0,$H4		
	vpaddq		$T4,$D4,$D4		
	 vmovdqa	-0x90(%r11),$T4		
	vpaddq		$T0,$D0,$D0		
	vpunpcklqdq	$H1,$H0,$H0		
	vpunpcklqdq	$H3,$H2,$H3		
	vpsrldq		\$`40/8`,$H4,$H4	
	vpsrlq		\$26,$H0,$H1
	vpand		$MASK,$H0,$H0		
	vpsrlq		\$4,$H3,$H2
	vpand		$MASK,$H1,$H1		
	vpand		0(%rcx),$H4,$H4		
	vpsrlq		\$30,$H3,$H3
	vpand		$MASK,$H2,$H2		
	vpand		$MASK,$H3,$H3		
	vpor		32(%rcx),$H4,$H4	
	vpaddq		0x00(%r11),$H0,$H0	
	vpaddq		0x10(%r11),$H1,$H1
	vpaddq		0x20(%r11),$H2,$H2
	vpaddq		0x30(%r11),$H3,$H3
	vpaddq		0x40(%r11),$H4,$H4
	lea		16*2($inp),%rax
	lea		16*4($inp),$inp
	sub		\$64,$len
	cmovc		%rax,$inp
	vpmuludq	$H0,$T4,$T0		
	vpmuludq	$H1,$T4,$T1		
	vpaddq		$T0,$D0,$D0
	vpaddq		$T1,$D1,$D1
	 vmovdqa	-0x80(%r11),$T2		
	vpmuludq	$H2,$T4,$T0		
	vpmuludq	$H3,$T4,$T1		
	vpaddq		$T0,$D2,$D2
	vpaddq		$T1,$D3,$D3
	vpmuludq	$H4,$T4,$T4		
	 vpmuludq	-0x70(%r11),$H4,$T0	
	vpaddq		$T4,$D4,$D4
	vpaddq		$T0,$D0,$D0		
	vpmuludq	$H2,$T2,$T1		
	vpmuludq	$H3,$T2,$T0		
	vpaddq		$T1,$D3,$D3		
	 vmovdqa	-0x60(%r11),$T3		
	vpaddq		$T0,$D4,$D4		
	vpmuludq	$H1,$T2,$T1		
	vpmuludq	$H0,$T2,$T2		
	vpaddq		$T1,$D2,$D2		
	vpaddq		$T2,$D1,$D1		
	 vmovdqa	-0x50(%r11),$T4		
	vpmuludq	$H2,$T3,$T0		
	vpmuludq	$H1,$T3,$T1		
	vpaddq		$T0,$D4,$D4		
	vpaddq		$T1,$D3,$D3		
	 vmovdqa	-0x40(%r11),$T2		
	vpmuludq	$H0,$T3,$T3		
	vpmuludq	$H4,$T4,$T0		
	vpaddq		$T3,$D2,$D2		
	vpaddq		$T0,$D1,$D1		
	 vmovdqa	-0x30(%r11),$T3		
	vpmuludq	$H3,$T4,$T4		
	 vpmuludq	$H1,$T2,$T1		
	vpaddq		$T4,$D0,$D0		
	 vmovdqa	-0x10(%r11),$T4		
	vpaddq		$T1,$D4,$D4		
	vpmuludq	$H0,$T2,$T2		
	vpmuludq	$H4,$T3,$T0		
	vpaddq		$T2,$D3,$D3		
	vpaddq		$T0,$D2,$D2		
	 vmovdqu	16*2($inp),$T0				
	vpmuludq	$H3,$T3,$T2		
	vpmuludq	$H2,$T3,$T3		
	vpaddq		$T2,$D1,$D1		
	 vmovdqu	16*3($inp),$T1				
	vpaddq		$T3,$D0,$D0		
	vpmuludq	$H2,$T4,$H2		
	vpmuludq	$H3,$T4,$H3		
	 vpsrldq	\$6,$T0,$T2				
	vpaddq		$H2,$D1,$D1		
	vpmuludq	$H4,$T4,$H4		
	 vpsrldq	\$6,$T1,$T3				
	vpaddq		$H3,$D2,$H2		
	vpaddq		$H4,$D3,$H3		
	vpmuludq	-0x20(%r11),$H0,$H4	
	vpmuludq	$H1,$T4,$H0
	 vpunpckhqdq	$T1,$T0,$T4		
	vpaddq		$H4,$D4,$H4		
	vpaddq		$H0,$D0,$H0		
	vpunpcklqdq	$T1,$T0,$T0		
	vpunpcklqdq	$T3,$T2,$T3		
	vpsrldq		\$`40/8`,$T4,$T4	
	vpsrlq		\$26,$T0,$T1
	 vmovdqa	0x00(%rsp),$D4		
	vpand		$MASK,$T0,$T0		
	vpsrlq		\$4,$T3,$T2
	vpand		$MASK,$T1,$T1		
	vpand		0(%rcx),$T4,$T4		
	vpsrlq		\$30,$T3,$T3
	vpand		$MASK,$T2,$T2		
	vpand		$MASK,$T3,$T3		
	vpor		32(%rcx),$T4,$T4	
	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		
	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$D1,$H1		
	vpsrlq		\$26,$H4,$D0
	vpand		$MASK,$H4,$H4
	vpsrlq		\$26,$H1,$D1
	vpand		$MASK,$H1,$H1
	vpaddq		$D1,$H2,$H2		
	vpaddq		$D0,$H0,$H0
	vpsllq		\$2,$D0,$D0
	vpaddq		$D0,$H0,$H0		
	vpsrlq		\$26,$H2,$D2
	vpand		$MASK,$H2,$H2
	vpaddq		$D2,$H3,$H3		
	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$H1,$H1		
	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		
	ja		.Loop_avx
.Lskip_loop_avx:
	vpshufd		\$0x10,$D4,$D4		
	add		\$32,$len
	jnz		.Long_tail_avx
	vpaddq		$H2,$T2,$T2
	vpaddq		$H0,$T0,$T0
	vpaddq		$H1,$T1,$T1
	vpaddq		$H3,$T3,$T3
	vpaddq		$H4,$T4,$T4
.Long_tail_avx:
	vmovdqa		$H2,0x20(%r11)
	vmovdqa		$H0,0x00(%r11)
	vmovdqa		$H1,0x10(%r11)
	vmovdqa		$H3,0x30(%r11)
	vmovdqa		$H4,0x40(%r11)
	vpmuludq	$T2,$D4,$D2		
	vpmuludq	$T0,$D4,$D0		
	 vpshufd	\$0x10,`16*1-64`($ctx),$H2		
	vpmuludq	$T1,$D4,$D1		
	vpmuludq	$T3,$D4,$D3		
	vpmuludq	$T4,$D4,$D4		
	vpmuludq	$T3,$H2,$H0		
	vpaddq		$H0,$D4,$D4		
	 vpshufd	\$0x10,`16*2-64`($ctx),$H3		
	vpmuludq	$T2,$H2,$H1		
	vpaddq		$H1,$D3,$D3		
	 vpshufd	\$0x10,`16*3-64`($ctx),$H4		
	vpmuludq	$T1,$H2,$H0		
	vpaddq		$H0,$D2,$D2		
	vpmuludq	$T0,$H2,$H2		
	vpaddq		$H2,$D1,$D1		
	vpmuludq	$T4,$H3,$H3		
	vpaddq		$H3,$D0,$D0		
	 vpshufd	\$0x10,`16*4-64`($ctx),$H2		
	vpmuludq	$T2,$H4,$H1		
	vpaddq		$H1,$D4,$D4		
	vpmuludq	$T1,$H4,$H0		
	vpaddq		$H0,$D3,$D3		
	 vpshufd	\$0x10,`16*5-64`($ctx),$H3		
	vpmuludq	$T0,$H4,$H4		
	vpaddq		$H4,$D2,$D2		
	vpmuludq	$T4,$H2,$H1		
	vpaddq		$H1,$D1,$D1		
	 vpshufd	\$0x10,`16*6-64`($ctx),$H4		
	vpmuludq	$T3,$H2,$H2		
	vpaddq		$H2,$D0,$D0		
	vpmuludq	$T1,$H3,$H0		
	vpaddq		$H0,$D4,$D4		
	vpmuludq	$T0,$H3,$H3		
	vpaddq		$H3,$D3,$D3		
	 vpshufd	\$0x10,`16*7-64`($ctx),$H2		
	vpmuludq	$T4,$H4,$H1		
	vpaddq		$H1,$D2,$D2		
	 vpshufd	\$0x10,`16*8-64`($ctx),$H3		
	vpmuludq	$T3,$H4,$H0		
	vpaddq		$H0,$D1,$D1		
	vpmuludq	$T2,$H4,$H4		
	vpaddq		$H4,$D0,$D0		
	vpmuludq	$T0,$H2,$H2		
	vpaddq		$H2,$D4,$D4		
	vpmuludq	$T4,$H3,$H1		
	vpaddq		$H1,$D3,$D3		
	vpmuludq	$T3,$H3,$H0		
	vpaddq		$H0,$D2,$D2		
	vpmuludq	$T2,$H3,$H1		
	vpaddq		$H1,$D1,$D1		
	vpmuludq	$T1,$H3,$H3		
	vpaddq		$H3,$D0,$D0		
	jz		.Lshort_tail_avx
	vmovdqu		16*0($inp),$H0		
	vmovdqu		16*1($inp),$H1
	vpsrldq		\$6,$H0,$H2		
	vpsrldq		\$6,$H1,$H3
	vpunpckhqdq	$H1,$H0,$H4		
	vpunpcklqdq	$H1,$H0,$H0		
	vpunpcklqdq	$H3,$H2,$H3		
	vpsrlq		\$40,$H4,$H4		
	vpsrlq		\$26,$H0,$H1
	vpand		$MASK,$H0,$H0		
	vpsrlq		\$4,$H3,$H2
	vpand		$MASK,$H1,$H1		
	vpsrlq		\$30,$H3,$H3
	vpand		$MASK,$H2,$H2		
	vpand		$MASK,$H3,$H3		
	vpor		32(%rcx),$H4,$H4	
	vpshufd		\$0x32,`16*0-64`($ctx),$T4	
	vpaddq		0x00(%r11),$H0,$H0
	vpaddq		0x10(%r11),$H1,$H1
	vpaddq		0x20(%r11),$H2,$H2
	vpaddq		0x30(%r11),$H3,$H3
	vpaddq		0x40(%r11),$H4,$H4
	vpmuludq	$H0,$T4,$T0		
	vpaddq		$T0,$D0,$D0		
	vpmuludq	$H1,$T4,$T1		
	vpaddq		$T1,$D1,$D1		
	vpmuludq	$H2,$T4,$T0		
	vpaddq		$T0,$D2,$D2		
	 vpshufd	\$0x32,`16*1-64`($ctx),$T2		
	vpmuludq	$H3,$T4,$T1		
	vpaddq		$T1,$D3,$D3		
	vpmuludq	$H4,$T4,$T4		
	vpaddq		$T4,$D4,$D4		
	vpmuludq	$H3,$T2,$T0		
	vpaddq		$T0,$D4,$D4		
	 vpshufd	\$0x32,`16*2-64`($ctx),$T3		
	vpmuludq	$H2,$T2,$T1		
	vpaddq		$T1,$D3,$D3		
	 vpshufd	\$0x32,`16*3-64`($ctx),$T4		
	vpmuludq	$H1,$T2,$T0		
	vpaddq		$T0,$D2,$D2		
	vpmuludq	$H0,$T2,$T2		
	vpaddq		$T2,$D1,$D1		
	vpmuludq	$H4,$T3,$T3		
	vpaddq		$T3,$D0,$D0		
	 vpshufd	\$0x32,`16*4-64`($ctx),$T2		
	vpmuludq	$H2,$T4,$T1		
	vpaddq		$T1,$D4,$D4		
	vpmuludq	$H1,$T4,$T0		
	vpaddq		$T0,$D3,$D3		
	 vpshufd	\$0x32,`16*5-64`($ctx),$T3		
	vpmuludq	$H0,$T4,$T4		
	vpaddq		$T4,$D2,$D2		
	vpmuludq	$H4,$T2,$T1		
	vpaddq		$T1,$D1,$D1		
	 vpshufd	\$0x32,`16*6-64`($ctx),$T4		
	vpmuludq	$H3,$T2,$T2		
	vpaddq		$T2,$D0,$D0		
	vpmuludq	$H1,$T3,$T0		
	vpaddq		$T0,$D4,$D4		
	vpmuludq	$H0,$T3,$T3		
	vpaddq		$T3,$D3,$D3		
	 vpshufd	\$0x32,`16*7-64`($ctx),$T2		
	vpmuludq	$H4,$T4,$T1		
	vpaddq		$T1,$D2,$D2		
	 vpshufd	\$0x32,`16*8-64`($ctx),$T3		
	vpmuludq	$H3,$T4,$T0		
	vpaddq		$T0,$D1,$D1		
	vpmuludq	$H2,$T4,$T4		
	vpaddq		$T4,$D0,$D0		
	vpmuludq	$H0,$T2,$T2		
	vpaddq		$T2,$D4,$D4		
	vpmuludq	$H4,$T3,$T1		
	vpaddq		$T1,$D3,$D3		
	vpmuludq	$H3,$T3,$T0		
	vpaddq		$T0,$D2,$D2		
	vpmuludq	$H2,$T3,$T1		
	vpaddq		$T1,$D1,$D1		
	vpmuludq	$H1,$T3,$T3		
	vpaddq		$T3,$D0,$D0		
.Lshort_tail_avx:
	vpsrldq		\$8,$D4,$T4
	vpsrldq		\$8,$D3,$T3
	vpsrldq		\$8,$D1,$T1
	vpsrldq		\$8,$D0,$T0
	vpsrldq		\$8,$D2,$T2
	vpaddq		$T3,$D3,$D3
	vpaddq		$T4,$D4,$D4
	vpaddq		$T0,$D0,$D0
	vpaddq		$T1,$D1,$D1
	vpaddq		$T2,$D2,$D2
	vpsrlq		\$26,$D3,$H3
	vpand		$MASK,$D3,$D3
	vpaddq		$H3,$D4,$D4		
	vpsrlq		\$26,$D0,$H0
	vpand		$MASK,$D0,$D0
	vpaddq		$H0,$D1,$D1		
	vpsrlq		\$26,$D4,$H4
	vpand		$MASK,$D4,$D4
	vpsrlq		\$26,$D1,$H1
	vpand		$MASK,$D1,$D1
	vpaddq		$H1,$D2,$D2		
	vpaddq		$H4,$D0,$D0
	vpsllq		\$2,$H4,$H4
	vpaddq		$H4,$D0,$D0		
	vpsrlq		\$26,$D2,$H2
	vpand		$MASK,$D2,$D2
	vpaddq		$H2,$D3,$D3		
	vpsrlq		\$26,$D0,$H0
	vpand		$MASK,$D0,$D0
	vpaddq		$H0,$D1,$D1		
	vpsrlq		\$26,$D3,$H3
	vpand		$MASK,$D3,$D3
	vpaddq		$H3,$D4,$D4		
	vmovd		$D0,`4*0-48-64`($ctx)	
	vmovd		$D1,`4*1-48-64`($ctx)
	vmovd		$D2,`4*2-48-64`($ctx)
	vmovd		$D3,`4*3-48-64`($ctx)
	vmovd		$D4,`4*4-48-64`($ctx)
___
$code.=<<___	if ($win64);
	vmovdqa		0x50(%r11),%xmm6
	vmovdqa		0x60(%r11),%xmm7
	vmovdqa		0x70(%r11),%xmm8
	vmovdqa		0x80(%r11),%xmm9
	vmovdqa		0x90(%r11),%xmm10
	vmovdqa		0xa0(%r11),%xmm11
	vmovdqa		0xb0(%r11),%xmm12
	vmovdqa		0xc0(%r11),%xmm13
	vmovdqa		0xd0(%r11),%xmm14
	vmovdqa		0xe0(%r11),%xmm15
	lea		0xf8(%r11),%rsp
.Ldo_avx_epilogue:
___
$code.=<<___	if (!$win64);
	lea		-8(%r10),%rsp
.cfi_def_cfa_register	%rsp
___
$code.=<<___;
	vzeroupper
	RET
.cfi_endproc
___
&end_function("poly1305_blocks_avx");
&declare_function("poly1305_emit_avx", 32, 3);
$code.=<<___;
	cmpl	\$0,20($ctx)	
	je	.Lemit
	mov	0($ctx),%eax	
	mov	4($ctx),%ecx
	mov	8($ctx),%r8d
	mov	12($ctx),%r11d
	mov	16($ctx),%r10d
	shl	\$26,%rcx	
	mov	%r8,%r9
	shl	\$52,%r8
	add	%rcx,%rax
	shr	\$12,%r9
	add	%rax,%r8	
	adc	\$0,%r9
	shl	\$14,%r11
	mov	%r10,%rax
	shr	\$24,%r10
	add	%r11,%r9
	shl	\$40,%rax
	add	%rax,%r9	
	adc	\$0,%r10	
	mov	%r10,%rax	
	mov	%r10,%rcx
	and	\$3,%r10
	shr	\$2,%rax
	and	\$-4,%rcx
	add	%rcx,%rax
	add	%rax,%r8
	adc	\$0,%r9
	adc	\$0,%r10
	mov	%r8,%rax
	add	\$5,%r8		
	mov	%r9,%rcx
	adc	\$0,%r9
	adc	\$0,%r10
	shr	\$2,%r10	
	cmovnz	%r8,%rax
	cmovnz	%r9,%rcx
	add	0($nonce),%rax	
	adc	8($nonce),%rcx
	mov	%rax,0($mac)	
	mov	%rcx,8($mac)
	RET
___
&end_function("poly1305_emit_avx");
if ($avx>1) {
my ($H0,$H1,$H2,$H3,$H4, $MASK, $T4,$T0,$T1,$T2,$T3, $D0,$D1,$D2,$D3,$D4) =
    map("%ymm$_",(0..15));
my $S4=$MASK;
sub poly1305_blocks_avxN {
	my ($avx512) = @_;
	my $suffix = $avx512 ? "_avx512" : "";
$code.=<<___;
.cfi_startproc
	mov	20($ctx),%r8d		
	cmp	\$128,$len
	jae	.Lblocks_avx2$suffix
	test	%r8d,%r8d
	jz	.Lblocks
.Lblocks_avx2$suffix:
	and	\$-16,$len
	jz	.Lno_data_avx2$suffix
	vzeroupper
	test	%r8d,%r8d
	jz	.Lbase2_64_avx2$suffix
	test	\$63,$len
	jz	.Leven_avx2$suffix
	push	%rbp
.cfi_push	%rbp
	mov 	%rsp,%rbp
	push	%rbx
.cfi_push	%rbx
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
.Lblocks_avx2_body$suffix:
	mov	$len,%r15		
	mov	0($ctx),$d1		
	mov	8($ctx),$d2
	mov	16($ctx),$h2
	mov	24($ctx),$r0		
	mov	32($ctx),$s1
	mov	$d1
	and	\$`-1*(1<<31)`,$d1
	mov	$d2,$r1			
	mov	$d2
	and	\$`-1*(1<<31)`,$d2
	shr	\$6,$d1
	shl	\$52,$r1
	add	$d1,$h0
	shr	\$12,$h1
	shr	\$18,$d2
	add	$r1,$h0
	adc	$d2,$h1
	mov	$h2,$d1
	shl	\$40,$d1
	shr	\$24,$h2
	add	$d1,$h1
	adc	\$0,$h2			
	mov	\$-4,$d2		
	mov	$h2,$d1
	and	$h2,$d2
	shr	\$2,$d1
	and	\$3,$h2
	add	$d2,$d1			
	add	$d1,$h0
	adc	\$0,$h1
	adc	\$0,$h2
	mov	$s1,$r1
	mov	$s1,%rax
	shr	\$2,$s1
	add	$r1,$s1			
.Lbase2_26_pre_avx2$suffix:
	add	0($inp),$h0		
	adc	8($inp),$h1
	lea	16($inp),$inp
	adc	$padbit,$h2
	sub	\$16,%r15
	call	__poly1305_block
	mov	$r1,%rax
	test	\$63,%r15
	jnz	.Lbase2_26_pre_avx2$suffix
	test	$padbit,$padbit		
	jz	.Lstore_base2_64_avx2$suffix	
	mov	$h0,%rax
	mov	$h0,%rdx
	shr	\$52,$h0
	mov	$h1,$r0
	mov	$h1,$r1
	shr	\$26,%rdx
	and	\$0x3ffffff,%rax	
	shl	\$12,$r0
	and	\$0x3ffffff,%rdx	
	shr	\$14,$h1
	or	$r0,$h0
	shl	\$24,$h2
	and	\$0x3ffffff,$h0		
	shr	\$40,$r1
	and	\$0x3ffffff,$h1		
	or	$r1,$h2			
	test	%r15,%r15
	jz	.Lstore_base2_26_avx2$suffix
	vmovd	%rax
	vmovd	%rdx
	vmovd	$h0
	vmovd	$h1
	vmovd	$h2
	jmp	.Lproceed_avx2$suffix
.align	32
.Lstore_base2_64_avx2$suffix:
	mov	$h0,0($ctx)
	mov	$h1,8($ctx)
	mov	$h2,16($ctx)		
	jmp	.Ldone_avx2$suffix
.align	16
.Lstore_base2_26_avx2$suffix:
	mov	%rax
	mov	%rdx
	mov	$h0
	mov	$h1
	mov	$h2
.align	16
.Ldone_avx2$suffix:
	pop 		%r15
.cfi_restore	%r15
	pop 		%r14
.cfi_restore	%r14
	pop 		%r13
.cfi_restore	%r13
	pop 		%r12
.cfi_restore	%r12
	pop 		%rbx
.cfi_restore	%rbx
	pop 		%rbp
.cfi_restore 	%rbp
.Lno_data_avx2$suffix:
.Lblocks_avx2_epilogue$suffix:
	RET
.cfi_endproc
.align	32
.Lbase2_64_avx2$suffix:
.cfi_startproc
	push	%rbp
.cfi_push	%rbp
	mov 	%rsp,%rbp
	push	%rbx
.cfi_push	%rbx
	push	%r12
.cfi_push	%r12
	push	%r13
.cfi_push	%r13
	push	%r14
.cfi_push	%r14
	push	%r15
.cfi_push	%r15
.Lbase2_64_avx2_body$suffix:
	mov	$len,%r15		
	mov	24($ctx),$r0		
	mov	32($ctx),$s1
	mov	0($ctx),$h0		
	mov	8($ctx),$h1
	mov	16($ctx),$h2
	mov	$s1,$r1
	mov	$s1,%rax
	shr	\$2,$s1
	add	$r1,$s1			
	test	\$63,$len
	jz	.Linit_avx2$suffix
.Lbase2_64_pre_avx2$suffix:
	add	0($inp),$h0		
	adc	8($inp),$h1
	lea	16($inp),$inp
	adc	$padbit,$h2
	sub	\$16,%r15
	call	__poly1305_block
	mov	$r1,%rax
	test	\$63,%r15
	jnz	.Lbase2_64_pre_avx2$suffix
.Linit_avx2$suffix:
	mov	$h0,%rax
	mov	$h0,%rdx
	shr	\$52,$h0
	mov	$h1,$d1
	mov	$h1,$d2
	shr	\$26,%rdx
	and	\$0x3ffffff,%rax	
	shl	\$12,$d1
	and	\$0x3ffffff,%rdx	
	shr	\$14,$h1
	or	$d1,$h0
	shl	\$24,$h2
	and	\$0x3ffffff,$h0		
	shr	\$40,$d2
	and	\$0x3ffffff,$h1		
	or	$d2,$h2			
	vmovd	%rax
	vmovd	%rdx
	vmovd	$h0
	vmovd	$h1
	vmovd	$h2
	movl	\$1,20($ctx)		
	call	__poly1305_init_avx
.Lproceed_avx2$suffix:
	mov	%r15,$len			
___
$code.=<<___ if (!$kernel);
	mov	OPENSSL_ia32cap_P+8(%rip),%r9d
	mov	\$`(1<<31|1<<30|1<<16)`,%r11d
___
$code.=<<___;
	pop 		%r15
.cfi_restore	%r15
	pop 		%r14
.cfi_restore	%r14
	pop 		%r13
.cfi_restore	%r13
	pop 		%r12
.cfi_restore	%r12
	pop 		%rbx
.cfi_restore	%rbx
	pop 		%rbp
.cfi_restore 	%rbp
.Lbase2_64_avx2_epilogue$suffix:
	jmp	.Ldo_avx2$suffix
.cfi_endproc
.align	32
.Leven_avx2$suffix:
.cfi_startproc
___
$code.=<<___ if (!$kernel);
	mov		OPENSSL_ia32cap_P+8(%rip),%r9d
___
$code.=<<___;
	vmovd		4*0($ctx),%x
	vmovd		4*1($ctx),%x
	vmovd		4*2($ctx),%x
	vmovd		4*3($ctx),%x
	vmovd		4*4($ctx),%x
.Ldo_avx2$suffix:
___
$code.=<<___		if (!$kernel && $avx>2);
	cmp		\$512,$len
	jb		.Lskip_avx512
	and		%r11d,%r9d
	test		\$`1<<16`,%r9d		
	jnz		.Lblocks_avx512
.Lskip_avx512$suffix:
___
$code.=<<___ if ($avx > 2 && $avx512 && $kernel);
	cmp		\$512,$len
	jae		.Lblocks_avx512
___
$code.=<<___	if (!$win64);
	lea		8(%rsp),%r10
.cfi_def_cfa_register	%r10
	sub		\$0x128,%rsp
___
$code.=<<___	if ($win64);
	lea		8(%rsp),%r10
	sub		\$0x1c8,%rsp
	vmovdqa		%xmm6,-0xb0(%r10)
	vmovdqa		%xmm7,-0xa0(%r10)
	vmovdqa		%xmm8,-0x90(%r10)
	vmovdqa		%xmm9,-0x80(%r10)
	vmovdqa		%xmm10,-0x70(%r10)
	vmovdqa		%xmm11,-0x60(%r10)
	vmovdqa		%xmm12,-0x50(%r10)
	vmovdqa		%xmm13,-0x40(%r10)
	vmovdqa		%xmm14,-0x30(%r10)
	vmovdqa		%xmm15,-0x20(%r10)
.Ldo_avx2_body$suffix:
___
$code.=<<___;
	lea		.Lconst(%rip),%rcx
	lea		48+64($ctx),$ctx	
	vmovdqa		96(%rcx),$T0		
	vmovdqu		`16*0-64`($ctx),%x
	and		\$-512,%rsp
	vmovdqu		`16*1-64`($ctx),%x
	vmovdqu		`16*2-64`($ctx),%x
	vmovdqu		`16*3-64`($ctx),%x
	vmovdqu		`16*4-64`($ctx),%x
	vmovdqu		`16*5-64`($ctx),%x
	lea		0x90(%rsp),%rax		
	vmovdqu		`16*6-64`($ctx),%x
	vpermd		$T2,$T0,$T2		
	vmovdqu		`16*7-64`($ctx),%x
	vpermd		$T3,$T0,$T3
	vmovdqu		`16*8-64`($ctx),%x
	vpermd		$T4,$T0,$T4
	vmovdqa		$T2,0x00(%rsp)
	vpermd		$D0,$T0,$D0
	vmovdqa		$T3,0x20-0x90(%rax)
	vpermd		$D1,$T0,$D1
	vmovdqa		$T4,0x40-0x90(%rax)
	vpermd		$D2,$T0,$D2
	vmovdqa		$D0,0x60-0x90(%rax)
	vpermd		$D3,$T0,$D3
	vmovdqa		$D1,0x80-0x90(%rax)
	vpermd		$D4,$T0,$D4
	vmovdqa		$D2,0xa0-0x90(%rax)
	vpermd		$MASK,$T0,$MASK
	vmovdqa		$D3,0xc0-0x90(%rax)
	vmovdqa		$D4,0xe0-0x90(%rax)
	vmovdqa		$MASK,0x100-0x90(%rax)
	vmovdqa		64(%rcx),$MASK		
	vmovdqu		16*0($inp),%x
	vmovdqu		16*1($inp),%x
	vinserti128	\$1,16*2($inp),$T0,$T0
	vinserti128	\$1,16*3($inp),$T1,$T1
	lea		16*4($inp),$inp
	vpsrldq		\$6,$T0,$T2		
	vpsrldq		\$6,$T1,$T3
	vpunpckhqdq	$T1,$T0,$T4		
	vpunpcklqdq	$T3,$T2,$T2		
	vpunpcklqdq	$T1,$T0,$T0		
	vpsrlq		\$30,$T2,$T3
	vpsrlq		\$4,$T2,$T2
	vpsrlq		\$26,$T0,$T1
	vpsrlq		\$40,$T4,$T4		
	vpand		$MASK,$T2,$T2		
	vpand		$MASK,$T0,$T0		
	vpand		$MASK,$T1,$T1		
	vpand		$MASK,$T3,$T3		
	vpor		32(%rcx),$T4,$T4	
	vpaddq		$H2,$T2,$H2		
	sub		\$64,$len
	jz		.Ltail_avx2$suffix
	jmp		.Loop_avx2$suffix
.align	32
.Loop_avx2$suffix:
	vpaddq		$H0,$T0,$H0
	vmovdqa		`32*0`(%rsp),$T0	
	vpaddq		$H1,$T1,$H1
	vmovdqa		`32*1`(%rsp),$T1	
	vpaddq		$H3,$T3,$H3
	vmovdqa		`32*3`(%rsp),$T2	
	vpaddq		$H4,$T4,$H4
	vmovdqa		`32*6-0x90`(%rax),$T3	
	vmovdqa		`32*8-0x90`(%rax),$S4	
	vpmuludq	$H2,$T0,$D2		
	vpmuludq	$H2,$T1,$D3		
	vpmuludq	$H2,$T2,$D4		
	vpmuludq	$H2,$T3,$D0		
	vpmuludq	$H2,$S4,$D1		
	vpmuludq	$H0,$T1,$T4		
	vpmuludq	$H1,$T1,$H2		
	vpaddq		$T4,$D1,$D1		
	vpaddq		$H2,$D2,$D2		
	vpmuludq	$H3,$T1,$T4		
	vpmuludq	`32*2`(%rsp),$H4,$H2	
	vpaddq		$T4,$D4,$D4		
	vpaddq		$H2,$D0,$D0		
	 vmovdqa	`32*4-0x90`(%rax),$T1	
	vpmuludq	$H0,$T0,$T4		
	vpmuludq	$H1,$T0,$H2		
	vpaddq		$T4,$D0,$D0		
	vpaddq		$H2,$D1,$D1		
	vpmuludq	$H3,$T0,$T4		
	vpmuludq	$H4,$T0,$H2		
	 vmovdqu	16*0($inp),%x
	vpaddq		$T4,$D3,$D3		
	vpaddq		$H2,$D4,$D4		
	 vinserti128	\$1,16*2($inp),$T0,$T0
	vpmuludq	$H3,$T1,$T4		
	vpmuludq	$H4,$T1,$H2		
	 vmovdqu	16*1($inp),%x
	vpaddq		$T4,$D0,$D0		
	vpaddq		$H2,$D1,$D1		
	 vmovdqa	`32*5-0x90`(%rax),$H2	
	vpmuludq	$H1,$T2,$T4		
	vpmuludq	$H0,$T2,$T2		
	vpaddq		$T4,$D3,$D3		
	vpaddq		$T2,$D2,$D2		
	 vinserti128	\$1,16*3($inp),$T1,$T1
	 lea		16*4($inp),$inp
	vpmuludq	$H1,$H2,$T4		
	vpmuludq	$H0,$H2,$H2		
	 vpsrldq	\$6,$T0,$T2		
	vpaddq		$T4,$D4,$D4		
	vpaddq		$H2,$D3,$D3		
	vpmuludq	$H3,$T3,$T4		
	vpmuludq	$H4,$T3,$H2		
	 vpsrldq	\$6,$T1,$T3
	vpaddq		$T4,$D1,$D1		
	vpaddq		$H2,$D2,$D2		
	 vpunpckhqdq	$T1,$T0,$T4		
	vpmuludq	$H3,$S4,$H3		
	vpmuludq	$H4,$S4,$H4		
	 vpunpcklqdq	$T1,$T0,$T0		
	vpaddq		$H3,$D2,$H2		
	vpaddq		$H4,$D3,$H3		
	 vpunpcklqdq	$T3,$T2,$T3		
	vpmuludq	`32*7-0x90`(%rax),$H0,$H4	
	vpmuludq	$H1,$S4,$H0		
	vmovdqa		64(%rcx),$MASK		
	vpaddq		$H4,$D4,$H4		
	vpaddq		$H0,$D0,$H0		
	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		
	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$D1,$H1		
	vpsrlq		\$26,$H4,$D4
	vpand		$MASK,$H4,$H4
	 vpsrlq		\$4,$T3,$T2
	vpsrlq		\$26,$H1,$D1
	vpand		$MASK,$H1,$H1
	vpaddq		$D1,$H2,$H2		
	vpaddq		$D4,$H0,$H0
	vpsllq		\$2,$D4,$D4
	vpaddq		$D4,$H0,$H0		
	 vpand		$MASK,$T2,$T2		
	 vpsrlq		\$26,$T0,$T1
	vpsrlq		\$26,$H2,$D2
	vpand		$MASK,$H2,$H2
	vpaddq		$D2,$H3,$H3		
	 vpaddq		$T2,$H2,$H2		
	 vpsrlq		\$30,$T3,$T3
	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$H1,$H1		
	 vpsrlq		\$40,$T4,$T4		
	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		
	 vpand		$MASK,$T0,$T0		
	 vpand		$MASK,$T1,$T1		
	 vpand		$MASK,$T3,$T3		
	 vpor		32(%rcx),$T4,$T4	
	sub		\$64,$len
	jnz		.Loop_avx2$suffix
	.byte		0x66,0x90
.Ltail_avx2$suffix:
	vpaddq		$H0,$T0,$H0
	vmovdqu		`32*0+4`(%rsp),$T0	
	vpaddq		$H1,$T1,$H1
	vmovdqu		`32*1+4`(%rsp),$T1	
	vpaddq		$H3,$T3,$H3
	vmovdqu		`32*3+4`(%rsp),$T2	
	vpaddq		$H4,$T4,$H4
	vmovdqu		`32*6+4-0x90`(%rax),$T3	
	vmovdqu		`32*8+4-0x90`(%rax),$S4	
	vpmuludq	$H2,$T0,$D2		
	vpmuludq	$H2,$T1,$D3		
	vpmuludq	$H2,$T2,$D4		
	vpmuludq	$H2,$T3,$D0		
	vpmuludq	$H2,$S4,$D1		
	vpmuludq	$H0,$T1,$T4		
	vpmuludq	$H1,$T1,$H2		
	vpaddq		$T4,$D1,$D1		
	vpaddq		$H2,$D2,$D2		
	vpmuludq	$H3,$T1,$T4		
	vpmuludq	`32*2+4`(%rsp),$H4,$H2	
	vpaddq		$T4,$D4,$D4		
	vpaddq		$H2,$D0,$D0		
	vpmuludq	$H0,$T0,$T4		
	vpmuludq	$H1,$T0,$H2		
	vpaddq		$T4,$D0,$D0		
	 vmovdqu	`32*4+4-0x90`(%rax),$T1	
	vpaddq		$H2,$D1,$D1		
	vpmuludq	$H3,$T0,$T4		
	vpmuludq	$H4,$T0,$H2		
	vpaddq		$T4,$D3,$D3		
	vpaddq		$H2,$D4,$D4		
	vpmuludq	$H3,$T1,$T4		
	vpmuludq	$H4,$T1,$H2		
	vpaddq		$T4,$D0,$D0		
	vpaddq		$H2,$D1,$D1		
	 vmovdqu	`32*5+4-0x90`(%rax),$H2	
	vpmuludq	$H1,$T2,$T4		
	vpmuludq	$H0,$T2,$T2		
	vpaddq		$T4,$D3,$D3		
	vpaddq		$T2,$D2,$D2		
	vpmuludq	$H1,$H2,$T4		
	vpmuludq	$H0,$H2,$H2		
	vpaddq		$T4,$D4,$D4		
	vpaddq		$H2,$D3,$D3		
	vpmuludq	$H3,$T3,$T4		
	vpmuludq	$H4,$T3,$H2		
	vpaddq		$T4,$D1,$D1		
	vpaddq		$H2,$D2,$D2		
	vpmuludq	$H3,$S4,$H3		
	vpmuludq	$H4,$S4,$H4		
	vpaddq		$H3,$D2,$H2		
	vpaddq		$H4,$D3,$H3		
	vpmuludq	`32*7+4-0x90`(%rax),$H0,$H4		
	vpmuludq	$H1,$S4,$H0		
	vmovdqa		64(%rcx),$MASK		
	vpaddq		$H4,$D4,$H4		
	vpaddq		$H0,$D0,$H0		
	vpsrldq		\$8,$D1,$T1
	vpsrldq		\$8,$H2,$T2
	vpsrldq		\$8,$H3,$T3
	vpsrldq		\$8,$H4,$T4
	vpsrldq		\$8,$H0,$T0
	vpaddq		$T1,$D1,$D1
	vpaddq		$T2,$H2,$H2
	vpaddq		$T3,$H3,$H3
	vpaddq		$T4,$H4,$H4
	vpaddq		$T0,$H0,$H0
	vpermq		\$0x2,$H3,$T3
	vpermq		\$0x2,$H4,$T4
	vpermq		\$0x2,$H0,$T0
	vpermq		\$0x2,$D1,$T1
	vpermq		\$0x2,$H2,$T2
	vpaddq		$T3,$H3,$H3
	vpaddq		$T4,$H4,$H4
	vpaddq		$T0,$H0,$H0
	vpaddq		$T1,$D1,$D1
	vpaddq		$T2,$H2,$H2
	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		
	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$D1,$H1		
	vpsrlq		\$26,$H4,$D4
	vpand		$MASK,$H4,$H4
	vpsrlq		\$26,$H1,$D1
	vpand		$MASK,$H1,$H1
	vpaddq		$D1,$H2,$H2		
	vpaddq		$D4,$H0,$H0
	vpsllq		\$2,$D4,$D4
	vpaddq		$D4,$H0,$H0		
	vpsrlq		\$26,$H2,$D2
	vpand		$MASK,$H2,$H2
	vpaddq		$D2,$H3,$H3		
	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	vpaddq		$D0,$H1,$H1		
	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		
	vmovd		%x
	vmovd		%x
	vmovd		%x
	vmovd		%x
	vmovd		%x
___
$code.=<<___	if ($win64);
	vmovdqa		-0xb0(%r10),%xmm6
	vmovdqa		-0xa0(%r10),%xmm7
	vmovdqa		-0x90(%r10),%xmm8
	vmovdqa		-0x80(%r10),%xmm9
	vmovdqa		-0x70(%r10),%xmm10
	vmovdqa		-0x60(%r10),%xmm11
	vmovdqa		-0x50(%r10),%xmm12
	vmovdqa		-0x40(%r10),%xmm13
	vmovdqa		-0x30(%r10),%xmm14
	vmovdqa		-0x20(%r10),%xmm15
	lea		-8(%r10),%rsp
.Ldo_avx2_epilogue$suffix:
___
$code.=<<___	if (!$win64);
	lea		-8(%r10),%rsp
.cfi_def_cfa_register	%rsp
___
$code.=<<___;
	vzeroupper
	RET
.cfi_endproc
___
if($avx > 2 && $avx512) {
my ($R0,$R1,$R2,$R3,$R4, $S1,$S2,$S3,$S4) = map("%zmm$_",(16..24));
my ($M0,$M1,$M2,$M3,$M4) = map("%zmm$_",(25..29));
my $PADBIT="%zmm30";
map(s/%y/%z/,($T4,$T0,$T1,$T2,$T3));		
map(s/%y/%z/,($D0,$D1,$D2,$D3,$D4));
map(s/%y/%z/,($H0,$H1,$H2,$H3,$H4));
map(s/%y/%z/,($MASK));
$code.=<<___;
.cfi_startproc
.Lblocks_avx512:
	mov		\$15,%eax
	kmovw		%eax,%k2
___
$code.=<<___	if (!$win64);
	lea		8(%rsp),%r10
.cfi_def_cfa_register	%r10
	sub		\$0x128,%rsp
___
$code.=<<___	if ($win64);
	lea		8(%rsp),%r10
	sub		\$0x1c8,%rsp
	vmovdqa		%xmm6,-0xb0(%r10)
	vmovdqa		%xmm7,-0xa0(%r10)
	vmovdqa		%xmm8,-0x90(%r10)
	vmovdqa		%xmm9,-0x80(%r10)
	vmovdqa		%xmm10,-0x70(%r10)
	vmovdqa		%xmm11,-0x60(%r10)
	vmovdqa		%xmm12,-0x50(%r10)
	vmovdqa		%xmm13,-0x40(%r10)
	vmovdqa		%xmm14,-0x30(%r10)
	vmovdqa		%xmm15,-0x20(%r10)
.Ldo_avx512_body:
___
$code.=<<___;
	lea		.Lconst(%rip),%rcx
	lea		48+64($ctx),$ctx	
	vmovdqa		96(%rcx),%y
	vmovdqu		`16*0-64`($ctx),%x
	and		\$-512,%rsp
	vmovdqu		`16*1-64`($ctx),%x
	mov		\$0x20,%rax
	vmovdqu		`16*2-64`($ctx),%x
	vmovdqu		`16*3-64`($ctx),%x
	vmovdqu		`16*4-64`($ctx),%x
	vmovdqu		`16*5-64`($ctx),%x
	vmovdqu		`16*6-64`($ctx),%x
	vmovdqu		`16*7-64`($ctx),%x
	vmovdqu		`16*8-64`($ctx),%x
	vpermd		$D0,$T2,$R0		
	vpbroadcastq	64(%rcx),$MASK		
	vpermd		$D1,$T2,$R1
	vpermd		$T0,$T2,$S1
	vpermd		$D2,$T2,$R2
	vmovdqa64	$R0,0x00(%rsp){%k2}	
	 vpsrlq		\$32,$R0,$T0		
	vpermd		$T1,$T2,$S2
	vmovdqu64	$R1,0x00(%rsp,%rax){%k2}
	 vpsrlq		\$32,$R1,$T1
	vpermd		$D3,$T2,$R3
	vmovdqa64	$S1,0x40(%rsp){%k2}
	vpermd		$T3,$T2,$S3
	vpermd		$D4,$T2,$R4
	vmovdqu64	$R2,0x40(%rsp,%rax){%k2}
	vpermd		$T4,$T2,$S4
	vmovdqa64	$S2,0x80(%rsp){%k2}
	vmovdqu64	$R3,0x80(%rsp,%rax){%k2}
	vmovdqa64	$S3,0xc0(%rsp){%k2}
	vmovdqu64	$R4,0xc0(%rsp,%rax){%k2}
	vmovdqa64	$S4,0x100(%rsp){%k2}
	vpmuludq	$T0,$R0,$D0		
	vpmuludq	$T0,$R1,$D1		
	vpmuludq	$T0,$R2,$D2		
	vpmuludq	$T0,$R3,$D3		
	vpmuludq	$T0,$R4,$D4		
	 vpsrlq		\$32,$R2,$T2
	vpmuludq	$T1,$S4,$M0
	vpmuludq	$T1,$R0,$M1
	vpmuludq	$T1,$R1,$M2
	vpmuludq	$T1,$R2,$M3
	vpmuludq	$T1,$R3,$M4
	 vpsrlq		\$32,$R3,$T3
	vpaddq		$M0,$D0,$D0		
	vpaddq		$M1,$D1,$D1		
	vpaddq		$M2,$D2,$D2		
	vpaddq		$M3,$D3,$D3		
	vpaddq		$M4,$D4,$D4		
	vpmuludq	$T2,$S3,$M0
	vpmuludq	$T2,$S4,$M1
	vpmuludq	$T2,$R1,$M3
	vpmuludq	$T2,$R2,$M4
	vpmuludq	$T2,$R0,$M2
	 vpsrlq		\$32,$R4,$T4
	vpaddq		$M0,$D0,$D0		
	vpaddq		$M1,$D1,$D1		
	vpaddq		$M3,$D3,$D3		
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M2,$D2,$D2		
	vpmuludq	$T3,$S2,$M0
	vpmuludq	$T3,$R0,$M3
	vpmuludq	$T3,$R1,$M4
	vpmuludq	$T3,$S3,$M1
	vpmuludq	$T3,$S4,$M2
	vpaddq		$M0,$D0,$D0		
	vpaddq		$M3,$D3,$D3		
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M1,$D1,$D1		
	vpaddq		$M2,$D2,$D2		
	vpmuludq	$T4,$S4,$M3
	vpmuludq	$T4,$R0,$M4
	vpmuludq	$T4,$S1,$M0
	vpmuludq	$T4,$S2,$M1
	vpmuludq	$T4,$S3,$M2
	vpaddq		$M3,$D3,$D3		
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M0,$D0,$D0		
	vpaddq		$M1,$D1,$D1		
	vpaddq		$M2,$D2,$D2		
	vmovdqu64	16*0($inp),%z
	vmovdqu64	16*4($inp),%z
	lea		16*8($inp),$inp
	vpsrlq		\$26,$D3,$M3
	vpandq		$MASK,$D3,$D3
	vpaddq		$M3,$D4,$D4		
	vpsrlq		\$26,$D0,$M0
	vpandq		$MASK,$D0,$D0
	vpaddq		$M0,$D1,$D1		
	vpsrlq		\$26,$D4,$M4
	vpandq		$MASK,$D4,$D4
	vpsrlq		\$26,$D1,$M1
	vpandq		$MASK,$D1,$D1
	vpaddq		$M1,$D2,$D2		
	vpaddq		$M4,$D0,$D0
	vpsllq		\$2,$M4,$M4
	vpaddq		$M4,$D0,$D0		
	vpsrlq		\$26,$D2,$M2
	vpandq		$MASK,$D2,$D2
	vpaddq		$M2,$D3,$D3		
	vpsrlq		\$26,$D0,$M0
	vpandq		$MASK,$D0,$D0
	vpaddq		$M0,$D1,$D1		
	vpsrlq		\$26,$D3,$M3
	vpandq		$MASK,$D3,$D3
	vpaddq		$M3,$D4,$D4		
	vpunpcklqdq	$T4,$T3,$T0	
	vpunpckhqdq	$T4,$T3,$T4
	vmovdqa32	128(%rcx),$M0		
	mov		\$0x7777,%eax
	kmovw		%eax,%k1
	vpermd		$R0,$M0,$R0		
	vpermd		$R1,$M0,$R1
	vpermd		$R2,$M0,$R2
	vpermd		$R3,$M0,$R3
	vpermd		$R4,$M0,$R4
	vpermd		$D0,$M0,${R0}{%k1}	
	vpermd		$D1,$M0,${R1}{%k1}
	vpermd		$D2,$M0,${R2}{%k1}
	vpermd		$D3,$M0,${R3}{%k1}
	vpermd		$D4,$M0,${R4}{%k1}
	vpslld		\$2,$R1,$S1		
	vpslld		\$2,$R2,$S2
	vpslld		\$2,$R3,$S3
	vpslld		\$2,$R4,$S4
	vpaddd		$R1,$S1,$S1
	vpaddd		$R2,$S2,$S2
	vpaddd		$R3,$S3,$S3
	vpaddd		$R4,$S4,$S4
	vpbroadcastq	32(%rcx),$PADBIT	
	vpsrlq		\$52,$T0,$T2		
	vpsllq		\$12,$T4,$T3
	vporq		$T3,$T2,$T2
	vpsrlq		\$26,$T0,$T1
	vpsrlq		\$14,$T4,$T3
	vpsrlq		\$40,$T4,$T4		
	vpandq		$MASK,$T2,$T2		
	vpandq		$MASK,$T0,$T0		
	vpaddq		$H2,$T2,$H2		
	sub		\$192,$len
	jbe		.Ltail_avx512
	jmp		.Loop_avx512
.align	32
.Loop_avx512:
	vpmuludq	$H2,$R1,$D3		
	 vpaddq		$H0,$T0,$H0
	vpmuludq	$H2,$R2,$D4		
	 vpandq		$MASK,$T1,$T1		
	vpmuludq	$H2,$S3,$D0		
	 vpandq		$MASK,$T3,$T3		
	vpmuludq	$H2,$S4,$D1		
	 vporq		$PADBIT,$T4,$T4		
	vpmuludq	$H2,$R0,$D2		
	 vpaddq		$H1,$T1,$H1		
	 vpaddq		$H3,$T3,$H3
	 vpaddq		$H4,$T4,$H4
	  vmovdqu64	16*0($inp),$T3		
	  vmovdqu64	16*4($inp),$T4
	  lea		16*8($inp),$inp
	vpmuludq	$H0,$R3,$M3
	vpmuludq	$H0,$R4,$M4
	vpmuludq	$H0,$R0,$M0
	vpmuludq	$H0,$R1,$M1
	vpaddq		$M3,$D3,$D3		
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M0,$D0,$D0		
	vpaddq		$M1,$D1,$D1		
	vpmuludq	$H1,$R2,$M3
	vpmuludq	$H1,$R3,$M4
	vpmuludq	$H1,$S4,$M0
	vpmuludq	$H0,$R2,$M2
	vpaddq		$M3,$D3,$D3		
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M0,$D0,$D0		
	vpaddq		$M2,$D2,$D2		
	  vpunpcklqdq	$T4,$T3,$T0		
	  vpunpckhqdq	$T4,$T3,$T4
	vpmuludq	$H3,$R0,$M3
	vpmuludq	$H3,$R1,$M4
	vpmuludq	$H1,$R0,$M1
	vpmuludq	$H1,$R1,$M2
	vpaddq		$M3,$D3,$D3		
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M1,$D1,$D1		
	vpaddq		$M2,$D2,$D2		
	vpmuludq	$H4,$S4,$M3
	vpmuludq	$H4,$R0,$M4
	vpmuludq	$H3,$S2,$M0
	vpmuludq	$H3,$S3,$M1
	vpaddq		$M3,$D3,$D3		
	vpmuludq	$H3,$S4,$M2
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M0,$D0,$D0		
	vpaddq		$M1,$D1,$D1		
	vpaddq		$M2,$D2,$D2		
	vpmuludq	$H4,$S1,$M0
	vpmuludq	$H4,$S2,$M1
	vpmuludq	$H4,$S3,$M2
	vpaddq		$M0,$D0,$H0		
	vpaddq		$M1,$D1,$H1		
	vpaddq		$M2,$D2,$H2		
	 vpsrlq		\$52,$T0,$T2		
	 vpsllq		\$12,$T4,$T3
	vpsrlq		\$26,$D3,$H3
	vpandq		$MASK,$D3,$D3
	vpaddq		$H3,$D4,$H4		
	 vporq		$T3,$T2,$T2
	vpsrlq		\$26,$H0,$D0
	vpandq		$MASK,$H0,$H0
	vpaddq		$D0,$H1,$H1		
	 vpandq		$MASK,$T2,$T2		
	vpsrlq		\$26,$H4,$D4
	vpandq		$MASK,$H4,$H4
	vpsrlq		\$26,$H1,$D1
	vpandq		$MASK,$H1,$H1
	vpaddq		$D1,$H2,$H2		
	vpaddq		$D4,$H0,$H0
	vpsllq		\$2,$D4,$D4
	vpaddq		$D4,$H0,$H0		
	 vpaddq		$T2,$H2,$H2		
	 vpsrlq		\$26,$T0,$T1
	vpsrlq		\$26,$H2,$D2
	vpandq		$MASK,$H2,$H2
	vpaddq		$D2,$D3,$H3		
	 vpsrlq		\$14,$T4,$T3
	vpsrlq		\$26,$H0,$D0
	vpandq		$MASK,$H0,$H0
	vpaddq		$D0,$H1,$H1		
	 vpsrlq		\$40,$T4,$T4		
	vpsrlq		\$26,$H3,$D3
	vpandq		$MASK,$H3,$H3
	vpaddq		$D3,$H4,$H4		
	 vpandq		$MASK,$T0,$T0		
	sub		\$128,$len
	ja		.Loop_avx512
.Ltail_avx512:
	vpsrlq		\$32,$R0,$R0		
	vpsrlq		\$32,$R1,$R1
	vpsrlq		\$32,$R2,$R2
	vpsrlq		\$32,$S3,$S3
	vpsrlq		\$32,$S4,$S4
	vpsrlq		\$32,$R3,$R3
	vpsrlq		\$32,$R4,$R4
	vpsrlq		\$32,$S1,$S1
	vpsrlq		\$32,$S2,$S2
	lea		($inp,$len),$inp
	vpaddq		$H0,$T0,$H0
	vpmuludq	$H2,$R1,$D3		
	vpmuludq	$H2,$R2,$D4		
	vpmuludq	$H2,$S3,$D0		
	 vpandq		$MASK,$T1,$T1		
	vpmuludq	$H2,$S4,$D1		
	 vpandq		$MASK,$T3,$T3		
	vpmuludq	$H2,$R0,$D2		
	 vporq		$PADBIT,$T4,$T4		
	 vpaddq		$H1,$T1,$H1		
	 vpaddq		$H3,$T3,$H3
	 vpaddq		$H4,$T4,$H4
	  vmovdqu	16*0($inp),%x
	vpmuludq	$H0,$R3,$M3
	vpmuludq	$H0,$R4,$M4
	vpmuludq	$H0,$R0,$M0
	vpmuludq	$H0,$R1,$M1
	vpaddq		$M3,$D3,$D3		
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M0,$D0,$D0		
	vpaddq		$M1,$D1,$D1		
	  vmovdqu	16*1($inp),%x
	vpmuludq	$H1,$R2,$M3
	vpmuludq	$H1,$R3,$M4
	vpmuludq	$H1,$S4,$M0
	vpmuludq	$H0,$R2,$M2
	vpaddq		$M3,$D3,$D3		
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M0,$D0,$D0		
	vpaddq		$M2,$D2,$D2		
	  vinserti128	\$1,16*2($inp),%y
	vpmuludq	$H3,$R0,$M3
	vpmuludq	$H3,$R1,$M4
	vpmuludq	$H1,$R0,$M1
	vpmuludq	$H1,$R1,$M2
	vpaddq		$M3,$D3,$D3		
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M1,$D1,$D1		
	vpaddq		$M2,$D2,$D2		
	  vinserti128	\$1,16*3($inp),%y
	vpmuludq	$H4,$S4,$M3
	vpmuludq	$H4,$R0,$M4
	vpmuludq	$H3,$S2,$M0
	vpmuludq	$H3,$S3,$M1
	vpmuludq	$H3,$S4,$M2
	vpaddq		$M3,$D3,$H3		
	vpaddq		$M4,$D4,$D4		
	vpaddq		$M0,$D0,$D0		
	vpaddq		$M1,$D1,$D1		
	vpaddq		$M2,$D2,$D2		
	vpmuludq	$H4,$S1,$M0
	vpmuludq	$H4,$S2,$M1
	vpmuludq	$H4,$S3,$M2
	vpaddq		$M0,$D0,$H0		
	vpaddq		$M1,$D1,$H1		
	vpaddq		$M2,$D2,$H2		
	mov		\$1,%eax
	vpermq		\$0xb1,$H3,$D3
	vpermq		\$0xb1,$D4,$H4
	vpermq		\$0xb1,$H0,$D0
	vpermq		\$0xb1,$H1,$D1
	vpermq		\$0xb1,$H2,$D2
	vpaddq		$D3,$H3,$H3
	vpaddq		$D4,$H4,$H4
	vpaddq		$D0,$H0,$H0
	vpaddq		$D1,$H1,$H1
	vpaddq		$D2,$H2,$H2
	kmovw		%eax,%k3
	vpermq		\$0x2,$H3,$D3
	vpermq		\$0x2,$H4,$D4
	vpermq		\$0x2,$H0,$D0
	vpermq		\$0x2,$H1,$D1
	vpermq		\$0x2,$H2,$D2
	vpaddq		$D3,$H3,$H3
	vpaddq		$D4,$H4,$H4
	vpaddq		$D0,$H0,$H0
	vpaddq		$D1,$H1,$H1
	vpaddq		$D2,$H2,$H2
	vextracti64x4	\$0x1,$H3,%y
	vextracti64x4	\$0x1,$H4,%y
	vextracti64x4	\$0x1,$H0,%y
	vextracti64x4	\$0x1,$H1,%y
	vextracti64x4	\$0x1,$H2,%y
	vpaddq		$D3,$H3,${H3}{%k3}{z}	
	vpaddq		$D4,$H4,${H4}{%k3}{z}	
	vpaddq		$D0,$H0,${H0}{%k3}{z}
	vpaddq		$D1,$H1,${H1}{%k3}{z}
	vpaddq		$D2,$H2,${H2}{%k3}{z}
___
map(s/%z/%y/,($T0,$T1,$T2,$T3,$T4, $PADBIT));
map(s/%z/%y/,($H0,$H1,$H2,$H3,$H4, $D0,$D1,$D2,$D3,$D4, $MASK));
$code.=<<___;
	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	 vpsrldq	\$6,$T0,$T2		
	 vpsrldq	\$6,$T1,$T3
	 vpunpckhqdq	$T1,$T0,$T4		
	vpaddq		$D3,$H4,$H4		
	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	 vpunpcklqdq	$T3,$T2,$T2		
	 vpunpcklqdq	$T1,$T0,$T0		
	vpaddq		$D0,$H1,$H1		
	vpsrlq		\$26,$H4,$D4
	vpand		$MASK,$H4,$H4
	vpsrlq		\$26,$H1,$D1
	vpand		$MASK,$H1,$H1
	 vpsrlq		\$30,$T2,$T3
	 vpsrlq		\$4,$T2,$T2
	vpaddq		$D1,$H2,$H2		
	vpaddq		$D4,$H0,$H0
	vpsllq		\$2,$D4,$D4
	 vpsrlq		\$26,$T0,$T1
	 vpsrlq		\$40,$T4,$T4		
	vpaddq		$D4,$H0,$H0		
	vpsrlq		\$26,$H2,$D2
	vpand		$MASK,$H2,$H2
	 vpand		$MASK,$T2,$T2		
	 vpand		$MASK,$T0,$T0		
	vpaddq		$D2,$H3,$H3		
	vpsrlq		\$26,$H0,$D0
	vpand		$MASK,$H0,$H0
	 vpaddq		$H2,$T2,$H2		
	 vpand		$MASK,$T1,$T1		
	vpaddq		$D0,$H1,$H1		
	vpsrlq		\$26,$H3,$D3
	vpand		$MASK,$H3,$H3
	 vpand		$MASK,$T3,$T3		
	 vpor		32(%rcx),$T4,$T4	
	vpaddq		$D3,$H4,$H4		
	lea		0x90(%rsp),%rax		
	add		\$64,$len
	jnz		.Ltail_avx2$suffix
	vpsubq		$T2,$H2,$H2		
	vmovd		%x
	vmovd		%x
	vmovd		%x
	vmovd		%x
	vmovd		%x
	vzeroall
___
$code.=<<___	if ($win64);
	movdqa		-0xb0(%r10),%xmm6
	movdqa		-0xa0(%r10),%xmm7
	movdqa		-0x90(%r10),%xmm8
	movdqa		-0x80(%r10),%xmm9
	movdqa		-0x70(%r10),%xmm10
	movdqa		-0x60(%r10),%xmm11
	movdqa		-0x50(%r10),%xmm12
	movdqa		-0x40(%r10),%xmm13
	movdqa		-0x30(%r10),%xmm14
	movdqa		-0x20(%r10),%xmm15
	lea		-8(%r10),%rsp
.Ldo_avx512_epilogue:
___
$code.=<<___	if (!$win64);
	lea		-8(%r10),%rsp
.cfi_def_cfa_register	%rsp
___
$code.=<<___;
	RET
.cfi_endproc
___
}
}
&declare_function("poly1305_blocks_avx2", 32, 4);
poly1305_blocks_avxN(0);
&end_function("poly1305_blocks_avx2");
if ($avx>2) {
if($kernel) {
	$code .= "#ifdef CONFIG_AS_AVX512\n";
}
&declare_function("poly1305_blocks_avx512", 32, 4);
poly1305_blocks_avxN(1);
&end_function("poly1305_blocks_avx512");
if ($kernel) {
	$code .= "#endif\n";
}
if (!$kernel && $avx>3) {
$code.=<<___;
.type	poly1305_init_base2_44,\@function,3
.align	32
poly1305_init_base2_44:
	xor	%eax,%eax
	mov	%rax,0($ctx)		
	mov	%rax,8($ctx)
	mov	%rax,16($ctx)
.Linit_base2_44:
	lea	poly1305_blocks_vpmadd52(%rip),%r10
	lea	poly1305_emit_base2_44(%rip),%r11
	mov	\$0x0ffffffc0fffffff,%rax
	mov	\$0x0ffffffc0ffffffc,%rcx
	and	0($inp),%rax
	mov	\$0x00000fffffffffff,%r8
	and	8($inp),%rcx
	mov	\$0x00000fffffffffff,%r9
	and	%rax,%r8
	shrd	\$44,%rcx,%rax
	mov	%r8,40($ctx)		
	and	%r9,%rax
	shr	\$24,%rcx
	mov	%rax,48($ctx)		
	lea	(%rax,%rax,4),%rax	
	mov	%rcx,56($ctx)		
	shl	\$2,%rax		
	lea	(%rcx,%rcx,4),%rcx	
	shl	\$2,%rcx		
	mov	%rax,24($ctx)		
	mov	%rcx,32($ctx)		
	movq	\$-1,64($ctx)		
___
$code.=<<___	if ($flavour !~ /elf32/);
	mov	%r10,0(%rdx)
	mov	%r11,8(%rdx)
___
$code.=<<___	if ($flavour =~ /elf32/);
	mov	%r10d,0(%rdx)
	mov	%r11d,4(%rdx)
___
$code.=<<___;
	mov	\$1,%eax
	RET
.size	poly1305_init_base2_44,.-poly1305_init_base2_44
___
{
my ($H0,$H1,$H2,$r2r1r0,$r1r0s2,$r0s2s1,$Dlo,$Dhi) = map("%ymm$_",(0..5,16,17));
my ($T0,$inp_permd,$inp_shift,$PAD) = map("%ymm$_",(18..21));
my ($reduc_mask,$reduc_rght,$reduc_left) = map("%ymm$_",(22..25));
$code.=<<___;
.type	poly1305_blocks_vpmadd52,\@function,4
.align	32
poly1305_blocks_vpmadd52:
	shr	\$4,$len
	jz	.Lno_data_vpmadd52		
	shl	\$40,$padbit
	mov	64($ctx),%r8			
	mov	\$3,%rax
	mov	\$1,%r10
	cmp	\$4,$len			
	cmovae	%r10,%rax
	test	%r8,%r8				
	cmovns	%r10,%rax
	and	$len,%rax			
	jz	.Lblocks_vpmadd52_4x
	sub		%rax,$len
	mov		\$7,%r10d
	mov		\$1,%r11d
	kmovw		%r10d,%k7
	lea		.L2_44_inp_permd(%rip),%r10
	kmovw		%r11d,%k1
	vmovq		$padbit,%x
	vmovdqa64	0(%r10),$inp_permd	
	vmovdqa64	32(%r10),$inp_shift	
	vpermq		\$0xcf,$PAD,$PAD
	vmovdqa64	64(%r10),$reduc_mask	
	vmovdqu64	0($ctx),${Dlo}{%k7}{z}		
	vmovdqu64	40($ctx),${r2r1r0}{%k7}{z}	
	vmovdqu64	32($ctx),${r1r0s2}{%k7}{z}
	vmovdqu64	24($ctx),${r0s2s1}{%k7}{z}
	vmovdqa64	96(%r10),$reduc_rght	
	vmovdqa64	128(%r10),$reduc_left	
	jmp		.Loop_vpmadd52
.align	32
.Loop_vpmadd52:
	vmovdqu32	0($inp),%x
	lea		16($inp),$inp
	vpermd		$T0,$inp_permd,$T0	
	vpsrlvq		$inp_shift,$T0,$T0
	vpandq		$reduc_mask,$T0,$T0
	vporq		$PAD,$T0,$T0
	vpaddq		$T0,$Dlo,$Dlo		
	vpermq		\$0,$Dlo,${H0}{%k7}{z}	
	vpermq		\$0b01010101,$Dlo,${H1}{%k7}{z}
	vpermq		\$0b10101010,$Dlo,${H2}{%k7}{z}
	vpxord		$Dlo,$Dlo,$Dlo
	vpxord		$Dhi,$Dhi,$Dhi
	vpmadd52luq	$r2r1r0,$H0,$Dlo
	vpmadd52huq	$r2r1r0,$H0,$Dhi
	vpmadd52luq	$r1r0s2,$H1,$Dlo
	vpmadd52huq	$r1r0s2,$H1,$Dhi
	vpmadd52luq	$r0s2s1,$H2,$Dlo
	vpmadd52huq	$r0s2s1,$H2,$Dhi
	vpsrlvq		$reduc_rght,$Dlo,$T0	
	vpsllvq		$reduc_left,$Dhi,$Dhi	
	vpandq		$reduc_mask,$Dlo,$Dlo
	vpaddq		$T0,$Dhi,$Dhi
	vpermq		\$0b10010011,$Dhi,$Dhi	
	vpaddq		$Dhi,$Dlo,$Dlo		
	vpsrlvq		$reduc_rght,$Dlo,$T0	
	vpandq		$reduc_mask,$Dlo,$Dlo
	vpermq		\$0b10010011,$T0,$T0
	vpaddq		$T0,$Dlo,$Dlo
	vpermq		\$0b10010011,$Dlo,${T0}{%k1}{z}
	vpaddq		$T0,$Dlo,$Dlo
	vpsllq		\$2,$T0,$T0
	vpaddq		$T0,$Dlo,$Dlo
	dec		%rax			
	jnz		.Loop_vpmadd52
	vmovdqu64	$Dlo,0($ctx){%k7}	
	test		$len,$len
	jnz		.Lblocks_vpmadd52_4x
.Lno_data_vpmadd52:
	RET
.size	poly1305_blocks_vpmadd52,.-poly1305_blocks_vpmadd52
___
}
{
my ($H0,$H1,$H2,$R0,$R1,$R2,$S1,$S2) = map("%ymm$_",(0..5,16,17));
my ($D0lo,$D0hi,$D1lo,$D1hi,$D2lo,$D2hi) = map("%ymm$_",(18..23));
my ($T0,$T1,$T2,$T3,$mask44,$mask42,$tmp,$PAD) = map("%ymm$_",(24..31));
$code.=<<___;
.type	poly1305_blocks_vpmadd52_4x,\@function,4
.align	32
poly1305_blocks_vpmadd52_4x:
	shr	\$4,$len
	jz	.Lno_data_vpmadd52_4x		
	shl	\$40,$padbit
	mov	64($ctx),%r8			
.Lblocks_vpmadd52_4x:
	vpbroadcastq	$padbit,$PAD
	vmovdqa64	.Lx_mask44(%rip),$mask44
	mov		\$5,%eax
	vmovdqa64	.Lx_mask42(%rip),$mask42
	kmovw		%eax,%k1		
	test		%r8,%r8			
	js		.Linit_vpmadd52		
	vmovq		0($ctx),%x
	vmovq		8($ctx),%x
	vmovq		16($ctx),%x
	test		\$3,$len		
	jnz		.Lblocks_vpmadd52_2x_do
.Lblocks_vpmadd52_4x_do:
	vpbroadcastq	64($ctx),$R0		
	vpbroadcastq	96($ctx),$R1
	vpbroadcastq	128($ctx),$R2
	vpbroadcastq	160($ctx),$S1
.Lblocks_vpmadd52_4x_key_loaded:
	vpsllq		\$2,$R2,$S2		
	vpaddq		$R2,$S2,$S2
	vpsllq		\$2,$S2,$S2
	test		\$7,$len		
	jz		.Lblocks_vpmadd52_8x
	vmovdqu64	16*0($inp),$T2		
	vmovdqu64	16*2($inp),$T3
	lea		16*4($inp),$inp
	vpunpcklqdq	$T3,$T2,$T1		
	vpunpckhqdq	$T3,$T2,$T3
	vpsrlq		\$24,$T3,$T2		
	vporq		$PAD,$T2,$T2
	 vpaddq		$T2,$H2,$H2		
	vpandq		$mask44,$T1,$T0
	vpsrlq		\$44,$T1,$T1
	vpsllq		\$20,$T3,$T3
	vporq		$T3,$T1,$T1
	vpandq		$mask44,$T1,$T1
	sub		\$4,$len
	jz		.Ltail_vpmadd52_4x
	jmp		.Loop_vpmadd52_4x
	ud2
.align	32
.Linit_vpmadd52:
	vmovq		24($ctx),%x
	vmovq		56($ctx),%x
	vmovq		32($ctx),%x
	vmovq		40($ctx),%x
	vmovq		48($ctx),%x
	vmovdqa		$R0,$H0
	vmovdqa		$R1,$H1
	vmovdqa		$H2,$R2
	mov		\$2,%eax
.Lmul_init_vpmadd52:
	vpxorq		$D0lo,$D0lo,$D0lo
	vpmadd52luq	$H2,$S1,$D0lo
	vpxorq		$D0hi,$D0hi,$D0hi
	vpmadd52huq	$H2,$S1,$D0hi
	vpxorq		$D1lo,$D1lo,$D1lo
	vpmadd52luq	$H2,$S2,$D1lo
	vpxorq		$D1hi,$D1hi,$D1hi
	vpmadd52huq	$H2,$S2,$D1hi
	vpxorq		$D2lo,$D2lo,$D2lo
	vpmadd52luq	$H2,$R0,$D2lo
	vpxorq		$D2hi,$D2hi,$D2hi
	vpmadd52huq	$H2,$R0,$D2hi
	vpmadd52luq	$H0,$R0,$D0lo
	vpmadd52huq	$H0,$R0,$D0hi
	vpmadd52luq	$H0,$R1,$D1lo
	vpmadd52huq	$H0,$R1,$D1hi
	vpmadd52luq	$H0,$R2,$D2lo
	vpmadd52huq	$H0,$R2,$D2hi
	vpmadd52luq	$H1,$S2,$D0lo
	vpmadd52huq	$H1,$S2,$D0hi
	vpmadd52luq	$H1,$R0,$D1lo
	vpmadd52huq	$H1,$R0,$D1hi
	vpmadd52luq	$H1,$R1,$D2lo
	vpmadd52huq	$H1,$R1,$D2hi
	vpsrlq		\$44,$D0lo,$tmp
	vpsllq		\$8,$D0hi,$D0hi
	vpandq		$mask44,$D0lo,$H0
	vpaddq		$tmp,$D0hi,$D0hi
	vpaddq		$D0hi,$D1lo,$D1lo
	vpsrlq		\$44,$D1lo,$tmp
	vpsllq		\$8,$D1hi,$D1hi
	vpandq		$mask44,$D1lo,$H1
	vpaddq		$tmp,$D1hi,$D1hi
	vpaddq		$D1hi,$D2lo,$D2lo
	vpsrlq		\$42,$D2lo,$tmp
	vpsllq		\$10,$D2hi,$D2hi
	vpandq		$mask42,$D2lo,$H2
	vpaddq		$tmp,$D2hi,$D2hi
	vpaddq		$D2hi,$H0,$H0
	vpsllq		\$2,$D2hi,$D2hi
	vpaddq		$D2hi,$H0,$H0
	vpsrlq		\$44,$H0,$tmp		
	vpandq		$mask44,$H0,$H0
	vpaddq		$tmp,$H1,$H1
	dec		%eax
	jz		.Ldone_init_vpmadd52
	vpunpcklqdq	$R1,$H1,$R1		
	vpbroadcastq	%x
	vpunpcklqdq	$R2,$H2,$R2
	vpbroadcastq	%x
	vpunpcklqdq	$R0,$H0,$R0
	vpbroadcastq	%x
	vpsllq		\$2,$R1,$S1		
	vpsllq		\$2,$R2,$S2		
	vpaddq		$R1,$S1,$S1
	vpaddq		$R2,$S2,$S2
	vpsllq		\$2,$S1,$S1
	vpsllq		\$2,$S2,$S2
	jmp		.Lmul_init_vpmadd52
	ud2
.align	32
.Ldone_init_vpmadd52:
	vinserti128	\$1,%x
	vinserti128	\$1,%x
	vinserti128	\$1,%x
	vpermq		\$0b11011000,$R1,$R1	
	vpermq		\$0b11011000,$R2,$R2
	vpermq		\$0b11011000,$R0,$R0
	vpsllq		\$2,$R1,$S1		
	vpaddq		$R1,$S1,$S1
	vpsllq		\$2,$S1,$S1
	vmovq		0($ctx),%x
	vmovq		8($ctx),%x
	vmovq		16($ctx),%x
	test		\$3,$len		
	jnz		.Ldone_init_vpmadd52_2x
	vmovdqu64	$R0,64($ctx)		
	vpbroadcastq	%x
	vmovdqu64	$R1,96($ctx)
	vpbroadcastq	%x
	vmovdqu64	$R2,128($ctx)
	vpbroadcastq	%x
	vmovdqu64	$S1,160($ctx)
	vpbroadcastq	%x
	jmp		.Lblocks_vpmadd52_4x_key_loaded
	ud2
.align	32
.Ldone_init_vpmadd52_2x:
	vmovdqu64	$R0,64($ctx)		
	vpsrldq		\$8,$R0,$R0		
	vmovdqu64	$R1,96($ctx)
	vpsrldq		\$8,$R1,$R1
	vmovdqu64	$R2,128($ctx)
	vpsrldq		\$8,$R2,$R2
	vmovdqu64	$S1,160($ctx)
	vpsrldq		\$8,$S1,$S1
	jmp		.Lblocks_vpmadd52_2x_key_loaded
	ud2
.align	32
.Lblocks_vpmadd52_2x_do:
	vmovdqu64	128+8($ctx),${R2}{%k1}{z}
	vmovdqu64	160+8($ctx),${S1}{%k1}{z}
	vmovdqu64	64+8($ctx),${R0}{%k1}{z}
	vmovdqu64	96+8($ctx),${R1}{%k1}{z}
.Lblocks_vpmadd52_2x_key_loaded:
	vmovdqu64	16*0($inp),$T2		
	vpxorq		$T3,$T3,$T3
	lea		16*2($inp),$inp
	vpunpcklqdq	$T3,$T2,$T1		
	vpunpckhqdq	$T3,$T2,$T3
	vpsrlq		\$24,$T3,$T2		
	vporq		$PAD,$T2,$T2
	 vpaddq		$T2,$H2,$H2		
	vpandq		$mask44,$T1,$T0
	vpsrlq		\$44,$T1,$T1
	vpsllq		\$20,$T3,$T3
	vporq		$T3,$T1,$T1
	vpandq		$mask44,$T1,$T1
	jmp		.Ltail_vpmadd52_2x
	ud2
.align	32
.Loop_vpmadd52_4x:
	vpaddq		$T0,$H0,$H0
	vpaddq		$T1,$H1,$H1
	vpxorq		$D0lo,$D0lo,$D0lo
	vpmadd52luq	$H2,$S1,$D0lo
	vpxorq		$D0hi,$D0hi,$D0hi
	vpmadd52huq	$H2,$S1,$D0hi
	vpxorq		$D1lo,$D1lo,$D1lo
	vpmadd52luq	$H2,$S2,$D1lo
	vpxorq		$D1hi,$D1hi,$D1hi
	vpmadd52huq	$H2,$S2,$D1hi
	vpxorq		$D2lo,$D2lo,$D2lo
	vpmadd52luq	$H2,$R0,$D2lo
	vpxorq		$D2hi,$D2hi,$D2hi
	vpmadd52huq	$H2,$R0,$D2hi
	 vmovdqu64	16*0($inp),$T2		
	 vmovdqu64	16*2($inp),$T3
	 lea		16*4($inp),$inp
	vpmadd52luq	$H0,$R0,$D0lo
	vpmadd52huq	$H0,$R0,$D0hi
	vpmadd52luq	$H0,$R1,$D1lo
	vpmadd52huq	$H0,$R1,$D1hi
	vpmadd52luq	$H0,$R2,$D2lo
	vpmadd52huq	$H0,$R2,$D2hi
	 vpunpcklqdq	$T3,$T2,$T1		
	 vpunpckhqdq	$T3,$T2,$T3
	vpmadd52luq	$H1,$S2,$D0lo
	vpmadd52huq	$H1,$S2,$D0hi
	vpmadd52luq	$H1,$R0,$D1lo
	vpmadd52huq	$H1,$R0,$D1hi
	vpmadd52luq	$H1,$R1,$D2lo
	vpmadd52huq	$H1,$R1,$D2hi
	vpsrlq		\$44,$D0lo,$tmp
	vpsllq		\$8,$D0hi,$D0hi
	vpandq		$mask44,$D0lo,$H0
	vpaddq		$tmp,$D0hi,$D0hi
	 vpsrlq		\$24,$T3,$T2
	 vporq		$PAD,$T2,$T2
	vpaddq		$D0hi,$D1lo,$D1lo
	vpsrlq		\$44,$D1lo,$tmp
	vpsllq		\$8,$D1hi,$D1hi
	vpandq		$mask44,$D1lo,$H1
	vpaddq		$tmp,$D1hi,$D1hi
	 vpandq		$mask44,$T1,$T0
	 vpsrlq		\$44,$T1,$T1
	 vpsllq		\$20,$T3,$T3
	vpaddq		$D1hi,$D2lo,$D2lo
	vpsrlq		\$42,$D2lo,$tmp
	vpsllq		\$10,$D2hi,$D2hi
	vpandq		$mask42,$D2lo,$H2
	vpaddq		$tmp,$D2hi,$D2hi
	  vpaddq	$T2,$H2,$H2		
	vpaddq		$D2hi,$H0,$H0
	vpsllq		\$2,$D2hi,$D2hi
	vpaddq		$D2hi,$H0,$H0
	 vporq		$T3,$T1,$T1
	 vpandq		$mask44,$T1,$T1
	vpsrlq		\$44,$H0,$tmp		
	vpandq		$mask44,$H0,$H0
	vpaddq		$tmp,$H1,$H1
	sub		\$4,$len		
	jnz		.Loop_vpmadd52_4x
.Ltail_vpmadd52_4x:
	vmovdqu64	128($ctx),$R2		
	vmovdqu64	160($ctx),$S1
	vmovdqu64	64($ctx),$R0
	vmovdqu64	96($ctx),$R1
.Ltail_vpmadd52_2x:
	vpsllq		\$2,$R2,$S2		
	vpaddq		$R2,$S2,$S2
	vpsllq		\$2,$S2,$S2
	vpaddq		$T0,$H0,$H0
	vpaddq		$T1,$H1,$H1
	vpxorq		$D0lo,$D0lo,$D0lo
	vpmadd52luq	$H2,$S1,$D0lo
	vpxorq		$D0hi,$D0hi,$D0hi
	vpmadd52huq	$H2,$S1,$D0hi
	vpxorq		$D1lo,$D1lo,$D1lo
	vpmadd52luq	$H2,$S2,$D1lo
	vpxorq		$D1hi,$D1hi,$D1hi
	vpmadd52huq	$H2,$S2,$D1hi
	vpxorq		$D2lo,$D2lo,$D2lo
	vpmadd52luq	$H2,$R0,$D2lo
	vpxorq		$D2hi,$D2hi,$D2hi
	vpmadd52huq	$H2,$R0,$D2hi
	vpmadd52luq	$H0,$R0,$D0lo
	vpmadd52huq	$H0,$R0,$D0hi
	vpmadd52luq	$H0,$R1,$D1lo
	vpmadd52huq	$H0,$R1,$D1hi
	vpmadd52luq	$H0,$R2,$D2lo
	vpmadd52huq	$H0,$R2,$D2hi
	vpmadd52luq	$H1,$S2,$D0lo
	vpmadd52huq	$H1,$S2,$D0hi
	vpmadd52luq	$H1,$R0,$D1lo
	vpmadd52huq	$H1,$R0,$D1hi
	vpmadd52luq	$H1,$R1,$D2lo
	vpmadd52huq	$H1,$R1,$D2hi
	mov		\$1,%eax
	kmovw		%eax,%k1
	vpsrldq		\$8,$D0lo,$T0
	vpsrldq		\$8,$D0hi,$H0
	vpsrldq		\$8,$D1lo,$T1
	vpsrldq		\$8,$D1hi,$H1
	vpaddq		$T0,$D0lo,$D0lo
	vpaddq		$H0,$D0hi,$D0hi
	vpsrldq		\$8,$D2lo,$T2
	vpsrldq		\$8,$D2hi,$H2
	vpaddq		$T1,$D1lo,$D1lo
	vpaddq		$H1,$D1hi,$D1hi
	 vpermq		\$0x2,$D0lo,$T0
	 vpermq		\$0x2,$D0hi,$H0
	vpaddq		$T2,$D2lo,$D2lo
	vpaddq		$H2,$D2hi,$D2hi
	vpermq		\$0x2,$D1lo,$T1
	vpermq		\$0x2,$D1hi,$H1
	vpaddq		$T0,$D0lo,${D0lo}{%k1}{z}
	vpaddq		$H0,$D0hi,${D0hi}{%k1}{z}
	vpermq		\$0x2,$D2lo,$T2
	vpermq		\$0x2,$D2hi,$H2
	vpaddq		$T1,$D1lo,${D1lo}{%k1}{z}
	vpaddq		$H1,$D1hi,${D1hi}{%k1}{z}
	vpaddq		$T2,$D2lo,${D2lo}{%k1}{z}
	vpaddq		$H2,$D2hi,${D2hi}{%k1}{z}
	vpsrlq		\$44,$D0lo,$tmp
	vpsllq		\$8,$D0hi,$D0hi
	vpandq		$mask44,$D0lo,$H0
	vpaddq		$tmp,$D0hi,$D0hi
	vpaddq		$D0hi,$D1lo,$D1lo
	vpsrlq		\$44,$D1lo,$tmp
	vpsllq		\$8,$D1hi,$D1hi
	vpandq		$mask44,$D1lo,$H1
	vpaddq		$tmp,$D1hi,$D1hi
	vpaddq		$D1hi,$D2lo,$D2lo
	vpsrlq		\$42,$D2lo,$tmp
	vpsllq		\$10,$D2hi,$D2hi
	vpandq		$mask42,$D2lo,$H2
	vpaddq		$tmp,$D2hi,$D2hi
	vpaddq		$D2hi,$H0,$H0
	vpsllq		\$2,$D2hi,$D2hi
	vpaddq		$D2hi,$H0,$H0
	vpsrlq		\$44,$H0,$tmp		
	vpandq		$mask44,$H0,$H0
	vpaddq		$tmp,$H1,$H1
	sub		\$2,$len		
	ja		.Lblocks_vpmadd52_4x_do
	vmovq		%x
	vmovq		%x
	vmovq		%x
	vzeroall
.Lno_data_vpmadd52_4x:
	RET
.size	poly1305_blocks_vpmadd52_4x,.-poly1305_blocks_vpmadd52_4x
___
}
{
my ($H0,$H1,$H2,$R0,$R1,$R2,$S1,$S2) = map("%ymm$_",(0..5,16,17));
my ($D0lo,$D0hi,$D1lo,$D1hi,$D2lo,$D2hi) = map("%ymm$_",(18..23));
my ($T0,$T1,$T2,$T3,$mask44,$mask42,$tmp,$PAD) = map("%ymm$_",(24..31));
my ($RR0,$RR1,$RR2,$SS1,$SS2) = map("%ymm$_",(6..10));
$code.=<<___;
.type	poly1305_blocks_vpmadd52_8x,\@function,4
.align	32
poly1305_blocks_vpmadd52_8x:
	shr	\$4,$len
	jz	.Lno_data_vpmadd52_8x		
	shl	\$40,$padbit
	mov	64($ctx),%r8			
	vmovdqa64	.Lx_mask44(%rip),$mask44
	vmovdqa64	.Lx_mask42(%rip),$mask42
	test	%r8,%r8				
	js	.Linit_vpmadd52			
	vmovq	0($ctx),%x
	vmovq	8($ctx),%x
	vmovq	16($ctx),%x
.Lblocks_vpmadd52_8x:
	vmovdqu64	128($ctx),$R2		
	vmovdqu64	160($ctx),$S1
	vmovdqu64	64($ctx),$R0
	vmovdqu64	96($ctx),$R1
	vpsllq		\$2,$R2,$S2		
	vpaddq		$R2,$S2,$S2
	vpsllq		\$2,$S2,$S2
	vpbroadcastq	%x
	vpbroadcastq	%x
	vpbroadcastq	%x
	vpxorq		$D0lo,$D0lo,$D0lo
	vpmadd52luq	$RR2,$S1,$D0lo
	vpxorq		$D0hi,$D0hi,$D0hi
	vpmadd52huq	$RR2,$S1,$D0hi
	vpxorq		$D1lo,$D1lo,$D1lo
	vpmadd52luq	$RR2,$S2,$D1lo
	vpxorq		$D1hi,$D1hi,$D1hi
	vpmadd52huq	$RR2,$S2,$D1hi
	vpxorq		$D2lo,$D2lo,$D2lo
	vpmadd52luq	$RR2,$R0,$D2lo
	vpxorq		$D2hi,$D2hi,$D2hi
	vpmadd52huq	$RR2,$R0,$D2hi
	vpmadd52luq	$RR0,$R0,$D0lo
	vpmadd52huq	$RR0,$R0,$D0hi
	vpmadd52luq	$RR0,$R1,$D1lo
	vpmadd52huq	$RR0,$R1,$D1hi
	vpmadd52luq	$RR0,$R2,$D2lo
	vpmadd52huq	$RR0,$R2,$D2hi
	vpmadd52luq	$RR1,$S2,$D0lo
	vpmadd52huq	$RR1,$S2,$D0hi
	vpmadd52luq	$RR1,$R0,$D1lo
	vpmadd52huq	$RR1,$R0,$D1hi
	vpmadd52luq	$RR1,$R1,$D2lo
	vpmadd52huq	$RR1,$R1,$D2hi
	vpsrlq		\$44,$D0lo,$tmp
	vpsllq		\$8,$D0hi,$D0hi
	vpandq		$mask44,$D0lo,$RR0
	vpaddq		$tmp,$D0hi,$D0hi
	vpaddq		$D0hi,$D1lo,$D1lo
	vpsrlq		\$44,$D1lo,$tmp
	vpsllq		\$8,$D1hi,$D1hi
	vpandq		$mask44,$D1lo,$RR1
	vpaddq		$tmp,$D1hi,$D1hi
	vpaddq		$D1hi,$D2lo,$D2lo
	vpsrlq		\$42,$D2lo,$tmp
	vpsllq		\$10,$D2hi,$D2hi
	vpandq		$mask42,$D2lo,$RR2
	vpaddq		$tmp,$D2hi,$D2hi
	vpaddq		$D2hi,$RR0,$RR0
	vpsllq		\$2,$D2hi,$D2hi
	vpaddq		$D2hi,$RR0,$RR0
	vpsrlq		\$44,$RR0,$tmp		
	vpandq		$mask44,$RR0,$RR0
	vpaddq		$tmp,$RR1,$RR1
	vpunpcklqdq	$R2,$RR2,$T2		
	vpunpckhqdq	$R2,$RR2,$R2		
	vpunpcklqdq	$R0,$RR0,$T0
	vpunpckhqdq	$R0,$RR0,$R0
	vpunpcklqdq	$R1,$RR1,$T1
	vpunpckhqdq	$R1,$RR1,$R1
___
map(s/%y/%z/, $H0,$H1,$H2,$R0,$R1,$R2,$S1,$S2);
map(s/%y/%z/, $D0lo,$D0hi,$D1lo,$D1hi,$D2lo,$D2hi);
map(s/%y/%z/, $T0,$T1,$T2,$T3,$mask44,$mask42,$tmp,$PAD);
map(s/%y/%z/, $RR0,$RR1,$RR2,$SS1,$SS2);
$code.=<<___;
	vshufi64x2	\$0x44,$R2,$T2,$RR2	
	vshufi64x2	\$0x44,$R0,$T0,$RR0
	vshufi64x2	\$0x44,$R1,$T1,$RR1
	vmovdqu64	16*0($inp),$T2		
	vmovdqu64	16*4($inp),$T3
	lea		16*8($inp),$inp
	vpsllq		\$2,$RR2,$SS2		
	vpsllq		\$2,$RR1,$SS1		
	vpaddq		$RR2,$SS2,$SS2
	vpaddq		$RR1,$SS1,$SS1
	vpsllq		\$2,$SS2,$SS2
	vpsllq		\$2,$SS1,$SS1
	vpbroadcastq	$padbit,$PAD
	vpbroadcastq	%x
	vpbroadcastq	%x
	vpbroadcastq	%x
	vpbroadcastq	%x
	vpbroadcastq	%x
	vpbroadcastq	%x
	vpbroadcastq	%x
	vpunpcklqdq	$T3,$T2,$T1		
	vpunpckhqdq	$T3,$T2,$T3
	vpsrlq		\$24,$T3,$T2		
	vporq		$PAD,$T2,$T2
	 vpaddq		$T2,$H2,$H2		
	vpandq		$mask44,$T1,$T0
	vpsrlq		\$44,$T1,$T1
	vpsllq		\$20,$T3,$T3
	vporq		$T3,$T1,$T1
	vpandq		$mask44,$T1,$T1
	sub		\$8,$len
	jz		.Ltail_vpmadd52_8x
	jmp		.Loop_vpmadd52_8x
.align	32
.Loop_vpmadd52_8x:
	vpaddq		$T0,$H0,$H0
	vpaddq		$T1,$H1,$H1
	vpxorq		$D0lo,$D0lo,$D0lo
	vpmadd52luq	$H2,$S1,$D0lo
	vpxorq		$D0hi,$D0hi,$D0hi
	vpmadd52huq	$H2,$S1,$D0hi
	vpxorq		$D1lo,$D1lo,$D1lo
	vpmadd52luq	$H2,$S2,$D1lo
	vpxorq		$D1hi,$D1hi,$D1hi
	vpmadd52huq	$H2,$S2,$D1hi
	vpxorq		$D2lo,$D2lo,$D2lo
	vpmadd52luq	$H2,$R0,$D2lo
	vpxorq		$D2hi,$D2hi,$D2hi
	vpmadd52huq	$H2,$R0,$D2hi
	 vmovdqu64	16*0($inp),$T2		
	 vmovdqu64	16*4($inp),$T3
	 lea		16*8($inp),$inp
	vpmadd52luq	$H0,$R0,$D0lo
	vpmadd52huq	$H0,$R0,$D0hi
	vpmadd52luq	$H0,$R1,$D1lo
	vpmadd52huq	$H0,$R1,$D1hi
	vpmadd52luq	$H0,$R2,$D2lo
	vpmadd52huq	$H0,$R2,$D2hi
	 vpunpcklqdq	$T3,$T2,$T1		
	 vpunpckhqdq	$T3,$T2,$T3
	vpmadd52luq	$H1,$S2,$D0lo
	vpmadd52huq	$H1,$S2,$D0hi
	vpmadd52luq	$H1,$R0,$D1lo
	vpmadd52huq	$H1,$R0,$D1hi
	vpmadd52luq	$H1,$R1,$D2lo
	vpmadd52huq	$H1,$R1,$D2hi
	vpsrlq		\$44,$D0lo,$tmp
	vpsllq		\$8,$D0hi,$D0hi
	vpandq		$mask44,$D0lo,$H0
	vpaddq		$tmp,$D0hi,$D0hi
	 vpsrlq		\$24,$T3,$T2
	 vporq		$PAD,$T2,$T2
	vpaddq		$D0hi,$D1lo,$D1lo
	vpsrlq		\$44,$D1lo,$tmp
	vpsllq		\$8,$D1hi,$D1hi
	vpandq		$mask44,$D1lo,$H1
	vpaddq		$tmp,$D1hi,$D1hi
	 vpandq		$mask44,$T1,$T0
	 vpsrlq		\$44,$T1,$T1
	 vpsllq		\$20,$T3,$T3
	vpaddq		$D1hi,$D2lo,$D2lo
	vpsrlq		\$42,$D2lo,$tmp
	vpsllq		\$10,$D2hi,$D2hi
	vpandq		$mask42,$D2lo,$H2
	vpaddq		$tmp,$D2hi,$D2hi
	  vpaddq	$T2,$H2,$H2		
	vpaddq		$D2hi,$H0,$H0
	vpsllq		\$2,$D2hi,$D2hi
	vpaddq		$D2hi,$H0,$H0
	 vporq		$T3,$T1,$T1
	 vpandq		$mask44,$T1,$T1
	vpsrlq		\$44,$H0,$tmp		
	vpandq		$mask44,$H0,$H0
	vpaddq		$tmp,$H1,$H1
	sub		\$8,$len		
	jnz		.Loop_vpmadd52_8x
.Ltail_vpmadd52_8x:
	vpaddq		$T0,$H0,$H0
	vpaddq		$T1,$H1,$H1
	vpxorq		$D0lo,$D0lo,$D0lo
	vpmadd52luq	$H2,$SS1,$D0lo
	vpxorq		$D0hi,$D0hi,$D0hi
	vpmadd52huq	$H2,$SS1,$D0hi
	vpxorq		$D1lo,$D1lo,$D1lo
	vpmadd52luq	$H2,$SS2,$D1lo
	vpxorq		$D1hi,$D1hi,$D1hi
	vpmadd52huq	$H2,$SS2,$D1hi
	vpxorq		$D2lo,$D2lo,$D2lo
	vpmadd52luq	$H2,$RR0,$D2lo
	vpxorq		$D2hi,$D2hi,$D2hi
	vpmadd52huq	$H2,$RR0,$D2hi
	vpmadd52luq	$H0,$RR0,$D0lo
	vpmadd52huq	$H0,$RR0,$D0hi
	vpmadd52luq	$H0,$RR1,$D1lo
	vpmadd52huq	$H0,$RR1,$D1hi
	vpmadd52luq	$H0,$RR2,$D2lo
	vpmadd52huq	$H0,$RR2,$D2hi
	vpmadd52luq	$H1,$SS2,$D0lo
	vpmadd52huq	$H1,$SS2,$D0hi
	vpmadd52luq	$H1,$RR0,$D1lo
	vpmadd52huq	$H1,$RR0,$D1hi
	vpmadd52luq	$H1,$RR1,$D2lo
	vpmadd52huq	$H1,$RR1,$D2hi
	mov		\$1,%eax
	kmovw		%eax,%k1
	vpsrldq		\$8,$D0lo,$T0
	vpsrldq		\$8,$D0hi,$H0
	vpsrldq		\$8,$D1lo,$T1
	vpsrldq		\$8,$D1hi,$H1
	vpaddq		$T0,$D0lo,$D0lo
	vpaddq		$H0,$D0hi,$D0hi
	vpsrldq		\$8,$D2lo,$T2
	vpsrldq		\$8,$D2hi,$H2
	vpaddq		$T1,$D1lo,$D1lo
	vpaddq		$H1,$D1hi,$D1hi
	 vpermq		\$0x2,$D0lo,$T0
	 vpermq		\$0x2,$D0hi,$H0
	vpaddq		$T2,$D2lo,$D2lo
	vpaddq		$H2,$D2hi,$D2hi
	vpermq		\$0x2,$D1lo,$T1
	vpermq		\$0x2,$D1hi,$H1
	vpaddq		$T0,$D0lo,$D0lo
	vpaddq		$H0,$D0hi,$D0hi
	vpermq		\$0x2,$D2lo,$T2
	vpermq		\$0x2,$D2hi,$H2
	vpaddq		$T1,$D1lo,$D1lo
	vpaddq		$H1,$D1hi,$D1hi
	 vextracti64x4	\$1,$D0lo,%y
	 vextracti64x4	\$1,$D0hi,%y
	vpaddq		$T2,$D2lo,$D2lo
	vpaddq		$H2,$D2hi,$D2hi
	vextracti64x4	\$1,$D1lo,%y
	vextracti64x4	\$1,$D1hi,%y
	vextracti64x4	\$1,$D2lo,%y
	vextracti64x4	\$1,$D2hi,%y
___
map(s/%z/%y/, $H0,$H1,$H2,$R0,$R1,$R2,$S1,$S2);
map(s/%z/%y/, $D0lo,$D0hi,$D1lo,$D1hi,$D2lo,$D2hi);
map(s/%z/%y/, $T0,$T1,$T2,$T3,$mask44,$mask42,$tmp,$PAD);
$code.=<<___;
	vpaddq		$T0,$D0lo,${D0lo}{%k1}{z}
	vpaddq		$H0,$D0hi,${D0hi}{%k1}{z}
	vpaddq		$T1,$D1lo,${D1lo}{%k1}{z}
	vpaddq		$H1,$D1hi,${D1hi}{%k1}{z}
	vpaddq		$T2,$D2lo,${D2lo}{%k1}{z}
	vpaddq		$H2,$D2hi,${D2hi}{%k1}{z}
	vpsrlq		\$44,$D0lo,$tmp
	vpsllq		\$8,$D0hi,$D0hi
	vpandq		$mask44,$D0lo,$H0
	vpaddq		$tmp,$D0hi,$D0hi
	vpaddq		$D0hi,$D1lo,$D1lo
	vpsrlq		\$44,$D1lo,$tmp
	vpsllq		\$8,$D1hi,$D1hi
	vpandq		$mask44,$D1lo,$H1
	vpaddq		$tmp,$D1hi,$D1hi
	vpaddq		$D1hi,$D2lo,$D2lo
	vpsrlq		\$42,$D2lo,$tmp
	vpsllq		\$10,$D2hi,$D2hi
	vpandq		$mask42,$D2lo,$H2
	vpaddq		$tmp,$D2hi,$D2hi
	vpaddq		$D2hi,$H0,$H0
	vpsllq		\$2,$D2hi,$D2hi
	vpaddq		$D2hi,$H0,$H0
	vpsrlq		\$44,$H0,$tmp		
	vpandq		$mask44,$H0,$H0
	vpaddq		$tmp,$H1,$H1
	vmovq		%x
	vmovq		%x
	vmovq		%x
	vzeroall
.Lno_data_vpmadd52_8x:
	RET
.size	poly1305_blocks_vpmadd52_8x,.-poly1305_blocks_vpmadd52_8x
___
}
$code.=<<___;
.type	poly1305_emit_base2_44,\@function,3
.align	32
poly1305_emit_base2_44:
	mov	0($ctx),%r8	
	mov	8($ctx),%r9
	mov	16($ctx),%r10
	mov	%r9,%rax
	shr	\$20,%r9
	shl	\$44,%rax
	mov	%r10,%rcx
	shr	\$40,%r10
	shl	\$24,%rcx
	add	%rax,%r8
	adc	%rcx,%r9
	adc	\$0,%r10
	mov	%r8,%rax
	add	\$5,%r8		
	mov	%r9,%rcx
	adc	\$0,%r9
	adc	\$0,%r10
	shr	\$2,%r10	
	cmovnz	%r8,%rax
	cmovnz	%r9,%rcx
	add	0($nonce),%rax	
	adc	8($nonce),%rcx
	mov	%rax,0($mac)	
	mov	%rcx,8($mac)
	RET
.size	poly1305_emit_base2_44,.-poly1305_emit_base2_44
___
}	}	}
}
if (!$kernel)
{	
my ($out,$inp,$otp,$len)=$win64 ? ("%rcx","%rdx","%r8", "%r9") :  
                                  ("%rdi","%rsi","%rdx","%rcx");  
$code.=<<___;
.globl	xor128_encrypt_n_pad
.type	xor128_encrypt_n_pad,\@abi-omnipotent
.align	16
xor128_encrypt_n_pad:
	sub	$otp,$inp
	sub	$otp,$out
	mov	$len,%r10		
	shr	\$4,$len		
	jz	.Ltail_enc
	nop
.Loop_enc_xmm:
	movdqu	($inp,$otp),%xmm0
	pxor	($otp),%xmm0
	movdqu	%xmm0,($out,$otp)
	movdqa	%xmm0,($otp)
	lea	16($otp),$otp
	dec	$len
	jnz	.Loop_enc_xmm
	and	\$15,%r10		
	jz	.Ldone_enc
.Ltail_enc:
	mov	\$16,$len
	sub	%r10,$len
	xor	%eax,%eax
.Loop_enc_byte:
	mov	($inp,$otp),%al
	xor	($otp),%al
	mov	%al,($out,$otp)
	mov	%al,($otp)
	lea	1($otp),$otp
	dec	%r10
	jnz	.Loop_enc_byte
	xor	%eax,%eax
.Loop_enc_pad:
	mov	%al,($otp)
	lea	1($otp),$otp
	dec	$len
	jnz	.Loop_enc_pad
.Ldone_enc:
	mov	$otp,%rax
	RET
.size	xor128_encrypt_n_pad,.-xor128_encrypt_n_pad
.globl	xor128_decrypt_n_pad
.type	xor128_decrypt_n_pad,\@abi-omnipotent
.align	16
xor128_decrypt_n_pad:
	sub	$otp,$inp
	sub	$otp,$out
	mov	$len,%r10		
	shr	\$4,$len		
	jz	.Ltail_dec
	nop
.Loop_dec_xmm:
	movdqu	($inp,$otp),%xmm0
	movdqa	($otp),%xmm1
	pxor	%xmm0,%xmm1
	movdqu	%xmm1,($out,$otp)
	movdqa	%xmm0,($otp)
	lea	16($otp),$otp
	dec	$len
	jnz	.Loop_dec_xmm
	pxor	%xmm1,%xmm1
	and	\$15,%r10		
	jz	.Ldone_dec
.Ltail_dec:
	mov	\$16,$len
	sub	%r10,$len
	xor	%eax,%eax
	xor	%r11d,%r11d
.Loop_dec_byte:
	mov	($inp,$otp),%r11b
	mov	($otp),%al
	xor	%r11b,%al
	mov	%al,($out,$otp)
	mov	%r11b,($otp)
	lea	1($otp),$otp
	dec	%r10
	jnz	.Loop_dec_byte
	xor	%eax,%eax
.Loop_dec_pad:
	mov	%al,($otp)
	lea	1($otp),$otp
	dec	$len
	jnz	.Loop_dec_pad
.Ldone_dec:
	mov	$otp,%rax
	RET
.size	xor128_decrypt_n_pad,.-xor128_decrypt_n_pad
___
}
if ($win64) {
$rec="%rcx";
$frame="%rdx";
$context="%r8";
$disp="%r9";
$code.=<<___;
.extern	__imp_RtlVirtualUnwind
.type	se_handler,\@abi-omnipotent
.align	16
se_handler:
	push	%rsi
	push	%rdi
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	pushfq
	sub	\$64,%rsp
	mov	120($context),%rax	
	mov	248($context),%rbx	
	mov	8($disp),%rsi		
	mov	56($disp),%r11		
	mov	0(%r11),%r10d		
	lea	(%rsi,%r10),%r10	
	cmp	%r10,%rbx		
	jb	.Lcommon_seh_tail
	mov	152($context),%rax	
	mov	4(%r11),%r10d		
	lea	(%rsi,%r10),%r10	
	cmp	%r10,%rbx		
	jae	.Lcommon_seh_tail
	lea	48(%rax),%rax
	mov	-8(%rax),%rbx
	mov	-16(%rax),%rbp
	mov	-24(%rax),%r12
	mov	-32(%rax),%r13
	mov	-40(%rax),%r14
	mov	-48(%rax),%r15
	mov	%rbx,144($context)	
	mov	%rbp,160($context)	
	mov	%r12,216($context)	
	mov	%r13,224($context)	
	mov	%r14,232($context)	
	mov	%r15,240($context)	
	jmp	.Lcommon_seh_tail
.size	se_handler,.-se_handler
.type	avx_handler,\@abi-omnipotent
.align	16
avx_handler:
	push	%rsi
	push	%rdi
	push	%rbx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	pushfq
	sub	\$64,%rsp
	mov	120($context),%rax	
	mov	248($context),%rbx	
	mov	8($disp),%rsi		
	mov	56($disp),%r11		
	mov	0(%r11),%r10d		
	lea	(%rsi,%r10),%r10	
	cmp	%r10,%rbx		
	jb	.Lcommon_seh_tail
	mov	152($context),%rax	
	mov	4(%r11),%r10d		
	lea	(%rsi,%r10),%r10	
	cmp	%r10,%rbx		
	jae	.Lcommon_seh_tail
	mov	208($context),%rax	
	lea	0x50(%rax),%rsi
	lea	0xf8(%rax),%rax
	lea	512($context),%rdi	
	mov	\$20,%ecx
	.long	0xa548f3fc		
.Lcommon_seh_tail:
	mov	8(%rax),%rdi
	mov	16(%rax),%rsi
	mov	%rax,152($context)	
	mov	%rsi,168($context)	
	mov	%rdi,176($context)	
	mov	40($disp),%rdi		
	mov	$context,%rsi		
	mov	\$154,%ecx		
	.long	0xa548f3fc		
	mov	$disp,%rsi
	xor	%ecx,%ecx		
	mov	8(%rsi),%rdx		
	mov	0(%rsi),%r8		
	mov	16(%rsi),%r9		
	mov	40(%rsi),%r10		
	lea	56(%rsi),%r11		
	lea	24(%rsi),%r12		
	mov	%r10,32(%rsp)		
	mov	%r11,40(%rsp)		
	mov	%r12,48(%rsp)		
	mov	%rcx,56(%rsp)		
	call	*__imp_RtlVirtualUnwind(%rip)
	mov	\$1,%eax		
	add	\$64,%rsp
	popfq
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rbx
	pop	%rdi
	pop	%rsi
	RET
.size	avx_handler,.-avx_handler
.section	.pdata
.align	4
	.rva	.LSEH_begin_poly1305_init_x86_64
	.rva	.LSEH_end_poly1305_init_x86_64
	.rva	.LSEH_info_poly1305_init_x86_64
	.rva	.LSEH_begin_poly1305_blocks_x86_64
	.rva	.LSEH_end_poly1305_blocks_x86_64
	.rva	.LSEH_info_poly1305_blocks_x86_64
	.rva	.LSEH_begin_poly1305_emit_x86_64
	.rva	.LSEH_end_poly1305_emit_x86_64
	.rva	.LSEH_info_poly1305_emit_x86_64
___
$code.=<<___ if ($avx);
	.rva	.LSEH_begin_poly1305_blocks_avx
	.rva	.Lbase2_64_avx
	.rva	.LSEH_info_poly1305_blocks_avx_1
	.rva	.Lbase2_64_avx
	.rva	.Leven_avx
	.rva	.LSEH_info_poly1305_blocks_avx_2
	.rva	.Leven_avx
	.rva	.LSEH_end_poly1305_blocks_avx
	.rva	.LSEH_info_poly1305_blocks_avx_3
	.rva	.LSEH_begin_poly1305_emit_avx
	.rva	.LSEH_end_poly1305_emit_avx
	.rva	.LSEH_info_poly1305_emit_avx
___
$code.=<<___ if ($avx>1);
	.rva	.LSEH_begin_poly1305_blocks_avx2
	.rva	.Lbase2_64_avx2
	.rva	.LSEH_info_poly1305_blocks_avx2_1
	.rva	.Lbase2_64_avx2
	.rva	.Leven_avx2
	.rva	.LSEH_info_poly1305_blocks_avx2_2
	.rva	.Leven_avx2
	.rva	.LSEH_end_poly1305_blocks_avx2
	.rva	.LSEH_info_poly1305_blocks_avx2_3
___
$code.=<<___ if ($avx>2);
	.rva	.LSEH_begin_poly1305_blocks_avx512
	.rva	.LSEH_end_poly1305_blocks_avx512
	.rva	.LSEH_info_poly1305_blocks_avx512
___
$code.=<<___;
.section	.xdata
.align	8
.LSEH_info_poly1305_init_x86_64:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.LSEH_begin_poly1305_init_x86_64,.LSEH_begin_poly1305_init_x86_64
.LSEH_info_poly1305_blocks_x86_64:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lblocks_body,.Lblocks_epilogue
.LSEH_info_poly1305_emit_x86_64:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.LSEH_begin_poly1305_emit_x86_64,.LSEH_begin_poly1305_emit_x86_64
___
$code.=<<___ if ($avx);
.LSEH_info_poly1305_blocks_avx_1:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lblocks_avx_body,.Lblocks_avx_epilogue		
.LSEH_info_poly1305_blocks_avx_2:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lbase2_64_avx_body,.Lbase2_64_avx_epilogue	
.LSEH_info_poly1305_blocks_avx_3:
	.byte	9,0,0,0
	.rva	avx_handler
	.rva	.Ldo_avx_body,.Ldo_avx_epilogue			
.LSEH_info_poly1305_emit_avx:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.LSEH_begin_poly1305_emit_avx,.LSEH_begin_poly1305_emit_avx
___
$code.=<<___ if ($avx>1);
.LSEH_info_poly1305_blocks_avx2_1:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lblocks_avx2_body,.Lblocks_avx2_epilogue	
.LSEH_info_poly1305_blocks_avx2_2:
	.byte	9,0,0,0
	.rva	se_handler
	.rva	.Lbase2_64_avx2_body,.Lbase2_64_avx2_epilogue	
.LSEH_info_poly1305_blocks_avx2_3:
	.byte	9,0,0,0
	.rva	avx_handler
	.rva	.Ldo_avx2_body,.Ldo_avx2_epilogue		
___
$code.=<<___ if ($avx>2);
.LSEH_info_poly1305_blocks_avx512:
	.byte	9,0,0,0
	.rva	avx_handler
	.rva	.Ldo_avx512_body,.Ldo_avx512_epilogue		
___
}
open SELF,$0;
while(<SELF>) {
	next if (/^
	last if (!s/^
	print;
}
close SELF;
foreach (split('\n',$code)) {
	s/\`([^\`]*)\`/eval($1)/ge;
	s/%r([a-z]+)
	s/%r([0-9]+)
	s/%x
	if ($kernel) {
		s/(^\.type.*),[0-9]+$/\1/;
		s/(^\.type.*),\@abi-omnipotent+$/\1,\@function/;
		next if /^\.cfi.*/;
	}
	print $_,"\n";
}
close STDOUT;
