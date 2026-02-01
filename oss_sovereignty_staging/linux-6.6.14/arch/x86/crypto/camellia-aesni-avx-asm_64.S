 

 

#include <linux/linkage.h>
#include <asm/frame.h>

#define CAMELLIA_TABLE_BYTE_LEN 272

 
#define key_table 0
#define key_length CAMELLIA_TABLE_BYTE_LEN

 
#define CTX %rdi

 
#define filter_8bit(x, lo_t, hi_t, mask4bit, tmp0) \
	vpand x, mask4bit, tmp0; \
	vpandn x, mask4bit, x; \
	vpsrld $4, x, x; \
	\
	vpshufb tmp0, lo_t, tmp0; \
	vpshufb x, hi_t, x; \
	vpxor tmp0, x, x;

 
#define roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, t0, t1, t2, t3, t4, t5, t6, \
		  t7, mem_cd, key) \
	  \
	vmovdqa .Linv_shift_row(%rip), t4; \
	vbroadcastss .L0f0f0f0f(%rip), t7; \
	vmovdqa .Lpre_tf_lo_s1(%rip), t0; \
	vmovdqa .Lpre_tf_hi_s1(%rip), t1; \
	\
	  \
	vpshufb t4, x0, x0; \
	vpshufb t4, x7, x7; \
	vpshufb t4, x1, x1; \
	vpshufb t4, x4, x4; \
	vpshufb t4, x2, x2; \
	vpshufb t4, x5, x5; \
	vpshufb t4, x3, x3; \
	vpshufb t4, x6, x6; \
	\
	  \
	vmovdqa .Lpre_tf_lo_s4(%rip), t2; \
	vmovdqa .Lpre_tf_hi_s4(%rip), t3; \
	filter_8bit(x0, t0, t1, t7, t6); \
	filter_8bit(x7, t0, t1, t7, t6); \
	filter_8bit(x1, t0, t1, t7, t6); \
	filter_8bit(x4, t0, t1, t7, t6); \
	filter_8bit(x2, t0, t1, t7, t6); \
	filter_8bit(x5, t0, t1, t7, t6); \
	\
	  \
	vpxor t4, t4, t4; \
	filter_8bit(x3, t2, t3, t7, t6); \
	filter_8bit(x6, t2, t3, t7, t6); \
	\
	  \
	vmovdqa .Lpost_tf_lo_s1(%rip), t0; \
	vmovdqa .Lpost_tf_hi_s1(%rip), t1; \
	vaesenclast t4, x0, x0; \
	vaesenclast t4, x7, x7; \
	vaesenclast t4, x1, x1; \
	vaesenclast t4, x4, x4; \
	vaesenclast t4, x2, x2; \
	vaesenclast t4, x5, x5; \
	vaesenclast t4, x3, x3; \
	vaesenclast t4, x6, x6; \
	\
	  \
	vmovdqa .Lpost_tf_lo_s3(%rip), t2; \
	vmovdqa .Lpost_tf_hi_s3(%rip), t3; \
	filter_8bit(x0, t0, t1, t7, t6); \
	filter_8bit(x7, t0, t1, t7, t6); \
	filter_8bit(x3, t0, t1, t7, t6); \
	filter_8bit(x6, t0, t1, t7, t6); \
	\
	  \
	vmovdqa .Lpost_tf_lo_s2(%rip), t4; \
	vmovdqa .Lpost_tf_hi_s2(%rip), t5; \
	filter_8bit(x2, t2, t3, t7, t6); \
	filter_8bit(x5, t2, t3, t7, t6); \
	\
	vpxor t6, t6, t6; \
	vmovq key, t0; \
	\
	  \
	filter_8bit(x1, t4, t5, t7, t2); \
	filter_8bit(x4, t4, t5, t7, t2); \
	\
	vpsrldq $5, t0, t5; \
	vpsrldq $1, t0, t1; \
	vpsrldq $2, t0, t2; \
	vpsrldq $3, t0, t3; \
	vpsrldq $4, t0, t4; \
	vpshufb t6, t0, t0; \
	vpshufb t6, t1, t1; \
	vpshufb t6, t2, t2; \
	vpshufb t6, t3, t3; \
	vpshufb t6, t4, t4; \
	vpsrldq $2, t5, t7; \
	vpshufb t6, t7, t7; \
	\
	  \
	vpxor x5, x0, x0; \
	vpxor x6, x1, x1; \
	vpxor x7, x2, x2; \
	vpxor x4, x3, x3; \
	\
	vpxor x2, x4, x4; \
	vpxor x3, x5, x5; \
	vpxor x0, x6, x6; \
	vpxor x1, x7, x7; \
	\
	vpxor x7, x0, x0; \
	vpxor x4, x1, x1; \
	vpxor x5, x2, x2; \
	vpxor x6, x3, x3; \
	\
	vpxor x3, x4, x4; \
	vpxor x0, x5, x5; \
	vpxor x1, x6, x6; \
	vpxor x2, x7, x7;   \
	\
	  \
	\
	vpxor t3, x4, x4; \
	vpxor 0 * 16(mem_cd), x4, x4; \
	\
	vpxor t2, x5, x5; \
	vpxor 1 * 16(mem_cd), x5, x5; \
	\
	vpsrldq $1, t5, t3; \
	vpshufb t6, t5, t5; \
	vpshufb t6, t3, t6; \
	\
	vpxor t1, x6, x6; \
	vpxor 2 * 16(mem_cd), x6, x6; \
	\
	vpxor t0, x7, x7; \
	vpxor 3 * 16(mem_cd), x7, x7; \
	\
	vpxor t7, x0, x0; \
	vpxor 4 * 16(mem_cd), x0, x0; \
	\
	vpxor t6, x1, x1; \
	vpxor 5 * 16(mem_cd), x1, x1; \
	\
	vpxor t5, x2, x2; \
	vpxor 6 * 16(mem_cd), x2, x2; \
	\
	vpxor t4, x3, x3; \
	vpxor 7 * 16(mem_cd), x3, x3;

 
.align 8
SYM_FUNC_START_LOCAL(roundsm16_x0_x1_x2_x3_x4_x5_x6_x7_y0_y1_y2_y3_y4_y5_y6_y7_cd)
	roundsm16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		  %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14, %xmm15,
		  %rcx, (%r9));
	RET;
SYM_FUNC_END(roundsm16_x0_x1_x2_x3_x4_x5_x6_x7_y0_y1_y2_y3_y4_y5_y6_y7_cd)

.align 8
SYM_FUNC_START_LOCAL(roundsm16_x4_x5_x6_x7_x0_x1_x2_x3_y4_y5_y6_y7_y0_y1_y2_y3_ab)
	roundsm16(%xmm4, %xmm5, %xmm6, %xmm7, %xmm0, %xmm1, %xmm2, %xmm3,
		  %xmm12, %xmm13, %xmm14, %xmm15, %xmm8, %xmm9, %xmm10, %xmm11,
		  %rax, (%r9));
	RET;
SYM_FUNC_END(roundsm16_x4_x5_x6_x7_x0_x1_x2_x3_y4_y5_y6_y7_y0_y1_y2_y3_ab)

 
#define two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, i, dir, store_ab) \
	leaq (key_table + (i) * 8)(CTX), %r9; \
	call roundsm16_x0_x1_x2_x3_x4_x5_x6_x7_y0_y1_y2_y3_y4_y5_y6_y7_cd; \
	\
	vmovdqu x4, 0 * 16(mem_cd); \
	vmovdqu x5, 1 * 16(mem_cd); \
	vmovdqu x6, 2 * 16(mem_cd); \
	vmovdqu x7, 3 * 16(mem_cd); \
	vmovdqu x0, 4 * 16(mem_cd); \
	vmovdqu x1, 5 * 16(mem_cd); \
	vmovdqu x2, 6 * 16(mem_cd); \
	vmovdqu x3, 7 * 16(mem_cd); \
	\
	leaq (key_table + ((i) + (dir)) * 8)(CTX), %r9; \
	call roundsm16_x4_x5_x6_x7_x0_x1_x2_x3_y4_y5_y6_y7_y0_y1_y2_y3_ab; \
	\
	store_ab(x0, x1, x2, x3, x4, x5, x6, x7, mem_ab);

#define dummy_store(x0, x1, x2, x3, x4, x5, x6, x7, mem_ab)  

#define store_ab_state(x0, x1, x2, x3, x4, x5, x6, x7, mem_ab) \
	  \
	vmovdqu x0, 0 * 16(mem_ab); \
	vmovdqu x1, 1 * 16(mem_ab); \
	vmovdqu x2, 2 * 16(mem_ab); \
	vmovdqu x3, 3 * 16(mem_ab); \
	vmovdqu x4, 4 * 16(mem_ab); \
	vmovdqu x5, 5 * 16(mem_ab); \
	vmovdqu x6, 6 * 16(mem_ab); \
	vmovdqu x7, 7 * 16(mem_ab);

#define enc_rounds16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, i) \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 2, 1, store_ab_state); \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 4, 1, store_ab_state); \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 6, 1, dummy_store);

#define dec_rounds16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, i) \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 7, -1, store_ab_state); \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 5, -1, store_ab_state); \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 3, -1, dummy_store);

 
#define rol32_1_16(v0, v1, v2, v3, t0, t1, t2, zero) \
	vpcmpgtb v0, zero, t0; \
	vpaddb v0, v0, v0; \
	vpabsb t0, t0; \
	\
	vpcmpgtb v1, zero, t1; \
	vpaddb v1, v1, v1; \
	vpabsb t1, t1; \
	\
	vpcmpgtb v2, zero, t2; \
	vpaddb v2, v2, v2; \
	vpabsb t2, t2; \
	\
	vpor t0, v1, v1; \
	\
	vpcmpgtb v3, zero, t0; \
	vpaddb v3, v3, v3; \
	vpabsb t0, t0; \
	\
	vpor t1, v2, v2; \
	vpor t2, v3, v3; \
	vpor t0, v0, v0;

 
#define fls16(l, l0, l1, l2, l3, l4, l5, l6, l7, r, t0, t1, t2, t3, tt0, \
	      tt1, tt2, tt3, kll, klr, krl, krr) \
	  \
	vpxor tt0, tt0, tt0; \
	vmovd kll, t0; \
	vpshufb tt0, t0, t3; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t2; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t1; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t0; \
	\
	vpand l0, t0, t0; \
	vpand l1, t1, t1; \
	vpand l2, t2, t2; \
	vpand l3, t3, t3; \
	\
	rol32_1_16(t3, t2, t1, t0, tt1, tt2, tt3, tt0); \
	\
	vpxor l4, t0, l4; \
	vmovdqu l4, 4 * 16(l); \
	vpxor l5, t1, l5; \
	vmovdqu l5, 5 * 16(l); \
	vpxor l6, t2, l6; \
	vmovdqu l6, 6 * 16(l); \
	vpxor l7, t3, l7; \
	vmovdqu l7, 7 * 16(l); \
	\
	  \
	\
	vmovd krr, t0; \
	vpshufb tt0, t0, t3; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t2; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t1; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t0; \
	\
	vpor 4 * 16(r), t0, t0; \
	vpor 5 * 16(r), t1, t1; \
	vpor 6 * 16(r), t2, t2; \
	vpor 7 * 16(r), t3, t3; \
	\
	vpxor 0 * 16(r), t0, t0; \
	vpxor 1 * 16(r), t1, t1; \
	vpxor 2 * 16(r), t2, t2; \
	vpxor 3 * 16(r), t3, t3; \
	vmovdqu t0, 0 * 16(r); \
	vmovdqu t1, 1 * 16(r); \
	vmovdqu t2, 2 * 16(r); \
	vmovdqu t3, 3 * 16(r); \
	\
	  \
	vmovd krl, t0; \
	vpshufb tt0, t0, t3; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t2; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t1; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t0; \
	\
	vpand 0 * 16(r), t0, t0; \
	vpand 1 * 16(r), t1, t1; \
	vpand 2 * 16(r), t2, t2; \
	vpand 3 * 16(r), t3, t3; \
	\
	rol32_1_16(t3, t2, t1, t0, tt1, tt2, tt3, tt0); \
	\
	vpxor 4 * 16(r), t0, t0; \
	vpxor 5 * 16(r), t1, t1; \
	vpxor 6 * 16(r), t2, t2; \
	vpxor 7 * 16(r), t3, t3; \
	vmovdqu t0, 4 * 16(r); \
	vmovdqu t1, 5 * 16(r); \
	vmovdqu t2, 6 * 16(r); \
	vmovdqu t3, 7 * 16(r); \
	\
	  \
	\
	vmovd klr, t0; \
	vpshufb tt0, t0, t3; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t2; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t1; \
	vpsrldq $1, t0, t0; \
	vpshufb tt0, t0, t0; \
	\
	vpor l4, t0, t0; \
	vpor l5, t1, t1; \
	vpor l6, t2, t2; \
	vpor l7, t3, t3; \
	\
	vpxor l0, t0, l0; \
	vmovdqu l0, 0 * 16(l); \
	vpxor l1, t1, l1; \
	vmovdqu l1, 1 * 16(l); \
	vpxor l2, t2, l2; \
	vmovdqu l2, 2 * 16(l); \
	vpxor l3, t3, l3; \
	vmovdqu l3, 3 * 16(l);

#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
	vpunpckhdq x1, x0, t2; \
	vpunpckldq x1, x0, x0; \
	\
	vpunpckldq x3, x2, t1; \
	vpunpckhdq x3, x2, x2; \
	\
	vpunpckhqdq t1, x0, x1; \
	vpunpcklqdq t1, x0, x0; \
	\
	vpunpckhqdq x2, t2, x3; \
	vpunpcklqdq x2, t2, x2;

#define byteslice_16x16b(a0, b0, c0, d0, a1, b1, c1, d1, a2, b2, c2, d2, a3, \
			 b3, c3, d3, st0, st1) \
	vmovdqu d2, st0; \
	vmovdqu d3, st1; \
	transpose_4x4(a0, a1, a2, a3, d2, d3); \
	transpose_4x4(b0, b1, b2, b3, d2, d3); \
	vmovdqu st0, d2; \
	vmovdqu st1, d3; \
	\
	vmovdqu a0, st0; \
	vmovdqu a1, st1; \
	transpose_4x4(c0, c1, c2, c3, a0, a1); \
	transpose_4x4(d0, d1, d2, d3, a0, a1); \
	\
	vmovdqu .Lshufb_16x16b(%rip), a0; \
	vmovdqu st1, a1; \
	vpshufb a0, a2, a2; \
	vpshufb a0, a3, a3; \
	vpshufb a0, b0, b0; \
	vpshufb a0, b1, b1; \
	vpshufb a0, b2, b2; \
	vpshufb a0, b3, b3; \
	vpshufb a0, a1, a1; \
	vpshufb a0, c0, c0; \
	vpshufb a0, c1, c1; \
	vpshufb a0, c2, c2; \
	vpshufb a0, c3, c3; \
	vpshufb a0, d0, d0; \
	vpshufb a0, d1, d1; \
	vpshufb a0, d2, d2; \
	vpshufb a0, d3, d3; \
	vmovdqu d3, st1; \
	vmovdqu st0, d3; \
	vpshufb a0, d3, a0; \
	vmovdqu d2, st0; \
	\
	transpose_4x4(a0, b0, c0, d0, d2, d3); \
	transpose_4x4(a1, b1, c1, d1, d2, d3); \
	vmovdqu st0, d2; \
	vmovdqu st1, d3; \
	\
	vmovdqu b0, st0; \
	vmovdqu b1, st1; \
	transpose_4x4(a2, b2, c2, d2, b0, b1); \
	transpose_4x4(a3, b3, c3, d3, b0, b1); \
	vmovdqu st0, b0; \
	vmovdqu st1, b1; \
	 

 
#define inpack16_pre(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		     y6, y7, rio, key) \
	vmovq key, x0; \
	vpshufb .Lpack_bswap(%rip), x0, x0; \
	\
	vpxor 0 * 16(rio), x0, y7; \
	vpxor 1 * 16(rio), x0, y6; \
	vpxor 2 * 16(rio), x0, y5; \
	vpxor 3 * 16(rio), x0, y4; \
	vpxor 4 * 16(rio), x0, y3; \
	vpxor 5 * 16(rio), x0, y2; \
	vpxor 6 * 16(rio), x0, y1; \
	vpxor 7 * 16(rio), x0, y0; \
	vpxor 8 * 16(rio), x0, x7; \
	vpxor 9 * 16(rio), x0, x6; \
	vpxor 10 * 16(rio), x0, x5; \
	vpxor 11 * 16(rio), x0, x4; \
	vpxor 12 * 16(rio), x0, x3; \
	vpxor 13 * 16(rio), x0, x2; \
	vpxor 14 * 16(rio), x0, x1; \
	vpxor 15 * 16(rio), x0, x0;

 
#define inpack16_post(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd) \
	byteslice_16x16b(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, \
			 y5, y6, y7, (mem_ab), (mem_cd)); \
	\
	vmovdqu x0, 0 * 16(mem_ab); \
	vmovdqu x1, 1 * 16(mem_ab); \
	vmovdqu x2, 2 * 16(mem_ab); \
	vmovdqu x3, 3 * 16(mem_ab); \
	vmovdqu x4, 4 * 16(mem_ab); \
	vmovdqu x5, 5 * 16(mem_ab); \
	vmovdqu x6, 6 * 16(mem_ab); \
	vmovdqu x7, 7 * 16(mem_ab); \
	vmovdqu y0, 0 * 16(mem_cd); \
	vmovdqu y1, 1 * 16(mem_cd); \
	vmovdqu y2, 2 * 16(mem_cd); \
	vmovdqu y3, 3 * 16(mem_cd); \
	vmovdqu y4, 4 * 16(mem_cd); \
	vmovdqu y5, 5 * 16(mem_cd); \
	vmovdqu y6, 6 * 16(mem_cd); \
	vmovdqu y7, 7 * 16(mem_cd);

 
#define outunpack16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, \
		    y5, y6, y7, key, stack_tmp0, stack_tmp1) \
	byteslice_16x16b(y0, y4, x0, x4, y1, y5, x1, x5, y2, y6, x2, x6, y3, \
			 y7, x3, x7, stack_tmp0, stack_tmp1); \
	\
	vmovdqu x0, stack_tmp0; \
	\
	vmovq key, x0; \
	vpshufb .Lpack_bswap(%rip), x0, x0; \
	\
	vpxor x0, y7, y7; \
	vpxor x0, y6, y6; \
	vpxor x0, y5, y5; \
	vpxor x0, y4, y4; \
	vpxor x0, y3, y3; \
	vpxor x0, y2, y2; \
	vpxor x0, y1, y1; \
	vpxor x0, y0, y0; \
	vpxor x0, x7, x7; \
	vpxor x0, x6, x6; \
	vpxor x0, x5, x5; \
	vpxor x0, x4, x4; \
	vpxor x0, x3, x3; \
	vpxor x0, x2, x2; \
	vpxor x0, x1, x1; \
	vpxor stack_tmp0, x0, x0;

#define write_output(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		     y6, y7, rio) \
	vmovdqu x0, 0 * 16(rio); \
	vmovdqu x1, 1 * 16(rio); \
	vmovdqu x2, 2 * 16(rio); \
	vmovdqu x3, 3 * 16(rio); \
	vmovdqu x4, 4 * 16(rio); \
	vmovdqu x5, 5 * 16(rio); \
	vmovdqu x6, 6 * 16(rio); \
	vmovdqu x7, 7 * 16(rio); \
	vmovdqu y0, 8 * 16(rio); \
	vmovdqu y1, 9 * 16(rio); \
	vmovdqu y2, 10 * 16(rio); \
	vmovdqu y3, 11 * 16(rio); \
	vmovdqu y4, 12 * 16(rio); \
	vmovdqu y5, 13 * 16(rio); \
	vmovdqu y6, 14 * 16(rio); \
	vmovdqu y7, 15 * 16(rio);


 
.section	.rodata.cst16, "aM", @progbits, 16
.align 16

#define SHUFB_BYTES(idx) \
	0 + (idx), 4 + (idx), 8 + (idx), 12 + (idx)

.Lshufb_16x16b:
	.byte SHUFB_BYTES(0), SHUFB_BYTES(1), SHUFB_BYTES(2), SHUFB_BYTES(3);

.Lpack_bswap:
	.long 0x00010203
	.long 0x04050607
	.long 0x80808080
	.long 0x80808080

 
.Lpre_tf_lo_s1:
	.byte 0x45, 0xe8, 0x40, 0xed, 0x2e, 0x83, 0x2b, 0x86
	.byte 0x4b, 0xe6, 0x4e, 0xe3, 0x20, 0x8d, 0x25, 0x88
.Lpre_tf_hi_s1:
	.byte 0x00, 0x51, 0xf1, 0xa0, 0x8a, 0xdb, 0x7b, 0x2a
	.byte 0x09, 0x58, 0xf8, 0xa9, 0x83, 0xd2, 0x72, 0x23

 
.Lpre_tf_lo_s4:
	.byte 0x45, 0x40, 0x2e, 0x2b, 0x4b, 0x4e, 0x20, 0x25
	.byte 0x14, 0x11, 0x7f, 0x7a, 0x1a, 0x1f, 0x71, 0x74
.Lpre_tf_hi_s4:
	.byte 0x00, 0xf1, 0x8a, 0x7b, 0x09, 0xf8, 0x83, 0x72
	.byte 0xad, 0x5c, 0x27, 0xd6, 0xa4, 0x55, 0x2e, 0xdf

 
.Lpost_tf_lo_s1:
	.byte 0x3c, 0xcc, 0xcf, 0x3f, 0x32, 0xc2, 0xc1, 0x31
	.byte 0xdc, 0x2c, 0x2f, 0xdf, 0xd2, 0x22, 0x21, 0xd1
.Lpost_tf_hi_s1:
	.byte 0x00, 0xf9, 0x86, 0x7f, 0xd7, 0x2e, 0x51, 0xa8
	.byte 0xa4, 0x5d, 0x22, 0xdb, 0x73, 0x8a, 0xf5, 0x0c

 
.Lpost_tf_lo_s2:
	.byte 0x78, 0x99, 0x9f, 0x7e, 0x64, 0x85, 0x83, 0x62
	.byte 0xb9, 0x58, 0x5e, 0xbf, 0xa5, 0x44, 0x42, 0xa3
.Lpost_tf_hi_s2:
	.byte 0x00, 0xf3, 0x0d, 0xfe, 0xaf, 0x5c, 0xa2, 0x51
	.byte 0x49, 0xba, 0x44, 0xb7, 0xe6, 0x15, 0xeb, 0x18

 
.Lpost_tf_lo_s3:
	.byte 0x1e, 0x66, 0xe7, 0x9f, 0x19, 0x61, 0xe0, 0x98
	.byte 0x6e, 0x16, 0x97, 0xef, 0x69, 0x11, 0x90, 0xe8
.Lpost_tf_hi_s3:
	.byte 0x00, 0xfc, 0x43, 0xbf, 0xeb, 0x17, 0xa8, 0x54
	.byte 0x52, 0xae, 0x11, 0xed, 0xb9, 0x45, 0xfa, 0x06

 
.Linv_shift_row:
	.byte 0x00, 0x0d, 0x0a, 0x07, 0x04, 0x01, 0x0e, 0x0b
	.byte 0x08, 0x05, 0x02, 0x0f, 0x0c, 0x09, 0x06, 0x03

 
.section	.rodata.cst4.L0f0f0f0f, "aM", @progbits, 4
.align 4
.L0f0f0f0f:
	.long 0x0f0f0f0f

.text

SYM_FUNC_START_LOCAL(__camellia_enc_blk16)
	 
	FRAME_BEGIN

	leaq 8 * 16(%rax), %rcx;

	inpack16_post(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		      %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		      %xmm15, %rax, %rcx);

	enc_rounds16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rax, %rcx, 0);

	fls16(%rax, %xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
	      %rcx, %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
	      %xmm15,
	      ((key_table + (8) * 8) + 0)(CTX),
	      ((key_table + (8) * 8) + 4)(CTX),
	      ((key_table + (8) * 8) + 8)(CTX),
	      ((key_table + (8) * 8) + 12)(CTX));

	enc_rounds16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rax, %rcx, 8);

	fls16(%rax, %xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
	      %rcx, %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
	      %xmm15,
	      ((key_table + (16) * 8) + 0)(CTX),
	      ((key_table + (16) * 8) + 4)(CTX),
	      ((key_table + (16) * 8) + 8)(CTX),
	      ((key_table + (16) * 8) + 12)(CTX));

	enc_rounds16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rax, %rcx, 16);

	movl $24, %r8d;
	cmpl $16, key_length(CTX);
	jne .Lenc_max32;

.Lenc_done:
	 
	vmovdqu 0 * 16(%rcx), %xmm8;
	vmovdqu 1 * 16(%rcx), %xmm9;
	vmovdqu 2 * 16(%rcx), %xmm10;
	vmovdqu 3 * 16(%rcx), %xmm11;
	vmovdqu 4 * 16(%rcx), %xmm12;
	vmovdqu 5 * 16(%rcx), %xmm13;
	vmovdqu 6 * 16(%rcx), %xmm14;
	vmovdqu 7 * 16(%rcx), %xmm15;

	outunpack16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		    %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		    %xmm15, (key_table)(CTX, %r8, 8), (%rax), 1 * 16(%rax));

	FRAME_END
	RET;

.align 8
.Lenc_max32:
	movl $32, %r8d;

	fls16(%rax, %xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
	      %rcx, %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
	      %xmm15,
	      ((key_table + (24) * 8) + 0)(CTX),
	      ((key_table + (24) * 8) + 4)(CTX),
	      ((key_table + (24) * 8) + 8)(CTX),
	      ((key_table + (24) * 8) + 12)(CTX));

	enc_rounds16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rax, %rcx, 24);

	jmp .Lenc_done;
SYM_FUNC_END(__camellia_enc_blk16)

SYM_FUNC_START_LOCAL(__camellia_dec_blk16)
	 
	FRAME_BEGIN

	leaq 8 * 16(%rax), %rcx;

	inpack16_post(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		      %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		      %xmm15, %rax, %rcx);

	cmpl $32, %r8d;
	je .Ldec_max32;

.Ldec_max24:
	dec_rounds16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rax, %rcx, 16);

	fls16(%rax, %xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
	      %rcx, %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
	      %xmm15,
	      ((key_table + (16) * 8) + 8)(CTX),
	      ((key_table + (16) * 8) + 12)(CTX),
	      ((key_table + (16) * 8) + 0)(CTX),
	      ((key_table + (16) * 8) + 4)(CTX));

	dec_rounds16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rax, %rcx, 8);

	fls16(%rax, %xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
	      %rcx, %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
	      %xmm15,
	      ((key_table + (8) * 8) + 8)(CTX),
	      ((key_table + (8) * 8) + 12)(CTX),
	      ((key_table + (8) * 8) + 0)(CTX),
	      ((key_table + (8) * 8) + 4)(CTX));

	dec_rounds16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rax, %rcx, 0);

	 
	vmovdqu 0 * 16(%rcx), %xmm8;
	vmovdqu 1 * 16(%rcx), %xmm9;
	vmovdqu 2 * 16(%rcx), %xmm10;
	vmovdqu 3 * 16(%rcx), %xmm11;
	vmovdqu 4 * 16(%rcx), %xmm12;
	vmovdqu 5 * 16(%rcx), %xmm13;
	vmovdqu 6 * 16(%rcx), %xmm14;
	vmovdqu 7 * 16(%rcx), %xmm15;

	outunpack16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		    %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		    %xmm15, (key_table)(CTX), (%rax), 1 * 16(%rax));

	FRAME_END
	RET;

.align 8
.Ldec_max32:
	dec_rounds16(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rax, %rcx, 24);

	fls16(%rax, %xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
	      %rcx, %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
	      %xmm15,
	      ((key_table + (24) * 8) + 8)(CTX),
	      ((key_table + (24) * 8) + 12)(CTX),
	      ((key_table + (24) * 8) + 0)(CTX),
	      ((key_table + (24) * 8) + 4)(CTX));

	jmp .Ldec_max24;
SYM_FUNC_END(__camellia_dec_blk16)

SYM_FUNC_START(camellia_ecb_enc_16way)
	 
	 FRAME_BEGIN

	inpack16_pre(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rdx, (key_table)(CTX));

	 
	movq	%rsi, %rax;

	call __camellia_enc_blk16;

	write_output(%xmm7, %xmm6, %xmm5, %xmm4, %xmm3, %xmm2, %xmm1, %xmm0,
		     %xmm15, %xmm14, %xmm13, %xmm12, %xmm11, %xmm10, %xmm9,
		     %xmm8, %rsi);

	FRAME_END
	RET;
SYM_FUNC_END(camellia_ecb_enc_16way)

SYM_FUNC_START(camellia_ecb_dec_16way)
	 
	 FRAME_BEGIN

	cmpl $16, key_length(CTX);
	movl $32, %r8d;
	movl $24, %eax;
	cmovel %eax, %r8d;  

	inpack16_pre(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rdx, (key_table)(CTX, %r8, 8));

	 
	movq	%rsi, %rax;

	call __camellia_dec_blk16;

	write_output(%xmm7, %xmm6, %xmm5, %xmm4, %xmm3, %xmm2, %xmm1, %xmm0,
		     %xmm15, %xmm14, %xmm13, %xmm12, %xmm11, %xmm10, %xmm9,
		     %xmm8, %rsi);

	FRAME_END
	RET;
SYM_FUNC_END(camellia_ecb_dec_16way)

SYM_FUNC_START(camellia_cbc_dec_16way)
	 
	FRAME_BEGIN

	cmpl $16, key_length(CTX);
	movl $32, %r8d;
	movl $24, %eax;
	cmovel %eax, %r8d;  

	inpack16_pre(%xmm0, %xmm1, %xmm2, %xmm3, %xmm4, %xmm5, %xmm6, %xmm7,
		     %xmm8, %xmm9, %xmm10, %xmm11, %xmm12, %xmm13, %xmm14,
		     %xmm15, %rdx, (key_table)(CTX, %r8, 8));

	 
	subq $(16 * 16), %rsp;
	movq %rsp, %rax;

	call __camellia_dec_blk16;

	addq $(16 * 16), %rsp;

	vpxor (0 * 16)(%rdx), %xmm6, %xmm6;
	vpxor (1 * 16)(%rdx), %xmm5, %xmm5;
	vpxor (2 * 16)(%rdx), %xmm4, %xmm4;
	vpxor (3 * 16)(%rdx), %xmm3, %xmm3;
	vpxor (4 * 16)(%rdx), %xmm2, %xmm2;
	vpxor (5 * 16)(%rdx), %xmm1, %xmm1;
	vpxor (6 * 16)(%rdx), %xmm0, %xmm0;
	vpxor (7 * 16)(%rdx), %xmm15, %xmm15;
	vpxor (8 * 16)(%rdx), %xmm14, %xmm14;
	vpxor (9 * 16)(%rdx), %xmm13, %xmm13;
	vpxor (10 * 16)(%rdx), %xmm12, %xmm12;
	vpxor (11 * 16)(%rdx), %xmm11, %xmm11;
	vpxor (12 * 16)(%rdx), %xmm10, %xmm10;
	vpxor (13 * 16)(%rdx), %xmm9, %xmm9;
	vpxor (14 * 16)(%rdx), %xmm8, %xmm8;
	write_output(%xmm7, %xmm6, %xmm5, %xmm4, %xmm3, %xmm2, %xmm1, %xmm0,
		     %xmm15, %xmm14, %xmm13, %xmm12, %xmm11, %xmm10, %xmm9,
		     %xmm8, %rsi);

	FRAME_END
	RET;
SYM_FUNC_END(camellia_cbc_dec_16way)
