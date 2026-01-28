#include "liblzo_interface.h"
#define M2_MIN_LEN      3
#define M2_MAX_LEN      8
#define M3_MAX_LEN      33
#define M4_MAX_LEN      9
#define M1_MAX_OFFSET   0x0400
#define M2_MAX_OFFSET   0x0800
#define M3_MAX_OFFSET   0x4000
#define M4_MAX_OFFSET   0xbfff
#define M1_MARKER       0
#define M3_MARKER       32
#define M4_MARKER       16
#define MX_MAX_OFFSET   (M1_MAX_OFFSET + M2_MAX_OFFSET)
#define MIN_LOOKAHEAD   (M2_MAX_LEN + 1)
#define LZO_EOF_CODE
#define GINDEX(m_pos,m_off,dict,dindex,in)    m_pos = dict[dindex]
#define DX2(p,s1,s2) \
        (((((unsigned)((p)[2]) << (s2)) ^ (p)[1]) << (s1)) ^ (p)[0])
#define DX3(p,s1,s2,s3) ((DX2((p)+1,s2,s3) << (s1)) ^ (p)[0])
#define D_SIZE        (1U << D_BITS)
#define D_MASK        ((1U << D_BITS) - 1)
#define D_HIGH        ((D_MASK >> 1) + 1)
#define LZO_CHECK_MPOS_NON_DET(m_pos,m_off,in,ip,max_offset) \
    ( \
        m_pos = ip - (unsigned)(ip - m_pos), \
        ((uintptr_t)m_pos < (uintptr_t)in \
	|| (m_off = (unsigned)(ip - m_pos)) <= 0 \
	|| m_off > max_offset) \
    )
#define DENTRY(p,in)                      (p)
#define UPDATE_I(dict,drun,index,p,in)    dict[index] = DENTRY(p,in)
#define DMS(v,s)  ((unsigned) (((v) & (D_MASK >> (s))) << (s)))
#define DM(v)     ((unsigned) ((v) & D_MASK))
#define DMUL(a,b) ((unsigned) ((a) * (b)))
#define pd(a,b)  ((unsigned)((a)-(b)))
#    define TEST_IP             (ip < ip_end)
#    define NEED_IP(x) \
            if ((unsigned)(ip_end - ip) < (unsigned)(x))  goto input_overrun
#    define TEST_IV(x)          if ((x) > (unsigned)0 - (511)) goto input_overrun
#    undef TEST_OP               
#    define TEST_OP             1
#    define NEED_OP(x) \
            if ((unsigned)(op_end - op) < (unsigned)(x))  goto output_overrun
#    define TEST_OV(x)          if ((x) > (unsigned)0 - (511)) goto output_overrun
#define HAVE_ANY_OP 1
#  define TEST_LB(m_pos)        if (m_pos < out || m_pos >= op) goto lookbehind_overrun
