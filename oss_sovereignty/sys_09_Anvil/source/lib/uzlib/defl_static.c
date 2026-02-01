 

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

 

static void outbits(uzlib_lz77_state_t *state, unsigned long bits, int nbits)
{
    assert(state->noutbits + nbits <= 32);
    state->outbits |= bits << state->noutbits;
    state->noutbits += nbits;
    while (state->noutbits >= 8) {
        state->dest_write_cb(state->dest_write_data, state->outbits & 0xFF);
        state->outbits >>= 8;
        state->noutbits -= 8;
    }
}

static const unsigned char mirrornibbles[16] = {
    0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
    0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf,
};

static unsigned int mirrorbyte(unsigned int b) {
    return mirrornibbles[b & 0xf] << 4 | mirrornibbles[b >> 4];
}

static int int_log2(int x) {
    int r = 0;
    while (x >>= 1) {
        ++r;
    }
    return r;
}

static void uzlib_literal(uzlib_lz77_state_t *state, unsigned char c)
{
    if (c <= 143) {
         
        outbits(state, mirrorbyte(0x30 + c), 8);
    } else {
         
        outbits(state, 1 + 2 * mirrorbyte(0x90 - 144 + c), 9);
    }
}

static void uzlib_match(uzlib_lz77_state_t *state, int distance, int len)
{
    assert(distance >= 1 && distance <= 32768);
    distance -= 1;

    while (len > 0) {
        int thislen;

         
        thislen = (len > 260 ? 258 : len <= 258 ? len : len - 3);
        len -= thislen;

        assert(thislen >= 3 && thislen <= 258);

        thislen -= 3;
        int lcode = 28;
        int x = int_log2(thislen);
        int y;
        if (thislen < 255) {
            if (x) {
                --x;
            }
            y = (thislen >> (x ? x - 1 : 0)) & 3;
            lcode = x * 4 + y;
        }

         
        if (lcode <= 22) {
            outbits(state, mirrorbyte((lcode + 1) * 2), 7);
        } else {
            outbits(state, mirrorbyte(lcode + 169), 8);
        }

         
        if (thislen < 255 && x > 1) {
            int extrabits = x - 1;
            int lmin = (thislen >> extrabits) << extrabits;
            outbits(state, thislen - lmin, extrabits);
        }

        x = int_log2(distance);
        y = (distance >> (x ? x - 1 : 0)) & 1;

         
        outbits(state, mirrorbyte((x * 2 + y) * 8), 5);

         
        if (x > 1) {
            int dextrabits = x - 1;
            int dmin = (distance >> dextrabits) << dextrabits;
            outbits(state, distance - dmin, dextrabits);
        }
    }
}

void uzlib_start_block(uzlib_lz77_state_t *state)
{
    
    
    outbits(state, 3, 3);
}

void uzlib_finish_block(uzlib_lz77_state_t *state)
{
    
    
    outbits(state, 0, 14);
}
