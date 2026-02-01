

 
#include <stdbool.h>

 
 
 

struct bitfields_only_mixed_types {
	int a: 3;
	long b: 2;
	bool c: 1;  
	enum {
		A,  
		B,  
	} d: 1;
	short e: 5;
	 
	unsigned f: 30;  
};

 
 
 
struct bitfield_mixed_with_others {
	char: 4;  
	int a: 4;
	 
	short b;  
	 
	long c;
	long d: 8;
	 
	int e;  
	int f;
	 
};

 
 
 
struct bitfield_flushed {
	int a: 4;
	long: 0;  
	long b: 16;
};

int f(struct {
	struct bitfields_only_mixed_types _1;
	struct bitfield_mixed_with_others _2;
	struct bitfield_flushed _3;
} *_)
{
	return 0;
}
