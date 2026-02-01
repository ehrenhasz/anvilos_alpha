

#include "../zlib_deflate/defutil.h"
#include "dfltcc_util.h"
#include "dfltcc_deflate.h"
#include <asm/setup.h>
#include <linux/export.h>
#include <linux/zutil.h>

#define GET_DFLTCC_DEFLATE_STATE(state) ((struct dfltcc_deflate_state *)GET_DFLTCC_STATE(state))

 
int dfltcc_can_deflate(
    z_streamp strm
)
{
    deflate_state *state = (deflate_state *)strm->state;
    struct dfltcc_deflate_state *dfltcc_state = GET_DFLTCC_DEFLATE_STATE(state);

     
    if (zlib_dfltcc_support == ZLIB_DFLTCC_DISABLED ||
            zlib_dfltcc_support == ZLIB_DFLTCC_INFLATE_ONLY)
        return 0;

     
    if (!dfltcc_are_params_ok(state->level, state->w_bits, state->strategy,
                              dfltcc_state->level_mask))
        return 0;

     
    if (!is_bit_set(dfltcc_state->common.af.fns, DFLTCC_GDHT) ||
            !is_bit_set(dfltcc_state->common.af.fns, DFLTCC_CMPR) ||
            !is_bit_set(dfltcc_state->common.af.fmts, DFLTCC_FMT0))
        return 0;

    return 1;
}
EXPORT_SYMBOL(dfltcc_can_deflate);

void dfltcc_reset_deflate_state(z_streamp strm) {
    deflate_state *state = (deflate_state *)strm->state;
    struct dfltcc_deflate_state *dfltcc_state = GET_DFLTCC_DEFLATE_STATE(state);

    dfltcc_reset_state(&dfltcc_state->common);

     
    if (zlib_dfltcc_support == ZLIB_DFLTCC_FULL_DEBUG)
        dfltcc_state->level_mask = DFLTCC_LEVEL_MASK_DEBUG;
    else
        dfltcc_state->level_mask = DFLTCC_LEVEL_MASK;
    dfltcc_state->block_size = DFLTCC_BLOCK_SIZE;
    dfltcc_state->block_threshold = DFLTCC_FIRST_FHT_BLOCK_SIZE;
    dfltcc_state->dht_threshold = DFLTCC_DHT_MIN_SAMPLE_SIZE;
}
EXPORT_SYMBOL(dfltcc_reset_deflate_state);

static void dfltcc_gdht(
    z_streamp strm
)
{
    deflate_state *state = (deflate_state *)strm->state;
    struct dfltcc_param_v0 *param = &GET_DFLTCC_STATE(state)->param;
    size_t avail_in = strm->avail_in;

    dfltcc(DFLTCC_GDHT,
           param, NULL, NULL,
           &strm->next_in, &avail_in, NULL);
}

static dfltcc_cc dfltcc_cmpr(
    z_streamp strm
)
{
    deflate_state *state = (deflate_state *)strm->state;
    struct dfltcc_param_v0 *param = &GET_DFLTCC_STATE(state)->param;
    size_t avail_in = strm->avail_in;
    size_t avail_out = strm->avail_out;
    dfltcc_cc cc;

    cc = dfltcc(DFLTCC_CMPR | HBT_CIRCULAR,
                param, &strm->next_out, &avail_out,
                &strm->next_in, &avail_in, state->window);
    strm->total_in += (strm->avail_in - avail_in);
    strm->total_out += (strm->avail_out - avail_out);
    strm->avail_in = avail_in;
    strm->avail_out = avail_out;
    return cc;
}

static void send_eobs(
    z_streamp strm,
    const struct dfltcc_param_v0 *param
)
{
    deflate_state *state = (deflate_state *)strm->state;

    zlib_tr_send_bits(
          state,
          bi_reverse(param->eobs >> (15 - param->eobl), param->eobl),
          param->eobl);
    flush_pending(strm);
    if (state->pending != 0) {
         
        memmove(state->pending_buf, state->pending_out, state->pending);
        state->pending_out = state->pending_buf;
    }
#ifdef ZLIB_DEBUG
    state->compressed_len += param->eobl;
#endif
}

int dfltcc_deflate(
    z_streamp strm,
    int flush,
    block_state *result
)
{
    deflate_state *state = (deflate_state *)strm->state;
    struct dfltcc_deflate_state *dfltcc_state = GET_DFLTCC_DEFLATE_STATE(state);
    struct dfltcc_param_v0 *param = &dfltcc_state->common.param;
    uInt masked_avail_in;
    dfltcc_cc cc;
    int need_empty_block;
    int soft_bcc;
    int no_flush;

    if (!dfltcc_can_deflate(strm)) {
         
        if (flush == Z_FULL_FLUSH)
            param->hl = 0;
        return 0;
    }

again:
    masked_avail_in = 0;
    soft_bcc = 0;
    no_flush = flush == Z_NO_FLUSH;

     
    if (strm->avail_in == 0 && !param->cf) {
         
        if (!no_flush && param->bcf) {
            send_eobs(strm, param);
            param->bcf = 0;
        }
         
        if (flush == Z_FINISH)
            return 0;
         
        if (flush == Z_FULL_FLUSH)
            param->hl = 0;
         
        *result = no_flush ? need_more : block_done;
        return 1;
    }

     
    if (param->bcf && no_flush &&
            strm->total_in > dfltcc_state->block_threshold &&
            strm->avail_in >= dfltcc_state->dht_threshold) {
        if (param->cf) {
             
            masked_avail_in += strm->avail_in;
            strm->avail_in = 0;
            no_flush = 0;
        } else {
             
            send_eobs(strm, param);
            param->bcf = 0;
            dfltcc_state->block_threshold =
                strm->total_in + dfltcc_state->block_size;
        }
    }

     
    if (strm->avail_out == 0) {
        *result = need_more;
        return 1;
    }

     
    if (no_flush && strm->avail_in > dfltcc_state->block_size) {
        masked_avail_in += (strm->avail_in - dfltcc_state->block_size);
        strm->avail_in = dfltcc_state->block_size;
    }

     
    need_empty_block = flush == Z_FINISH && param->bcf && !param->bhf;

     
    param->cvt = CVT_ADLER32;
    if (!no_flush)
         
        soft_bcc = 1;
    if (flush == Z_FINISH && !param->bcf)
         
        param->bhf = 1;
     
    Assert(state->pending == 0, "There must be no pending bytes");
    Assert(state->bi_valid < 8, "There must be less than 8 pending bits");
    param->sbb = (unsigned int)state->bi_valid;
    if (param->sbb > 0)
        *strm->next_out = (Byte)state->bi_buf;
     
    param->nt = 0;
    param->cv = strm->adler;

     
    if (!param->bcf) {
        if (strm->total_in == 0 && dfltcc_state->block_threshold > 0) {
            param->htt = HTT_FIXED;
        }
        else {
            param->htt = HTT_DYNAMIC;
            dfltcc_gdht(strm);
        }
    }

     
    do {
        cc = dfltcc_cmpr(strm);
        if (strm->avail_in < 4096 && masked_avail_in > 0)
             
            break;
    } while (cc == DFLTCC_CC_AGAIN);

     
    strm->msg = oesc_msg(dfltcc_state->common.msg, param->oesc);
    state->bi_valid = param->sbb;
    if (state->bi_valid == 0)
        state->bi_buf = 0;  
    else
        state->bi_buf = *strm->next_out & ((1 << state->bi_valid) - 1);
    strm->adler = param->cv;

     
    strm->avail_in += masked_avail_in;
    masked_avail_in = 0;

     
    Assert(cc != DFLTCC_CC_OP2_CORRUPT || param->oesc == 0, "BUG");

     
    if (cc == DFLTCC_CC_OK) {
        if (soft_bcc) {
            send_eobs(strm, param);
            param->bcf = 0;
            dfltcc_state->block_threshold =
                strm->total_in + dfltcc_state->block_size;
        } else
            param->bcf = 1;
        if (flush == Z_FINISH) {
            if (need_empty_block)
                 
                return 0;
            else {
                bi_windup(state);
                *result = finish_done;
            }
        } else {
            if (flush == Z_FULL_FLUSH)
                param->hl = 0;  
            *result = flush == Z_NO_FLUSH ? need_more : block_done;
        }
    } else {
        param->bcf = 1;
        *result = need_more;
    }
    if (strm->avail_in != 0 && strm->avail_out != 0)
        goto again;  
    return 1;
}
EXPORT_SYMBOL(dfltcc_deflate);
