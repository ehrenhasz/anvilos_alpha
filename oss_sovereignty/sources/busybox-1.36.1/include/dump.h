

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN

enum dump_vflag_t { ALL, DUP, FIRST, WAIT };	

typedef struct PR {
	struct PR *nextpr;		
	unsigned flags;			
	int bcnt;			
	char *cchar;			
	char *fmt;			
	char *nospace;			
} PR;

typedef struct FU {
	struct FU *nextfu;		
	struct PR *nextpr;		
	unsigned flags;			
	int reps;			
	int bcnt;			
	char *fmt;			
} FU;

typedef struct FS {			
	struct FS *nextfs;		
	struct FU *nextfu;		
	int bcnt;
} FS;

typedef struct dumper_t {
	off_t dump_skip;                
	int dump_length;                
	smallint dump_vflag;            
	FS *fshead;
	const char *xxd_eofstring;
	off_t address;           
	long long xxd_displayoff;
} dumper_t;

dumper_t* alloc_dumper(void) FAST_FUNC;
extern void bb_dump_add(dumper_t *dumper, const char *fmt) FAST_FUNC;
extern int bb_dump_dump(dumper_t *dumper, char **argv) FAST_FUNC;

POP_SAVED_FUNCTION_VISIBILITY
