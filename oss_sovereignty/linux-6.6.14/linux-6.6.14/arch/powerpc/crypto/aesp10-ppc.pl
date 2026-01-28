$flavour = shift;
if ($flavour =~ /64/) {
	$SIZE_T	=8;
	$LRSAVE	=2*$SIZE_T;
	$STU	="stdu";
	$POP	="ld";
	$PUSH	="std";
	$UCMP	="cmpld";
	$SHL	="sldi";
} elsif ($flavour =~ /32/) {
	$SIZE_T	=4;
	$LRSAVE	=$SIZE_T;
	$STU	="stwu";
	$POP	="lwz";
	$PUSH	="stw";
	$UCMP	="cmplw";
	$SHL	="slwi";
} else { die "nonsense $flavour"; }
$LITTLE_ENDIAN = ($flavour=~/le$/) ? $SIZE_T : 0;
$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}ppc-xlate.pl" and -f $xlate ) or
( $xlate="${dir}../../perlasm/ppc-xlate.pl" and -f $xlate) or
die "can't locate ppc-xlate.pl";
open STDOUT,"| $^X $xlate $flavour ".shift || die "can't call $xlate: $!";
$FRAME=8*$SIZE_T;
$prefix="aes_p10";
$sp="r1";
$vrsave="r12";
{{{	
my ($inp,$bits,$out,$ptr,$cnt,$rounds)=map("r$_",(3..8));
my ($zero,$in0,$in1,$key,$rcon,$mask,$tmp)=map("v$_",(0..6));
my ($stage,$outperm,$outmask,$outhead,$outtail)=map("v$_",(7..11));
$code.=<<___;
.machine	"any"
.text
.align	7
rcon:
.long	0x01000000, 0x01000000, 0x01000000, 0x01000000	?rev
.long	0x1b000000, 0x1b000000, 0x1b000000, 0x1b000000	?rev
.long	0x0d0e0f0c, 0x0d0e0f0c, 0x0d0e0f0c, 0x0d0e0f0c	?rev
.long	0,0,0,0						?asis
Lconsts:
	mflr	r0
	bcl	20,31,\$+4
	mflr	$ptr	 
	addi	$ptr,$ptr,-0x48
	mtlr	r0
	blr
	.long	0
	.byte	0,12,0x14,0,0,0,0,0
.asciz	"AES for PowerISA 2.07, CRYPTOGAMS by <appro\@openssl.org>"
.globl	.${prefix}_set_encrypt_key
Lset_encrypt_key:
	mflr		r11
	$PUSH		r11,$LRSAVE($sp)
	li		$ptr,-1
	${UCMP}i	$inp,0
	beq-		Lenc_key_abort		
	${UCMP}i	$out,0
	beq-		Lenc_key_abort		
	li		$ptr,-2
	cmpwi		$bits,128
	blt-		Lenc_key_abort
	cmpwi		$bits,256
	bgt-		Lenc_key_abort
	andi.		r0,$bits,0x3f
	bne-		Lenc_key_abort
	lis		r0,0xfff0
	mfspr		$vrsave,256
	mtspr		256,r0
	bl		Lconsts
	mtlr		r11
	neg		r9,$inp
	lvx		$in0,0,$inp
	addi		$inp,$inp,15		
	lvsr		$key,0,r9		
	li		r8,0x20
	cmpwi		$bits,192
	lvx		$in1,0,$inp
	le?vspltisb	$mask,0x0f		
	lvx		$rcon,0,$ptr
	le?vxor		$key,$key,$mask		
	lvx		$mask,r8,$ptr
	addi		$ptr,$ptr,0x10
	vperm		$in0,$in0,$in1,$key	
	li		$cnt,8
	vxor		$zero,$zero,$zero
	mtctr		$cnt
	?lvsr		$outperm,0,$out
	vspltisb	$outmask,-1
	lvx		$outhead,0,$out
	?vperm		$outmask,$zero,$outmask,$outperm
	blt		Loop128
	addi		$inp,$inp,8
	beq		L192
	addi		$inp,$inp,8
	b		L256
.align	4
Loop128:
	vperm		$key,$in0,$in0,$mask	
	vsldoi		$tmp,$zero,$in0,12	
	 vperm		$outtail,$in0,$in0,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	vcipherlast	$key,$key,$rcon
	 stvx		$stage,0,$out
	 addi		$out,$out,16
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	 vadduwm	$rcon,$rcon,$rcon
	vxor		$in0,$in0,$key
	bdnz		Loop128
	lvx		$rcon,0,$ptr		
	vperm		$key,$in0,$in0,$mask	
	vsldoi		$tmp,$zero,$in0,12	
	 vperm		$outtail,$in0,$in0,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	vcipherlast	$key,$key,$rcon
	 stvx		$stage,0,$out
	 addi		$out,$out,16
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	 vadduwm	$rcon,$rcon,$rcon
	vxor		$in0,$in0,$key
	vperm		$key,$in0,$in0,$mask	
	vsldoi		$tmp,$zero,$in0,12	
	 vperm		$outtail,$in0,$in0,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	vcipherlast	$key,$key,$rcon
	 stvx		$stage,0,$out
	 addi		$out,$out,16
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	vxor		$in0,$in0,$key
	 vperm		$outtail,$in0,$in0,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	 stvx		$stage,0,$out
	addi		$inp,$out,15		
	addi		$out,$out,0x50
	li		$rounds,10
	b		Ldone
.align	4
L192:
	lvx		$tmp,0,$inp
	li		$cnt,4
	 vperm		$outtail,$in0,$in0,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	 stvx		$stage,0,$out
	 addi		$out,$out,16
	vperm		$in1,$in1,$tmp,$key	
	vspltisb	$key,8			
	mtctr		$cnt
	vsububm		$mask,$mask,$key	
Loop192:
	vperm		$key,$in1,$in1,$mask	
	vsldoi		$tmp,$zero,$in0,12	
	vcipherlast	$key,$key,$rcon
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	 vsldoi		$stage,$zero,$in1,8
	vspltw		$tmp,$in0,3
	vxor		$tmp,$tmp,$in1
	vsldoi		$in1,$zero,$in1,12	
	 vadduwm	$rcon,$rcon,$rcon
	vxor		$in1,$in1,$tmp
	vxor		$in0,$in0,$key
	vxor		$in1,$in1,$key
	 vsldoi		$stage,$stage,$in0,8
	vperm		$key,$in1,$in1,$mask	
	vsldoi		$tmp,$zero,$in0,12	
	 vperm		$outtail,$stage,$stage,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	vcipherlast	$key,$key,$rcon
	 stvx		$stage,0,$out
	 addi		$out,$out,16
	 vsldoi		$stage,$in0,$in1,8
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	 vperm		$outtail,$stage,$stage,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	 stvx		$stage,0,$out
	 addi		$out,$out,16
	vspltw		$tmp,$in0,3
	vxor		$tmp,$tmp,$in1
	vsldoi		$in1,$zero,$in1,12	
	 vadduwm	$rcon,$rcon,$rcon
	vxor		$in1,$in1,$tmp
	vxor		$in0,$in0,$key
	vxor		$in1,$in1,$key
	 vperm		$outtail,$in0,$in0,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	 stvx		$stage,0,$out
	 addi		$inp,$out,15		
	 addi		$out,$out,16
	bdnz		Loop192
	li		$rounds,12
	addi		$out,$out,0x20
	b		Ldone
.align	4
L256:
	lvx		$tmp,0,$inp
	li		$cnt,7
	li		$rounds,14
	 vperm		$outtail,$in0,$in0,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	 stvx		$stage,0,$out
	 addi		$out,$out,16
	vperm		$in1,$in1,$tmp,$key	
	mtctr		$cnt
Loop256:
	vperm		$key,$in1,$in1,$mask	
	vsldoi		$tmp,$zero,$in0,12	
	 vperm		$outtail,$in1,$in1,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	vcipherlast	$key,$key,$rcon
	 stvx		$stage,0,$out
	 addi		$out,$out,16
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in0,$in0,$tmp
	 vadduwm	$rcon,$rcon,$rcon
	vxor		$in0,$in0,$key
	 vperm		$outtail,$in0,$in0,$outperm	
	 vsel		$stage,$outhead,$outtail,$outmask
	 vmr		$outhead,$outtail
	 stvx		$stage,0,$out
	 addi		$inp,$out,15		
	 addi		$out,$out,16
	bdz		Ldone
	vspltw		$key,$in0,3		
	vsldoi		$tmp,$zero,$in1,12	
	vsbox		$key,$key
	vxor		$in1,$in1,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in1,$in1,$tmp
	vsldoi		$tmp,$zero,$tmp,12	
	vxor		$in1,$in1,$tmp
	vxor		$in1,$in1,$key
	b		Loop256
.align	4
Ldone:
	lvx		$in1,0,$inp		
	vsel		$in1,$outhead,$in1,$outmask
	stvx		$in1,0,$inp
	li		$ptr,0
	mtspr		256,$vrsave
	stw		$rounds,0($out)
Lenc_key_abort:
	mr		r3,$ptr
	blr
	.long		0
	.byte		0,12,0x14,1,0,0,3,0
	.long		0
.size	.${prefix}_set_encrypt_key,.-.${prefix}_set_encrypt_key
.globl	.${prefix}_set_decrypt_key
	$STU		$sp,-$FRAME($sp)
	mflr		r10
	$PUSH		r10,$FRAME+$LRSAVE($sp)
	bl		Lset_encrypt_key
	mtlr		r10
	cmpwi		r3,0
	bne-		Ldec_key_abort
	slwi		$cnt,$rounds,4
	subi		$inp,$out,240		
	srwi		$rounds,$rounds,1
	add		$out,$inp,$cnt		
	mtctr		$rounds
Ldeckey:
	lwz		r0, 0($inp)
	lwz		r6, 4($inp)
	lwz		r7, 8($inp)
	lwz		r8, 12($inp)
	addi		$inp,$inp,16
	lwz		r9, 0($out)
	lwz		r10,4($out)
	lwz		r11,8($out)
	lwz		r12,12($out)
	stw		r0, 0($out)
	stw		r6, 4($out)
	stw		r7, 8($out)
	stw		r8, 12($out)
	subi		$out,$out,16
	stw		r9, -16($inp)
	stw		r10,-12($inp)
	stw		r11,-8($inp)
	stw		r12,-4($inp)
	bdnz		Ldeckey
	xor		r3,r3,r3		
Ldec_key_abort:
	addi		$sp,$sp,$FRAME
	blr
	.long		0
	.byte		0,12,4,1,0x80,0,3,0
	.long		0
.size	.${prefix}_set_decrypt_key,.-.${prefix}_set_decrypt_key
___
}}}
{{{	
sub gen_block () {
my $dir = shift;
my $n   = $dir eq "de" ? "n" : "";
my ($inp,$out,$key,$rounds,$idx)=map("r$_",(3..7));
$code.=<<___;
.globl	.${prefix}_${dir}crypt
	lwz		$rounds,240($key)
	lis		r0,0xfc00
	mfspr		$vrsave,256
	li		$idx,15			
	mtspr		256,r0
	lvx		v0,0,$inp
	neg		r11,$out
	lvx		v1,$idx,$inp
	lvsl		v2,0,$inp		
	le?vspltisb	v4,0x0f
	?lvsl		v3,0,r11		
	le?vxor		v2,v2,v4
	li		$idx,16
	vperm		v0,v0,v1,v2		
	lvx		v1,0,$key
	?lvsl		v5,0,$key		
	srwi		$rounds,$rounds,1
	lvx		v2,$idx,$key
	addi		$idx,$idx,16
	subi		$rounds,$rounds,1
	?vperm		v1,v1,v2,v5		
	vxor		v0,v0,v1
	lvx		v1,$idx,$key
	addi		$idx,$idx,16
	mtctr		$rounds
Loop_${dir}c:
	?vperm		v2,v2,v1,v5
	v${n}cipher	v0,v0,v2
	lvx		v2,$idx,$key
	addi		$idx,$idx,16
	?vperm		v1,v1,v2,v5
	v${n}cipher	v0,v0,v1
	lvx		v1,$idx,$key
	addi		$idx,$idx,16
	bdnz		Loop_${dir}c
	?vperm		v2,v2,v1,v5
	v${n}cipher	v0,v0,v2
	lvx		v2,$idx,$key
	?vperm		v1,v1,v2,v5
	v${n}cipherlast	v0,v0,v1
	vspltisb	v2,-1
	vxor		v1,v1,v1
	li		$idx,15			
	?vperm		v2,v1,v2,v3		
	le?vxor		v3,v3,v4
	lvx		v1,0,$out		
	vperm		v0,v0,v0,v3		
	vsel		v1,v1,v0,v2
	lvx		v4,$idx,$out
	stvx		v1,0,$out
	vsel		v0,v0,v4,v2
	stvx		v0,$idx,$out
	mtspr		256,$vrsave
	blr
	.long		0
	.byte		0,12,0x14,0,0,0,3,0
	.long		0
.size	.${prefix}_${dir}crypt,.-.${prefix}_${dir}crypt
___
}
&gen_block("en");
&gen_block("de");
}}}
my $consts=1;
foreach(split("\n",$code)) {
        s/\`([^\`]*)\`/eval($1)/geo;
	if ($consts && m/\.(long|byte)\s+(.+)\s+(\?[a-z]*)$/o) {
	    my $conv=$3;
	    my @bytes=();
	    if ($1 eq "long") {
	      foreach (split(/,\s*/,$2)) {
		my $l = /^0/?oct:int;
		push @bytes,($l>>24)&0xff,($l>>16)&0xff,($l>>8)&0xff,$l&0xff;
	      }
	    } else {
		@bytes = map(/^0/?oct:int,split(/,\s*/,$2));
	    }
	    if ($flavour =~ /le$/o) {
		SWITCH: for($conv)  {
		    /\?inv/ && do   { @bytes=map($_^0xf,@bytes); last; };
		    /\?rev/ && do   { @bytes=reverse(@bytes);    last; };
		}
	    }
	    print ".byte\t",join(',',map (sprintf("0x%02x",$_),@bytes)),"\n";
	    next;
	}
	$consts=0 if (m/Lconsts:/o);	
	if ($flavour =~ /le$/o) {	
	    s/le\?//o		or
	    s/be\?/
	    s/\?lvsr/lvsl/o	or
	    s/\?lvsl/lvsr/o	or
	    s/\?(vperm\s+v[0-9]+,\s*)(v[0-9]+,\s*)(v[0-9]+,\s*)(v[0-9]+)/$1$3$2$4/o or
	    s/\?(vsldoi\s+v[0-9]+,\s*)(v[0-9]+,)\s*(v[0-9]+,\s*)([0-9]+)/$1$3$2 16-$4/o or
	    s/\?(vspltw\s+v[0-9]+,\s*)(v[0-9]+,)\s*([0-9])/$1$2 3-$3/o;
	} else {			
	    s/le\?/
	    s/be\?//o		or
	    s/\?([a-z]+)/$1/o;
	}
        print $_,"\n";
}
close STDOUT;
