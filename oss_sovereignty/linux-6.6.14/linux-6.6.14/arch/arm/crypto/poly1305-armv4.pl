$flavour = shift;
if ($flavour=~/\w[\w\-]*\.\w+$/) { $output=$flavour; undef $flavour; }
else { while (($output=shift) && ($output!~/\w[\w\-]*\.\w+$/)) {} }
if ($flavour && $flavour ne "void") {
    $0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
    ( $xlate="${dir}arm-xlate.pl" and -f $xlate ) or
    ( $xlate="${dir}../../perlasm/arm-xlate.pl" and -f $xlate) or
    die "can't locate arm-xlate.pl";
    open STDOUT,"| \"$^X\" $xlate $flavour $output";
} else {
    open STDOUT,">$output";
}
($ctx,$inp,$len,$padbit)=map("r$_",(0..3));
$code.=<<___;
.globl	poly1305_blocks_neon
.syntax	unified
.thumb
.code	32
.text
.globl	poly1305_emit
.globl	poly1305_blocks
.globl	poly1305_init
.type	poly1305_init,%function
.align	5
poly1305_init:
.Lpoly1305_init:
	stmdb	sp!,{r4-r11}
	eor	r3,r3,r3
	cmp	$inp,
	str	r3,[$ctx,
	str	r3,[$ctx,
	str	r3,[$ctx,
	str	r3,[$ctx,
	str	r3,[$ctx,
	str	r3,[$ctx,
	add	$ctx,$ctx,
	it	eq
	moveq	r0,
	beq	.Lno_key
	mov	r3,
	str	r3,[$ctx,
	adr	r11,.Lpoly1305_init
	ldr	r12,.LOPENSSL_armcap
	ldrb	r4,[$inp,
	mov	r10,
	ldrb	r5,[$inp,
	and	r3,r10,
	ldrb	r6,[$inp,
	ldrb	r7,[$inp,
	orr	r4,r4,r5,lsl
	ldrb	r5,[$inp,
	orr	r4,r4,r6,lsl
	ldrb	r6,[$inp,
	orr	r4,r4,r7,lsl
	ldrb	r7,[$inp,
	and	r4,r4,r10
	ldr	r12,[r11,r12]		@ OPENSSL_armcap_P
	ldr	r12,[r12]
	ldrb	r8,[$inp,
	orr	r5,r5,r6,lsl
	ldrb	r6,[$inp,
	orr	r5,r5,r7,lsl
	ldrb	r7,[$inp,
	orr	r5,r5,r8,lsl
	ldrb	r8,[$inp,
	and	r5,r5,r3
	tst	r12,
	adr	r9,.Lpoly1305_blocks_neon
	adr	r11,.Lpoly1305_blocks
	it	ne
	movne	r11,r9
	adr	r12,.Lpoly1305_emit
	orr	r11,r11,
	orr	r12,r12,
	add	r12,r11,
	ite	eq
	addeq	r11,r11,
	addne	r11,r11,
	ldrb	r9,[$inp,
	orr	r6,r6,r7,lsl
	ldrb	r7,[$inp,
	orr	r6,r6,r8,lsl
	ldrb	r8,[$inp,
	orr	r6,r6,r9,lsl
	ldrb	r9,[$inp,
	and	r6,r6,r3
	ldrb	r10,[$inp,
	orr	r7,r7,r8,lsl
	str	r4,[$ctx,
	orr	r7,r7,r9,lsl
	str	r5,[$ctx,
	orr	r7,r7,r10,lsl
	str	r6,[$ctx,
	and	r7,r7,r3
	str	r7,[$ctx,
	stmia	r2,{r11,r12}		@ fill functions table
	mov	r0,
	mov	r0,
.Lno_key:
	ldmia	sp!,{r4-r11}
	ret				@ bx	lr
	tst	lr,
	moveq	pc,lr			@ be binary compatible with V4, yet
	bx	lr			@ interoperable with Thumb ISA:-)
.size	poly1305_init,.-poly1305_init
___
{
my ($h0,$h1,$h2,$h3,$h4,$r0,$r1,$r2,$r3)=map("r$_",(4..12));
my ($s1,$s2,$s3)=($r1,$r2,$r3);
$code.=<<___;
.type	poly1305_blocks,%function
.align	5
poly1305_blocks:
.Lpoly1305_blocks:
	stmdb	sp!,{r3-r11,lr}
	ands	$len,$len,
	beq	.Lno_data
	add	$len,$len,$inp		@ end pointer
	sub	sp,sp,
	ldmia	$ctx,{$h0-$r3}		@ load context
	add	$ctx,$ctx,
	str	$len,[sp,
	str	$ctx,[sp,
	ldr	lr,[$ctx,
	ldmia	$ctx!,{$h0-$h4}		@ load hash value
	str	$len,[sp,
	str	$ctx,[sp,
	adds	$r0,$h0,$h1,lsl
	mov	$r1,$h1,lsr
	adcs	$r1,$r1,$h2,lsl
	mov	$r2,$h2,lsr
	adcs	$r2,$r2,$h3,lsl
	mov	$r3,$h3,lsr
	adcs	$r3,$r3,$h4,lsl
	mov	$len,
	teq	lr,
	str	$len,[$ctx,
	adc	$len,$len,$h4,lsr
	itttt	ne
	movne	$h0,$r0			@ choose between radixes
	movne	$h1,$r1
	movne	$h2,$r2
	movne	$h3,$r3
	ldmia	$ctx,{$r0-$r3}		@ load key
	it	ne
	movne	$h4,$len
	mov	lr,$inp
	cmp	$padbit,
	str	$r1,[sp,
	str	$r2,[sp,
	str	$r3,[sp,
	b	.Loop
.align	4
.Loop:
	ldrb	r0,[lr],
	it	hi
	addhi	$h4,$h4,
	ldrb	r1,[lr,
	ldrb	r2,[lr,
	ldrb	r3,[lr,
	orr	r1,r0,r1,lsl
	ldrb	r0,[lr,
	orr	r2,r1,r2,lsl
	ldrb	r1,[lr,
	orr	r3,r2,r3,lsl
	ldrb	r2,[lr,
	adds	$h0,$h0,r3		@ accumulate input
	ldrb	r3,[lr,
	orr	r1,r0,r1,lsl
	ldrb	r0,[lr,
	orr	r2,r1,r2,lsl
	ldrb	r1,[lr,
	orr	r3,r2,r3,lsl
	ldrb	r2,[lr,
	adcs	$h1,$h1,r3
	ldrb	r3,[lr,
	orr	r1,r0,r1,lsl
	ldrb	r0,[lr,
	orr	r2,r1,r2,lsl
	ldrb	r1,[lr,
	orr	r3,r2,r3,lsl
	ldrb	r2,[lr,
	adcs	$h2,$h2,r3
	ldrb	r3,[lr,
	orr	r1,r0,r1,lsl
	str	lr,[sp,
	orr	r2,r1,r2,lsl
	add	$s1,$r1,$r1,lsr
	orr	r3,r2,r3,lsl
	ldr	r0,[lr],
	it	hi
	addhi	$h4,$h4,
	ldr	r1,[lr,
	ldr	r2,[lr,
	ldr	r3,[lr,
	rev	r0,r0
	rev	r1,r1
	rev	r2,r2
	rev	r3,r3
	adds	$h0,$h0,r0		@ accumulate input
	str	lr,[sp,
	adcs	$h1,$h1,r1
	add	$s1,$r1,$r1,lsr
	adcs	$h2,$h2,r2
	add	$s2,$r2,$r2,lsr
	adcs	$h3,$h3,r3
	add	$s3,$r3,$r3,lsr
	umull	r2,r3,$h1,$r0
	 adc	$h4,$h4,
	umull	r0,r1,$h0,$r0
	umlal	r2,r3,$h4,$s1
	umlal	r0,r1,$h3,$s1
	ldr	$r1,[sp,
	umlal	r2,r3,$h2,$s3
	umlal	r0,r1,$h1,$s3
	umlal	r2,r3,$h3,$s2
	umlal	r0,r1,$h2,$s2
	umlal	r2,r3,$h0,$r1
	str	r0,[sp,
	 mul	r0,$s2,$h4
	ldr	$r2,[sp,
	adds	r2,r2,r1		@ d1+=d0>>32
	 eor	r1,r1,r1
	adc	lr,r3,
	str	r2,[sp,
	mul	r2,$s3,$h4
	eor	r3,r3,r3
	umlal	r0,r1,$h3,$s3
	ldr	$r3,[sp,
	umlal	r2,r3,$h3,$r0
	umlal	r0,r1,$h2,$r0
	umlal	r2,r3,$h2,$r1
	umlal	r0,r1,$h1,$r1
	umlal	r2,r3,$h1,$r2
	umlal	r0,r1,$h0,$r2
	umlal	r2,r3,$h0,$r3
	ldr	$h0,[sp,
	mul	$h4,$r0,$h4
	ldr	$h1,[sp,
	adds	$h2,lr,r0		@ d2+=d1>>32
	ldr	lr,[sp,
	adc	r1,r1,
	adds	$h3,r2,r1		@ d3+=d2>>32
	ldr	r0,[sp,
	adc	r3,r3,
	add	$h4,$h4,r3		@ h4+=d3>>32
	and	r1,$h4,
	and	$h4,$h4,
	add	r1,r1,r1,lsr
	adds	$h0,$h0,r1
	adcs	$h1,$h1,
	adcs	$h2,$h2,
	adcs	$h3,$h3,
	adc	$h4,$h4,
	cmp	r0,lr			@ done yet?
	bhi	.Loop
	ldr	$ctx,[sp,
	add	sp,sp,
	stmdb	$ctx,{$h0-$h4}		@ store the result
.Lno_data:
	ldmia	sp!,{r3-r11,pc}
	ldmia	sp!,{r3-r11,lr}
	tst	lr,
	moveq	pc,lr			@ be binary compatible with V4, yet
	bx	lr			@ interoperable with Thumb ISA:-)
.size	poly1305_blocks,.-poly1305_blocks
___
}
{
my ($ctx,$mac,$nonce)=map("r$_",(0..2));
my ($h0,$h1,$h2,$h3,$h4,$g0,$g1,$g2,$g3)=map("r$_",(3..11));
my $g4=$ctx;
$code.=<<___;
.type	poly1305_emit,%function
.align	5
poly1305_emit:
.Lpoly1305_emit:
	stmdb	sp!,{r4-r11}
	ldmia	$ctx,{$h0-$h4}
	ldr	ip,[$ctx,
	adds	$g0,$h0,$h1,lsl
	mov	$g1,$h1,lsr
	adcs	$g1,$g1,$h2,lsl
	mov	$g2,$h2,lsr
	adcs	$g2,$g2,$h3,lsl
	mov	$g3,$h3,lsr
	adcs	$g3,$g3,$h4,lsl
	mov	$g4,
	adc	$g4,$g4,$h4,lsr
	tst	ip,ip
	itttt	ne
	movne	$h0,$g0
	movne	$h1,$g1
	movne	$h2,$g2
	movne	$h3,$g3
	it	ne
	movne	$h4,$g4
	adds	$g0,$h0,
	adcs	$g1,$h1,
	adcs	$g2,$h2,
	adcs	$g3,$h3,
	adc	$g4,$h4,
	tst	$g4,
	it	ne
	movne	$h0,$g0
	ldr	$g0,[$nonce,
	it	ne
	movne	$h1,$g1
	ldr	$g1,[$nonce,
	it	ne
	movne	$h2,$g2
	ldr	$g2,[$nonce,
	it	ne
	movne	$h3,$g3
	ldr	$g3,[$nonce,
	adds	$h0,$h0,$g0
	adcs	$h1,$h1,$g1
	adcs	$h2,$h2,$g2
	adc	$h3,$h3,$g3
	rev	$h0,$h0
	rev	$h1,$h1
	rev	$h2,$h2
	rev	$h3,$h3
	str	$h0,[$mac,
	str	$h1,[$mac,
	str	$h2,[$mac,
	str	$h3,[$mac,
	strb	$h0,[$mac,
	mov	$h0,$h0,lsr
	strb	$h1,[$mac,
	mov	$h1,$h1,lsr
	strb	$h2,[$mac,
	mov	$h2,$h2,lsr
	strb	$h3,[$mac,
	mov	$h3,$h3,lsr
	strb	$h0,[$mac,
	mov	$h0,$h0,lsr
	strb	$h1,[$mac,
	mov	$h1,$h1,lsr
	strb	$h2,[$mac,
	mov	$h2,$h2,lsr
	strb	$h3,[$mac,
	mov	$h3,$h3,lsr
	strb	$h0,[$mac,
	mov	$h0,$h0,lsr
	strb	$h1,[$mac,
	mov	$h1,$h1,lsr
	strb	$h2,[$mac,
	mov	$h2,$h2,lsr
	strb	$h3,[$mac,
	mov	$h3,$h3,lsr
	strb	$h0,[$mac,
	strb	$h1,[$mac,
	strb	$h2,[$mac,
	strb	$h3,[$mac,
	ldmia	sp!,{r4-r11}
	ret				@ bx	lr
	tst	lr,
	moveq	pc,lr			@ be binary compatible with V4, yet
	bx	lr			@ interoperable with Thumb ISA:-)
.size	poly1305_emit,.-poly1305_emit
___
{
my ($R0,$R1,$S1,$R2,$S2,$R3,$S3,$R4,$S4) = map("d$_",(0..9));
my ($D0,$D1,$D2,$D3,$D4, $H0,$H1,$H2,$H3,$H4) = map("q$_",(5..14));
my ($T0,$T1,$MASK) = map("q$_",(15,4,0));
my ($in2,$zeros,$tbl0,$tbl1) = map("r$_",(4..7));
$code.=<<___;
.fpu	neon
.type	poly1305_init_neon,%function
.align	5
poly1305_init_neon:
.Lpoly1305_init_neon:
	ldr	r3,[$ctx,
	cmp	r3,
	bne	.Lno_init_neon
	ldr	r4,[$ctx,
	ldr	r5,[$ctx,
	ldr	r6,[$ctx,
	ldr	r7,[$ctx,
	and	r2,r4,
	mov	r3,r4,lsr
	mov	r4,r5,lsr
	orr	r3,r3,r5,lsl
	mov	r5,r6,lsr
	orr	r4,r4,r6,lsl
	mov	r6,r7,lsr
	orr	r5,r5,r7,lsl
	and	r3,r3,
	and	r4,r4,
	and	r5,r5,
	vdup.32	$R0,r2			@ r^1 in both lanes
	add	r2,r3,r3,lsl
	vdup.32	$R1,r3
	add	r3,r4,r4,lsl
	vdup.32	$S1,r2
	vdup.32	$R2,r4
	add	r4,r5,r5,lsl
	vdup.32	$S2,r3
	vdup.32	$R3,r5
	add	r5,r6,r6,lsl
	vdup.32	$S3,r4
	vdup.32	$R4,r6
	vdup.32	$S4,r5
	mov	$zeros,
.Lsquare_neon:
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4
	@ d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	@ d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	@ d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	@ d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	vmull.u32	$D0,$R0,${R0}[1]
	vmull.u32	$D1,$R1,${R0}[1]
	vmull.u32	$D2,$R2,${R0}[1]
	vmull.u32	$D3,$R3,${R0}[1]
	vmull.u32	$D4,$R4,${R0}[1]
	vmlal.u32	$D0,$R4,${S1}[1]
	vmlal.u32	$D1,$R0,${R1}[1]
	vmlal.u32	$D2,$R1,${R1}[1]
	vmlal.u32	$D3,$R2,${R1}[1]
	vmlal.u32	$D4,$R3,${R1}[1]
	vmlal.u32	$D0,$R3,${S2}[1]
	vmlal.u32	$D1,$R4,${S2}[1]
	vmlal.u32	$D3,$R1,${R2}[1]
	vmlal.u32	$D2,$R0,${R2}[1]
	vmlal.u32	$D4,$R2,${R2}[1]
	vmlal.u32	$D0,$R2,${S3}[1]
	vmlal.u32	$D3,$R0,${R3}[1]
	vmlal.u32	$D1,$R3,${S3}[1]
	vmlal.u32	$D2,$R4,${S3}[1]
	vmlal.u32	$D4,$R1,${R3}[1]
	vmlal.u32	$D3,$R4,${S4}[1]
	vmlal.u32	$D0,$R1,${S4}[1]
	vmlal.u32	$D1,$R2,${S4}[1]
	vmlal.u32	$D2,$R3,${S4}[1]
	vmlal.u32	$D4,$R0,${R4}[1]
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ lazy reduction as discussed in "NEON crypto" by D.J. Bernstein
	@ and P. Schwabe
	@
	@ H0>>+H1>>+H2>>+H3>>+H4
	@ H3>>+H4>>*5+H0>>+H1
	@
	@ Trivia.
	@
	@ Result of multiplication of n-bit number by m-bit number is
	@ n+m bits wide. However! Even though 2^n is a n+1-bit number,
	@ m-bit number multiplied by 2^n is still n+m bits wide.
	@
	@ Sum of two n-bit numbers is n+1 bits wide, sum of three - n+2,
	@ and so is sum of four. Sum of 2^m n-m-bit numbers and n-bit
	@ one is n+1 bits wide.
	@
	@ >>+ denotes Hnext += Hn>>26, Hn &= 0x3ffffff. This means that
	@ H0, H2, H3 are guaranteed to be 26 bits wide, while H1 and H4
	@ can be 27. However! In cases when their width exceeds 26 bits
	@ they are limited by 2^26+2^6. This in turn means that *sum*
	@ of the products with these values can still be viewed as sum
	@ of 52-bit numbers as long as the amount of addends is not a
	@ power of 2. For example,
	@
	@ H4 = H4*R0 + H3*R1 + H2*R2 + H1*R3 + H0 * R4,
	@
	@ which can't be larger than 5 * (2^26 + 2^6) * (2^26 + 2^6), or
	@ 5 * (2^52 + 2*2^32 + 2^12), which in turn is smaller than
	@ 8 * (2^52) or 2^55. However, the value is then multiplied by
	@ by 5, so we should be looking at 5 * 5 * (2^52 + 2^33 + 2^12),
	@ which is less than 32 * (2^52) or 2^57. And when processing
	@ data we are looking at triple as many addends...
	@
	@ In key setup procedure pre-reduced H0 is limited by 5*4+1 and
	@ 5*H4 - by 5*5 52-bit addends, or 57 bits. But when hashing the
	@ input H0 is limited by (5*4+1)*3 addends, or 58 bits, while
	@ 5*H4 by 5*5*3, or 59[!] bits. How is this relevant? vmlal.u32
	@ instruction accepts 2x32-bit input and writes 2x64-bit result.
	@ This means that result of reduction have to be compressed upon
	@ loop wrap-around. This can be done in the process of reduction
	@ to minimize amount of instructions [as well as amount of
	@ 128-bit instructions, which benefits low-end processors], but
	@ one has to watch for H2 (which is narrower than H0) and 5*H4
	@ not being wider than 58 bits, so that result of right shift
	@ by 26 bits fits in 32 bits. This is also useful on x86,
	@ because it allows to use paddd in place for paddq, which
	@ benefits Atom, where paddq is ridiculously slow.
	vshr.u64	$T0,$D3,
	vmovn.i64	$D3
	 vshr.u64	$T1,$D0,
	 vmovn.i64	$D0
	vadd.i64	$D4,$D4,$T0		@ h3 -> h4
	vbic.i32	$D3
	 vadd.i64	$D1,$D1,$T1		@ h0 -> h1
	 vbic.i32	$D0
	vshrn.u64	$T0
	vmovn.i64	$D4
	 vshr.u64	$T1,$D1,
	 vmovn.i64	$D1
	 vadd.i64	$D2,$D2,$T1		@ h1 -> h2
	vbic.i32	$D4
	 vbic.i32	$D1
	vadd.i32	$D0
	vshl.u32	$T0
	 vshrn.u64	$T1
	 vmovn.i64	$D2
	vadd.i32	$D0
	 vadd.i32	$D3
	 vbic.i32	$D2
	vshr.u32	$T0
	vbic.i32	$D0
	 vshr.u32	$T1
	 vbic.i32	$D3
	vadd.i32	$D1
	 vadd.i32	$D4
	subs		$zeros,$zeros,
	beq		.Lsquare_break_neon
	add		$tbl0,$ctx,
	add		$tbl1,$ctx,
	vtrn.32		$R0,$D0
	vtrn.32		$R2,$D2
	vtrn.32		$R3,$D3
	vtrn.32		$R1,$D1
	vtrn.32		$R4,$D4
	vshl.u32	$S2,$R2,
	vshl.u32	$S3,$R3,
	vshl.u32	$S1,$R1,
	vshl.u32	$S4,$R4,
	vadd.i32	$S2,$S2,$R2
	vadd.i32	$S1,$S1,$R1
	vadd.i32	$S3,$S3,$R3
	vadd.i32	$S4,$S4,$R4
	vst4.32		{${R0}[0],${R1}[0],${S1}[0],${R2}[0]},[$tbl0]!
	vst4.32		{${R0}[1],${R1}[1],${S1}[1],${R2}[1]},[$tbl1]!
	vst4.32		{${S2}[0],${R3}[0],${S3}[0],${R4}[0]},[$tbl0]!
	vst4.32		{${S2}[1],${R3}[1],${S3}[1],${R4}[1]},[$tbl1]!
	vst1.32		{${S4}[0]},[$tbl0,:32]
	vst1.32		{${S4}[1]},[$tbl1,:32]
	b		.Lsquare_neon
.align	4
.Lsquare_break_neon:
	add		$tbl0,$ctx,
	add		$tbl1,$ctx,
	vmov		$R0,$D0
	vshl.u32	$S1,$D1
	vmov		$R1,$D1
	vshl.u32	$S2,$D2
	vmov		$R2,$D2
	vshl.u32	$S3,$D3
	vmov		$R3,$D3
	vshl.u32	$S4,$D4
	vmov		$R4,$D4
	vadd.i32	$S1,$S1,$D1
	vadd.i32	$S2,$S2,$D2
	vadd.i32	$S3,$S3,$D3
	vadd.i32	$S4,$S4,$D4
	vst4.32		{${R0}[0],${R1}[0],${S1}[0],${R2}[0]},[$tbl0]!
	vst4.32		{${R0}[1],${R1}[1],${S1}[1],${R2}[1]},[$tbl1]!
	vst4.32		{${S2}[0],${R3}[0],${S3}[0],${R4}[0]},[$tbl0]!
	vst4.32		{${S2}[1],${R3}[1],${S3}[1],${R4}[1]},[$tbl1]!
	vst1.32		{${S4}[0]},[$tbl0]
	vst1.32		{${S4}[1]},[$tbl1]
.Lno_init_neon:
	ret				@ bx	lr
.size	poly1305_init_neon,.-poly1305_init_neon
.type	poly1305_blocks_neon,%function
.align	5
poly1305_blocks_neon:
.Lpoly1305_blocks_neon:
	ldr	ip,[$ctx,
	cmp	$len,
	blo	.Lpoly1305_blocks
	stmdb	sp!,{r4-r7}
	vstmdb	sp!,{d8-d15}		@ ABI specification says so
	tst	ip,ip			@ is_base2_26?
	bne	.Lbase2_26_neon
	stmdb	sp!,{r1-r3,lr}
	bl	.Lpoly1305_init_neon
	ldr	r4,[$ctx,
	ldr	r5,[$ctx,
	ldr	r6,[$ctx,
	ldr	r7,[$ctx,
	ldr	ip,[$ctx,
	and	r2,r4,
	mov	r3,r4,lsr
	 veor	$D0
	mov	r4,r5,lsr
	orr	r3,r3,r5,lsl
	 veor	$D1
	mov	r5,r6,lsr
	orr	r4,r4,r6,lsl
	 veor	$D2
	mov	r6,r7,lsr
	orr	r5,r5,r7,lsl
	 veor	$D3
	and	r3,r3,
	orr	r6,r6,ip,lsl
	 veor	$D4
	and	r4,r4,
	mov	r1,
	and	r5,r5,
	str	r1,[$ctx,
	vmov.32	$D0
	vmov.32	$D1
	vmov.32	$D2
	vmov.32	$D3
	vmov.32	$D4
	adr	$zeros,.Lzeros
	ldmia	sp!,{r1-r3,lr}
	b	.Lhash_loaded
.align	4
.Lbase2_26_neon:
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ load hash value
	veor		$D0
	veor		$D1
	veor		$D2
	veor		$D3
	veor		$D4
	vld4.32		{$D0
	adr		$zeros,.Lzeros
	vld1.32		{$D4
	sub		$ctx,$ctx,
.Lhash_loaded:
	add		$in2,$inp,
	mov		$padbit,$padbit,lsl
	tst		$len,
	beq		.Leven
	vld4.32		{$H0
	vmov.32		$H4
	sub		$len,$len,
	add		$in2,$inp,
	vrev32.8	$H0,$H0
	vrev32.8	$H3,$H3
	vrev32.8	$H1,$H1
	vrev32.8	$H2,$H2
	vsri.u32	$H4
	vshl.u32	$H3
	vsri.u32	$H3
	vshl.u32	$H2
	vadd.i32	$H4
	vbic.i32	$H3
	vsri.u32	$H2
	vshl.u32	$H1
	vbic.i32	$H2
	vsri.u32	$H1
	vadd.i32	$H3
	vbic.i32	$H0
	vbic.i32	$H1
	vadd.i32	$H2
	vadd.i32	$H0
	vadd.i32	$H1
	mov		$tbl1,$zeros
	add		$tbl0,$ctx,
	cmp		$len,$len
	b		.Long_tail
.align	4
.Leven:
	subs		$len,$len,
	it		lo
	movlo		$in2,$zeros
	vmov.i32	$H4,
	vld4.32		{$H0
	add		$inp,$inp,
	vld4.32		{$H0
	add		$in2,$in2,
	itt		hi
	addhi		$tbl1,$ctx,
	addhi		$tbl0,$ctx,
	vrev32.8	$H0,$H0
	vrev32.8	$H3,$H3
	vrev32.8	$H1,$H1
	vrev32.8	$H2,$H2
	vsri.u32	$H4,$H3,
	vshl.u32	$H3,$H3,
	vsri.u32	$H3,$H2,
	vshl.u32	$H2,$H2,
	vbic.i32	$H3,
	vsri.u32	$H2,$H1,
	vshl.u32	$H1,$H1,
	vbic.i32	$H2,
	vsri.u32	$H1,$H0,
	vbic.i32	$H0,
	vbic.i32	$H1,
	bls		.Lskip_loop
	vld4.32		{${R0}[1],${R1}[1],${S1}[1],${R2}[1]},[$tbl1]!	@ load r^2
	vld4.32		{${R0}[0],${R1}[0],${S1}[0],${R2}[0]},[$tbl0]!	@ load r^4
	vld4.32		{${S2}[1],${R3}[1],${S3}[1],${R4}[1]},[$tbl1]!
	vld4.32		{${S2}[0],${R3}[0],${S3}[0],${R4}[0]},[$tbl0]!
	b		.Loop_neon
.align	5
.Loop_neon:
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ ((inp[0]*r^4+inp[2]*r^2+inp[4])*r^4+inp[6]*r^2
	@ ((inp[1]*r^4+inp[3]*r^2+inp[5])*r^3+inp[7]*r
	@   \___________________/
	@ ((inp[0]*r^4+inp[2]*r^2+inp[4])*r^4+inp[6]*r^2+inp[8])*r^2
	@ ((inp[1]*r^4+inp[3]*r^2+inp[5])*r^4+inp[7]*r^2+inp[9])*r
	@   \___________________/ \____________________/
	@
	@ Note that we start with inp[2:3]*r^2. This is because it
	@ doesn't depend on reduction in previous iteration.
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ d4 = h4*r0 + h3*r1   + h2*r2   + h1*r3   + h0*r4
	@ d3 = h3*r0 + h2*r1   + h1*r2   + h0*r3   + h4*5*r4
	@ d2 = h2*r0 + h1*r1   + h0*r2   + h4*5*r3 + h3*5*r4
	@ d1 = h1*r0 + h0*r1   + h4*5*r2 + h3*5*r3 + h2*5*r4
	@ d0 = h0*r0 + h4*5*r1 + h3*5*r2 + h2*5*r3 + h1*5*r4
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ inp[2:3]*r^2
	vadd.i32	$H2
	vmull.u32	$D2,$H2
	vadd.i32	$H0
	vmull.u32	$D0,$H0
	vadd.i32	$H3
	vmull.u32	$D3,$H3
	vmlal.u32	$D2,$H1
	vadd.i32	$H1
	vmull.u32	$D1,$H1
	vadd.i32	$H4
	vmull.u32	$D4,$H4
	subs		$len,$len,
	vmlal.u32	$D0,$H4
	it		lo
	movlo		$in2,$zeros
	vmlal.u32	$D3,$H2
	vld1.32		${S4}[1],[$tbl1,:32]
	vmlal.u32	$D1,$H0
	vmlal.u32	$D4,$H3
	vmlal.u32	$D0,$H3
	vmlal.u32	$D3,$H1
	vmlal.u32	$D4,$H2
	vmlal.u32	$D1,$H4
	vmlal.u32	$D2,$H0
	vmlal.u32	$D3,$H0
	vmlal.u32	$D0,$H2
	vmlal.u32	$D4,$H1
	vmlal.u32	$D1,$H3
	vmlal.u32	$D2,$H4
	vmlal.u32	$D3,$H4
	vmlal.u32	$D0,$H1
	vmlal.u32	$D4,$H0
	vmlal.u32	$D1,$H2
	vmlal.u32	$D2,$H3
	vld4.32		{$H0
	add		$in2,$in2,
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ (hash+inp[0:1])*r^4 and accumulate
	vmlal.u32	$D3,$H3
	vmlal.u32	$D0,$H0
	vmlal.u32	$D4,$H4
	vmlal.u32	$D1,$H1
	vmlal.u32	$D2,$H2
	vld1.32		${S4}[0],[$tbl0,:32]
	vmlal.u32	$D3,$H2
	vmlal.u32	$D0,$H4
	vmlal.u32	$D4,$H3
	vmlal.u32	$D1,$H0
	vmlal.u32	$D2,$H1
	vmlal.u32	$D3,$H1
	vmlal.u32	$D0,$H3
	vmlal.u32	$D4,$H2
	vmlal.u32	$D1,$H4
	vmlal.u32	$D2,$H0
	vmlal.u32	$D3,$H0
	vmlal.u32	$D0,$H2
	vmlal.u32	$D4,$H1
	vmlal.u32	$D1,$H3
	vmlal.u32	$D3,$H4
	vmlal.u32	$D2,$H4
	vmlal.u32	$D0,$H1
	vmlal.u32	$D4,$H0
	vmov.i32	$H4,
	vmlal.u32	$D1,$H2
	vmlal.u32	$D2,$H3
	vld4.32		{$H0
	add		$inp,$inp,
	vrev32.8	$H0,$H0
	vrev32.8	$H1,$H1
	vrev32.8	$H2,$H2
	vrev32.8	$H3,$H3
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ lazy reduction interleaved with base 2^32 -> base 2^26 of
	@ inp[0:3] previously loaded to $H0-$H3 and smashed to $H0-$H4.
	vshr.u64	$T0,$D3,
	vmovn.i64	$D3
	 vshr.u64	$T1,$D0,
	 vmovn.i64	$D0
	vadd.i64	$D4,$D4,$T0		@ h3 -> h4
	vbic.i32	$D3
	  vsri.u32	$H4,$H3,
	 vadd.i64	$D1,$D1,$T1		@ h0 -> h1
	  vshl.u32	$H3,$H3,
	 vbic.i32	$D0
	vshrn.u64	$T0
	vmovn.i64	$D4
	 vshr.u64	$T1,$D1,
	 vmovn.i64	$D1
	 vadd.i64	$D2,$D2,$T1		@ h1 -> h2
	  vsri.u32	$H3,$H2,
	vbic.i32	$D4
	  vshl.u32	$H2,$H2,
	 vbic.i32	$D1
	vadd.i32	$D0
	vshl.u32	$T0
	  vbic.i32	$H3,
	 vshrn.u64	$T1
	 vmovn.i64	$D2
	vaddl.u32	$D0,$D0
	  vsri.u32	$H2,$H1,
	 vadd.i32	$D3
	  vshl.u32	$H1,$H1,
	 vbic.i32	$D2
	  vbic.i32	$H2,
	vshrn.u64	$T0
	vmovn.i64	$D0
	  vsri.u32	$H1,$H0,
	  vbic.i32	$H0,
	 vshr.u32	$T1
	 vbic.i32	$D3
	vbic.i32	$D0
	vadd.i32	$D1
	 vadd.i32	$D4
	  vbic.i32	$H1,
	bhi		.Loop_neon
.Lskip_loop:
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ multiply (inp[0:1]+hash) or inp[2:3] by r^2:r^1
	add		$tbl1,$ctx,
	add		$tbl0,$ctx,
	adds		$len,$len,
	it		ne
	movne		$len,
	bne		.Long_tail
	vadd.i32	$H2
	vadd.i32	$H0
	vadd.i32	$H3
	vadd.i32	$H1
	vadd.i32	$H4
.Long_tail:
	vld4.32		{${R0}[1],${R1}[1],${S1}[1],${R2}[1]},[$tbl1]!	@ load r^1
	vld4.32		{${R0}[0],${R1}[0],${S1}[0],${R2}[0]},[$tbl0]!	@ load r^2
	vadd.i32	$H2
	vmull.u32	$D2,$H2
	vadd.i32	$H0
	vmull.u32	$D0,$H0
	vadd.i32	$H3
	vmull.u32	$D3,$H3
	vadd.i32	$H1
	vmull.u32	$D1,$H1
	vadd.i32	$H4
	vmull.u32	$D4,$H4
	vmlal.u32	$D0,$H4
	vld4.32		{${S2}[1],${R3}[1],${S3}[1],${R4}[1]},[$tbl1]!
	vmlal.u32	$D3,$H2
	vld4.32		{${S2}[0],${R3}[0],${S3}[0],${R4}[0]},[$tbl0]!
	vmlal.u32	$D1,$H0
	vmlal.u32	$D4,$H3
	vmlal.u32	$D2,$H1
	vmlal.u32	$D3,$H1
	vld1.32		${S4}[1],[$tbl1,:32]
	vmlal.u32	$D0,$H3
	vld1.32		${S4}[0],[$tbl0,:32]
	vmlal.u32	$D4,$H2
	vmlal.u32	$D1,$H4
	vmlal.u32	$D2,$H0
	vmlal.u32	$D3,$H0
	 it		ne
	 addne		$tbl1,$ctx,
	vmlal.u32	$D0,$H2
	 it		ne
	 addne		$tbl0,$ctx,
	vmlal.u32	$D4,$H1
	vmlal.u32	$D1,$H3
	vmlal.u32	$D2,$H4
	vmlal.u32	$D3,$H4
	 vorn		$MASK,$MASK,$MASK	@ all-ones, can be redundant
	vmlal.u32	$D0,$H1
	 vshr.u64	$MASK,$MASK,
	vmlal.u32	$D4,$H0
	vmlal.u32	$D1,$H2
	vmlal.u32	$D2,$H3
	beq		.Lshort_tail
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ (hash+inp[0:1])*r^4:r^3 and accumulate
	vld4.32		{${R0}[1],${R1}[1],${S1}[1],${R2}[1]},[$tbl1]!	@ load r^3
	vld4.32		{${R0}[0],${R1}[0],${S1}[0],${R2}[0]},[$tbl0]!	@ load r^4
	vmlal.u32	$D2,$H2
	vmlal.u32	$D0,$H0
	vmlal.u32	$D3,$H3
	vmlal.u32	$D1,$H1
	vmlal.u32	$D4,$H4
	vmlal.u32	$D0,$H4
	vld4.32		{${S2}[1],${R3}[1],${S3}[1],${R4}[1]},[$tbl1]!
	vmlal.u32	$D3,$H2
	vld4.32		{${S2}[0],${R3}[0],${S3}[0],${R4}[0]},[$tbl0]!
	vmlal.u32	$D1,$H0
	vmlal.u32	$D4,$H3
	vmlal.u32	$D2,$H1
	vmlal.u32	$D3,$H1
	vld1.32		${S4}[1],[$tbl1,:32]
	vmlal.u32	$D0,$H3
	vld1.32		${S4}[0],[$tbl0,:32]
	vmlal.u32	$D4,$H2
	vmlal.u32	$D1,$H4
	vmlal.u32	$D2,$H0
	vmlal.u32	$D3,$H0
	vmlal.u32	$D0,$H2
	vmlal.u32	$D4,$H1
	vmlal.u32	$D1,$H3
	vmlal.u32	$D2,$H4
	vmlal.u32	$D3,$H4
	 vorn		$MASK,$MASK,$MASK	@ all-ones
	vmlal.u32	$D0,$H1
	 vshr.u64	$MASK,$MASK,
	vmlal.u32	$D4,$H0
	vmlal.u32	$D1,$H2
	vmlal.u32	$D2,$H3
.Lshort_tail:
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ horizontal addition
	vadd.i64	$D3
	vadd.i64	$D0
	vadd.i64	$D4
	vadd.i64	$D1
	vadd.i64	$D2
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ lazy reduction, but without narrowing
	vshr.u64	$T0,$D3,
	vand.i64	$D3,$D3,$MASK
	 vshr.u64	$T1,$D0,
	 vand.i64	$D0,$D0,$MASK
	vadd.i64	$D4,$D4,$T0		@ h3 -> h4
	 vadd.i64	$D1,$D1,$T1		@ h0 -> h1
	vshr.u64	$T0,$D4,
	vand.i64	$D4,$D4,$MASK
	 vshr.u64	$T1,$D1,
	 vand.i64	$D1,$D1,$MASK
	 vadd.i64	$D2,$D2,$T1		@ h1 -> h2
	vadd.i64	$D0,$D0,$T0
	vshl.u64	$T0,$T0,
	 vshr.u64	$T1,$D2,
	 vand.i64	$D2,$D2,$MASK
	vadd.i64	$D0,$D0,$T0		@ h4 -> h0
	 vadd.i64	$D3,$D3,$T1		@ h2 -> h3
	vshr.u64	$T0,$D0,
	vand.i64	$D0,$D0,$MASK
	 vshr.u64	$T1,$D3,
	 vand.i64	$D3,$D3,$MASK
	vadd.i64	$D1,$D1,$T0		@ h0 -> h1
	 vadd.i64	$D4,$D4,$T1		@ h3 -> h4
	cmp		$len,
	bne		.Leven
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@ store hash value
	vst4.32		{$D0
	vst1.32		{$D4
	vldmia	sp!,{d8-d15}			@ epilogue
	ldmia	sp!,{r4-r7}
	ret					@ bx	lr
.size	poly1305_blocks_neon,.-poly1305_blocks_neon
.align	5
.Lzeros:
.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
.LOPENSSL_armcap:
.word	OPENSSL_armcap_P
.word	OPENSSL_armcap_P-.Lpoly1305_init
.comm	OPENSSL_armcap_P,4,4
.hidden	OPENSSL_armcap_P
___
}	}
$code.=<<___;
.asciz	"Poly1305 for ARMv4/NEON, CRYPTOGAMS by \@dot-asm"
.align	2
___
foreach (split("\n",$code)) {
	s/\`([^\`]*)\`/eval $1/geo;
	s/\bq([0-9]+)
	s/\bret\b/bx	lr/go						or
	s/\bbx\s+lr\b/.word\t0xe12fff1e/go;	
	print $_,"\n";
}
close STDOUT; 
