#ifndef _AESTAB_H
#define	_AESTAB_H
#ifdef	__cplusplus
extern "C" {
#endif
#include <sys/types.h>
#define	t_dec(m, n) t_##m##n
#define	t_set(m, n) t_##m##n
#define	t_use(m, n) t_##m##n
#if defined(DO_TABLES) && defined(FIXED_TABLES)
#define	d_1(t, n, b, e)		 static const t n[256]    =   b(e)
#define	d_4(t, n, b, e, f, g, h) static const t n[4][256] = \
					{b(e), b(f), b(g), b(h)}
static const uint32_t t_dec(r, c)[RC_LENGTH] = rc_data(w0);
#else
#define	d_1(t, n, b, e)			static const t n[256]
#define	d_4(t, n, b, e, f, g, h)	static const t n[4][256]
static const uint32_t t_dec(r, c)[RC_LENGTH];
#endif
#if defined(SBX_SET)
	d_1(uint8_t, t_dec(s, box), sb_data, h0);
#endif
#if defined(ISB_SET)
	d_1(uint8_t, t_dec(i, box), isb_data, h0);
#endif
#if defined(FT1_SET)
	d_1(uint32_t, t_dec(f, n), sb_data, u0);
#endif
#if defined(FT4_SET)
	d_4(uint32_t, t_dec(f, n), sb_data, u0, u1, u2, u3);
#endif
#if defined(FL1_SET)
	d_1(uint32_t, t_dec(f, l), sb_data, w0);
#endif
#if defined(FL4_SET)
	d_4(uint32_t, t_dec(f, l), sb_data, w0, w1, w2, w3);
#endif
#if defined(IT1_SET)
	d_1(uint32_t, t_dec(i, n), isb_data, v0);
#endif
#if defined(IT4_SET)
	d_4(uint32_t, t_dec(i, n), isb_data, v0, v1, v2, v3);
#endif
#if defined(IL1_SET)
	d_1(uint32_t, t_dec(i, l), isb_data, w0);
#endif
#if defined(IL4_SET)
	d_4(uint32_t, t_dec(i, l), isb_data, w0, w1, w2, w3);
#endif
#if defined(LS1_SET)
#if defined(FL1_SET)
#undef  LS1_SET
#else
	d_1(uint32_t, t_dec(l, s), sb_data, w0);
#endif
#endif
#if defined(LS4_SET)
#if defined(FL4_SET)
#undef  LS4_SET
#else
	d_4(uint32_t, t_dec(l, s), sb_data, w0, w1, w2, w3);
#endif
#endif
#if defined(IM1_SET)
	d_1(uint32_t, t_dec(i, m), mm_data, v0);
#endif
#if defined(IM4_SET)
	d_4(uint32_t, t_dec(i, m), mm_data, v0, v1, v2, v3);
#endif
#ifdef	__cplusplus
}
#endif
#endif	 
