 
#ifndef CLEARSTATE_DEFS_H
#define CLEARSTATE_DEFS_H

enum section_id {
    SECT_NONE,
    SECT_CONTEXT,
    SECT_CLEAR,
    SECT_CTRLCONST
};

struct cs_extent_def {
    const unsigned int *extent;
    const unsigned int reg_index;
    const unsigned int reg_count;
};

struct cs_section_def {
    const struct cs_extent_def *section;
    const enum section_id id;
};

#endif
