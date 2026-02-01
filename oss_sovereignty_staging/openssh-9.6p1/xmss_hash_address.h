#ifdef WITH_XMSS
 
 

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

void setLayerADRS(uint32_t adrs[8], uint32_t layer);

void setTreeADRS(uint32_t adrs[8], uint64_t tree);

void setType(uint32_t adrs[8], uint32_t type);

void setKeyAndMask(uint32_t adrs[8], uint32_t keyAndMask);



void setOTSADRS(uint32_t adrs[8], uint32_t ots);

void setChainADRS(uint32_t adrs[8], uint32_t chain);

void setHashADRS(uint32_t adrs[8], uint32_t hash);



void setLtreeADRS(uint32_t adrs[8], uint32_t ltree);



void setTreeHeight(uint32_t adrs[8], uint32_t treeHeight);

void setTreeIndex(uint32_t adrs[8], uint32_t treeIndex);

#endif  
