exec >hash_md5_sha_x86-64.S
xmmT1="%xmm4"
xmmT2="%xmm5"
xmmRCONST="%xmm6"
xmmALLRCONST="%xmm7"
T=`printf '\t'`
interleave=false
INTERLEAVE() {
	$interleave || \
	{
		echo "$1"
		echo "$2"
		return
	}
	(
	echo "$1" | grep -v '^$' >"$0.temp1"
	echo "$2" | grep -v '^$' >"$0.temp2"
	exec 3<"$0.temp1"
	exec 4<"$0.temp2"
	IFS=''
	while :; do
		line1=''
		line2=''
		while :; do
			read -r line1 <&3
			if test "${line1:0:1}" != "#" && test "${line1:0:2}" != "$T#"; then
				break
			fi
			echo "$line1"
		done
		while :; do
			read -r line2 <&4
			if test "${line2:0:4}" = "${T}lea"; then
				echo "$line2"
				read -r line2 <&4
				echo "$line2"
				continue
			fi
			if test "${line2:0:1}" != "#" && test "${line2:0:2}" != "$T#"; then
				break
			fi
			echo "$line2"
		done
		test "$line1$line2" || break
		echo "$line1"
		echo "$line2"
	done
	rm "$0.temp1" "$0.temp2"
	)
}
echo \
"
	.section	.note.GNU-stack, \"\", @progbits
	.section	.text.sha1_process_block64, \"ax\", @progbits
	.globl	sha1_process_block64
	.hidden	sha1_process_block64
	.type	sha1_process_block64, @function
	.balign	8	
sha1_process_block64:
	pushq	%rbp	
	pushq	%rbx	
	pushq	%r14	
	pushq	%r13	
	pushq	%r12	
	pushq	%rdi	
	movl	80(%rdi), %eax		
	movl	84(%rdi), %ebx		
	movl	88(%rdi), %ecx		
	movl	92(%rdi), %edx		
	movl	96(%rdi), %ebp		
	movaps	sha1const(%rip), $xmmALLRCONST
	pshufd	\$0x00, $xmmALLRCONST, $xmmRCONST
	movq	4*0(%rdi), %rsi
	movq	4*2(%rdi), %r8
	bswapq	%rsi
	bswapq	%r8
	rolq	\$32, %rsi		
	rolq	\$32, %r8		
	movq	%rsi, %xmm0
	movq	%r8, $xmmT1
	punpcklqdq $xmmT1, %xmm0	
	movq	4*4(%rdi), %r9
	movq	4*6(%rdi), %r10
	bswapq	%r9
	bswapq	%r10
	rolq	\$32, %r9		
	rolq	\$32, %r10		
	movq	%r9, %xmm1
	movq	%r10, $xmmT1
	punpcklqdq $xmmT1, %xmm1	
	movq	4*8(%rdi), %r11
	movq	4*10(%rdi), %r12
	bswapq	%r11
	bswapq	%r12
	rolq	\$32, %r11		
	rolq	\$32, %r12		
	movq	%r11, %xmm2
	movq	%r12, $xmmT1
	punpcklqdq $xmmT1, %xmm2	
	movq	4*12(%rdi), %r13
	movq	4*14(%rdi), %r14
	bswapq	%r13
	bswapq	%r14
	rolq	\$32, %r13		
	rolq	\$32, %r14		
	movq	%r13, %xmm3
	movq	%r14, $xmmT1
	punpcklqdq $xmmT1, %xmm3	
"
PREP() {
local xmmW0=$1
local xmmW4=$2
local xmmW8=$3
local xmmW12=$4
local dstmem=$5
echo "
	movaps	$xmmW12, $xmmT1
	psrldq	\$4, $xmmT1	
	movaps	$xmmW0, $xmmT2
	shufps	\$0x4e, $xmmW4, $xmmT2	
	xorps	$xmmW8, $xmmW0	
	xorps	$xmmT1, $xmmT2	
	xorps	$xmmT2, $xmmW0	
	movaps	$xmmW0, $xmmT2
	xorps	$xmmT1, $xmmT1	
	pcmpgtd	$xmmW0, $xmmT1	
	paddd	$xmmW0, $xmmW0	
	psubd	$xmmT1, $xmmW0	
	pslldq	\$12, $xmmT2	
	movaps	$xmmT2, $xmmT1
	pslld	\$2, $xmmT2
	psrld	\$30, $xmmT1
	xorps	$xmmT1, $xmmW0	
	xorps	$xmmT2, $xmmW0	
"
echo "
	movaps	$xmmW0, $xmmT2
	paddd	$xmmRCONST, $xmmT2
	movups	$xmmT2, $dstmem
"
}
RD1A() {
local a=$1;local b=$2;local c=$3;local d=$4;local e=$5
local n=$(($6))
local n0=$(((n+0) & 15))
local rN=$((7+n0/2))
echo "
";test $n0 = 0 && echo "
	leal	$RCONST(%r$e,%rsi), %e$e 
	shrq	\$32, %rsi
";test $n0 = 1 && echo "
	leal	$RCONST(%r$e,%rsi), %e$e 
";test $n0 -ge 2 && test $((n0 & 1)) = 0 && echo "
	leal	$RCONST(%r$e,%r$rN), %e$e 
	shrq	\$32, %r$rN
";test $n0 -ge 2 && test $((n0 & 1)) = 1 && echo "
	leal	$RCONST(%r$e,%r$rN), %e$e 
";echo "
	movl	%e$c, %edi		
	xorl	%e$d, %edi		
	andl	%e$b, %edi		
	xorl	%e$d, %edi		
	addl	%edi, %e$e		
	movl	%e$a, %edi		
	roll	\$5, %edi		
	addl	%edi, %e$e		
	rorl	\$2, %e$b		
"
}
RD1B() {
local a=$1;local b=$2;local c=$3;local d=$4;local e=$5
local n=$(($6))
local n13=$(((n+13) & 15))
local n8=$(((n+8) & 15))
local n2=$(((n+2) & 15))
local n0=$(((n+0) & 15))
echo "
	movl	%e$c, %edi		
	xorl	%e$d, %edi		
	andl	%e$b, %edi		
	xorl	%e$d, %edi		
	addl	-64+4*$n0(%rsp), %e$e	
	addl	%edi, %e$e		
	movl	%e$a, %esi		
	roll	\$5, %esi		
	addl	%esi, %e$e		
	rorl	\$2, %e$b		
"
}
RD2() {
local a=$1;local b=$2;local c=$3;local d=$4;local e=$5
local n=$(($6))
local n13=$(((n+13) & 15))
local n8=$(((n+8) & 15))
local n2=$(((n+2) & 15))
local n0=$(((n+0) & 15))
echo "
	movl	%e$c, %edi		
	xorl	%e$d, %edi		
	xorl	%e$b, %edi		
	addl	-64+4*$n0(%rsp), %e$e	
	addl	%edi, %e$e		
	movl	%e$a, %esi		
	roll	\$5, %esi		
	addl	%esi, %e$e		
	rorl	\$2, %e$b		
"
}
RD3() {
local a=$1;local b=$2;local c=$3;local d=$4;local e=$5
local n=$(($6))
local n13=$(((n+13) & 15))
local n8=$(((n+8) & 15))
local n2=$(((n+2) & 15))
local n0=$(((n+0) & 15))
echo "
	movl	%e$b, %edi		
	movl	%e$b, %esi		
	orl	%e$c, %edi		
	andl	%e$c, %esi		
	andl	%e$d, %edi		
	orl	%esi, %edi		
	addl	%edi, %e$e		
	addl	-64+4*$n0(%rsp), %e$e	
	movl	%e$a, %esi		
	roll	\$5, %esi		
	addl	%esi, %e$e		
	rorl	\$2, %e$b		
"
}
{
RCONST=0x5A827999
RD1A ax bx cx dx bp  0; RD1A bp ax bx cx dx  1; RD1A dx bp ax bx cx  2; RD1A cx dx bp ax bx  3;
RD1A bx cx dx bp ax  4; RD1A ax bx cx dx bp  5; RD1A bp ax bx cx dx  6; RD1A dx bp ax bx cx  7;
a=`PREP %xmm0 %xmm1 %xmm2 %xmm3 "-64+16*0(%rsp)"`
b=`RD1A cx dx bp ax bx  8; RD1A bx cx dx bp ax  9; RD1A ax bx cx dx bp 10; RD1A bp ax bx cx dx 11;`
INTERLEAVE "$a" "$b"
a=`echo "	pshufd	\\$0x55, $xmmALLRCONST, $xmmRCONST"
   PREP %xmm1 %xmm2 %xmm3 %xmm0 "-64+16*1(%rsp)"`
b=`RD1A dx bp ax bx cx 12; RD1A cx dx bp ax bx 13; RD1A bx cx dx bp ax 14; RD1A ax bx cx dx bp 15;`
INTERLEAVE "$a" "$b"
a=`PREP %xmm2 %xmm3 %xmm0 %xmm1 "-64+16*2(%rsp)"`
b=`RD1B bp ax bx cx dx 16; RD1B dx bp ax bx cx 17; RD1B cx dx bp ax bx 18; RD1B bx cx dx bp ax 19;`
INTERLEAVE "$a" "$b"
RCONST=0x6ED9EBA1
a=`PREP %xmm3 %xmm0 %xmm1 %xmm2 "-64+16*3(%rsp)"`
b=`RD2 ax bx cx dx bp 20; RD2 bp ax bx cx dx 21; RD2 dx bp ax bx cx 22; RD2 cx dx bp ax bx 23;`
INTERLEAVE "$a" "$b"
a=`PREP %xmm0 %xmm1 %xmm2 %xmm3 "-64+16*0(%rsp)"`
b=`RD2 bx cx dx bp ax 24; RD2 ax bx cx dx bp 25; RD2 bp ax bx cx dx 26; RD2 dx bp ax bx cx 27;`
INTERLEAVE "$a" "$b"
a=`PREP %xmm1 %xmm2 %xmm3 %xmm0 "-64+16*1(%rsp)"`
b=`RD2 cx dx bp ax bx 28; RD2 bx cx dx bp ax 29; RD2 ax bx cx dx bp 30; RD2 bp ax bx cx dx 31;`
INTERLEAVE "$a" "$b"
a=`echo "	pshufd	\\$0xaa, $xmmALLRCONST, $xmmRCONST"
   PREP %xmm2 %xmm3 %xmm0 %xmm1 "-64+16*2(%rsp)"`
b=`RD2 dx bp ax bx cx 32; RD2 cx dx bp ax bx 33; RD2 bx cx dx bp ax 34; RD2 ax bx cx dx bp 35;`
INTERLEAVE "$a" "$b"
a=`PREP %xmm3 %xmm0 %xmm1 %xmm2 "-64+16*3(%rsp)"`
b=`RD2 bp ax bx cx dx 36; RD2 dx bp ax bx cx 37; RD2 cx dx bp ax bx 38; RD2 bx cx dx bp ax 39;`
INTERLEAVE "$a" "$b"
RCONST=0x8F1BBCDC
a=`PREP %xmm0 %xmm1 %xmm2 %xmm3 "-64+16*0(%rsp)"`
b=`RD3 ax bx cx dx bp 40; RD3 bp ax bx cx dx 41; RD3 dx bp ax bx cx 42; RD3 cx dx bp ax bx 43;`
INTERLEAVE "$a" "$b"
a=`PREP %xmm1 %xmm2 %xmm3 %xmm0 "-64+16*1(%rsp)"`
b=`RD3 bx cx dx bp ax 44; RD3 ax bx cx dx bp 45; RD3 bp ax bx cx dx 46; RD3 dx bp ax bx cx 47;`
INTERLEAVE "$a" "$b"
a=`PREP %xmm2 %xmm3 %xmm0 %xmm1 "-64+16*2(%rsp)"`
b=`RD3 cx dx bp ax bx 48; RD3 bx cx dx bp ax 49; RD3 ax bx cx dx bp 50; RD3 bp ax bx cx dx 51;`
INTERLEAVE "$a" "$b"
a=`echo "	pshufd	\\$0xff, $xmmALLRCONST, $xmmRCONST"
   PREP %xmm3 %xmm0 %xmm1 %xmm2 "-64+16*3(%rsp)"`
b=`RD3 dx bp ax bx cx 52; RD3 cx dx bp ax bx 53; RD3 bx cx dx bp ax 54; RD3 ax bx cx dx bp 55;`
INTERLEAVE "$a" "$b"
a=`PREP %xmm0 %xmm1 %xmm2 %xmm3 "-64+16*0(%rsp)"`
b=`RD3 bp ax bx cx dx 56; RD3 dx bp ax bx cx 57; RD3 cx dx bp ax bx 58; RD3 bx cx dx bp ax 59;`
INTERLEAVE "$a" "$b"
RCONST=0xCA62C1D6
a=`PREP %xmm1 %xmm2 %xmm3 %xmm0 "-64+16*1(%rsp)"`
b=`RD2 ax bx cx dx bp 60; RD2 bp ax bx cx dx 61; RD2 dx bp ax bx cx 62; RD2 cx dx bp ax bx 63;`
INTERLEAVE "$a" "$b"
a=`PREP %xmm2 %xmm3 %xmm0 %xmm1 "-64+16*2(%rsp)"`
b=`RD2 bx cx dx bp ax 64; RD2 ax bx cx dx bp 65; RD2 bp ax bx cx dx 66; RD2 dx bp ax bx cx 67;`
INTERLEAVE "$a" "$b"
a=`PREP %xmm3 %xmm0 %xmm1 %xmm2 "-64+16*3(%rsp)"`
b=`RD2 cx dx bp ax bx 68; RD2 bx cx dx bp ax 69; RD2 ax bx cx dx bp 70; RD2 bp ax bx cx dx 71;`
INTERLEAVE "$a" "$b"
RD2 dx bp ax bx cx 72; RD2 cx dx bp ax bx 73; RD2 bx cx dx bp ax 74; RD2 ax bx cx dx bp 75;
RD2 bp ax bx cx dx 76; RD2 dx bp ax bx cx 77; RD2 cx dx bp ax bx 78; RD2 bx cx dx bp ax 79;
} | grep -v '^$'
echo "
	popq	%rdi		
	popq	%r12		
	addl	%eax, 80(%rdi)	
	popq	%r13		
	addl	%ebx, 84(%rdi)	
	popq	%r14		
	addl	%ecx, 88(%rdi)	
	addl	%edx, 92(%rdi)	
	popq	%rbx		
	addl	%ebp, 96(%rdi)	
	popq	%rbp		
	ret
	.size	sha1_process_block64, .-sha1_process_block64
	.section	.rodata.cst16.sha1const, \"aM\", @progbits, 16
	.balign	16
sha1const:
	.long	0x5A827999
	.long	0x6ED9EBA1
	.long	0x8F1BBCDC
	.long	0xCA62C1D6
