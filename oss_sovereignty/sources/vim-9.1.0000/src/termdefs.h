



#if defined(SASC) && SASC < 658

typedef unsigned char char_u;
#endif


enum SpecialKey
{
    KS_NAME = 0,
    KS_CE,	
    KS_AL,	
    KS_CAL,	
    KS_DL,	
    KS_CDL,	
    KS_CS,	
    KS_CL,	
    KS_CD,	
    KS_UT,	
    KS_DA,	
    KS_DB,	
    KS_VI,	
    KS_VE,	
    KS_VS,	
    KS_CVS,	
    KS_CSH,	
    KS_CRC,	
    KS_CRS,	
    KS_ME,	
    KS_MR,	
    KS_MD,	
    KS_SE,	
    KS_SO,	
    KS_CZH,	
    KS_CZR,	
    KS_UE,	
    KS_US,	
    KS_UCE,	
    KS_UCS,	
    KS_USS,	
    KS_DS,	
    KS_CDS,	
    KS_STE,	
    KS_STS,	
    KS_MS,	
    KS_CM,	
    KS_SR,	
    KS_CRI,	
    KS_VB,	
    KS_KS,	
    KS_KE,	
    KS_TI,	
    KS_CTI,	
    KS_CRK,	
    KS_TE,	
    KS_CTE,	
    KS_BC,	
    KS_CCS,	
    KS_CCO,	
    KS_CSF,	
    KS_CSB,	
    KS_XS,	
    KS_XN,	
    KS_MB,	
    KS_CAF,	
    KS_CAB,	
    KS_CAU,	
    KS_LE,	
    KS_ND,	
    KS_CIS,	
    KS_CIE,	
    KS_CSC,	
    KS_CEC,	
    KS_TS,	
    KS_FS,	
    KS_CWP,	
    KS_CGP,	
    KS_CWS,	
    KS_CRV,	
    KS_CXM,	
    KS_RFG,	
    KS_RBG,	
    KS_CSI,	
    KS_CEI,	
    KS_CSR,	
    KS_CSV,	
    KS_OP,	
    KS_U7,	
    KS_8F,	
    KS_8B,	
    KS_8U,	
    KS_CBE,	
    KS_CBD,	
    KS_CST,	
    KS_CRT,	
    KS_SSI,	
    KS_SRI,	
    KS_FD,	
    KS_FE	
};

#define KS_LAST	    KS_FE



extern char_u *(term_strings[]);    


#define T_NAME	(TERM_STR(KS_NAME))	
#define T_CE	(TERM_STR(KS_CE))	
#define T_AL	(TERM_STR(KS_AL))	
#define T_CAL	(TERM_STR(KS_CAL))	
#define T_DL	(TERM_STR(KS_DL))	
#define T_CDL	(TERM_STR(KS_CDL))	
#define T_CS	(TERM_STR(KS_CS))	
#define T_CSV	(TERM_STR(KS_CSV))	
#define T_CL	(TERM_STR(KS_CL))	
#define T_CD	(TERM_STR(KS_CD))	
#define T_UT	(TERM_STR(KS_UT))	
#define T_DA	(TERM_STR(KS_DA))	
#define T_DB	(TERM_STR(KS_DB))	
#define T_VI	(TERM_STR(KS_VI))	
#define T_VE	(TERM_STR(KS_VE))	
#define T_VS	(TERM_STR(KS_VS))	
#define T_CVS	(TERM_STR(KS_CVS))	
#define T_CSH	(TERM_STR(KS_CSH))	
#define T_CRC	(TERM_STR(KS_CRC))	
#define T_CRS	(TERM_STR(KS_CRS))	
#define T_ME	(TERM_STR(KS_ME))	
#define T_MR	(TERM_STR(KS_MR))	
#define T_MD	(TERM_STR(KS_MD))	
#define T_SE	(TERM_STR(KS_SE))	
#define T_SO	(TERM_STR(KS_SO))	
#define T_CZH	(TERM_STR(KS_CZH))	
#define T_CZR	(TERM_STR(KS_CZR))	
#define T_UE	(TERM_STR(KS_UE))	
#define T_US	(TERM_STR(KS_US))	
#define T_UCE	(TERM_STR(KS_UCE))	
#define T_UCS	(TERM_STR(KS_UCS))	
#define T_USS	(TERM_STR(KS_USS))	
#define T_DS	(TERM_STR(KS_DS))	
#define T_CDS	(TERM_STR(KS_CDS))	
#define T_STE	(TERM_STR(KS_STE))	
#define T_STS	(TERM_STR(KS_STS))	
#define T_MS	(TERM_STR(KS_MS))	
#define T_CM	(TERM_STR(KS_CM))	
#define T_SR	(TERM_STR(KS_SR))	
#define T_CRI	(TERM_STR(KS_CRI))	
#define T_VB	(TERM_STR(KS_VB))	
#define T_KS	(TERM_STR(KS_KS))	
#define T_KE	(TERM_STR(KS_KE))	
#define T_TI	(TERM_STR(KS_TI))	
#define T_CTI	(TERM_STR(KS_CTI))	
#define T_CRK	(TERM_STR(KS_CRK))	
#define T_TE	(TERM_STR(KS_TE))	
#define T_CTE	(TERM_STR(KS_CTE))	
#define T_BC	(TERM_STR(KS_BC))	
#define T_CCS	(TERM_STR(KS_CCS))	
#define T_CCO	(TERM_STR(KS_CCO))	
#define T_CSF	(TERM_STR(KS_CSF))	
#define T_CSB	(TERM_STR(KS_CSB))	
#define T_XS	(TERM_STR(KS_XS))	
#define T_XN	(TERM_STR(KS_XN))	
#define T_MB	(TERM_STR(KS_MB))	
#define T_CAF	(TERM_STR(KS_CAF))	
#define T_CAB	(TERM_STR(KS_CAB))	
#define T_CAU	(TERM_STR(KS_CAU))	
#define T_LE	(TERM_STR(KS_LE))	
#define T_ND	(TERM_STR(KS_ND))	
#define T_CIS	(TERM_STR(KS_CIS))	
#define T_CIE	(TERM_STR(KS_CIE))	
#define T_TS	(TERM_STR(KS_TS))	
#define T_FS	(TERM_STR(KS_FS))	
#define T_CSC	(TERM_STR(KS_CSC))	
#define T_CEC	(TERM_STR(KS_CEC))	
#define T_CWP	(TERM_STR(KS_CWP))	
#define T_CGP	(TERM_STR(KS_CGP))	
#define T_CWS	(TERM_STR(KS_CWS))	
#define T_CSI	(TERM_STR(KS_CSI))	
#define T_CEI	(TERM_STR(KS_CEI))	
#define T_CSR	(TERM_STR(KS_CSR))	
#define T_CRV	(TERM_STR(KS_CRV))	
#define T_CXM	(TERM_STR(KS_CXM))	
#define T_RFG	(TERM_STR(KS_RFG))	
#define T_RBG	(TERM_STR(KS_RBG))	
#define T_OP	(TERM_STR(KS_OP))	
#define T_U7	(TERM_STR(KS_U7))	
#define T_8F	(TERM_STR(KS_8F))	
#define T_8B	(TERM_STR(KS_8B))	
#define T_8U	(TERM_STR(KS_8U))	
#define T_BE	(TERM_STR(KS_CBE))	
#define T_BD	(TERM_STR(KS_CBD))	
#define T_CST	(TERM_STR(KS_CST))	
#define T_CRT	(TERM_STR(KS_CRT))	
#define T_SSI	(TERM_STR(KS_SSI))	
#define T_SRI	(TERM_STR(KS_SRI))	
#define T_FD	(TERM_STR(KS_FD))	
#define T_FE	(TERM_STR(KS_FE))	

typedef enum {
    TMODE_COOK,	    
    TMODE_SLEEP,    
    TMODE_RAW,	    
    TMODE_UNKNOWN   
} tmode_T;
