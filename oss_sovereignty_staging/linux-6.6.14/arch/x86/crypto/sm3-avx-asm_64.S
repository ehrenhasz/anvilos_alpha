 
 

 

#include <linux/linkage.h>
#include <linux/cfi_types.h>
#include <asm/frame.h>

 

#define state_h0 0
#define state_h1 4
#define state_h2 8
#define state_h3 12
#define state_h4 16
#define state_h5 20
#define state_h6 24
#define state_h7 28

 

 

#define K0   2043430169   
#define K1   -208106958   
#define K2   -416213915   
#define K3   -832427829   
#define K4  -1664855657   
#define K5    965255983   
#define K6   1930511966   
#define K7   -433943364   
#define K8   -867886727   
#define K9  -1735773453   
#define K10   823420391   
#define K11  1646840782   
#define K12 -1001285732   
#define K13 -2002571463   
#define K14   289824371   
#define K15   579648742   
#define K16 -1651869049   
#define K17   991229199   
#define K18  1982458398   
#define K19  -330050500   
#define K20  -660100999   
#define K21 -1320201997   
#define K22  1654563303   
#define K23  -985840690   
#define K24 -1971681379   
#define K25   351604539   
#define K26   703209078   
#define K27  1406418156   
#define K28 -1482130984   
#define K29  1330705329   
#define K30 -1633556638   
#define K31  1027854021   
#define K32  2055708042   
#define K33  -183551212   
#define K34  -367102423   
#define K35  -734204845   
#define K36 -1468409689   
#define K37  1358147919   
#define K38 -1578671458   
#define K39  1137624381   
#define K40 -2019718534   
#define K41   255530229   
#define K42   511060458   
#define K43  1022120916   
#define K44  2044241832   
#define K45  -206483632   
#define K46  -412967263   
#define K47  -825934525   
#define K48 -1651869049   
#define K49   991229199   
#define K50  1982458398   
#define K51  -330050500   
#define K52  -660100999   
#define K53 -1320201997   
#define K54  1654563303   
#define K55  -985840690   
#define K56 -1971681379   
#define K57   351604539   
#define K58   703209078   
#define K59  1406418156   
#define K60 -1482130984   
#define K61  1330705329   
#define K62 -1633556638   
#define K63  1027854021   

 

#define RSTATE %rdi
#define RDATA  %rsi
#define RNBLKS %rdx

#define t0 %eax
#define t1 %ebx
#define t2 %ecx

#define a %r8d
#define b %r9d
#define c %r10d
#define d %r11d
#define e %r12d
#define f %r13d
#define g %r14d
#define h %r15d

#define W0 %xmm0
#define W1 %xmm1
#define W2 %xmm2
#define W3 %xmm3
#define W4 %xmm4
#define W5 %xmm5

#define XTMP0 %xmm6
#define XTMP1 %xmm7
#define XTMP2 %xmm8
#define XTMP3 %xmm9
#define XTMP4 %xmm10
#define XTMP5 %xmm11
#define XTMP6 %xmm12

#define BSWAP_REG %xmm15

 

#define STACK_W_SIZE        (32 * 2 * 3)
#define STACK_REG_SAVE_SIZE (64)

#define STACK_W             (0)
#define STACK_REG_SAVE      (STACK_W + STACK_W_SIZE)
#define STACK_SIZE          (STACK_REG_SAVE + STACK_REG_SAVE_SIZE)

 

#define roll2(v, reg)		\
	roll $(v), reg;

#define roll3mov(v, src, dst)	\
	movl src, dst;		\
	roll $(v), dst;

#define roll3(v, src, dst)	\
	rorxl $(32-(v)), src, dst;

#define addl2(a, out)		\
	leal (a, out), out;

 

#define GG1(x, y, z, o, t)	\
	movl x, o;		\
	xorl y, o;		\
	xorl z, o;

#define FF1(x, y, z, o, t) GG1(x, y, z, o, t)

#define GG2(x, y, z, o, t)	\
	andnl z, x, o;		\
	movl y, t;		\
	andl x, t;		\
	addl2(t, o);

#define FF2(x, y, z, o, t)	\
	movl y, o;		\
	xorl x, o;		\
	movl y, t;		\
	andl x, t;		\
	andl z, o;		\
	xorl t, o;

#define R(i, a, b, c, d, e, f, g, h, round, widx, wtype)		\
	 						\
	roll3mov(12, a, t0);   \
	 				\
	leal K##round(t0, e, 1), t1;					\
	roll2(7, t1);							\
	 						\
	addl wtype##_W1_ADDR(round, widx), h;				\
	 						\
	addl2(t1, h);							\
	 						\
	xorl t1, t0;							\
	 						\
	addl wtype##_W1W2_ADDR(round, widx), d;				\
	 					\
	FF##i(a, b, c, t1, t2);						\
	 						\
	addl2(t1, d);							\
	 						\
	GG##i(e, f, g, t2, t1);						\
	 						\
	addl2(t2, h);							\
	 						\
	roll2(19, f);							\
	 						\
	addl2(t0, d);							\
	 						\
	roll2(9, b);							\
	 						\
	roll3(9, h, t2);						\
	roll3(17, h, t1);						\
	xorl t2, h;							\
	xorl t1, h;

#define R1(a, b, c, d, e, f, g, h, round, widx, wtype) \
	R(1, a, b, c, d, e, f, g, h, round, widx, wtype)

#define R2(a, b, c, d, e, f, g, h, round, widx, wtype) \
	R(2, a, b, c, d, e, f, g, h, round, widx, wtype)

 

 
#define IW_W_ADDR(round, widx, offs) \
	(STACK_W + ((round) / 4) * 64 + (offs) + ((widx) * 4))(%rsp)

 
#define XW_W_ADDR(round, widx, offs) \
	(STACK_W + ((((round) / 3) - 4) % 2) * 64 + (offs) + ((widx) * 4))(%rsp)

 
#define IW_W1_ADDR(round, widx)   IW_W_ADDR(round, widx, 0)
#define IW_W1W2_ADDR(round, widx) IW_W_ADDR(round, widx, 32)

 
#define XW_W1_ADDR(round, widx)   XW_W_ADDR(round, widx, 0)
#define XW_W1W2_ADDR(round, widx) XW_W_ADDR(round, widx, 32)

 
#define LOAD_W_XMM_1()							\
	vmovdqu 0*16(RDATA), XTMP0;  		\
	vmovdqu 1*16(RDATA), XTMP1;  		\
	vmovdqu 2*16(RDATA), XTMP2;  	\
	vmovdqu 3*16(RDATA), XTMP3;  	\
	vpshufb BSWAP_REG, XTMP0, XTMP0;				\
	vpshufb BSWAP_REG, XTMP1, XTMP1;				\
	vpshufb BSWAP_REG, XTMP2, XTMP2;				\
	vpshufb BSWAP_REG, XTMP3, XTMP3;				\
	vpxor XTMP0, XTMP1, XTMP4;					\
	vpxor XTMP1, XTMP2, XTMP5;					\
	vpxor XTMP2, XTMP3, XTMP6;					\
	leaq 64(RDATA), RDATA;						\
	vmovdqa XTMP0, IW_W1_ADDR(0, 0);				\
	vmovdqa XTMP4, IW_W1W2_ADDR(0, 0);				\
	vmovdqa XTMP1, IW_W1_ADDR(4, 0);				\
	vmovdqa XTMP5, IW_W1W2_ADDR(4, 0);

#define LOAD_W_XMM_2()				\
	vmovdqa XTMP2, IW_W1_ADDR(8, 0);	\
	vmovdqa XTMP6, IW_W1W2_ADDR(8, 0);

#define LOAD_W_XMM_3()							\
	vpshufd $0b00000000, XTMP0, W0;  	\
	vpshufd $0b11111001, XTMP0, W1;  	\
	vmovdqa XTMP1, W2;               	\
	vpalignr $12, XTMP1, XTMP2, W3;  	\
	vpalignr $8, XTMP2, XTMP3, W4;   	\
	vpshufd $0b11111001, XTMP3, W5;  

 
#define SCHED_W_0(round, w0, w1, w2, w3, w4, w5)			\
	 					\
	vpshufd $0b10111111, w0, XTMP0;					\
	vpalignr $12, XTMP0, w1, XTMP0;  	\
	 					\
	vpshufd $0b10111111, w1, XTMP1;					\
	vpalignr $12, XTMP1, w2, XTMP1;					\
	 						\
	 					\
	vpxor w3, XTMP0, XTMP0;

#define SCHED_W_1(round, w0, w1, w2, w3, w4, w5)	\
	 				\
	 		\
	vpslld $15, w5, XTMP2;				\
	vpsrld $(32-15), w5, XTMP3;			\
	vpxor XTMP2, XTMP3, XTMP3;			\
	vpxor XTMP3, XTMP0, XTMP0;			\
	 			\
	vpslld $7, XTMP1, XTMP5;			\
	vpsrld $(32-7), XTMP1, XTMP1;			\
	vpxor XTMP5, XTMP1, XTMP1;			\
	 			\
	vpxor w4, XTMP1, XTMP1;				\
	 				\
	 			\
	vpslld $15, XTMP0, XTMP5;			\
	vpsrld $(32-15), XTMP0, XTMP6;			\
	vpslld $23, XTMP0, XTMP2;			\
	vpsrld $(32-23), XTMP0, XTMP3;			\
	vpxor XTMP0, XTMP1, XTMP1;			\
	vpxor XTMP6, XTMP5, XTMP5;			\
	vpxor XTMP3, XTMP2, XTMP2;			\
	vpxor XTMP2, XTMP5, XTMP5;			\
	vpxor XTMP5, XTMP1, w0;

#define SCHED_W_2(round, w0, w1, w2, w3, w4, w5)	\
	 				\
	vpshufd $0b10111111, w4, XTMP4;			\
	vpalignr $12, XTMP4, w5, XTMP4;			\
	vmovdqa XTMP4, XW_W1_ADDR((round), 0);		\
	 				\
	vpxor w0, XTMP4, XTMP1;				\
	vmovdqa XTMP1, XW_W1W2_ADDR((round), 0);


.section	.rodata.cst16, "aM", @progbits, 16
.align 16

.Lbe32mask:
	.long 0x00010203, 0x04050607, 0x08090a0b, 0x0c0d0e0f

.text

 
SYM_TYPED_FUNC_START(sm3_transform_avx)
	 
	vzeroupper;

	pushq %rbp;
	movq %rsp, %rbp;

	movq %rdx, RNBLKS;

	subq $STACK_SIZE, %rsp;
	andq $(~63), %rsp;

	movq %rbx, (STACK_REG_SAVE + 0 * 8)(%rsp);
	movq %r15, (STACK_REG_SAVE + 1 * 8)(%rsp);
	movq %r14, (STACK_REG_SAVE + 2 * 8)(%rsp);
	movq %r13, (STACK_REG_SAVE + 3 * 8)(%rsp);
	movq %r12, (STACK_REG_SAVE + 4 * 8)(%rsp);

	vmovdqa .Lbe32mask (%rip), BSWAP_REG;

	 
	movl state_h0(RSTATE), a;
	movl state_h1(RSTATE), b;
	movl state_h2(RSTATE), c;
	movl state_h3(RSTATE), d;
	movl state_h4(RSTATE), e;
	movl state_h5(RSTATE), f;
	movl state_h6(RSTATE), g;
	movl state_h7(RSTATE), h;

.align 16
.Loop:
	 
	LOAD_W_XMM_1();

	leaq -1(RNBLKS), RNBLKS;

	 
	R1(a, b, c, d, e, f, g, h, 0, 0, IW); LOAD_W_XMM_2();
	R1(d, a, b, c, h, e, f, g, 1, 1, IW);
	R1(c, d, a, b, g, h, e, f, 2, 2, IW);
	R1(b, c, d, a, f, g, h, e, 3, 3, IW); LOAD_W_XMM_3();

	 
	R1(a, b, c, d, e, f, g, h, 4, 0, IW);
	R1(d, a, b, c, h, e, f, g, 5, 1, IW);
	R1(c, d, a, b, g, h, e, f, 6, 2, IW); SCHED_W_0(12, W0, W1, W2, W3, W4, W5);
	R1(b, c, d, a, f, g, h, e, 7, 3, IW); SCHED_W_1(12, W0, W1, W2, W3, W4, W5);

	 
	R1(a, b, c, d, e, f, g, h, 8, 0, IW); SCHED_W_2(12, W0, W1, W2, W3, W4, W5);
	R1(d, a, b, c, h, e, f, g, 9, 1, IW); SCHED_W_0(15, W1, W2, W3, W4, W5, W0);
	R1(c, d, a, b, g, h, e, f, 10, 2, IW); SCHED_W_1(15, W1, W2, W3, W4, W5, W0);
	R1(b, c, d, a, f, g, h, e, 11, 3, IW); SCHED_W_2(15, W1, W2, W3, W4, W5, W0);

	 
	R1(a, b, c, d, e, f, g, h, 12, 0, XW); SCHED_W_0(18, W2, W3, W4, W5, W0, W1);
	R1(d, a, b, c, h, e, f, g, 13, 1, XW); SCHED_W_1(18, W2, W3, W4, W5, W0, W1);
	R1(c, d, a, b, g, h, e, f, 14, 2, XW); SCHED_W_2(18, W2, W3, W4, W5, W0, W1);

	 
	R1(b, c, d, a, f, g, h, e, 15, 0, XW); SCHED_W_0(21, W3, W4, W5, W0, W1, W2);
	R2(a, b, c, d, e, f, g, h, 16, 1, XW); SCHED_W_1(21, W3, W4, W5, W0, W1, W2);
	R2(d, a, b, c, h, e, f, g, 17, 2, XW); SCHED_W_2(21, W3, W4, W5, W0, W1, W2);

	 
	R2(c, d, a, b, g, h, e, f, 18, 0, XW); SCHED_W_0(24, W4, W5, W0, W1, W2, W3);
	R2(b, c, d, a, f, g, h, e, 19, 1, XW); SCHED_W_1(24, W4, W5, W0, W1, W2, W3);
	R2(a, b, c, d, e, f, g, h, 20, 2, XW); SCHED_W_2(24, W4, W5, W0, W1, W2, W3);

	 
	R2(d, a, b, c, h, e, f, g, 21, 0, XW); SCHED_W_0(27, W5, W0, W1, W2, W3, W4);
	R2(c, d, a, b, g, h, e, f, 22, 1, XW); SCHED_W_1(27, W5, W0, W1, W2, W3, W4);
	R2(b, c, d, a, f, g, h, e, 23, 2, XW); SCHED_W_2(27, W5, W0, W1, W2, W3, W4);

	 
	R2(a, b, c, d, e, f, g, h, 24, 0, XW); SCHED_W_0(30, W0, W1, W2, W3, W4, W5);
	R2(d, a, b, c, h, e, f, g, 25, 1, XW); SCHED_W_1(30, W0, W1, W2, W3, W4, W5);
	R2(c, d, a, b, g, h, e, f, 26, 2, XW); SCHED_W_2(30, W0, W1, W2, W3, W4, W5);

	 
	R2(b, c, d, a, f, g, h, e, 27, 0, XW); SCHED_W_0(33, W1, W2, W3, W4, W5, W0);
	R2(a, b, c, d, e, f, g, h, 28, 1, XW); SCHED_W_1(33, W1, W2, W3, W4, W5, W0);
	R2(d, a, b, c, h, e, f, g, 29, 2, XW); SCHED_W_2(33, W1, W2, W3, W4, W5, W0);

	 
	R2(c, d, a, b, g, h, e, f, 30, 0, XW); SCHED_W_0(36, W2, W3, W4, W5, W0, W1);
	R2(b, c, d, a, f, g, h, e, 31, 1, XW); SCHED_W_1(36, W2, W3, W4, W5, W0, W1);
	R2(a, b, c, d, e, f, g, h, 32, 2, XW); SCHED_W_2(36, W2, W3, W4, W5, W0, W1);

	 
	R2(d, a, b, c, h, e, f, g, 33, 0, XW); SCHED_W_0(39, W3, W4, W5, W0, W1, W2);
	R2(c, d, a, b, g, h, e, f, 34, 1, XW); SCHED_W_1(39, W3, W4, W5, W0, W1, W2);
	R2(b, c, d, a, f, g, h, e, 35, 2, XW); SCHED_W_2(39, W3, W4, W5, W0, W1, W2);

	 
	R2(a, b, c, d, e, f, g, h, 36, 0, XW); SCHED_W_0(42, W4, W5, W0, W1, W2, W3);
	R2(d, a, b, c, h, e, f, g, 37, 1, XW); SCHED_W_1(42, W4, W5, W0, W1, W2, W3);
	R2(c, d, a, b, g, h, e, f, 38, 2, XW); SCHED_W_2(42, W4, W5, W0, W1, W2, W3);

	 
	R2(b, c, d, a, f, g, h, e, 39, 0, XW); SCHED_W_0(45, W5, W0, W1, W2, W3, W4);
	R2(a, b, c, d, e, f, g, h, 40, 1, XW); SCHED_W_1(45, W5, W0, W1, W2, W3, W4);
	R2(d, a, b, c, h, e, f, g, 41, 2, XW); SCHED_W_2(45, W5, W0, W1, W2, W3, W4);

	 
	R2(c, d, a, b, g, h, e, f, 42, 0, XW); SCHED_W_0(48, W0, W1, W2, W3, W4, W5);
	R2(b, c, d, a, f, g, h, e, 43, 1, XW); SCHED_W_1(48, W0, W1, W2, W3, W4, W5);
	R2(a, b, c, d, e, f, g, h, 44, 2, XW); SCHED_W_2(48, W0, W1, W2, W3, W4, W5);

	 
	R2(d, a, b, c, h, e, f, g, 45, 0, XW); SCHED_W_0(51, W1, W2, W3, W4, W5, W0);
	R2(c, d, a, b, g, h, e, f, 46, 1, XW); SCHED_W_1(51, W1, W2, W3, W4, W5, W0);
	R2(b, c, d, a, f, g, h, e, 47, 2, XW); SCHED_W_2(51, W1, W2, W3, W4, W5, W0);

	 
	R2(a, b, c, d, e, f, g, h, 48, 0, XW); SCHED_W_0(54, W2, W3, W4, W5, W0, W1);
	R2(d, a, b, c, h, e, f, g, 49, 1, XW); SCHED_W_1(54, W2, W3, W4, W5, W0, W1);
	R2(c, d, a, b, g, h, e, f, 50, 2, XW); SCHED_W_2(54, W2, W3, W4, W5, W0, W1);

	 
	R2(b, c, d, a, f, g, h, e, 51, 0, XW); SCHED_W_0(57, W3, W4, W5, W0, W1, W2);
	R2(a, b, c, d, e, f, g, h, 52, 1, XW); SCHED_W_1(57, W3, W4, W5, W0, W1, W2);
	R2(d, a, b, c, h, e, f, g, 53, 2, XW); SCHED_W_2(57, W3, W4, W5, W0, W1, W2);

	 
	R2(c, d, a, b, g, h, e, f, 54, 0, XW); SCHED_W_0(60, W4, W5, W0, W1, W2, W3);
	R2(b, c, d, a, f, g, h, e, 55, 1, XW); SCHED_W_1(60, W4, W5, W0, W1, W2, W3);
	R2(a, b, c, d, e, f, g, h, 56, 2, XW); SCHED_W_2(60, W4, W5, W0, W1, W2, W3);

	 
	R2(d, a, b, c, h, e, f, g, 57, 0, XW); SCHED_W_0(63, W5, W0, W1, W2, W3, W4);
	R2(c, d, a, b, g, h, e, f, 58, 1, XW);
	R2(b, c, d, a, f, g, h, e, 59, 2, XW); SCHED_W_1(63, W5, W0, W1, W2, W3, W4);

	 
	R2(a, b, c, d, e, f, g, h, 60, 0, XW);
	R2(d, a, b, c, h, e, f, g, 61, 1, XW); SCHED_W_2(63, W5, W0, W1, W2, W3, W4);
	R2(c, d, a, b, g, h, e, f, 62, 2, XW);

	 
	R2(b, c, d, a, f, g, h, e, 63, 0, XW);

	 
	xorl state_h0(RSTATE), a;
	xorl state_h1(RSTATE), b;
	xorl state_h2(RSTATE), c;
	xorl state_h3(RSTATE), d;
	movl a, state_h0(RSTATE);
	movl b, state_h1(RSTATE);
	movl c, state_h2(RSTATE);
	movl d, state_h3(RSTATE);
	xorl state_h4(RSTATE), e;
	xorl state_h5(RSTATE), f;
	xorl state_h6(RSTATE), g;
	xorl state_h7(RSTATE), h;
	movl e, state_h4(RSTATE);
	movl f, state_h5(RSTATE);
	movl g, state_h6(RSTATE);
	movl h, state_h7(RSTATE);

	cmpq $0, RNBLKS;
	jne .Loop;

	vzeroall;

	movq (STACK_REG_SAVE + 0 * 8)(%rsp), %rbx;
	movq (STACK_REG_SAVE + 1 * 8)(%rsp), %r15;
	movq (STACK_REG_SAVE + 2 * 8)(%rsp), %r14;
	movq (STACK_REG_SAVE + 3 * 8)(%rsp), %r13;
	movq (STACK_REG_SAVE + 4 * 8)(%rsp), %r12;

	vmovdqa %xmm0, IW_W1_ADDR(0, 0);
	vmovdqa %xmm0, IW_W1W2_ADDR(0, 0);
	vmovdqa %xmm0, IW_W1_ADDR(4, 0);
	vmovdqa %xmm0, IW_W1W2_ADDR(4, 0);
	vmovdqa %xmm0, IW_W1_ADDR(8, 0);
	vmovdqa %xmm0, IW_W1W2_ADDR(8, 0);

	movq %rbp, %rsp;
	popq %rbp;
	RET;
SYM_FUNC_END(sm3_transform_avx)
