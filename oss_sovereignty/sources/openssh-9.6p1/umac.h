

 
 
#ifndef HEADER_UMAC_H
#define HEADER_UMAC_H


#ifdef __cplusplus
    extern "C" {
#endif

struct umac_ctx *umac_new(const u_char key[]);


#if 0
int umac_reset(struct umac_ctx *ctx);

#endif

int umac_update(struct umac_ctx *ctx, const u_char *input, long len);


int umac_final(struct umac_ctx *ctx, u_char tag[], const u_char nonce[8]);


int umac_delete(struct umac_ctx *ctx);


#if 0
int umac(struct umac_ctx *ctx, u_char *input, 
         long len, u_char tag[],
         u_char nonce[8]);

#endif




#if 0
typedef struct uhash_ctx *uhash_ctx_t;
  
  
 
uhash_ctx_t uhash_alloc(u_char key[16]);
  
  
  
  
  
  
int uhash_free(uhash_ctx_t ctx);

int uhash_set_params(uhash_ctx_t ctx,
                   void       *params);

int uhash_reset(uhash_ctx_t ctx);

int uhash_update(uhash_ctx_t ctx,
               u_char       *input,
               long        len);

int uhash_final(uhash_ctx_t ctx,
              u_char        output[]);

int uhash(uhash_ctx_t ctx,
        u_char       *input,
        long        len,
        u_char        output[]);

#endif


struct umac_ctx *umac128_new(const u_char key[]);
int umac128_update(struct umac_ctx *ctx, const u_char *input, long len);
int umac128_final(struct umac_ctx *ctx, u_char tag[], const u_char nonce[8]);
int umac128_delete(struct umac_ctx *ctx);

#ifdef __cplusplus
    }
#endif

#endif 
