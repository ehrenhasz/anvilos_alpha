#ifndef S390_CPUMSF_KERNEL_H
#define S390_CPUMSF_KERNEL_H
#define	S390_CPUMSF_PAGESZ	4096	 
#define	S390_CPUMSF_DIAG_DEF_FIRST	0x8001	 
struct hws_basic_entry {
	unsigned int def:16;	     
	unsigned int R:4;	     
	unsigned int U:4;	     
	unsigned int z:2;	     
	unsigned int T:1;	     
	unsigned int W:1;	     
	unsigned int P:1;	     
	unsigned int AS:2;	     
	unsigned int I:1;	     
	unsigned int CL:2;	     
	unsigned int:14;
	unsigned int prim_asn:16;    
	unsigned long long ia;	     
	unsigned long long gpp;      
	unsigned long long hpp;      
};
struct hws_diag_entry {
	unsigned int def:16;	     
	unsigned int R:15;	     
	unsigned int I:1;	     
	u8	     data[];	     
};
struct hws_combined_entry {
	struct hws_basic_entry	basic;	 
	struct hws_diag_entry	diag;	 
};
struct hws_trailer_entry {
	union {
		struct {
			unsigned int f:1;	 
			unsigned int a:1;	 
			unsigned int t:1;	 
			unsigned int:29;	 
			unsigned int bsdes:16;	 
			unsigned int dsdes:16;	 
		};
		unsigned long long flags;	 
	};
	unsigned long long overflow;	  
	unsigned char timestamp[16];	  
	unsigned long long reserved1;	  
	unsigned long long reserved2;	  
	union {				  
		struct {
			unsigned long long clock_base:1;  
			unsigned long long progusage1:63;
			unsigned long long progusage2;
		};
		unsigned long long progusage[2];
	};
};
#endif
