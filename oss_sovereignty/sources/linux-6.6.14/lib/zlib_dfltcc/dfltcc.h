
#ifndef DFLTCC_H
#define DFLTCC_H

#include "../zlib_deflate/defutil.h"
#include <asm/facility.h>
#include <asm/setup.h>


#define DFLTCC_LEVEL_MASK 0x2 
#define DFLTCC_LEVEL_MASK_DEBUG 0x3fe 
#define DFLTCC_BLOCK_SIZE 1048576
#define DFLTCC_FIRST_FHT_BLOCK_SIZE 4096
#define DFLTCC_DHT_MIN_SAMPLE_SIZE 4096
#define DFLTCC_RIBM 0

#define DFLTCC_FACILITY 151


struct dfltcc_qaf_param {
    char fns[16];
    char reserved1[8];
    char fmts[2];
    char reserved2[6];
};

static_assert(sizeof(struct dfltcc_qaf_param) == 32);

#define DFLTCC_FMT0 0


struct dfltcc_param_v0 {
    uint16_t pbvn;                     
    uint8_t mvn;                       
    uint8_t ribm;                      
    unsigned reserved32 : 31;
    unsigned cf : 1;                   
    uint8_t reserved64[8];
    unsigned nt : 1;                   
    unsigned reserved129 : 1;
    unsigned cvt : 1;                  
    unsigned reserved131 : 1;
    unsigned htt : 1;                  
    unsigned bcf : 1;                  
    unsigned bcc : 1;                  
    unsigned bhf : 1;                  
    unsigned reserved136 : 1;
    unsigned reserved137 : 1;
    unsigned dhtgc : 1;                
    unsigned reserved139 : 5;
    unsigned reserved144 : 5;
    unsigned sbb : 3;                  
    uint8_t oesc;                      
    unsigned reserved160 : 12;
    unsigned ifs : 4;                  
    uint16_t ifl;                      
    uint8_t reserved192[8];
    uint8_t reserved256[8];
    uint8_t reserved320[4];
    uint16_t hl;                       
    unsigned reserved368 : 1;
    uint16_t ho : 15;                  
    uint32_t cv;                       
    unsigned eobs : 15;                
    unsigned reserved431: 1;
    uint8_t eobl : 4;                  
    unsigned reserved436 : 12;
    unsigned reserved448 : 4;
    uint16_t cdhtl : 12;               
    uint8_t reserved464[6];
    uint8_t cdht[288];
    uint8_t reserved[32];
    uint8_t csb[1152];
};

static_assert(sizeof(struct dfltcc_param_v0) == 1536);

#define CVT_CRC32 0
#define CVT_ADLER32 1
#define HTT_FIXED 0
#define HTT_DYNAMIC 1


struct dfltcc_state {
    struct dfltcc_param_v0 param;      
    struct dfltcc_qaf_param af;        
    char msg[64];                      
};


struct dfltcc_deflate_state {
    struct dfltcc_state common;        
    uLong level_mask;                  
    uLong block_size;                  
    uLong block_threshold;             
    uLong dht_threshold;               
};

#define ALIGN_UP(p, size) (__typeof__(p))(((uintptr_t)(p) + ((size) - 1)) & ~((size) - 1))

#define GET_DFLTCC_STATE(state) ((struct dfltcc_state *)((char *)(state) + ALIGN_UP(sizeof(*state), 8)))

void dfltcc_reset_state(struct dfltcc_state *dfltcc_state);

static inline int is_dfltcc_enabled(void)
{
return (zlib_dfltcc_support != ZLIB_DFLTCC_DISABLED &&
        test_facility(DFLTCC_FACILITY));
}

#define DEFLATE_DFLTCC_ENABLED() is_dfltcc_enabled()

#endif 
