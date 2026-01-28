#ifndef DEFUTIL_H
#define DEFUTIL_H

#include <linux/zutil.h>

#define Assert(err, str) 
#define Trace(dummy) 
#define Tracev(dummy) 
#define Tracecv(err, dummy) 
#define Tracevv(dummy) 



#define LENGTH_CODES 29


#define LITERALS  256


#define L_CODES (LITERALS+1+LENGTH_CODES)


#define D_CODES   30


#define BL_CODES  19


#define HEAP_SIZE (2*L_CODES+1)


#define MAX_BITS 15


#define INIT_STATE    42
#define BUSY_STATE   113
#define FINISH_STATE 666




typedef struct ct_data_s {
    union {
        ush  freq;       
        ush  code;       
    } fc;
    union {
        ush  dad;        
        ush  len;        
    } dl;
} ct_data;

#define Freq fc.freq
#define Code fc.code
#define Dad  dl.dad
#define Len  dl.len

typedef struct static_tree_desc_s  static_tree_desc;

typedef struct tree_desc_s {
    ct_data *dyn_tree;           
    int     max_code;            
    static_tree_desc *stat_desc; 
} tree_desc;

typedef ush Pos;
typedef unsigned IPos;



typedef struct deflate_state {
    z_streamp strm;      
    int   status;        
    Byte *pending_buf;   
    ulg   pending_buf_size; 
    Byte *pending_out;   
    int   pending;       
    int   noheader;      
    Byte  data_type;     
    Byte  method;        
    int   last_flush;    

                

    uInt  w_size;        
    uInt  w_bits;        
    uInt  w_mask;        

    Byte *window;
    

    ulg window_size;
    

    Pos *prev;
    

    Pos *head; 

    uInt  ins_h;          
    uInt  hash_size;      
    uInt  hash_bits;      
    uInt  hash_mask;      

    uInt  hash_shift;
    

    long block_start;
    

    uInt match_length;           
    IPos prev_match;             
    int match_available;         
    uInt strstart;               
    uInt match_start;            
    uInt lookahead;              

    uInt prev_length;
    

    uInt max_chain_length;
    

    uInt max_lazy_match;
    
#   define max_insert_length  max_lazy_match
    

    int level;    
    int strategy; 

    uInt good_match;
    

    int nice_match; 

                
    
    struct ct_data_s dyn_ltree[HEAP_SIZE];   
    struct ct_data_s dyn_dtree[2*D_CODES+1]; 
    struct ct_data_s bl_tree[2*BL_CODES+1];  

    struct tree_desc_s l_desc;               
    struct tree_desc_s d_desc;               
    struct tree_desc_s bl_desc;              

    ush bl_count[MAX_BITS+1];
    

    int heap[2*L_CODES+1];      
    int heap_len;               
    int heap_max;               
    

    uch depth[2*L_CODES+1];
    

    uch *l_buf;          

    uInt  lit_bufsize;
    

    uInt last_lit;      

    ush *d_buf;
    

    ulg opt_len;        
    ulg static_len;     
    ulg compressed_len; 
    uInt matches;       
    int last_eob_len;   

#ifdef DEBUG_ZLIB
    ulg bits_sent;      
#endif

    ush bi_buf;
    
    int bi_valid;
    

} deflate_state;

#ifdef CONFIG_ZLIB_DFLTCC
#define zlib_deflate_window_memsize(windowBits) \
	(2 * (1 << (windowBits)) * sizeof(Byte) + PAGE_SIZE)
#else
#define zlib_deflate_window_memsize(windowBits) \
	(2 * (1 << (windowBits)) * sizeof(Byte))
#endif
#define zlib_deflate_prev_memsize(windowBits) \
	((1 << (windowBits)) * sizeof(Pos))
#define zlib_deflate_head_memsize(memLevel) \
	((1 << ((memLevel)+7)) * sizeof(Pos))
#define zlib_deflate_overlay_memsize(memLevel) \
	((1 << ((memLevel)+6)) * (sizeof(ush)+2))


#define put_byte(s, c) {s->pending_buf[s->pending++] = (c);}


#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)


#define MAX_DIST(s)  ((s)->w_size-MIN_LOOKAHEAD)


        
void zlib_tr_init         (deflate_state *s);
int  zlib_tr_tally        (deflate_state *s, unsigned dist, unsigned lc);
ulg  zlib_tr_flush_block  (deflate_state *s, char *buf, ulg stored_len,
			   int eof);
void zlib_tr_align        (deflate_state *s);
void zlib_tr_stored_block (deflate_state *s, char *buf, ulg stored_len,
			   int eof);
void zlib_tr_stored_type_only (deflate_state *);



#define put_short(s, w) { \
    put_byte(s, (uch)((w) & 0xff)); \
    put_byte(s, (uch)((ush)(w) >> 8)); \
}


static inline unsigned  bi_reverse(
    unsigned code, 
    int len        
)
{
    register unsigned res = 0;
    do {
        res |= code & 1;
        code >>= 1, res <<= 1;
    } while (--len > 0);
    return res >> 1;
}


static inline void bi_flush(deflate_state *s)
{
    if (s->bi_valid == 16) {
        put_short(s, s->bi_buf);
        s->bi_buf = 0;
        s->bi_valid = 0;
    } else if (s->bi_valid >= 8) {
        put_byte(s, (Byte)s->bi_buf);
        s->bi_buf >>= 8;
        s->bi_valid -= 8;
    }
}


static inline void bi_windup(deflate_state *s)
{
    if (s->bi_valid > 8) {
        put_short(s, s->bi_buf);
    } else if (s->bi_valid > 0) {
        put_byte(s, (Byte)s->bi_buf);
    }
    s->bi_buf = 0;
    s->bi_valid = 0;
#ifdef DEBUG_ZLIB
    s->bits_sent = (s->bits_sent+7) & ~7;
#endif
}

typedef enum {
    need_more,      
    block_done,     
    finish_started, 
    finish_done     
} block_state;

#define Buf_size (8 * 2*sizeof(char))



#ifdef DEBUG_ZLIB
static void send_bits      (deflate_state *s, int value, int length);

static void send_bits(
    deflate_state *s,
    int value,  
    int length  
)
{
    Tracevv((stderr," l %2d v %4x ", length, value));
    Assert(length > 0 && length <= 15, "invalid length");
    s->bits_sent += (ulg)length;

    
    if (s->bi_valid > (int)Buf_size - length) {
        s->bi_buf |= (value << s->bi_valid);
        put_short(s, s->bi_buf);
        s->bi_buf = (ush)value >> (Buf_size - s->bi_valid);
        s->bi_valid += length - Buf_size;
    } else {
        s->bi_buf |= value << s->bi_valid;
        s->bi_valid += length;
    }
}
#else 

#define send_bits(s, value, length) \
{ int len = length;\
  if (s->bi_valid > (int)Buf_size - len) {\
    int val = value;\
    s->bi_buf |= (val << s->bi_valid);\
    put_short(s, s->bi_buf);\
    s->bi_buf = (ush)val >> (Buf_size - s->bi_valid);\
    s->bi_valid += len - Buf_size;\
  } else {\
    s->bi_buf |= (value) << s->bi_valid;\
    s->bi_valid += len;\
  }\
}
#endif 

static inline void zlib_tr_send_bits(
    deflate_state *s,
    int value,
    int length
)
{
    send_bits(s, value, length);
}


static inline void flush_pending(
	z_streamp strm
)
{
    unsigned len;
    deflate_state *s = (deflate_state *) strm->state;

    bi_flush(s);
    len = s->pending;
    if (len > strm->avail_out) len = strm->avail_out;
    if (len == 0) return;

    if (strm->next_out != NULL) {
	memcpy(strm->next_out, s->pending_out, len);
	strm->next_out += len;
    }
    s->pending_out += len;
    strm->total_out += len;
    strm->avail_out  -= len;
    s->pending -= len;
    if (s->pending == 0) {
        s->pending_out = s->pending_buf;
    }
}
#endif 
