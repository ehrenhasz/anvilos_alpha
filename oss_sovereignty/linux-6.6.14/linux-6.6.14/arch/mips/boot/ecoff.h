#include <stdint.h>
typedef struct filehdr {
	uint16_t	f_magic;	 
	uint16_t	f_nscns;	 
	int32_t		f_timdat;	 
	int32_t		f_symptr;	 
	int32_t		f_nsyms;	 
	uint16_t	f_opthdr;	 
	uint16_t	f_flags;	 
} FILHDR;
#define FILHSZ	sizeof(FILHDR)
#define MIPSEBMAGIC	0x160
#define MIPSELMAGIC	0x162
typedef struct scnhdr {
	char		s_name[8];	 
	int32_t		s_paddr;	 
	int32_t		s_vaddr;	 
	int32_t		s_size;		 
	int32_t		s_scnptr;	 
	int32_t		s_relptr;	 
	int32_t		s_lnnoptr;	 
	uint16_t	s_nreloc;	 
	uint16_t	s_nlnno;	 
	int32_t		s_flags;	 
} SCNHDR;
#define SCNHSZ		sizeof(SCNHDR)
#define SCNROUND	((int32_t)16)
typedef struct aouthdr {
	int16_t	magic;		 
	int16_t	vstamp;		 
	int32_t	tsize;		 
	int32_t	dsize;		 
	int32_t	bsize;		 
	int32_t	entry;		 
	int32_t	text_start;	 
	int32_t	data_start;	 
	int32_t	bss_start;	 
	int32_t	gprmask;	 
	int32_t	cprmask[4];	 
	int32_t	gp_value;	 
} AOUTHDR;
#define AOUTHSZ sizeof(AOUTHDR)
#define OMAGIC		0407
#define NMAGIC		0410
#define ZMAGIC		0413
#define SMAGIC		0411
#define LIBMAGIC	0443
#define N_TXTOFF(f, a) \
 ((a).magic == ZMAGIC || (a).magic == LIBMAGIC ? 0 : \
  ((a).vstamp < 23 ? \
   ((FILHSZ + AOUTHSZ + (f).f_nscns * SCNHSZ + 7) & 0xfffffff8) : \
   ((FILHSZ + AOUTHSZ + (f).f_nscns * SCNHSZ + SCNROUND-1) & ~(SCNROUND-1)) ) )
#define N_DATOFF(f, a) \
  N_TXTOFF(f, a) + (a).tsize;
