#ifdef WITH_XMSS
 
 

#include "xmss_wots.h"

#ifndef XMSS_H
#define XMSS_H
typedef struct{
  unsigned int level;
  unsigned long long subtree;
  unsigned int subleaf;
} leafaddr;

typedef struct{
  wots_params wots_par;
  unsigned int n;
  unsigned int h;
  unsigned int k;
} xmss_params;

typedef struct{
  xmss_params xmss_par;
  unsigned int n;
  unsigned int h;
  unsigned int d;
  unsigned int index_len;
} xmssmt_params;

typedef struct{
  unsigned int h;
  unsigned int next_idx;
  unsigned int stackusage;
  unsigned char completed;
  unsigned char *node;
} treehash_inst;

typedef struct {
  unsigned char *stack;
  unsigned int stackoffset;
  unsigned char *stacklevels;
  unsigned char *auth;
  unsigned char *keep;
  treehash_inst *treehash;
  unsigned char *retain;
  unsigned int next_leaf;
} bds_state;

 
void xmss_set_bds_state(bds_state *state, unsigned char *stack, int stackoffset, unsigned char *stacklevels, unsigned char *auth, unsigned char *keep, treehash_inst *treehash, unsigned char *retain, int next_leaf);
 
int xmss_set_params(xmss_params *params, int n, int h, int w, int k);
 
int xmssmt_set_params(xmssmt_params *params, int n, int h, int d, int w, int k);
 
int xmss_keypair(unsigned char *pk, unsigned char *sk, bds_state *state, xmss_params *params);
 
int xmss_sign(unsigned char *sk, bds_state *state, unsigned char *sig_msg, unsigned long long *sig_msg_len, const unsigned char *msg,unsigned long long msglen, const xmss_params *params);
 
int xmss_sign_open(unsigned char *msg,unsigned long long *msglen, const unsigned char *sig_msg,unsigned long long sig_msg_len, const unsigned char *pk, const xmss_params *params);

 
int xmssmt_keypair(unsigned char *pk, unsigned char *sk, bds_state *states, unsigned char *wots_sigs, xmssmt_params *params);
 
int xmssmt_sign(unsigned char *sk, bds_state *state, unsigned char *wots_sigs, unsigned char *sig_msg, unsigned long long *sig_msg_len, const unsigned char *msg, unsigned long long msglen, const xmssmt_params *params);
 
int xmssmt_sign_open(unsigned char *msg, unsigned long long *msglen, const unsigned char *sig_msg, unsigned long long sig_msg_len, const unsigned char *pk, const xmssmt_params *params);
#endif
#endif  
