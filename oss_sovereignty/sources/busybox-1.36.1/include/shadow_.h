




#ifndef BB_SHADOW_H
#define BB_SHADOW_H 1

PUSH_AND_SET_FUNCTION_VISIBILITY_TO_HIDDEN


struct spwd {
	char *sp_namp;          
	char *sp_pwdp;          
	long sp_lstchg;         
	long sp_min;            
	long sp_max;            
	long sp_warn;           
	long sp_inact;          
	long sp_expire;         
	unsigned long sp_flag;  
};

#define setspent    bb_internal_setspent
#define endspent    bb_internal_endspent
#define getspent    bb_internal_getspent
#define getspnam    bb_internal_getspnam
#define sgetspent   bb_internal_sgetspent
#define fgetspent   bb_internal_fgetspent
#define putspent    bb_internal_putspent
#define getspent_r  bb_internal_getspent_r
#define getspnam_r  bb_internal_getspnam_r
#define sgetspent_r bb_internal_sgetspent_r
#define fgetspent_r bb_internal_fgetspent_r
#define lckpwdf     bb_internal_lckpwdf
#define ulckpwdf    bb_internal_ulckpwdf




#ifdef UNUSED_FOR_NOW

void FAST_FUNC setspent(void);


void FAST_FUNC endspent(void);


struct spwd* FAST_FUNC getspent(void);


struct spwd* FAST_FUNC getspnam(const char *__name);


struct spwd* FAST_FUNC sgetspent(const char *__string);


struct spwd* FAST_FUNC fgetspent(FILE *__stream);


int FAST_FUNC putspent(const struct spwd *__p, FILE *__stream);


int FAST_FUNC getspent_r(struct spwd *__result_buf, char *__buffer,
		size_t __buflen, struct spwd **__result);
#endif

int FAST_FUNC getspnam_r(const char *__name, struct spwd *__result_buf,
		char *__buffer, size_t __buflen,
		struct spwd **__result);

#ifdef UNUSED_FOR_NOW
int FAST_FUNC sgetspent_r(const char *__string, struct spwd *__result_buf,
		char *__buffer, size_t __buflen,
		struct spwd **__result);

int FAST_FUNC fgetspent_r(FILE *__stream, struct spwd *__result_buf,
		char *__buffer, size_t __buflen,
		struct spwd **__result);

int FAST_FUNC lckpwdf(void);


int FAST_FUNC ulckpwdf(void);
#endif

POP_SAVED_FUNCTION_VISIBILITY

#endif 
