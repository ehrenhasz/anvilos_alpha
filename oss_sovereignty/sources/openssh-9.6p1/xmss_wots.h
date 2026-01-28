#ifdef WITH_XMSS



#ifndef WOTS_H
#define WOTS_H

#ifdef HAVE_STDINT_H
#include "stdint.h"
#endif


typedef struct {
  uint32_t len_1;
  uint32_t len_2;
  uint32_t len;
  uint32_t n;
  uint32_t w;
  uint32_t log_w;
  uint32_t keysize;
} wots_params;


void wots_set_params(wots_params *params, int n, int w);


void wots_pkgen(unsigned char *pk, const unsigned char *sk, const wots_params *params, const unsigned char *pub_seed, uint32_t addr[8]);


int wots_sign(unsigned char *sig, const unsigned char *msg, const unsigned char *sk, const wots_params *params, const unsigned char *pub_seed, uint32_t addr[8]);


int wots_pkFromSig(unsigned char *pk, const unsigned char *sig, const unsigned char *msg, const wots_params *params, const unsigned char *pub_seed, uint32_t addr[8]);

#endif
#endif 
