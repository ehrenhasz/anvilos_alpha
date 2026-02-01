 
 

#ifndef INTEL_MOCS_H
#define INTEL_MOCS_H

 

struct intel_engine_cs;
struct intel_gt;

void intel_mocs_init(struct intel_gt *gt);
void intel_mocs_init_engine(struct intel_engine_cs *engine);
void intel_set_mocs_index(struct intel_gt *gt);

#endif
