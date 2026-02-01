 

#include <assert.h>
#include "uzlib.h"

#define UZLIB_DUMP_ARRAY(heading, arr, size) \
    { \
        printf("%s", heading); \
        for (int i = 0; i < size; ++i) { \
            printf(" %d", (arr)[i]); \
        } \
        printf("\n"); \
    }

uint32_t tinf_get_le_uint32(uzlib_uncomp_t *d);
uint32_t tinf_get_be_uint32(uzlib_uncomp_t *d);

 

typedef struct {
    unsigned char length_bits : 3;
    unsigned short length_base : 9;
    unsigned char dist_bits : 4;
    unsigned short dist_base : 15;
} lookup_table_entry_t;

const lookup_table_entry_t lookup_table[30] = {
    {0, 3, 0, 1},
    {0, 4, 0, 2},
    {0, 5, 0, 3},
    {0, 6, 0, 4},
    {0, 7, 1, 5},
    {0, 8, 1, 7},
    {0, 9, 2, 9},
    {0, 10, 2, 13},
    {1, 11, 3, 17},
    {1, 13, 3, 25},
    {1, 15, 4, 33},
    {1, 17, 4, 49},
    {2, 19, 5, 65},
    {2, 23, 5, 97},
    {2, 27, 6, 129},
    {2, 31, 6, 193},
    {3, 35, 7, 257},
    {3, 43, 7, 385},
    {3, 51, 8, 513},
    {3, 59, 8, 769},
    {4, 67, 9, 1025},
    {4, 83, 9, 1537},
    {4, 99, 10, 2049},
    {4, 115, 10, 3073},
    {5, 131, 11, 4097},
    {5, 163, 11, 6145},
    {5, 195, 12, 8193},
    {5, 227, 12, 12289},
    {0, 258, 13, 16385},
    {0, 0, 13, 24577},
};

 
const unsigned char clcidx[] = {
   16, 17, 18, 0, 8, 7, 9, 6,
   10, 5, 11, 4, 12, 3, 13, 2,
   14, 1, 15
};

 

 
static void tinf_build_fixed_trees(TINF_TREE *lt, TINF_TREE *dt)
{
   int i;

    
   for (i = 0; i < 7; ++i) lt->table[i] = 0;

   lt->table[7] = 24;
   lt->table[8] = 152;
   lt->table[9] = 112;

   for (i = 0; i < 24; ++i) lt->trans[i] = 256 + i;
   for (i = 0; i < 144; ++i) lt->trans[24 + i] = i;
   for (i = 0; i < 8; ++i) lt->trans[24 + 144 + i] = 280 + i;
   for (i = 0; i < 112; ++i) lt->trans[24 + 144 + 8 + i] = 144 + i;

    
   for (i = 0; i < 5; ++i) dt->table[i] = 0;

   dt->table[5] = 32;

   for (i = 0; i < 32; ++i) dt->trans[i] = i;
}

 
static void tinf_build_tree(TINF_TREE *t, const unsigned char *lengths, unsigned int num)
{
   unsigned short offs[16];
   unsigned int i, sum;

    
   for (i = 0; i < 16; ++i) t->table[i] = 0;

    
   for (i = 0; i < num; ++i) t->table[lengths[i]]++;

   #if UZLIB_CONF_DEBUG_LOG >= 2
   UZLIB_DUMP_ARRAY("codelen counts:", t->table, TINF_ARRAY_SIZE(t->table));
   #endif

    
   t->table[0] = 0;

    
   for (sum = 0, i = 0; i < 16; ++i)
   {
      offs[i] = sum;
      sum += t->table[i];
   }

   #if UZLIB_CONF_DEBUG_LOG >= 2
   UZLIB_DUMP_ARRAY("codelen offsets:", offs, TINF_ARRAY_SIZE(offs));
   #endif

    
   for (i = 0; i < num; ++i)
   {
      if (lengths[i]) t->trans[offs[lengths[i]]++] = i;
   }
}

 

unsigned char uzlib_get_byte(uzlib_uncomp_t *d)
{
     
    if (d->source < d->source_limit) {
        return *d->source++;
    }

     
    if (d->source_read_cb && !d->eof) {
        int val = d->source_read_cb(d->source_read_data);
        if (val >= 0) {
            return (unsigned char)val;
        }
    }

     
    d->eof = true;

    return 0;
}

uint32_t tinf_get_le_uint32(uzlib_uncomp_t *d)
{
    uint32_t val = 0;
    int i;
    for (i = 4; i--;) {
        val = val >> 8 | ((uint32_t)uzlib_get_byte(d)) << 24;
    }
    return val;
}

uint32_t tinf_get_be_uint32(uzlib_uncomp_t *d)
{
    uint32_t val = 0;
    int i;
    for (i = 4; i--;) {
        val = val << 8 | uzlib_get_byte(d);
    }
    return val;
}

 
static int tinf_getbit(uzlib_uncomp_t *d)
{
   unsigned int bit;

    
   if (!d->bitcount--)
   {
       
      d->tag = uzlib_get_byte(d);
      d->bitcount = 7;
   }

    
   bit = d->tag & 0x01;
   d->tag >>= 1;

   return bit;
}

 
static unsigned int tinf_read_bits(uzlib_uncomp_t *d, int num, int base)
{
   unsigned int val = 0;

    
   if (num)
   {
      unsigned int limit = 1 << (num);
      unsigned int mask;

      for (mask = 1; mask < limit; mask *= 2)
         if (tinf_getbit(d)) val += mask;
   }

   return val + base;
}

 
static int tinf_decode_symbol(uzlib_uncomp_t *d, TINF_TREE *t)
{
   int sum = 0, cur = 0, len = 0;

    
   do {

      cur = 2*cur + tinf_getbit(d);

      if (++len == TINF_ARRAY_SIZE(t->table)) {
         return UZLIB_DATA_ERROR;
      }

      sum += t->table[len];
      cur -= t->table[len];

   } while (cur >= 0);

   sum += cur;
   #if UZLIB_CONF_PARANOID_CHECKS
   if (sum < 0 || sum >= TINF_ARRAY_SIZE(t->trans)) {
      return UZLIB_DATA_ERROR;
   }
   #endif

   return t->trans[sum];
}

 
static int tinf_decode_trees(uzlib_uncomp_t *d, TINF_TREE *lt, TINF_TREE *dt)
{
    
   unsigned char lengths[288+32];
   unsigned int hlit, hdist, hclen, hlimit;
   unsigned int i, num, length;

    
   hlit = tinf_read_bits(d, 5, 257);

    
   hdist = tinf_read_bits(d, 5, 1);

    
   hclen = tinf_read_bits(d, 4, 4);

   for (i = 0; i < 19; ++i) lengths[i] = 0;

    
   for (i = 0; i < hclen; ++i)
   {
       
      unsigned int clen = tinf_read_bits(d, 3, 0);

      lengths[clcidx[i]] = clen;
   }

    
   tinf_build_tree(lt, lengths, 19);

    
   hlimit = hlit + hdist;
   for (num = 0; num < hlimit; )
   {
      int sym = tinf_decode_symbol(d, lt);
      unsigned char fill_value = 0;
      int lbits, lbase = 3;

       
      if (sym < 0) return sym;

      switch (sym)
      {
      case 16:
          
         if (num == 0) return UZLIB_DATA_ERROR;
         fill_value = lengths[num - 1];
         lbits = 2;
         break;
      case 17:
          
         lbits = 3;
         break;
      case 18:
          
         lbits = 7;
         lbase = 11;
         break;
      default:
          
         lengths[num++] = sym;
          
         continue;
      }

       
      length = tinf_read_bits(d, lbits, lbase);
      if (num + length > hlimit) return UZLIB_DATA_ERROR;
      for (; length; --length)
      {
         lengths[num++] = fill_value;
      }
   }

   #if UZLIB_CONF_DEBUG_LOG >= 2
   printf("lit code lengths (%d):", hlit);
   UZLIB_DUMP_ARRAY("", lengths, hlit);
   printf("dist code lengths (%d):", hdist);
   UZLIB_DUMP_ARRAY("", lengths + hlit, hdist);
   #endif

   #if UZLIB_CONF_PARANOID_CHECKS
    
   if (lengths[256] == 0) {
      return UZLIB_DATA_ERROR;
   }
   #endif

    
   tinf_build_tree(lt, lengths, hlit);
   tinf_build_tree(dt, lengths + hlit, hdist);

   return UZLIB_OK;
}

 

 
static int tinf_inflate_block_data(uzlib_uncomp_t *d, TINF_TREE *lt, TINF_TREE *dt)
{
    if (d->curlen == 0) {
        unsigned int offs;
        int dist;
        int sym = tinf_decode_symbol(d, lt);
         

        if (d->eof) {
            return UZLIB_DATA_ERROR;
        }

         
        if (sym < 256) {
            TINF_PUT(d, sym);
            return UZLIB_OK;
        }

         
        if (sym == 256) {
            return UZLIB_DONE;
        }

         
        sym -= 257;
        if (sym >= 29) {
            return UZLIB_DATA_ERROR;
        }

         
        d->curlen = tinf_read_bits(d, lookup_table[sym].length_bits, lookup_table[sym].length_base);

        dist = tinf_decode_symbol(d, dt);
        if (dist >= 30) {
            return UZLIB_DATA_ERROR;
        }

         
        offs = tinf_read_bits(d, lookup_table[dist].dist_bits, lookup_table[dist].dist_base);

         
        if (d->dict_ring) {
            if (offs > d->dict_size) {
                return UZLIB_DICT_ERROR;
            }
             

            d->lzOff = d->dict_idx - offs;
            if (d->lzOff < 0) {
                d->lzOff += d->dict_size;
            }
        } else {
             
            if (offs > (unsigned int)(d->dest - d->dest_start)) {
                return UZLIB_DATA_ERROR;
            }
            d->lzOff = -offs;
        }
    }

     
    if (d->dict_ring) {
        TINF_PUT(d, d->dict_ring[d->lzOff]);
        if ((unsigned)++d->lzOff == d->dict_size) {
            d->lzOff = 0;
        }
    } else {
        d->dest[0] = d->dest[d->lzOff];
        d->dest++;
    }
    d->curlen--;
    return UZLIB_OK;
}

 
static int tinf_inflate_uncompressed_block(uzlib_uncomp_t *d)
{
    if (d->curlen == 0) {
        unsigned int length, invlength;

         
        length = uzlib_get_byte(d);
        length += 256 * uzlib_get_byte(d);
         
        invlength = uzlib_get_byte(d);
        invlength += 256 * uzlib_get_byte(d);
         
        if (length != (~invlength & 0x0000ffff)) return UZLIB_DATA_ERROR;

         
        d->curlen = length + 1;

         
        d->bitcount = 0;
    }

    if (--d->curlen == 0) {
        return UZLIB_DONE;
    }

    unsigned char c = uzlib_get_byte(d);
    TINF_PUT(d, c);
    return UZLIB_OK;
}

 

 
void uzlib_uncompress_init(uzlib_uncomp_t *d, void *dict, unsigned int dictLen)
{
   d->eof = 0;
   d->bitcount = 0;
   d->bfinal = 0;
   d->btype = -1;
   d->dict_size = dictLen;
   d->dict_ring = dict;
   d->dict_idx = 0;
   d->curlen = 0;
}

 
int uzlib_uncompress(uzlib_uncomp_t *d)
{
    do {
        int res;

         
        if (d->btype == -1) {
next_blk:
             
            d->bfinal = tinf_getbit(d);
             
            d->btype = tinf_read_bits(d, 2, 0);

            #if UZLIB_CONF_DEBUG_LOG >= 1
            printf("Started new block: type=%d final=%d\n", d->btype, d->bfinal);
            #endif

            if (d->btype == 1) {
                 
                tinf_build_fixed_trees(&d->ltree, &d->dtree);
            } else if (d->btype == 2) {
                 
                res = tinf_decode_trees(d, &d->ltree, &d->dtree);
                if (res != UZLIB_OK) {
                    return res;
                }
            }
        }

         
        switch (d->btype)
        {
        case 0:
             
            res = tinf_inflate_uncompressed_block(d);
            break;
        case 1:
        case 2:
             
             
            res = tinf_inflate_block_data(d, &d->ltree, &d->dtree);
            break;
        default:
            return UZLIB_DATA_ERROR;
        }

        if (res == UZLIB_DONE && !d->bfinal) {
             
            goto next_blk;
        }

        if (res != UZLIB_OK) {
            return res;
        }

    } while (d->dest < d->dest_limit);

    return UZLIB_OK;
}

 
int uzlib_uncompress_chksum(uzlib_uncomp_t *d)
{
    int res;
    unsigned char *data = d->dest;

    res = uzlib_uncompress(d);

    if (res < 0) return res;

    switch (d->checksum_type) {

    case UZLIB_CHKSUM_ADLER:
        d->checksum = uzlib_adler32(data, d->dest - data, d->checksum);
        break;

    case UZLIB_CHKSUM_CRC:
        d->checksum = uzlib_crc32(data, d->dest - data, d->checksum);
        break;
    }

    if (res == UZLIB_DONE) {
        unsigned int val;

        switch (d->checksum_type) {

        case UZLIB_CHKSUM_ADLER:
            val = tinf_get_be_uint32(d);
            if (d->checksum != val) {
                return UZLIB_CHKSUM_ERROR;
            }
            break;

        case UZLIB_CHKSUM_CRC:
            val = tinf_get_le_uint32(d);
            if (~d->checksum != val) {
                return UZLIB_CHKSUM_ERROR;
            }
            
            val = tinf_get_le_uint32(d);
            break;
        }
    }

    return res;
}
