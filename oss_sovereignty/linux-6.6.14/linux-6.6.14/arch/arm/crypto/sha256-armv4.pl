while (($output=shift) && ($output!~/^\w[\w\-]*\.\w+$/)) {}
open STDOUT,">$output";
$ctx="r0";	$t0="r0";
$inp="r1";	$t4="r1";
$len="r2";	$t1="r2";
$T1="r3";	$t3="r3";
$A="r4";
$B="r5";
$C="r6";
$D="r7";
$E="r8";
$F="r9";
$G="r10";
$H="r11";
@V=($A,$B,$C,$D,$E,$F,$G,$H);
$t2="r12";
$Ktbl="r14";
@Sigma0=( 2,13,22);
@Sigma1=( 6,11,25);
@sigma0=( 7,18, 3);
@sigma1=(17,19,10);
sub BODY_00_15 {
my ($i,$a,$b,$c,$d,$e,$f,$g,$h) = @_;
$code.=<<___ if ($i<16);
	@ ldr	$t1,[$inp],
	str	$inp,[sp,
	eor	$t0,$e,$e,ror
	add	$a,$a,$t2			@ h+=Maj(a,b,c) from the past
	eor	$t0,$t0,$e,ror
	rev	$t1,$t1
	@ ldrb	$t1,[$inp,
	add	$a,$a,$t2			@ h+=Maj(a,b,c) from the past
	ldrb	$t2,[$inp,
	ldrb	$t0,[$inp,
	orr	$t1,$t1,$t2,lsl
	ldrb	$t2,[$inp],
	orr	$t1,$t1,$t0,lsl
	str	$inp,[sp,
	eor	$t0,$e,$e,ror
	orr	$t1,$t1,$t2,lsl
	eor	$t0,$t0,$e,ror
___
$code.=<<___;
	ldr	$t2,[$Ktbl],
	add	$h,$h,$t1			@ h+=X[i]
	str	$t1,[sp,
	eor	$t1,$f,$g
	add	$h,$h,$t0,ror
	and	$t1,$t1,$e
	add	$h,$h,$t2			@ h+=K256[i]
	eor	$t1,$t1,$g			@ Ch(e,f,g)
	eor	$t0,$a,$a,ror
	add	$h,$h,$t1			@ h+=Ch(e,f,g)
	and	$t2,$t2,
	cmp	$t2,
	ldr	$t1,[$inp],
	ldrb	$t1,[$inp,
	eor	$t2,$a,$b			@ a^b, b^c in next round
	ldr	$t1,[sp,
	eor	$t2,$a,$b			@ a^b, b^c in next round
	ldr	$t4,[sp,
	eor	$t0,$t0,$a,ror
	and	$t3,$t3,$t2			@ (b^c)&=(a^b)
	add	$d,$d,$h			@ d+=h
	eor	$t3,$t3,$b			@ Maj(a,b,c)
	add	$h,$h,$t0,ror
	@ add	$h,$h,$t3			@ h+=Maj(a,b,c)
___
	($t2,$t3)=($t3,$t2);
}
sub BODY_16_XX {
my ($i,$a,$b,$c,$d,$e,$f,$g,$h) = @_;
$code.=<<___;
	@ ldr	$t1,[sp,
	@ ldr	$t4,[sp,
	mov	$t0,$t1,ror
	add	$a,$a,$t2			@ h+=Maj(a,b,c) from the past
	mov	$t2,$t4,ror
	eor	$t0,$t0,$t1,ror
	eor	$t2,$t2,$t4,ror
	eor	$t0,$t0,$t1,lsr
	ldr	$t1,[sp,
	eor	$t2,$t2,$t4,lsr
	ldr	$t4,[sp,
	add	$t2,$t2,$t0
	eor	$t0,$e,$e,ror
	add	$t1,$t1,$t2
	eor	$t0,$t0,$e,ror
	add	$t1,$t1,$t4			@ X[i]
___
	&BODY_00_15(@_);
}
$code=<<___;
.text
.code	32
.syntax unified
.thumb
.code   32
.type	K256,%object
.align	5
K256:
.word	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5
.word	0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5
.word	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3
.word	0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174
.word	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc
.word	0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da
.word	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7
.word	0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967
.word	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13
.word	0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85
.word	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3
.word	0xd192e819,0xd6990624,0xf40e3585,0x106aa070
.word	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5
.word	0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3
.word	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208
.word	0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
.size	K256,.-K256
.word	0				@ terminator
.LOPENSSL_armcap:
.word	OPENSSL_armcap_P-sha256_block_data_order
.align	5
.global	sha256_block_data_order
.type	sha256_block_data_order,%function
sha256_block_data_order:
.Lsha256_block_data_order:
	sub	r3,pc,
	adr	r3,.Lsha256_block_data_order
	ldr	r12,.LOPENSSL_armcap
	ldr	r12,[r3,r12]		@ OPENSSL_armcap_P
	tst	r12,
	bne	.LARMv8
	tst	r12,
	bne	.LNEON
	add	$len,$inp,$len,lsl
	stmdb	sp!,{$ctx,$inp,$len,r4-r11,lr}
	ldmia	$ctx,{$A,$B,$C,$D,$E,$F,$G,$H}
	sub	$Ktbl,r3,
	sub	sp,sp,
.Loop:
	ldr	$t1,[$inp],
	ldrb	$t1,[$inp,
	eor	$t3,$B,$C		@ magic
	eor	$t2,$t2,$t2
___
for($i=0;$i<16;$i++)	{ &BODY_00_15($i,@V); unshift(@V,pop(@V)); }
$code.=".Lrounds_16_xx:\n";
for (;$i<32;$i++)	{ &BODY_16_XX($i,@V); unshift(@V,pop(@V)); }
$code.=<<___;
	ite	eq			@ Thumb2 thing, sanity check in ARM
	ldreq	$t3,[sp,
	bne	.Lrounds_16_xx
	add	$A,$A,$t2		@ h+=Maj(a,b,c) from the past
	ldr	$t0,[$t3,
	ldr	$t1,[$t3,
	ldr	$t2,[$t3,
	add	$A,$A,$t0
	ldr	$t0,[$t3,
	add	$B,$B,$t1
	ldr	$t1,[$t3,
	add	$C,$C,$t2
	ldr	$t2,[$t3,
	add	$D,$D,$t0
	ldr	$t0,[$t3,
	add	$E,$E,$t1
	ldr	$t1,[$t3,
	add	$F,$F,$t2
	ldr	$inp,[sp,
	ldr	$t2,[sp,
	add	$G,$G,$t0
	add	$H,$H,$t1
	stmia	$t3,{$A,$B,$C,$D,$E,$F,$G,$H}
	cmp	$inp,$t2
	sub	$Ktbl,$Ktbl,
	bne	.Loop
	add	sp,sp,
	ldmia	sp!,{r4-r11,pc}
	ldmia	sp!,{r4-r11,lr}
	tst	lr,
	moveq	pc,lr			@ be binary compatible with V4, yet
	bx	lr			@ interoperable with Thumb ISA:-)
.size	sha256_block_data_order,.-sha256_block_data_order
___
{{{
my @X=map("q$_",(0..3));
my ($T0,$T1,$T2,$T3,$T4,$T5)=("q8","q9","q10","q11","d24","d25");
my $Xfer=$t4;
my $j=0;
sub Dlo()   { shift=~m|q([1]?[0-9])|?"d".($1*2):"";     }
sub Dhi()   { shift=~m|q([1]?[0-9])|?"d".($1*2+1):"";   }
sub AUTOLOAD()          
{ my $opcode = $AUTOLOAD; $opcode =~ s/.*:://; $opcode =~ s/_/\./;
  my $arg = pop;
    $arg = "#$arg" if ($arg*1 eq $arg);
    $code .= "\t$opcode\t".join(',',@_,$arg)."\n";
}
sub Xupdate()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);
  my ($a,$b,$c,$d,$e,$f,$g,$h);
	&vext_8		($T0,@X[0],@X[1],4);	
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vext_8		($T1,@X[2],@X[3],4);	
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vshr_u32	($T2,$T0,$sigma0[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vadd_i32	(@X[0],@X[0],$T1);	
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vshr_u32	($T1,$T0,$sigma0[2]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vsli_32	($T2,$T0,32-$sigma0[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vshr_u32	($T3,$T0,$sigma0[1]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&veor		($T1,$T1,$T2);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vsli_32	($T3,$T0,32-$sigma0[1]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vshr_u32	($T4,&Dhi(@X[3]),$sigma1[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&veor		($T1,$T1,$T3);		
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vsli_32	($T4,&Dhi(@X[3]),32-$sigma1[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vshr_u32	($T5,&Dhi(@X[3]),$sigma1[2]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vadd_i32	(@X[0],@X[0],$T1);	
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &veor		($T5,$T5,$T4);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vshr_u32	($T4,&Dhi(@X[3]),$sigma1[1]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vsli_32	($T4,&Dhi(@X[3]),32-$sigma1[1]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &veor		($T5,$T5,$T4);		
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vadd_i32	(&Dlo(@X[0]),&Dlo(@X[0]),$T5);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vshr_u32	($T4,&Dlo(@X[0]),$sigma1[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vsli_32	($T4,&Dlo(@X[0]),32-$sigma1[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vshr_u32	($T5,&Dlo(@X[0]),$sigma1[2]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &veor		($T5,$T5,$T4);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vshr_u32	($T4,&Dlo(@X[0]),$sigma1[1]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vld1_32	("{$T0}","[$Ktbl,:128]!");
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &vsli_32	($T4,&Dlo(@X[0]),32-$sigma1[1]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	  &veor		($T5,$T5,$T4);		
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vadd_i32	(&Dhi(@X[0]),&Dhi(@X[0]),$T5);
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vadd_i32	($T0,$T0,@X[0]);
	 while($
	&vst1_32	("{$T0}","[$Xfer,:128]!");
	 eval(shift(@insns));
	 eval(shift(@insns));
	push(@X,shift(@X));		
}
sub Xpreload()
{ use integer;
  my $body = shift;
  my @insns = (&$body,&$body,&$body,&$body);
  my ($a,$b,$c,$d,$e,$f,$g,$h);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vld1_32	("{$T0}","[$Ktbl,:128]!");
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vrev32_8	(@X[0],@X[0]);
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	 eval(shift(@insns));
	&vadd_i32	($T0,$T0,@X[0]);
	 foreach (@insns) { eval; }	
	&vst1_32	("{$T0}","[$Xfer,:128]!");
	push(@X,shift(@X));		
}
sub body_00_15 () {
	(
	'($a,$b,$c,$d,$e,$f,$g,$h)=@V;'.
	'&add	($h,$h,$t1)',			
	'&eor	($t1,$f,$g)',
	'&eor	($t0,$e,$e,"ror#".($Sigma1[1]-$Sigma1[0]))',
	'&add	($a,$a,$t2)',			
	'&and	($t1,$t1,$e)',
	'&eor	($t2,$t0,$e,"ror#".($Sigma1[2]-$Sigma1[0]))',	
	'&eor	($t0,$a,$a,"ror#".($Sigma0[1]-$Sigma0[0]))',
	'&eor	($t1,$t1,$g)',			
	'&add	($h,$h,$t2,"ror#$Sigma1[0]")',	
	'&eor	($t2,$a,$b)',			
	'&eor	($t0,$t0,$a,"ror#".($Sigma0[2]-$Sigma0[0]))',	
	'&add	($h,$h,$t1)',			
	'&ldr	($t1,sprintf "[sp,#%d]",4*(($j+1)&15))	if (($j&15)!=15);'.
	'&ldr	($t1,"[$Ktbl]")				if ($j==15);'.
	'&ldr	($t1,"[sp,#64]")			if ($j==31)',
	'&and	($t3,$t3,$t2)',			
	'&add	($d,$d,$h)',			
	'&add	($h,$h,$t0,"ror#$Sigma0[0]");'.	
	'&eor	($t3,$t3,$b)',			
	'$j++;	unshift(@V,pop(@V)); ($t2,$t3)=($t3,$t2);'
	)
}
$code.=<<___;
.arch	armv7-a
.fpu	neon
.global	sha256_block_data_order_neon
.type	sha256_block_data_order_neon,%function
.align	4
sha256_block_data_order_neon:
.LNEON:
	stmdb	sp!,{r4-r12,lr}
	sub	$H,sp,
	adr	$Ktbl,.Lsha256_block_data_order
	sub	$Ktbl,$Ktbl,
	bic	$H,$H,
	mov	$t2,sp
	mov	sp,$H			@ alloca
	add	$len,$inp,$len,lsl
	vld1.8		{@X[0]},[$inp]!
	vld1.8		{@X[1]},[$inp]!
	vld1.8		{@X[2]},[$inp]!
	vld1.8		{@X[3]},[$inp]!
	vld1.32		{$T0},[$Ktbl,:128]!
	vld1.32		{$T1},[$Ktbl,:128]!
	vld1.32		{$T2},[$Ktbl,:128]!
	vld1.32		{$T3},[$Ktbl,:128]!
	vrev32.8	@X[0],@X[0]		@ yes, even on
	str		$ctx,[sp,
	vrev32.8	@X[1],@X[1]		@ big-endian
	str		$inp,[sp,
	mov		$Xfer,sp
	vrev32.8	@X[2],@X[2]
	str		$len,[sp,
	vrev32.8	@X[3],@X[3]
	str		$t2,[sp,
	vadd.i32	$T0,$T0,@X[0]
	vadd.i32	$T1,$T1,@X[1]
	vst1.32		{$T0},[$Xfer,:128]!
	vadd.i32	$T2,$T2,@X[2]
	vst1.32		{$T1},[$Xfer,:128]!
	vadd.i32	$T3,$T3,@X[3]
	vst1.32		{$T2},[$Xfer,:128]!
	vst1.32		{$T3},[$Xfer,:128]!
	ldmia		$ctx,{$A-$H}
	sub		$Xfer,$Xfer,
	ldr		$t1,[sp,
	eor		$t2,$t2,$t2
	eor		$t3,$B,$C
	b		.L_00_48
.align	4
.L_00_48:
___
	&Xupdate(\&body_00_15);
	&Xupdate(\&body_00_15);
	&Xupdate(\&body_00_15);
	&Xupdate(\&body_00_15);
$code.=<<___;
	teq	$t1,
	ldr	$t1,[sp,
	sub	$Xfer,$Xfer,
	bne	.L_00_48
	ldr		$inp,[sp,
	ldr		$t0,[sp,
	sub		$Ktbl,$Ktbl,
	teq		$inp,$t0
	it		eq
	subeq		$inp,$inp,
	vld1.8		{@X[0]},[$inp]!		@ load next input block
	vld1.8		{@X[1]},[$inp]!
	vld1.8		{@X[2]},[$inp]!
	vld1.8		{@X[3]},[$inp]!
	it		ne
	strne		$inp,[sp,
	mov		$Xfer,sp
___
	&Xpreload(\&body_00_15);
	&Xpreload(\&body_00_15);
	&Xpreload(\&body_00_15);
	&Xpreload(\&body_00_15);
$code.=<<___;
	ldr	$t0,[$t1,
	add	$A,$A,$t2			@ h+=Maj(a,b,c) from the past
	ldr	$t2,[$t1,
	ldr	$t3,[$t1,
	ldr	$t4,[$t1,
	add	$A,$A,$t0			@ accumulate
	ldr	$t0,[$t1,
	add	$B,$B,$t2
	ldr	$t2,[$t1,
	add	$C,$C,$t3
	ldr	$t3,[$t1,
	add	$D,$D,$t4
	ldr	$t4,[$t1,
	add	$E,$E,$t0
	str	$A,[$t1],
	add	$F,$F,$t2
	str	$B,[$t1],
	add	$G,$G,$t3
	str	$C,[$t1],
	add	$H,$H,$t4
	str	$D,[$t1],
	stmia	$t1,{$E-$H}
	ittte	ne
	movne	$Xfer,sp
	ldrne	$t1,[sp,
	eorne	$t2,$t2,$t2
	ldreq	sp,[sp,
	itt	ne
	eorne	$t3,$B,$C
	bne	.L_00_48
	ldmia	sp!,{r4-r12,pc}
.size	sha256_block_data_order_neon,.-sha256_block_data_order_neon
___
}}}
{{{
my ($ABCD,$EFGH,$abcd)=map("q$_",(0..2));
my @MSG=map("q$_",(8..11));
my ($W0,$W1,$ABCD_SAVE,$EFGH_SAVE)=map("q$_",(12..15));
my $Ktbl="r3";
$code.=<<___;
.type	sha256_block_data_order_armv8,%function
.align	5
sha256_block_data_order_armv8:
.LARMv8:
	vld1.32	{$ABCD,$EFGH},[$ctx]
	adr	$Ktbl,.LARMv8
	sub	$Ktbl,$Ktbl,
	adrl	$Ktbl,K256
	add	$len,$inp,$len,lsl
.Loop_v8:
	vld1.8		{@MSG[0]-@MSG[1]},[$inp]!
	vld1.8		{@MSG[2]-@MSG[3]},[$inp]!
	vld1.32		{$W0},[$Ktbl]!
	vrev32.8	@MSG[0],@MSG[0]
	vrev32.8	@MSG[1],@MSG[1]
	vrev32.8	@MSG[2],@MSG[2]
	vrev32.8	@MSG[3],@MSG[3]
	vmov		$ABCD_SAVE,$ABCD	@ offload
	vmov		$EFGH_SAVE,$EFGH
	teq		$inp,$len
___
for($i=0;$i<12;$i++) {
$code.=<<___;
	vld1.32		{$W1},[$Ktbl]!
	vadd.i32	$W0,$W0,@MSG[0]
	sha256su0	@MSG[0],@MSG[1]
	vmov		$abcd,$ABCD
	sha256h		$ABCD,$EFGH,$W0
	sha256h2	$EFGH,$abcd,$W0
	sha256su1	@MSG[0],@MSG[2],@MSG[3]
___
	($W0,$W1)=($W1,$W0);	push(@MSG,shift(@MSG));
}
$code.=<<___;
	vld1.32		{$W1},[$Ktbl]!
	vadd.i32	$W0,$W0,@MSG[0]
	vmov		$abcd,$ABCD
	sha256h		$ABCD,$EFGH,$W0
	sha256h2	$EFGH,$abcd,$W0
	vld1.32		{$W0},[$Ktbl]!
	vadd.i32	$W1,$W1,@MSG[1]
	vmov		$abcd,$ABCD
	sha256h		$ABCD,$EFGH,$W1
	sha256h2	$EFGH,$abcd,$W1
	vld1.32		{$W1},[$Ktbl]
	vadd.i32	$W0,$W0,@MSG[2]
	sub		$Ktbl,$Ktbl,
	vmov		$abcd,$ABCD
	sha256h		$ABCD,$EFGH,$W0
	sha256h2	$EFGH,$abcd,$W0
	vadd.i32	$W1,$W1,@MSG[3]
	vmov		$abcd,$ABCD
	sha256h		$ABCD,$EFGH,$W1
	sha256h2	$EFGH,$abcd,$W1
	vadd.i32	$ABCD,$ABCD,$ABCD_SAVE
	vadd.i32	$EFGH,$EFGH,$EFGH_SAVE
	it		ne
	bne		.Loop_v8
	vst1.32		{$ABCD,$EFGH},[$ctx]
	ret		@ bx lr
.size	sha256_block_data_order_armv8,.-sha256_block_data_order_armv8
___
}}}
$code.=<<___;
.asciz  "SHA256 block transform for ARMv4/NEON/ARMv8, CRYPTOGAMS by <appro\@openssl.org>"
.align	2
.comm   OPENSSL_armcap_P,4,4
___
open SELF,$0;
while(<SELF>) {
	next if (/^
	last if (!s/^
	print;
}
close SELF;
{   my  %opcode = (
	"sha256h"	=> 0xf3000c40,	"sha256h2"	=> 0xf3100c40,
	"sha256su0"	=> 0xf3ba03c0,	"sha256su1"	=> 0xf3200c40	);
    sub unsha256 {
	my ($mnemonic,$arg)=@_;
	if ($arg =~ m/q([0-9]+)(?:,\s*q([0-9]+))?,\s*q([0-9]+)/o) {
	    my $word = $opcode{$mnemonic}|(($1&7)<<13)|(($1&8)<<19)
					 |(($2&7)<<17)|(($2&8)<<4)
					 |(($3&7)<<1) |(($3&8)<<2);
	    sprintf "INST(0x%02x,0x%02x,0x%02x,0x%02x)\t@ %s %s",
			$word&0xff,($word>>8)&0xff,
			($word>>16)&0xff,($word>>24)&0xff,
			$mnemonic,$arg;
	}
    }
}
foreach (split($/,$code)) {
	s/\`([^\`]*)\`/eval $1/geo;
	s/\b(sha256\w+)\s+(q.*)/unsha256($1,$2)/geo;
	s/\bret\b/bx	lr/go		or
	s/\bbx\s+lr\b/.word\t0xe12fff1e/go;	
	print $_,"\n";
}
close STDOUT; 
