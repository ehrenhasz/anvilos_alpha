 

 


#include <string.h>

#include "ff.h"          
#include "diskio.h"      


#define DIR FF_DIR

 

#if FF_DEFINED != 86604  
#error Wrong include file (ff.h).
#endif


 
#define MAX_DIR     0x200000         
#define MAX_DIR_EX  0x10000000       
#define MAX_FAT12   0xFF5            
#define MAX_FAT16   0xFFF5           
#define MAX_FAT32   0x0FFFFFF5       
#define MAX_EXFAT   0x7FFFFFFD       


 
#define IsUpper(c)      ((c) >= 'A' && (c) <= 'Z')
#define IsLower(c)      ((c) >= 'a' && (c) <= 'z')
#define IsDigit(c)      ((c) >= '0' && (c) <= '9')
#define IsSurrogate(c)  ((c) >= 0xD800 && (c) <= 0xDFFF)
#define IsSurrogateH(c) ((c) >= 0xD800 && (c) <= 0xDBFF)
#define IsSurrogateL(c) ((c) >= 0xDC00 && (c) <= 0xDFFF)


 
#define FA_SEEKEND  0x20     
#define FA_MODIFIED 0x40     
#define FA_DIRTY    0x80     


 
#define AM_VOL      0x08     
#define AM_LFN      0x0F     
#define AM_MASK     0x3F     


 
#define NSFLAG      11       
#define NS_LOSS     0x01     
#define NS_LFN      0x02     
#define NS_LAST     0x04     
#define NS_BODY     0x08     
#define NS_EXT      0x10     
#define NS_DOT      0x20     
#define NS_NOLFN    0x40     
#define NS_NONAME   0x80     


 
#define ET_BITMAP   0x81     
#define ET_UPCASE   0x82     
#define ET_VLABEL   0x83     
#define ET_FILEDIR  0x85     
#define ET_STREAM   0xC0     
#define ET_FILENAME 0xC1     


 

#define BS_JmpBoot          0        
#define BS_OEMName          3        
#define BPB_BytsPerSec      11       
#define BPB_SecPerClus      13       
#define BPB_RsvdSecCnt      14       
#define BPB_NumFATs         16       
#define BPB_RootEntCnt      17       
#define BPB_TotSec16        19       
#define BPB_Media           21       
#define BPB_FATSz16         22       
#define BPB_SecPerTrk       24       
#define BPB_NumHeads        26       
#define BPB_HiddSec         28       
#define BPB_TotSec32        32       
#define BS_DrvNum           36       
#define BS_NTres            37       
#define BS_BootSig          38       
#define BS_VolID            39       
#define BS_VolLab           43       
#define BS_FilSysType       54       
#define BS_BootCode         62       
#define BS_55AA             510      

#define BPB_FATSz32         36       
#define BPB_ExtFlags32      40       
#define BPB_FSVer32         42       
#define BPB_RootClus32      44       
#define BPB_FSInfo32        48       
#define BPB_BkBootSec32     50       
#define BS_DrvNum32         64       
#define BS_NTres32          65       
#define BS_BootSig32        66       
#define BS_VolID32          67       
#define BS_VolLab32         71       
#define BS_FilSysType32     82       
#define BS_BootCode32       90       

#define BPB_ZeroedEx        11       
#define BPB_VolOfsEx        64       
#define BPB_TotSecEx        72       
#define BPB_FatOfsEx        80       
#define BPB_FatSzEx         84       
#define BPB_DataOfsEx       88       
#define BPB_NumClusEx       92       
#define BPB_RootClusEx      96       
#define BPB_VolIDEx         100      
#define BPB_FSVerEx         104      
#define BPB_VolFlagEx       106      
#define BPB_BytsPerSecEx    108      
#define BPB_SecPerClusEx    109      
#define BPB_NumFATsEx       110      
#define BPB_DrvNumEx        111      
#define BPB_PercInUseEx     112      
#define BPB_RsvdEx          113      
#define BS_BootCodeEx       120      

#define DIR_Name            0        
#define DIR_Attr            11       
#define DIR_NTres           12       
#define DIR_CrtTime10       13       
#define DIR_CrtTime         14       
#define DIR_LstAccDate      18       
#define DIR_FstClusHI       20       
#define DIR_ModTime         22       
#define DIR_FstClusLO       26       
#define DIR_FileSize        28       
#define LDIR_Ord            0        
#define LDIR_Attr           11       
#define LDIR_Type           12       
#define LDIR_Chksum         13       
#define LDIR_FstClusLO      26       
#define XDIR_Type           0        
#define XDIR_NumLabel       1        
#define XDIR_Label          2        
#define XDIR_CaseSum        4        
#define XDIR_NumSec         1        
#define XDIR_SetSum         2        
#define XDIR_Attr           4        
#define XDIR_CrtTime        8        
#define XDIR_ModTime        12       
#define XDIR_AccTime        16       
#define XDIR_CrtTime10      20       
#define XDIR_ModTime10      21       
#define XDIR_CrtTZ          22       
#define XDIR_ModTZ          23       
#define XDIR_AccTZ          24       
#define XDIR_GenFlags       33       
#define XDIR_NumName        35       
#define XDIR_NameHash       36       
#define XDIR_ValidFileSize  40       
#define XDIR_FstClus        52       
#define XDIR_FileSize       56       

#define SZDIRE              32       
#define DDEM                0xE5     
#define RDDEM               0x05     
#define LLEF                0x40     

#define FSI_LeadSig         0        
#define FSI_StrucSig        484      
#define FSI_Free_Count      488      
#define FSI_Nxt_Free        492      

#define MBR_Table           446      
#define SZ_PTE              16       
#define PTE_Boot            0        
#define PTE_StHead          1        
#define PTE_StSec           2        
#define PTE_StCyl           3        
#define PTE_System          4        
#define PTE_EdHead          5        
#define PTE_EdSec           6        
#define PTE_EdCyl           7        
#define PTE_StLba           8        
#define PTE_SizLba          12       


 
#define ABORT(fs, res)      { fp->err = (BYTE)(res); LEAVE_FF(fs, res); }


 
#if FF_FS_REENTRANT
#if FF_USE_LFN == 1
#error Static LFN work area cannot be used at thread-safe configuration
#endif
#define LEAVE_FF(fs, res)   { unlock_fs(fs, res); return res; }
#else
#define LEAVE_FF(fs, res)   return res
#endif


 
#if FF_MULTI_PARTITION
#define LD2PT(fs) (fs->part)     
#else
#define LD2PT(fs) 0              
#endif


 
#if (FF_MAX_SS < FF_MIN_SS) || (FF_MAX_SS != 512 && FF_MAX_SS != 1024 && FF_MAX_SS != 2048 && FF_MAX_SS != 4096) || (FF_MIN_SS != 512 && FF_MIN_SS != 1024 && FF_MIN_SS != 2048 && FF_MIN_SS != 4096)
#error Wrong sector size configuration
#endif
#if FF_MAX_SS == FF_MIN_SS
#define SS(fs)  ((UINT)FF_MAX_SS)    
#else
#define SS(fs)  ((fs)->ssize)    
#endif


 
#if FF_FS_NORTC == 1
#if FF_NORTC_YEAR < 1980 || FF_NORTC_YEAR > 2107 || FF_NORTC_MON < 1 || FF_NORTC_MON > 12 || FF_NORTC_MDAY < 1 || FF_NORTC_MDAY > 31
#error Invalid FF_FS_NORTC settings
#endif
#define GET_FATTIME()   ((DWORD)(FF_NORTC_YEAR - 1980) << 25 | (DWORD)FF_NORTC_MON << 21 | (DWORD)FF_NORTC_MDAY << 16)
#else
#define GET_FATTIME()   get_fattime()
#endif


 
#if FF_FS_LOCK != 0
#if FF_FS_READONLY
#error FF_FS_LOCK must be 0 at read-only configuration
#endif
typedef struct {
    FATFS *fs;       
    DWORD clu;       
    DWORD ofs;       
    WORD ctr;        
} FILESEM;
#endif


 
#define TBL_CT437  {0x80,0x9A,0x45,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x49,0x49,0x49,0x8E,0x8F, \
                    0x90,0x92,0x92,0x4F,0x99,0x4F,0x55,0x55,0x59,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                    0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT720  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
                    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                    0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT737  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
                    0x90,0x92,0x92,0x93,0x94,0x95,0x96,0x97,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87, \
                    0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,0x90,0x91,0xAA,0x92,0x93,0x94,0x95,0x96, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0x97,0xEA,0xEB,0xEC,0xE4,0xED,0xEE,0xEF,0xF5,0xF0,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT771  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
                    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDC,0xDE,0xDE, \
                    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                    0xF0,0xF0,0xF2,0xF2,0xF4,0xF4,0xF6,0xF6,0xF8,0xF8,0xFA,0xFA,0xFC,0xFC,0xFE,0xFF}
#define TBL_CT775  {0x80,0x9A,0x91,0xA0,0x8E,0x95,0x8F,0x80,0xAD,0xED,0x8A,0x8A,0xA1,0x8D,0x8E,0x8F, \
                    0x90,0x92,0x92,0xE2,0x99,0x95,0x96,0x97,0x97,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9F, \
                    0xA0,0xA1,0xE0,0xA3,0xA3,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xB5,0xB6,0xB7,0xB8,0xBD,0xBE,0xC6,0xC7,0xA5,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE3,0xE8,0xE8,0xEA,0xEA,0xEE,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT850  {0x43,0x55,0x45,0x41,0x41,0x41,0x41,0x43,0x45,0x45,0x45,0x49,0x49,0x49,0x41,0x41, \
                    0x45,0x92,0x92,0x4F,0x4F,0x4F,0x55,0x55,0x59,0x4F,0x55,0x4F,0x9C,0x4F,0x9E,0x9F, \
                    0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0x41,0x41,0x41,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0x41,0x41,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD1,0xD1,0x45,0x45,0x45,0x49,0x49,0x49,0x49,0xD9,0xDA,0xDB,0xDC,0xDD,0x49,0xDF, \
                    0x4F,0xE1,0x4F,0x4F,0x4F,0x4F,0xE6,0xE8,0xE8,0x55,0x55,0x55,0x59,0x59,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT852  {0x80,0x9A,0x90,0xB6,0x8E,0xDE,0x8F,0x80,0x9D,0xD3,0x8A,0x8A,0xD7,0x8D,0x8E,0x8F, \
                    0x90,0x91,0x91,0xE2,0x99,0x95,0x95,0x97,0x97,0x99,0x9A,0x9B,0x9B,0x9D,0x9E,0xAC, \
                    0xB5,0xD6,0xE0,0xE9,0xA4,0xA4,0xA6,0xA6,0xA8,0xA8,0xAA,0x8D,0xAC,0xB8,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBD,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC6,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD1,0xD1,0xD2,0xD3,0xD2,0xD5,0xD6,0xD7,0xB7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE3,0xD5,0xE6,0xE6,0xE8,0xE9,0xE8,0xEB,0xED,0xED,0xDD,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xEB,0xFC,0xFC,0xFE,0xFF}
#define TBL_CT855  {0x81,0x81,0x83,0x83,0x85,0x85,0x87,0x87,0x89,0x89,0x8B,0x8B,0x8D,0x8D,0x8F,0x8F, \
                    0x91,0x91,0x93,0x93,0x95,0x95,0x97,0x97,0x99,0x99,0x9B,0x9B,0x9D,0x9D,0x9F,0x9F, \
                    0xA1,0xA1,0xA3,0xA3,0xA5,0xA5,0xA7,0xA7,0xA9,0xA9,0xAB,0xAB,0xAD,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB6,0xB6,0xB8,0xB8,0xB9,0xBA,0xBB,0xBC,0xBE,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD1,0xD1,0xD3,0xD3,0xD5,0xD5,0xD7,0xD7,0xDD,0xD9,0xDA,0xDB,0xDC,0xDD,0xE0,0xDF, \
                    0xE0,0xE2,0xE2,0xE4,0xE4,0xE6,0xE6,0xE8,0xE8,0xEA,0xEA,0xEC,0xEC,0xEE,0xEE,0xEF, \
                    0xF0,0xF2,0xF2,0xF4,0xF4,0xF6,0xF6,0xF8,0xF8,0xFA,0xFA,0xFC,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT857  {0x80,0x9A,0x90,0xB6,0x8E,0xB7,0x8F,0x80,0xD2,0xD3,0xD4,0xD8,0xD7,0x49,0x8E,0x8F, \
                    0x90,0x92,0x92,0xE2,0x99,0xE3,0xEA,0xEB,0x98,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9E, \
                    0xB5,0xD6,0xE0,0xE9,0xA5,0xA5,0xA6,0xA6,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC7,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0x49,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE5,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xDE,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT860  {0x80,0x9A,0x90,0x8F,0x8E,0x91,0x86,0x80,0x89,0x89,0x92,0x8B,0x8C,0x98,0x8E,0x8F, \
                    0x90,0x91,0x92,0x8C,0x99,0xA9,0x96,0x9D,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                    0x86,0x8B,0x9F,0x96,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT861  {0x80,0x9A,0x90,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x8B,0x8B,0x8D,0x8E,0x8F, \
                    0x90,0x92,0x92,0x4F,0x99,0x8D,0x55,0x97,0x97,0x99,0x9A,0x9D,0x9C,0x9D,0x9E,0x9F, \
                    0xA4,0xA5,0xA6,0xA7,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT862  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
                    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                    0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT863  {0x43,0x55,0x45,0x41,0x41,0x41,0x86,0x43,0x45,0x45,0x45,0x49,0x49,0x8D,0x41,0x8F, \
                    0x45,0x45,0x45,0x4F,0x45,0x49,0x55,0x55,0x98,0x4F,0x55,0x9B,0x9C,0x55,0x55,0x9F, \
                    0xA0,0xA1,0x4F,0x55,0xA4,0xA5,0xA6,0xA7,0x49,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT864  {0x80,0x9A,0x45,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x49,0x49,0x49,0x8E,0x8F, \
                    0x90,0x92,0x92,0x4F,0x99,0x4F,0x55,0x55,0x59,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                    0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT865  {0x80,0x9A,0x90,0x41,0x8E,0x41,0x8F,0x80,0x45,0x45,0x45,0x49,0x49,0x49,0x8E,0x8F, \
                    0x90,0x92,0x92,0x4F,0x99,0x4F,0x55,0x55,0x59,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                    0x41,0x49,0x4F,0x55,0xA5,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF, \
                    0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT866  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
                    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF, \
                    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F, \
                    0xF0,0xF0,0xF2,0xF2,0xF4,0xF4,0xF6,0xF6,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}
#define TBL_CT869  {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F, \
                    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9A,0x86,0x9C,0x8D,0x8F,0x90, \
                    0x91,0x90,0x92,0x95,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF, \
                    0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF, \
                    0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF, \
                    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xA4,0xA5,0xA6,0xD9,0xDA,0xDB,0xDC,0xA7,0xA8,0xDF, \
                    0xA9,0xAA,0xAC,0xAD,0xB5,0xB6,0xB7,0xB8,0xBD,0xBE,0xC6,0xC7,0xCF,0xCF,0xD0,0xEF, \
                    0xF0,0xF1,0xD1,0xD2,0xD3,0xF5,0xD4,0xF7,0xF8,0xF9,0xD5,0x96,0x95,0x98,0xFE,0xFF}


 
#define TBL_DC932 {0x81, 0x9F, 0xE0, 0xFC, 0x40, 0x7E, 0x80, 0xFC, 0x00, 0x00}
#define TBL_DC936 {0x81, 0xFE, 0x00, 0x00, 0x40, 0x7E, 0x80, 0xFE, 0x00, 0x00}
#define TBL_DC949 {0x81, 0xFE, 0x00, 0x00, 0x41, 0x5A, 0x61, 0x7A, 0x81, 0xFE}
#define TBL_DC950 {0x81, 0xFE, 0x00, 0x00, 0x40, 0x7E, 0xA1, 0xFE, 0x00, 0x00}


 
#define MERGE_2STR(a, b) a ## b
#define MKCVTBL(hd, cp) MERGE_2STR(hd, cp)




 
 

 
 
 

#if FF_VOLUMES < 1 || FF_VOLUMES > 10
#error Wrong FF_VOLUMES setting
#endif
static WORD Fsid;                    

#if FF_FS_LOCK != 0
static FILESEM Files[FF_FS_LOCK];    
#endif

#if FF_STR_VOLUME_ID
#ifdef FF_VOLUME_STRS
static const char* const VolumeStr[FF_VOLUMES] = {FF_VOLUME_STRS};   
#endif
#endif


 
 
 

#if FF_USE_LFN == 0      
#if FF_FS_EXFAT
#error LFN must be enabled when enable exFAT
#endif
#define DEF_NAMBUF
#define INIT_NAMBUF(fs)
#define FREE_NAMBUF()
#define LEAVE_MKFS(res) return res

#else                    
#if FF_MAX_LFN < 12 || FF_MAX_LFN > 255
#error Wrong setting of FF_MAX_LFN
#endif
#if FF_LFN_BUF < FF_SFN_BUF || FF_SFN_BUF < 12
#error Wrong setting of FF_LFN_BUF or FF_SFN_BUF
#endif
#if FF_LFN_UNICODE < 0 || FF_LFN_UNICODE > 3
#error Wrong setting of FF_LFN_UNICODE
#endif
static const BYTE LfnOfs[] = {1,3,5,7,9,14,16,18,20,22,24,28,30};    
#define MAXDIRB(nc) ((nc + 44U) / 15 * SZDIRE)   

#if FF_USE_LFN == 1      
#if FF_FS_EXFAT
static BYTE DirBuf[MAXDIRB(FF_MAX_LFN)];     
#endif
static WCHAR LfnBuf[FF_MAX_LFN + 1];         
#define DEF_NAMBUF
#define INIT_NAMBUF(fs)
#define FREE_NAMBUF()
#define LEAVE_MKFS(res) return res

#elif FF_USE_LFN == 2    
#if FF_FS_EXFAT
#define DEF_NAMBUF      WCHAR lbuf[FF_MAX_LFN+1]; BYTE dbuf[MAXDIRB(FF_MAX_LFN)];    
#define INIT_NAMBUF(fs) { (fs)->lfnbuf = lbuf; (fs)->dirbuf = dbuf; }
#define FREE_NAMBUF()
#else
#define DEF_NAMBUF      WCHAR lbuf[FF_MAX_LFN+1];    
#define INIT_NAMBUF(fs) { (fs)->lfnbuf = lbuf; }
#define FREE_NAMBUF()
#endif
#define LEAVE_MKFS(res) return res

#elif FF_USE_LFN == 3    
#if FF_FS_EXFAT
#define DEF_NAMBUF      WCHAR *lfn;  
#define INIT_NAMBUF(fs) { lfn = ff_memalloc((FF_MAX_LFN+1)*2 + MAXDIRB(FF_MAX_LFN)); if (!lfn) LEAVE_FF(fs, FR_NOT_ENOUGH_CORE); (fs)->lfnbuf = lfn; (fs)->dirbuf = (BYTE*)(lfn+FF_MAX_LFN+1); }
#define FREE_NAMBUF()   ff_memfree(lfn)
#else
#define DEF_NAMBUF      WCHAR *lfn;  
#define INIT_NAMBUF(fs) { lfn = ff_memalloc((FF_MAX_LFN+1)*2); if (!lfn) LEAVE_FF(fs, FR_NOT_ENOUGH_CORE); (fs)->lfnbuf = lfn; }
#define FREE_NAMBUF()   ff_memfree(lfn)
#endif
#define LEAVE_MKFS(res) { if (!work) ff_memfree(buf); return res; }
#define MAX_MALLOC  0x8000   

#else
#error Wrong setting of FF_USE_LFN

#endif   
#endif   



 
 
 

#if FF_CODE_PAGE == 0        
#define CODEPAGE CodePage
static WORD CodePage;    
static const BYTE *ExCvt, *DbcTbl;   

static const BYTE Ct437[] = TBL_CT437;
static const BYTE Ct720[] = TBL_CT720;
static const BYTE Ct737[] = TBL_CT737;
static const BYTE Ct771[] = TBL_CT771;
static const BYTE Ct775[] = TBL_CT775;
static const BYTE Ct850[] = TBL_CT850;
static const BYTE Ct852[] = TBL_CT852;
static const BYTE Ct855[] = TBL_CT855;
static const BYTE Ct857[] = TBL_CT857;
static const BYTE Ct860[] = TBL_CT860;
static const BYTE Ct861[] = TBL_CT861;
static const BYTE Ct862[] = TBL_CT862;
static const BYTE Ct863[] = TBL_CT863;
static const BYTE Ct864[] = TBL_CT864;
static const BYTE Ct865[] = TBL_CT865;
static const BYTE Ct866[] = TBL_CT866;
static const BYTE Ct869[] = TBL_CT869;
static const BYTE Dc932[] = TBL_DC932;
static const BYTE Dc936[] = TBL_DC936;
static const BYTE Dc949[] = TBL_DC949;
static const BYTE Dc950[] = TBL_DC950;

#elif FF_CODE_PAGE < 900     
#define CODEPAGE FF_CODE_PAGE
static const BYTE ExCvt[] = MKCVTBL(TBL_CT, FF_CODE_PAGE);

#else                    
#define CODEPAGE FF_CODE_PAGE
static const BYTE DbcTbl[] = MKCVTBL(TBL_DC, FF_CODE_PAGE);

#endif




 


 
 
 

static WORD ld_word (const BYTE* ptr)    
{
    WORD rv;

    rv = ptr[1];
    rv = rv << 8 | ptr[0];
    return rv;
}

static DWORD ld_dword (const BYTE* ptr)  
{
    DWORD rv;

    rv = ptr[3];
    rv = rv << 8 | ptr[2];
    rv = rv << 8 | ptr[1];
    rv = rv << 8 | ptr[0];
    return rv;
}

#if FF_FS_EXFAT
static QWORD ld_qword (const BYTE* ptr)  
{
    QWORD rv;

    rv = ptr[7];
    rv = rv << 8 | ptr[6];
    rv = rv << 8 | ptr[5];
    rv = rv << 8 | ptr[4];
    rv = rv << 8 | ptr[3];
    rv = rv << 8 | ptr[2];
    rv = rv << 8 | ptr[1];
    rv = rv << 8 | ptr[0];
    return rv;
}
#endif

#if !FF_FS_READONLY
static void st_word (BYTE* ptr, WORD val)    
{
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val;
}

static void st_dword (BYTE* ptr, DWORD val)  
{
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val;
}

#if FF_FS_EXFAT
static void st_qword (BYTE* ptr, QWORD val)  
{
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val; val >>= 8;
    *ptr++ = (BYTE)val;
}
#endif
#endif   



 
 
 




#define mem_cpy memcpy
#define mem_set memset
#define mem_cmp memcmp


 
static int chk_chr (const char* str, int chr)    
{
    while (*str && *str != chr) str++;
    return *str;
}


 
static int dbc_1st (BYTE c)
{
#if FF_CODE_PAGE == 0        
    if (DbcTbl && c >= DbcTbl[0]) {
        if (c <= DbcTbl[1]) return 1;                    
        if (c >= DbcTbl[2] && c <= DbcTbl[3]) return 1;  
    }
#elif FF_CODE_PAGE >= 900    
    if (c >= DbcTbl[0]) {
        if (c <= DbcTbl[1]) return 1;
        if (c >= DbcTbl[2] && c <= DbcTbl[3]) return 1;
    }
#else                        
    if (c != 0) return 0;    
#endif
    return 0;
}


 
static int dbc_2nd (BYTE c)
{
#if FF_CODE_PAGE == 0        
    if (DbcTbl && c >= DbcTbl[4]) {
        if (c <= DbcTbl[5]) return 1;                    
        if (c >= DbcTbl[6] && c <= DbcTbl[7]) return 1;  
        if (c >= DbcTbl[8] && c <= DbcTbl[9]) return 1;  
    }
#elif FF_CODE_PAGE >= 900    
    if (c >= DbcTbl[4]) {
        if (c <= DbcTbl[5]) return 1;
        if (c >= DbcTbl[6] && c <= DbcTbl[7]) return 1;
        if (c >= DbcTbl[8] && c <= DbcTbl[9]) return 1;
    }
#else                        
    if (c != 0) return 0;    
#endif
    return 0;
}


#if FF_USE_LFN

 
static DWORD tchar2uni (     
    const TCHAR** str        
)
{
    DWORD uc;
    const TCHAR *p = *str;

#if FF_LFN_UNICODE == 1      
    WCHAR wc;

    uc = *p++;   
    if (IsSurrogate(uc)) {   
        wc = *p++;       
        if (!IsSurrogateH(uc) || !IsSurrogateL(wc)) return 0xFFFFFFFF;   
        uc = uc << 16 | wc;
    }

#elif FF_LFN_UNICODE == 2    
    BYTE b;
    int nf;

    uc = (BYTE)*p++;     
    if (uc & 0x80) {     
        if ((uc & 0xE0) == 0xC0) {   
            uc &= 0x1F; nf = 1;
        } else {
            if ((uc & 0xF0) == 0xE0) {   
                uc &= 0x0F; nf = 2;
            } else {
                if ((uc & 0xF8) == 0xF0) {   
                    uc &= 0x07; nf = 3;
                } else {                     
                    return 0xFFFFFFFF;
                }
            }
        }
        do {     
            b = (BYTE)*p++;
            if ((b & 0xC0) != 0x80) return 0xFFFFFFFF;   
            uc = uc << 6 | (b & 0x3F);
        } while (--nf != 0);
        if (uc < 0x80 || IsSurrogate(uc) || uc >= 0x110000) return 0xFFFFFFFF;   
        if (uc >= 0x010000) uc = 0xD800DC00 | ((uc - 0x10000) << 6 & 0x3FF0000) | (uc & 0x3FF);  
    }

#elif FF_LFN_UNICODE == 3    
    uc = (TCHAR)*p++;    
    if (uc >= 0x110000) return 0xFFFFFFFF;   
    if (uc >= 0x010000) uc = 0xD800DC00 | ((uc - 0x10000) << 6 & 0x3FF0000) | (uc & 0x3FF);  

#else        
    BYTE b;
    WCHAR wc;

    wc = (BYTE)*p++;             
    if (dbc_1st((BYTE)wc)) {     
        b = (BYTE)*p++;          
        if (!dbc_2nd(b)) return 0xFFFFFFFF;  
        wc = (wc << 8) + b;      
    }
    if (wc != 0) {
        wc = ff_oem2uni(wc, CODEPAGE);   
        if (wc == 0) return 0xFFFFFFFF;  
    }
    uc = wc;

#endif
    *str = p;    
    return uc;
}


 
static BYTE put_utf (    
    DWORD chr,   
    TCHAR* buf,  
    UINT szb     
)
{
#if FF_LFN_UNICODE == 1  
    WCHAR hs, wc;

    hs = (WCHAR)(chr >> 16);
    wc = (WCHAR)chr;
    if (hs == 0) {   
        if (szb < 1 || IsSurrogate(wc)) return 0;    
        *buf = wc;
        return 1;
    }
    if (szb < 2 || !IsSurrogateH(hs) || !IsSurrogateL(wc)) return 0;     
    *buf++ = hs;
    *buf++ = wc;
    return 2;

#elif FF_LFN_UNICODE == 2    
    DWORD hc;

    if (chr < 0x80) {    
        if (szb < 1) return 0;   
        *buf = (TCHAR)chr;
        return 1;
    }
    if (chr < 0x800) {   
        if (szb < 2) return 0;   
        *buf++ = (TCHAR)(0xC0 | (chr >> 6 & 0x1F));
        *buf++ = (TCHAR)(0x80 | (chr >> 0 & 0x3F));
        return 2;
    }
    if (chr < 0x10000) {     
        if (szb < 3 || IsSurrogate(chr)) return 0;   
        *buf++ = (TCHAR)(0xE0 | (chr >> 12 & 0x0F));
        *buf++ = (TCHAR)(0x80 | (chr >> 6 & 0x3F));
        *buf++ = (TCHAR)(0x80 | (chr >> 0 & 0x3F));
        return 3;
    }
     
    if (szb < 4) return 0;   
    hc = ((chr & 0xFFFF0000) - 0xD8000000) >> 6;     
    chr = (chr & 0xFFFF) - 0xDC00;                   
    if (hc >= 0x100000 || chr >= 0x400) return 0;    
    chr = (hc | chr) + 0x10000;
    *buf++ = (TCHAR)(0xF0 | (chr >> 18 & 0x07));
    *buf++ = (TCHAR)(0x80 | (chr >> 12 & 0x3F));
    *buf++ = (TCHAR)(0x80 | (chr >> 6 & 0x3F));
    *buf++ = (TCHAR)(0x80 | (chr >> 0 & 0x3F));
    return 4;

#elif FF_LFN_UNICODE == 3    
    DWORD hc;

    if (szb < 1) return 0;   
    if (chr >= 0x10000) {    
        hc = ((chr & 0xFFFF0000) - 0xD8000000) >> 6;     
        chr = (chr & 0xFFFF) - 0xDC00;                   
        if (hc >= 0x100000 || chr >= 0x400) return 0;    
        chr = (hc | chr) + 0x10000;
    }
    *buf++ = (TCHAR)chr;
    return 1;

#else                        
    WCHAR wc;

    wc = ff_uni2oem(chr, CODEPAGE);
    if (wc >= 0x100) {   
        if (szb < 2) return 0;
        *buf++ = (char)(wc >> 8);    
        *buf++ = (TCHAR)wc;          
        return 2;
    }
    if (wc == 0 || szb < 1) return 0;    
    *buf++ = (TCHAR)wc;                  
    return 1;
#endif
}
#endif   


#if FF_FS_REENTRANT
 
 
 
static int lock_fs (         
    FATFS* fs        
)
{
    return ff_req_grant(fs->sobj);
}


static void unlock_fs (
    FATFS* fs,       
    FRESULT res      
)
{
    if (fs && res != FR_NOT_ENABLED && res != FR_INVALID_DRIVE && res != FR_TIMEOUT) {
        ff_rel_grant(fs->sobj);
    }
}

#endif



#if FF_FS_LOCK != 0
 
 
 

static FRESULT chk_lock (    
    DIR* dp,         
    int acc          
)
{
    UINT i, be;

     
    be = 0;
    for (i = 0; i < FF_FS_LOCK; i++) {
        if (Files[i].fs) {   
            if (Files[i].fs == dp->obj.fs &&         
                Files[i].clu == dp->obj.sclust &&
                Files[i].ofs == dp->dptr) break;
        } else {             
            be = 1;
        }
    }
    if (i == FF_FS_LOCK) {   
        return (!be && acc != 2) ? FR_TOO_MANY_OPEN_FILES : FR_OK;   
    }

     
    return (acc != 0 || Files[i].ctr == 0x100) ? FR_LOCKED : FR_OK;
}


static int enq_lock (void)   
{
    UINT i;

    for (i = 0; i < FF_FS_LOCK && Files[i].fs; i++) ;
    return (i == FF_FS_LOCK) ? 0 : 1;
}


static UINT inc_lock (   
    DIR* dp,     
    int acc      
)
{
    UINT i;


    for (i = 0; i < FF_FS_LOCK; i++) {   
        if (Files[i].fs == dp->obj.fs &&
            Files[i].clu == dp->obj.sclust &&
            Files[i].ofs == dp->dptr) break;
    }

    if (i == FF_FS_LOCK) {               
        for (i = 0; i < FF_FS_LOCK && Files[i].fs; i++) ;
        if (i == FF_FS_LOCK) return 0;   
        Files[i].fs = dp->obj.fs;
        Files[i].clu = dp->obj.sclust;
        Files[i].ofs = dp->dptr;
        Files[i].ctr = 0;
    }

    if (acc >= 1 && Files[i].ctr) return 0;  

    Files[i].ctr = acc ? 0x100 : Files[i].ctr + 1;   

    return i + 1;    
}


static FRESULT dec_lock (    
    UINT i           
)
{
    WORD n;
    FRESULT res;


    if (--i < FF_FS_LOCK) {  
        n = Files[i].ctr;
        if (n == 0x100) n = 0;       
        if (n > 0) n--;              
        Files[i].ctr = n;
        if (n == 0) Files[i].fs = 0;     
        res = FR_OK;
    } else {
        res = FR_INT_ERR;            
    }
    return res;
}


static void clear_lock (     
    FATFS *fs
)
{
    UINT i;

    for (i = 0; i < FF_FS_LOCK; i++) {
        if (Files[i].fs == fs) Files[i].fs = 0;
    }
}

#endif   



 
 
 
#if !FF_FS_READONLY
static FRESULT sync_window (     
    FATFS* fs            
)
{
    FRESULT res = FR_OK;


    if (fs->wflag) {     
        if (disk_write(fs->drv, fs->win, fs->winsect, 1) == RES_OK) {    
            fs->wflag = 0;   
            if (fs->winsect - fs->fatbase < fs->fsize) {     
                if (fs->n_fats == 2) disk_write(fs->drv, fs->win, fs->winsect + fs->fsize, 1);  
            }
        } else {
            res = FR_DISK_ERR;
        }
    }
    return res;
}
#endif


static FRESULT move_window (     
    FATFS* fs,           
    DWORD sector         
)
{
    FRESULT res = FR_OK;


    if (sector != fs->winsect) {     
#if !FF_FS_READONLY
        res = sync_window(fs);       
#endif
        if (res == FR_OK) {          
            if (disk_read(fs->drv, fs->win, sector, 1) != RES_OK) {
                sector = 0xFFFFFFFF;     
                res = FR_DISK_ERR;
            }
            fs->winsect = sector;
        }
    }
    return res;
}




#if !FF_FS_READONLY
 
 
 

static FRESULT sync_fs (     
    FATFS* fs        
)
{
    FRESULT res;


    res = sync_window(fs);
    if (res == FR_OK) {
        if (fs->fs_type == FS_FAT32 && fs->fsi_flag == 1) {  
             
            mem_set(fs->win, 0, sizeof fs->win);
            st_word(fs->win + BS_55AA, 0xAA55);
            st_dword(fs->win + FSI_LeadSig, 0x41615252);
            st_dword(fs->win + FSI_StrucSig, 0x61417272);
            st_dword(fs->win + FSI_Free_Count, fs->free_clst);
            st_dword(fs->win + FSI_Nxt_Free, fs->last_clst);
             
            fs->winsect = fs->volbase + 1;
            disk_write(fs->drv, fs->win, fs->winsect, 1);
            fs->fsi_flag = 0;
        }
         
        if (disk_ioctl(fs->drv, CTRL_SYNC, 0) != RES_OK) res = FR_DISK_ERR;
    }

    return res;
}

#endif



 
 
 

static DWORD clst2sect (     
    FATFS* fs,       
    DWORD clst       
)
{
    clst -= 2;       
    if (clst >= fs->n_fatent - 2) return 0;      
    return fs->database + fs->csize * clst;      
}




 
 
 

static DWORD get_fat (       
    FFOBJID* obj,    
    DWORD clst       
)
{
    UINT wc, bc;
    DWORD val;
    FATFS *fs = obj->fs;


    if (clst < 2 || clst >= fs->n_fatent) {  
        val = 1;     

    } else {
        val = 0xFFFFFFFF;    

        switch (fs->fs_type) {
        case FS_FAT12 :
            bc = (UINT)clst; bc += bc / 2;
            if (move_window(fs, fs->fatbase + (bc / SS(fs))) != FR_OK) break;
            wc = fs->win[bc++ % SS(fs)];         
            if (move_window(fs, fs->fatbase + (bc / SS(fs))) != FR_OK) break;
            wc |= fs->win[bc % SS(fs)] << 8;     
            val = (clst & 1) ? (wc >> 4) : (wc & 0xFFF);     
            break;

        case FS_FAT16 :
            if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 2))) != FR_OK) break;
            val = ld_word(fs->win + clst * 2 % SS(fs));      
            break;

        case FS_FAT32 :
            if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 4))) != FR_OK) break;
            val = ld_dword(fs->win + clst * 4 % SS(fs)) & 0x0FFFFFFF;    
            break;
#if FF_FS_EXFAT
        case FS_EXFAT :
            if ((obj->objsize != 0 && obj->sclust != 0) || obj->stat == 0) {     
                DWORD cofs = clst - obj->sclust;     
                DWORD clen = (DWORD)((obj->objsize - 1) / SS(fs)) / fs->csize;   

                if (obj->stat == 2 && cofs <= clen) {    
                    val = (cofs == clen) ? 0x7FFFFFFF : clst + 1;    
                    break;
                }
                if (obj->stat == 3 && cofs < obj->n_cont) {  
                    val = clst + 1;      
                    break;
                }
                if (obj->stat != 2) {    
                    if (obj->n_frag != 0) {  
                        val = 0x7FFFFFFF;    
                    } else {
                        if (move_window(fs, fs->fatbase + (clst / (SS(fs) / 4))) != FR_OK) break;
                        val = ld_dword(fs->win + clst * 4 % SS(fs)) & 0x7FFFFFFF;
                    }
                    break;
                }
            }
             
#endif
        default:
            val = 1;     
        }
    }

    return val;
}




#if !FF_FS_READONLY
 
 
 

static FRESULT put_fat (     
    FATFS* fs,       
    DWORD clst,      
    DWORD val        
)
{
    UINT bc;
    BYTE *p;
    FRESULT res = FR_INT_ERR;


    if (clst >= 2 && clst < fs->n_fatent) {  
        switch (fs->fs_type) {
        case FS_FAT12 :
            bc = (UINT)clst; bc += bc / 2;   
            res = move_window(fs, fs->fatbase + (bc / SS(fs)));
            if (res != FR_OK) break;
            p = fs->win + bc++ % SS(fs);
            *p = (clst & 1) ? ((*p & 0x0F) | ((BYTE)val << 4)) : (BYTE)val;      
            fs->wflag = 1;
            res = move_window(fs, fs->fatbase + (bc / SS(fs)));
            if (res != FR_OK) break;
            p = fs->win + bc % SS(fs);
            *p = (clst & 1) ? (BYTE)(val >> 4) : ((*p & 0xF0) | ((BYTE)(val >> 8) & 0x0F));  
            fs->wflag = 1;
            break;

        case FS_FAT16 :
            res = move_window(fs, fs->fatbase + (clst / (SS(fs) / 2)));
            if (res != FR_OK) break;
            st_word(fs->win + clst * 2 % SS(fs), (WORD)val);     
            fs->wflag = 1;
            break;

        case FS_FAT32 :
#if FF_FS_EXFAT
        case FS_EXFAT :
#endif
            res = move_window(fs, fs->fatbase + (clst / (SS(fs) / 4)));
            if (res != FR_OK) break;
            if (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) {
                val = (val & 0x0FFFFFFF) | (ld_dword(fs->win + clst * 4 % SS(fs)) & 0xF0000000);
            }
            st_dword(fs->win + clst * 4 % SS(fs), val);
            fs->wflag = 1;
            break;
        }
    }
    return res;
}

#endif  




#if FF_FS_EXFAT && !FF_FS_READONLY
 
 
 

 
 
 

static DWORD find_bitmap (   
    FATFS* fs,   
    DWORD clst,  
    DWORD ncl    
)
{
    BYTE bm, bv;
    UINT i;
    DWORD val, scl, ctr;


    clst -= 2;   
    if (clst >= fs->n_fatent - 2) clst = 0;
    scl = val = clst; ctr = 0;
    for (;;) {
        if (move_window(fs, fs->bitbase + val / 8 / SS(fs)) != FR_OK) return 0xFFFFFFFF;
        i = val / 8 % SS(fs); bm = 1 << (val % 8);
        do {
            do {
                bv = fs->win[i] & bm; bm <<= 1;      
                if (++val >= fs->n_fatent - 2) {     
                    val = 0; bm = 0; i = SS(fs);
                }
                if (bv == 0) {   
                    if (++ctr == ncl) return scl + 2;    
                } else {
                    scl = val; ctr = 0;      
                }
                if (val == clst) return 0;   
            } while (bm != 0);
            bm = 1;
        } while (++i < SS(fs));
    }
}


 
 
 

static FRESULT change_bitmap (
    FATFS* fs,   
    DWORD clst,  
    DWORD ncl,   
    int bv       
)
{
    BYTE bm;
    UINT i;
    DWORD sect;


    clst -= 2;   
    sect = fs->bitbase + clst / 8 / SS(fs);  
    i = clst / 8 % SS(fs);                   
    bm = 1 << (clst % 8);                    
    for (;;) {
        if (move_window(fs, sect++) != FR_OK) return FR_DISK_ERR;
        do {
            do {
                if (bv == (int)((fs->win[i] & bm) != 0)) return FR_INT_ERR;  
                fs->win[i] ^= bm;    
                fs->wflag = 1;
                if (--ncl == 0) return FR_OK;    
            } while (bm <<= 1);      
            bm = 1;
        } while (++i < SS(fs));      
        i = 0;
    }
}


 
 
 

static FRESULT fill_first_frag (
    FFOBJID* obj     
)
{
    FRESULT res;
    DWORD cl, n;


    if (obj->stat == 3) {    
        for (cl = obj->sclust, n = obj->n_cont; n; cl++, n--) {  
            res = put_fat(obj->fs, cl, cl + 1);
            if (res != FR_OK) return res;
        }
        obj->stat = 0;   
    }
    return FR_OK;
}


 
 
 

static FRESULT fill_last_frag (
    FFOBJID* obj,    
    DWORD lcl,       
    DWORD term       
)
{
    FRESULT res;


    while (obj->n_frag > 0) {    
        res = put_fat(obj->fs, lcl - obj->n_frag + 1, (obj->n_frag > 1) ? lcl - obj->n_frag + 2 : term);
        if (res != FR_OK) return res;
        obj->n_frag--;
    }
    return FR_OK;
}

#endif   



#if !FF_FS_READONLY
 
 
 

static FRESULT remove_chain (    
    FFOBJID* obj,        
    DWORD clst,          
    DWORD pclst          
)
{
    FRESULT res = FR_OK;
    DWORD nxt;
    FATFS *fs = obj->fs;
#if FF_FS_EXFAT || FF_USE_TRIM
    DWORD scl = clst, ecl = clst;
#endif
#if FF_USE_TRIM
    DWORD rt[2];
#endif

    if (clst < 2 || clst >= fs->n_fatent) return FR_INT_ERR;     

     
    if (pclst != 0 && (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT || obj->stat != 2)) {
        res = put_fat(fs, pclst, 0xFFFFFFFF);
        if (res != FR_OK) return res;
    }

     
    do {
        nxt = get_fat(obj, clst);            
        if (nxt == 0) break;                 
        if (nxt == 1) return FR_INT_ERR;     
        if (nxt == 0xFFFFFFFF) return FR_DISK_ERR;   
        if (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) {
            res = put_fat(fs, clst, 0);      
            if (res != FR_OK) return res;
        }
        if (fs->free_clst < fs->n_fatent - 2) {  
            fs->free_clst++;
            fs->fsi_flag |= 1;
        }
#if FF_FS_EXFAT || FF_USE_TRIM
        if (ecl + 1 == nxt) {    
            ecl = nxt;
        } else {                 
#if FF_FS_EXFAT
            if (fs->fs_type == FS_EXFAT) {
                res = change_bitmap(fs, scl, ecl - scl + 1, 0);  
                if (res != FR_OK) return res;
            }
#endif
#if FF_USE_TRIM
            rt[0] = clst2sect(fs, scl);                  
            rt[1] = clst2sect(fs, ecl) + fs->csize - 1;  
            disk_ioctl(fs->drv, CTRL_TRIM, rt);          
#endif
            scl = ecl = nxt;
        }
#endif
        clst = nxt;                  
    } while (clst < fs->n_fatent);   

#if FF_FS_EXFAT
     
    if (fs->fs_type == FS_EXFAT) {
        if (pclst == 0) {    
            obj->stat = 0;       
        } else {
            if (obj->stat == 0) {    
                clst = obj->sclust;      
                while (clst != pclst) {
                    nxt = get_fat(obj, clst);
                    if (nxt < 2) return FR_INT_ERR;
                    if (nxt == 0xFFFFFFFF) return FR_DISK_ERR;
                    if (nxt != clst + 1) break;  
                    clst++;
                }
                if (clst == pclst) {     
                    obj->stat = 2;       
                }
            } else {
                if (obj->stat == 3 && pclst >= obj->sclust && pclst <= obj->sclust + obj->n_cont) {  
                    obj->stat = 2;   
                }
            }
        }
    }
#endif
    return FR_OK;
}




 
 
 

static DWORD create_chain (  
    FFOBJID* obj,        
    DWORD clst           
)
{
    DWORD cs, ncl, scl;
    FRESULT res;
    FATFS *fs = obj->fs;


    if (clst == 0) {     
        scl = fs->last_clst;                 
        if (scl == 0 || scl >= fs->n_fatent) scl = 1;
    }
    else {               
        cs = get_fat(obj, clst);             
        if (cs < 2) return 1;                
        if (cs == 0xFFFFFFFF) return cs;     
        if (cs < fs->n_fatent) return cs;    
        scl = clst;                          
    }
    if (fs->free_clst == 0) return 0;        

#if FF_FS_EXFAT
    if (fs->fs_type == FS_EXFAT) {   
        ncl = find_bitmap(fs, scl, 1);               
        if (ncl == 0 || ncl == 0xFFFFFFFF) return ncl;   
        res = change_bitmap(fs, ncl, 1, 1);          
        if (res == FR_INT_ERR) return 1;
        if (res == FR_DISK_ERR) return 0xFFFFFFFF;
        if (clst == 0) {                             
            obj->stat = 2;                           
        } else {                                     
            if (obj->stat == 2 && ncl != scl + 1) {  
                obj->n_cont = scl - obj->sclust;     
                obj->stat = 3;                       
            }
        }
        if (obj->stat != 2) {    
            if (ncl == clst + 1) {   
                obj->n_frag = obj->n_frag ? obj->n_frag + 1 : 2;     
            } else {                 
                if (obj->n_frag == 0) obj->n_frag = 1;
                res = fill_last_frag(obj, clst, ncl);    
                if (res == FR_OK) obj->n_frag = 1;
            }
        }
    } else
#endif
    {    
        ncl = 0;
        if (scl == clst) {                       
            ncl = scl + 1;                       
            if (ncl >= fs->n_fatent) ncl = 2;
            cs = get_fat(obj, ncl);              
            if (cs == 1 || cs == 0xFFFFFFFF) return cs;  
            if (cs != 0) {                       
                cs = fs->last_clst;              
                if (cs >= 2 && cs < fs->n_fatent) scl = cs;
                ncl = 0;
            }
        }
        if (ncl == 0) {  
            ncl = scl;   
            for (;;) {
                ncl++;                           
                if (ncl >= fs->n_fatent) {       
                    ncl = 2;
                    if (ncl > scl) return 0;     
                }
                cs = get_fat(obj, ncl);          
                if (cs == 0) break;              
                if (cs == 1 || cs == 0xFFFFFFFF) return cs;  
                if (ncl == scl) return 0;        
            }
        }
        res = put_fat(fs, ncl, 0xFFFFFFFF);      
        if (res == FR_OK && clst != 0) {
            res = put_fat(fs, clst, ncl);        
        }
    }

    if (res == FR_OK) {          
        fs->last_clst = ncl;
        if (fs->free_clst <= fs->n_fatent - 2) fs->free_clst--;
        fs->fsi_flag |= 1;
    } else {
        ncl = (res == FR_DISK_ERR) ? 0xFFFFFFFF : 1;     
    }

    return ncl;      
}

#endif  




#if FF_USE_FASTSEEK
 
 
 

static DWORD clmt_clust (    
    FIL* fp,         
    FSIZE_t ofs      
)
{
    DWORD cl, ncl, *tbl;
    FATFS *fs = fp->obj.fs;


    tbl = fp->cltbl + 1;     
    cl = (DWORD)(ofs / SS(fs) / fs->csize);  
    for (;;) {
        ncl = *tbl++;            
        if (ncl == 0) return 0;  
        if (cl < ncl) break;     
        cl -= ncl; tbl++;        
    }
    return cl + *tbl;    
}

#endif   




 
 
 

#if !FF_FS_READONLY
static FRESULT dir_clear (   
    FATFS *fs,       
    DWORD clst       
)
{
    DWORD sect;
    UINT n, szb;
    BYTE *ibuf;


    if (sync_window(fs) != FR_OK) return FR_DISK_ERR;    
    sect = clst2sect(fs, clst);      
    fs->winsect = sect;              
    mem_set(fs->win, 0, sizeof fs->win);     
#if FF_USE_LFN == 3      
     
    for (szb = ((DWORD)fs->csize * SS(fs) >= MAX_MALLOC) ? MAX_MALLOC : fs->csize * SS(fs), ibuf = 0; szb > SS(fs) && (ibuf = ff_memalloc(szb)) == 0; szb /= 2) ;
    if (szb > SS(fs)) {      
        mem_set(ibuf, 0, szb);
        szb /= SS(fs);       
        for (n = 0; n < fs->csize && disk_write(fs->drv, ibuf, sect + n, szb) == RES_OK; n += szb) ;    
        ff_memfree(ibuf);
    } else
#endif
    {
        ibuf = fs->win; szb = 1;     
        for (n = 0; n < fs->csize && disk_write(fs->drv, ibuf, sect + n, szb) == RES_OK; n += szb) ;    
    }
    return (n == fs->csize) ? FR_OK : FR_DISK_ERR;
}
#endif   




 
 
 

static FRESULT dir_sdi (     
    DIR* dp,         
    DWORD ofs        
)
{
    DWORD csz, clst;
    FATFS *fs = dp->obj.fs;


    if (ofs >= (DWORD)((FF_FS_EXFAT && fs->fs_type == FS_EXFAT) ? MAX_DIR_EX : MAX_DIR) || ofs % SZDIRE) {   
        return FR_INT_ERR;
    }
    dp->dptr = ofs;              
    clst = dp->obj.sclust;       
    if (clst == 0 && fs->fs_type >= FS_FAT32) {  
        clst = fs->dirbase;
        if (FF_FS_EXFAT) dp->obj.stat = 0;   
    }

    if (clst == 0) {     
        if (ofs / SZDIRE >= fs->n_rootdir) return FR_INT_ERR;    
        dp->sect = fs->dirbase;

    } else {             
        csz = (DWORD)fs->csize * SS(fs);     
        while (ofs >= csz) {                 
            clst = get_fat(&dp->obj, clst);              
            if (clst == 0xFFFFFFFF) return FR_DISK_ERR;  
            if (clst < 2 || clst >= fs->n_fatent) return FR_INT_ERR;     
            ofs -= csz;
        }
        dp->sect = clst2sect(fs, clst);
    }
    dp->clust = clst;                    
    if (dp->sect == 0) return FR_INT_ERR;
    dp->sect += ofs / SS(fs);            
    dp->dir = fs->win + (ofs % SS(fs));  

    return FR_OK;
}




 
 
 

static FRESULT dir_next (    
    DIR* dp,                 
    int stretch              
)
{
    DWORD ofs, clst;
    FATFS *fs = dp->obj.fs;


    ofs = dp->dptr + SZDIRE;     
    if (ofs >= (DWORD)((FF_FS_EXFAT && fs->fs_type == FS_EXFAT) ? MAX_DIR_EX : MAX_DIR)) dp->sect = 0;   
    if (dp->sect == 0) return FR_NO_FILE;    

    if (ofs % SS(fs) == 0) {     
        dp->sect++;              

        if (dp->clust == 0) {    
            if (ofs / SZDIRE >= fs->n_rootdir) {     
                dp->sect = 0; return FR_NO_FILE;
            }
        }
        else {                   
            if ((ofs / SS(fs) & (fs->csize - 1)) == 0) {     
                clst = get_fat(&dp->obj, dp->clust);         
                if (clst <= 1) return FR_INT_ERR;            
                if (clst == 0xFFFFFFFF) return FR_DISK_ERR;  
                if (clst >= fs->n_fatent) {                  
#if !FF_FS_READONLY
                    if (!stretch) {                              
                        dp->sect = 0; return FR_NO_FILE;
                    }
                    clst = create_chain(&dp->obj, dp->clust);    
                    if (clst == 0) return FR_DENIED;             
                    if (clst == 1) return FR_INT_ERR;            
                    if (clst == 0xFFFFFFFF) return FR_DISK_ERR;  
                    if (dir_clear(fs, clst) != FR_OK) return FR_DISK_ERR;    
                    if (FF_FS_EXFAT) dp->obj.stat |= 4;          
#else
                    if (!stretch) dp->sect = 0;                  
                    dp->sect = 0; return FR_NO_FILE;             
#endif
                }
                dp->clust = clst;        
                dp->sect = clst2sect(fs, clst);
            }
        }
    }
    dp->dptr = ofs;                      
    dp->dir = fs->win + ofs % SS(fs);    

    return FR_OK;
}




#if !FF_FS_READONLY
 
 
 

static FRESULT dir_alloc (   
    DIR* dp,                 
    UINT nent                
)
{
    FRESULT res;
    UINT n;
    FATFS *fs = dp->obj.fs;


    res = dir_sdi(dp, 0);
    if (res == FR_OK) {
        n = 0;
        do {
            res = move_window(fs, dp->sect);
            if (res != FR_OK) break;
#if FF_FS_EXFAT
            if ((fs->fs_type == FS_EXFAT) ? (int)((dp->dir[XDIR_Type] & 0x80) == 0) : (int)(dp->dir[DIR_Name] == DDEM || dp->dir[DIR_Name] == 0)) {
#else
            if (dp->dir[DIR_Name] == DDEM || dp->dir[DIR_Name] == 0) {
#endif
                if (++n == nent) break;  
            } else {
                n = 0;                   
            }
            res = dir_next(dp, 1);
        } while (res == FR_OK);  
    }

    if (res == FR_NO_FILE) res = FR_DENIED;  
    return res;
}

#endif   




 
 
 

static DWORD ld_clust (  
    FATFS* fs,           
    const BYTE* dir      
)
{
    DWORD cl;

    cl = ld_word(dir + DIR_FstClusLO);
    if (fs->fs_type == FS_FAT32) {
        cl |= (DWORD)ld_word(dir + DIR_FstClusHI) << 16;
    }

    return cl;
}


#if !FF_FS_READONLY
static void st_clust (
    FATFS* fs,   
    BYTE* dir,   
    DWORD cl     
)
{
    st_word(dir + DIR_FstClusLO, (WORD)cl);
    if (fs->fs_type == FS_FAT32) {
        st_word(dir + DIR_FstClusHI, (WORD)(cl >> 16));
    }
}
#endif



#if FF_USE_LFN
 
 
 

static int cmp_lfn (         
    const WCHAR* lfnbuf,     
    BYTE* dir                
)
{
    UINT i, s;
    WCHAR wc, uc;


    if (ld_word(dir + LDIR_FstClusLO) != 0) return 0;    

    i = ((dir[LDIR_Ord] & 0x3F) - 1) * 13;   

    for (wc = 1, s = 0; s < 13; s++) {       
        uc = ld_word(dir + LfnOfs[s]);       
        if (wc != 0) {
            if (i >= FF_MAX_LFN + 1 || ff_wtoupper(uc) != ff_wtoupper(lfnbuf[i++])) {  
                return 0;                    
            }
            wc = uc;
        } else {
            if (uc != 0xFFFF) return 0;      
        }
    }

    if ((dir[LDIR_Ord] & LLEF) && wc && lfnbuf[i]) return 0;     

    return 1;        
}


#if FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2 || FF_USE_LABEL || FF_FS_EXFAT
 
 
 

static int pick_lfn (    
    WCHAR* lfnbuf,       
    BYTE* dir            
)
{
    UINT i, s;
    WCHAR wc, uc;


    if (ld_word(dir + LDIR_FstClusLO) != 0) return 0;    

    i = ((dir[LDIR_Ord] & ~LLEF) - 1) * 13;  

    for (wc = 1, s = 0; s < 13; s++) {       
        uc = ld_word(dir + LfnOfs[s]);       
        if (wc != 0) {
            if (i >= FF_MAX_LFN + 1) return 0;  
            lfnbuf[i++] = wc = uc;           
        } else {
            if (uc != 0xFFFF) return 0;      
        }
    }

    if (dir[LDIR_Ord] & LLEF && wc != 0) {   
        if (i >= FF_MAX_LFN + 1) return 0;   
        lfnbuf[i] = 0;
    }

    return 1;        
}
#endif


#if !FF_FS_READONLY
 
 
 

static void put_lfn (
    const WCHAR* lfn,    
    BYTE* dir,           
    BYTE ord,            
    BYTE sum             
)
{
    UINT i, s;
    WCHAR wc;


    dir[LDIR_Chksum] = sum;          
    dir[LDIR_Attr] = AM_LFN;         
    dir[LDIR_Type] = 0;
    st_word(dir + LDIR_FstClusLO, 0);

    i = (ord - 1) * 13;              
    s = wc = 0;
    do {
        if (wc != 0xFFFF) wc = lfn[i++];     
        st_word(dir + LfnOfs[s], wc);        
        if (wc == 0) wc = 0xFFFF;        
    } while (++s < 13);
    if (wc == 0xFFFF || !lfn[i]) ord |= LLEF;    
    dir[LDIR_Ord] = ord;             
}

#endif   
#endif   



#if FF_USE_LFN && !FF_FS_READONLY
 
 
 

static void gen_numname (
    BYTE* dst,           
    const BYTE* src,     
    const WCHAR* lfn,    
    UINT seq             
)
{
    BYTE ns[8], c;
    UINT i, j;
    WCHAR wc;
    DWORD sr;


    mem_cpy(dst, src, 11);

    if (seq > 5) {   
        sr = seq;
        while (*lfn) {   
            wc = *lfn++;
            for (i = 0; i < 16; i++) {
                sr = (sr << 1) + (wc & 1);
                wc >>= 1;
                if (sr & 0x10000) sr ^= 0x11021;
            }
        }
        seq = (UINT)sr;
    }

     
    i = 7;
    do {
        c = (BYTE)((seq % 16) + '0');
        if (c > '9') c += 7;
        ns[i--] = c;
        seq /= 16;
    } while (seq);
    ns[i] = '~';

     
    for (j = 0; j < i && dst[j] != ' '; j++) {
        if (dbc_1st(dst[j])) {
            if (j == i - 1) break;
            j++;
        }
    }
    do {
        dst[j++] = (i < 8) ? ns[i++] : ' ';
    } while (j < 8);
}
#endif   



#if FF_USE_LFN
 
 
 

static BYTE sum_sfn (
    const BYTE* dir      
)
{
    BYTE sum = 0;
    UINT n = 11;

    do {
        sum = (sum >> 1) + (sum << 7) + *dir++;
    } while (--n);
    return sum;
}

#endif   



#if FF_FS_EXFAT
 
 
 

static WORD xdir_sum (   
    const BYTE* dir      
)
{
    UINT i, szblk;
    WORD sum;


    szblk = (dir[XDIR_NumSec] + 1) * SZDIRE;     
    for (i = sum = 0; i < szblk; i++) {
        if (i == XDIR_SetSum) {  
            i++;
        } else {
            sum = ((sum & 1) ? 0x8000 : 0) + (sum >> 1) + dir[i];
        }
    }
    return sum;
}



static WORD xname_sum (  
    const WCHAR* name    
)
{
    WCHAR chr;
    WORD sum = 0;


    while ((chr = *name++) != 0) {
        chr = (WCHAR)ff_wtoupper(chr);       
        sum = ((sum & 1) ? 0x8000 : 0) + (sum >> 1) + (chr & 0xFF);
        sum = ((sum & 1) ? 0x8000 : 0) + (sum >> 1) + (chr >> 8);
    }
    return sum;
}


#if !FF_FS_READONLY && FF_USE_MKFS
static DWORD xsum32 (    
    BYTE  dat,           
    DWORD sum            
)
{
    sum = ((sum & 1) ? 0x80000000 : 0) + (sum >> 1) + dat;
    return sum;
}
#endif


#if FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2
 
 
 

static void get_xfileinfo (
    BYTE* dirb,          
    FILINFO* fno         
)
{
    WCHAR wc, hs;
    UINT di, si, nc;

     
    si = SZDIRE * 2;     
    nc = 0; hs = 0; di = 0;
    while (nc < dirb[XDIR_NumName]) {
        if (si >= MAXDIRB(FF_MAX_LFN)) { di = 0; break; }    
        if ((si % SZDIRE) == 0) si += 2;         
        wc = ld_word(dirb + si); si += 2; nc++;  
        if (hs == 0 && IsSurrogate(wc)) {    
            hs = wc; continue;   
        }
        wc = put_utf((DWORD)hs << 16 | wc, &fno->fname[di], FF_LFN_BUF - di);    
        if (wc == 0) { di = 0; break; }  
        di += wc;
        hs = 0;
    }
    if (hs != 0) di = 0;                     
    if (di == 0) fno->fname[di++] = '?';     
    fno->fname[di] = 0;                      
    fno->altname[0] = 0;                     

    fno->fattrib = dirb[XDIR_Attr];          
    fno->fsize = (fno->fattrib & AM_DIR) ? 0 : ld_qword(dirb + XDIR_FileSize);   
    fno->ftime = ld_word(dirb + XDIR_ModTime + 0);   
    fno->fdate = ld_word(dirb + XDIR_ModTime + 2);   
}

#endif   


 
 
 

static FRESULT load_xdir (   
    DIR* dp                  
)
{
    FRESULT res;
    UINT i, sz_ent;
    BYTE* dirb = dp->obj.fs->dirbuf;     


     
    res = move_window(dp->obj.fs, dp->sect);
    if (res != FR_OK) return res;
    if (dp->dir[XDIR_Type] != ET_FILEDIR) return FR_INT_ERR;     
    mem_cpy(dirb + 0 * SZDIRE, dp->dir, SZDIRE);
    sz_ent = (dirb[XDIR_NumSec] + 1) * SZDIRE;
    if (sz_ent < 3 * SZDIRE || sz_ent > 19 * SZDIRE) return FR_INT_ERR;

     
    res = dir_next(dp, 0);
    if (res == FR_NO_FILE) res = FR_INT_ERR;     
    if (res != FR_OK) return res;
    res = move_window(dp->obj.fs, dp->sect);
    if (res != FR_OK) return res;
    if (dp->dir[XDIR_Type] != ET_STREAM) return FR_INT_ERR;  
    mem_cpy(dirb + 1 * SZDIRE, dp->dir, SZDIRE);
    if (MAXDIRB(dirb[XDIR_NumName]) > sz_ent) return FR_INT_ERR;

     
    i = 2 * SZDIRE;  
    do {
        res = dir_next(dp, 0);
        if (res == FR_NO_FILE) res = FR_INT_ERR;     
        if (res != FR_OK) return res;
        res = move_window(dp->obj.fs, dp->sect);
        if (res != FR_OK) return res;
        if (dp->dir[XDIR_Type] != ET_FILENAME) return FR_INT_ERR;    
        if (i < MAXDIRB(FF_MAX_LFN)) mem_cpy(dirb + i, dp->dir, SZDIRE);
    } while ((i += SZDIRE) < sz_ent);

     
    if (i <= MAXDIRB(FF_MAX_LFN)) {
        if (xdir_sum(dirb) != ld_word(dirb + XDIR_SetSum)) return FR_INT_ERR;
    }
    return FR_OK;
}


 
 
 

static void init_alloc_info (
    FATFS* fs,       
    FFOBJID* obj     
)
{
    obj->sclust = ld_dword(fs->dirbuf + XDIR_FstClus);       
    obj->objsize = ld_qword(fs->dirbuf + XDIR_FileSize);     
    obj->stat = fs->dirbuf[XDIR_GenFlags] & 2;               
    obj->n_frag = 0;                                         
}



#if !FF_FS_READONLY || FF_FS_RPATH != 0
 
 
 

static FRESULT load_obj_xdir (
    DIR* dp,             
    const FFOBJID* obj   
)
{
    FRESULT res;

     
    dp->obj.fs = obj->fs;
    dp->obj.sclust = obj->c_scl;
    dp->obj.stat = (BYTE)obj->c_size;
    dp->obj.objsize = obj->c_size & 0xFFFFFF00;
    dp->obj.n_frag = 0;
    dp->blk_ofs = obj->c_ofs;

    res = dir_sdi(dp, dp->blk_ofs);  
    if (res == FR_OK) {
        res = load_xdir(dp);         
    }
    return res;
}
#endif


#if !FF_FS_READONLY
 
 
 

static FRESULT store_xdir (
    DIR* dp              
)
{
    FRESULT res;
    UINT nent;
    BYTE* dirb = dp->obj.fs->dirbuf;     

     
    st_word(dirb + XDIR_SetSum, xdir_sum(dirb));
    nent = dirb[XDIR_NumSec] + 1;

     
    res = dir_sdi(dp, dp->blk_ofs);
    while (res == FR_OK) {
        res = move_window(dp->obj.fs, dp->sect);
        if (res != FR_OK) break;
        mem_cpy(dp->dir, dirb, SZDIRE);
        dp->obj.fs->wflag = 1;
        if (--nent == 0) break;
        dirb += SZDIRE;
        res = dir_next(dp, 0);
    }
    return (res == FR_OK || res == FR_DISK_ERR) ? res : FR_INT_ERR;
}



 
 
 

static void create_xdir (
    BYTE* dirb,          
    const WCHAR* lfn     
)
{
    UINT i;
    BYTE nc1, nlen;
    WCHAR wc;


     
    mem_set(dirb, 0, 2 * SZDIRE);
    dirb[0 * SZDIRE + XDIR_Type] = ET_FILEDIR;
    dirb[1 * SZDIRE + XDIR_Type] = ET_STREAM;

     
    i = SZDIRE * 2;  
    nlen = nc1 = 0; wc = 1;
    do {
        dirb[i++] = ET_FILENAME; dirb[i++] = 0;
        do {     
            if (wc != 0 && (wc = lfn[nlen]) != 0) nlen++;    
            st_word(dirb + i, wc);       
            i += 2;
        } while (i % SZDIRE != 0);
        nc1++;
    } while (lfn[nlen]);     

    dirb[XDIR_NumName] = nlen;       
    dirb[XDIR_NumSec] = 1 + nc1;     
    st_word(dirb + XDIR_NameHash, xname_sum(lfn));   
}

#endif   
#endif   



#if FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2 || FF_USE_LABEL || FF_FS_EXFAT
 
 
 

#define DIR_READ_FILE(dp) dir_read(dp, 0)
#define DIR_READ_LABEL(dp) dir_read(dp, 1)

static FRESULT dir_read (
    DIR* dp,         
    int vol          
)
{
    FRESULT res = FR_NO_FILE;
    FATFS *fs = dp->obj.fs;
    BYTE attr, b;
#if FF_USE_LFN
    BYTE ord = 0xFF, sum = 0xFF;
#endif

    while (dp->sect) {
        res = move_window(fs, dp->sect);
        if (res != FR_OK) break;
        b = dp->dir[DIR_Name];   
        if (b == 0) {
            res = FR_NO_FILE; break;  
        }
#if FF_FS_EXFAT
        if (fs->fs_type == FS_EXFAT) {   
            if (FF_USE_LABEL && vol) {
                if (b == ET_VLABEL) break;   
            } else {
                if (b == ET_FILEDIR) {       
                    dp->blk_ofs = dp->dptr;  
                    res = load_xdir(dp);     
                    if (res == FR_OK) {
                        dp->obj.attr = fs->dirbuf[XDIR_Attr] & AM_MASK;  
                    }
                    break;
                }
            }
        } else
#endif
        {    
            dp->obj.attr = attr = dp->dir[DIR_Attr] & AM_MASK;   
#if FF_USE_LFN       
            if (b == DDEM || b == '.' || (int)((attr & ~AM_ARC) == AM_VOL) != vol) {     
                ord = 0xFF;
            } else {
                if (attr == AM_LFN) {            
                    if (b & LLEF) {          
                        sum = dp->dir[LDIR_Chksum];
                        b &= (BYTE)~LLEF; ord = b;
                        dp->blk_ofs = dp->dptr;
                    }
                     
                    ord = (b == ord && sum == dp->dir[LDIR_Chksum] && pick_lfn(fs->lfnbuf, dp->dir)) ? ord - 1 : 0xFF;
                } else {                     
                    if (ord != 0 || sum != sum_sfn(dp->dir)) {   
                        dp->blk_ofs = 0xFFFFFFFF;            
                    }
                    break;
                }
            }
#else        
            if (b != DDEM && b != '.' && attr != AM_LFN && (int)((attr & ~AM_ARC) == AM_VOL) == vol) {   
                break;
            }
#endif
        }
        res = dir_next(dp, 0);       
        if (res != FR_OK) break;
    }

    if (res != FR_OK) dp->sect = 0;      
    return res;
}

#endif   



 
 
 

static FRESULT dir_find (    
    DIR* dp                  
)
{
    FRESULT res;
    FATFS *fs = dp->obj.fs;
    BYTE c;
#if FF_USE_LFN
    BYTE a, ord, sum;
#endif

    res = dir_sdi(dp, 0);            
    if (res != FR_OK) return res;
#if FF_FS_EXFAT
    if (fs->fs_type == FS_EXFAT) {   
        BYTE nc;
        UINT di, ni;
        WORD hash = xname_sum(fs->lfnbuf);       

        while ((res = DIR_READ_FILE(dp)) == FR_OK) {     
#if FF_MAX_LFN < 255
            if (fs->dirbuf[XDIR_NumName] > FF_MAX_LFN) continue;             
#endif
            if (ld_word(fs->dirbuf + XDIR_NameHash) != hash) continue;   
            for (nc = fs->dirbuf[XDIR_NumName], di = SZDIRE * 2, ni = 0; nc; nc--, di += 2, ni++) {  
                if ((di % SZDIRE) == 0) di += 2;
                if (ff_wtoupper(ld_word(fs->dirbuf + di)) != ff_wtoupper(fs->lfnbuf[ni])) break;
            }
            if (nc == 0 && !fs->lfnbuf[ni]) break;   
        }
        return res;
    }
#endif
     
#if FF_USE_LFN
    ord = sum = 0xFF; dp->blk_ofs = 0xFFFFFFFF;  
#endif
    do {
        res = move_window(fs, dp->sect);
        if (res != FR_OK) break;
        c = dp->dir[DIR_Name];
        if (c == 0) { res = FR_NO_FILE; break; }     
#if FF_USE_LFN       
        dp->obj.attr = a = dp->dir[DIR_Attr] & AM_MASK;
        if (c == DDEM || ((a & AM_VOL) && a != AM_LFN)) {    
            ord = 0xFF; dp->blk_ofs = 0xFFFFFFFF;    
        } else {
            if (a == AM_LFN) {           
                if (!(dp->fn[NSFLAG] & NS_NOLFN)) {
                    if (c & LLEF) {      
                        sum = dp->dir[LDIR_Chksum];
                        c &= (BYTE)~LLEF; ord = c;   
                        dp->blk_ofs = dp->dptr;  
                    }
                     
                    ord = (c == ord && sum == dp->dir[LDIR_Chksum] && cmp_lfn(fs->lfnbuf, dp->dir)) ? ord - 1 : 0xFF;
                }
            } else {                     
                if (ord == 0 && sum == sum_sfn(dp->dir)) break;  
                if (!(dp->fn[NSFLAG] & NS_LOSS) && !mem_cmp(dp->dir, dp->fn, 11)) break;     
                ord = 0xFF; dp->blk_ofs = 0xFFFFFFFF;    
            }
        }
#else        
        dp->obj.attr = dp->dir[DIR_Attr] & AM_MASK;
        if (!(dp->dir[DIR_Attr] & AM_VOL) && !mem_cmp(dp->dir, dp->fn, 11)) break;   
#endif
        res = dir_next(dp, 0);   
    } while (res == FR_OK);

    return res;
}




#if !FF_FS_READONLY
 
 
 

static FRESULT dir_register (    
    DIR* dp                      
)
{
    FRESULT res;
    FATFS *fs = dp->obj.fs;
#if FF_USE_LFN       
    UINT n, nlen, nent;
    BYTE sn[12], sum;


    if (dp->fn[NSFLAG] & (NS_DOT | NS_NONAME)) return FR_INVALID_NAME;   
    for (nlen = 0; fs->lfnbuf[nlen]; nlen++) ;   

#if FF_FS_EXFAT
    if (fs->fs_type == FS_EXFAT) {   
        nent = (nlen + 14) / 15 + 2;     
        res = dir_alloc(dp, nent);       
        if (res != FR_OK) return res;
        dp->blk_ofs = dp->dptr - SZDIRE * (nent - 1);    

        if (dp->obj.stat & 4) {          
            dp->obj.stat &= ~4;
            res = fill_first_frag(&dp->obj);     
            if (res != FR_OK) return res;
            res = fill_last_frag(&dp->obj, dp->clust, 0xFFFFFFFF);   
            if (res != FR_OK) return res;
            if (dp->obj.sclust != 0) {       
                DIR dj;

                res = load_obj_xdir(&dj, &dp->obj);  
                if (res != FR_OK) return res;
                dp->obj.objsize += (DWORD)fs->csize * SS(fs);            
                st_qword(fs->dirbuf + XDIR_FileSize, dp->obj.objsize);   
                st_qword(fs->dirbuf + XDIR_ValidFileSize, dp->obj.objsize);
                fs->dirbuf[XDIR_GenFlags] = dp->obj.stat | 1;
                res = store_xdir(&dj);               
                if (res != FR_OK) return res;
            }
        }

        create_xdir(fs->dirbuf, fs->lfnbuf);     
        return FR_OK;
    }
#endif
     
    mem_cpy(sn, dp->fn, 12);
    if (sn[NSFLAG] & NS_LOSS) {          
        dp->fn[NSFLAG] = NS_NOLFN;       
        for (n = 1; n < 100; n++) {
            gen_numname(dp->fn, sn, fs->lfnbuf, n);  
            res = dir_find(dp);              
            if (res != FR_OK) break;
        }
        if (n == 100) return FR_DENIED;      
        if (res != FR_NO_FILE) return res;   
        dp->fn[NSFLAG] = sn[NSFLAG];
    }

     
    nent = (sn[NSFLAG] & NS_LFN) ? (nlen + 12) / 13 + 1 : 1;     
    res = dir_alloc(dp, nent);       
    if (res == FR_OK && --nent) {    
        res = dir_sdi(dp, dp->dptr - nent * SZDIRE);
        if (res == FR_OK) {
            sum = sum_sfn(dp->fn);   
            do {                     
                res = move_window(fs, dp->sect);
                if (res != FR_OK) break;
                put_lfn(fs->lfnbuf, dp->dir, (BYTE)nent, sum);
                fs->wflag = 1;
                res = dir_next(dp, 0);   
            } while (res == FR_OK && --nent);
        }
    }

#else    
    res = dir_alloc(dp, 1);      

#endif

     
    if (res == FR_OK) {
        res = move_window(fs, dp->sect);
        if (res == FR_OK) {
            mem_set(dp->dir, 0, SZDIRE);     
            mem_cpy(dp->dir + DIR_Name, dp->fn, 11);     
#if FF_USE_LFN
            dp->dir[DIR_NTres] = dp->fn[NSFLAG] & (NS_BODY | NS_EXT);    
#endif
            fs->wflag = 1;
        }
    }

    return res;
}

#endif  



#if !FF_FS_READONLY && FF_FS_MINIMIZE == 0
 
 
 

static FRESULT dir_remove (  
    DIR* dp                  
)
{
    FRESULT res;
    FATFS *fs = dp->obj.fs;
#if FF_USE_LFN       
    DWORD last = dp->dptr;

    res = (dp->blk_ofs == 0xFFFFFFFF) ? FR_OK : dir_sdi(dp, dp->blk_ofs);    
    if (res == FR_OK) {
        do {
            res = move_window(fs, dp->sect);
            if (res != FR_OK) break;
            if (FF_FS_EXFAT && fs->fs_type == FS_EXFAT) {    
                dp->dir[XDIR_Type] &= 0x7F;  
            } else {                                     
                dp->dir[DIR_Name] = DDEM;    
            }
            fs->wflag = 1;
            if (dp->dptr >= last) break;     
            res = dir_next(dp, 0);   
        } while (res == FR_OK);
        if (res == FR_NO_FILE) res = FR_INT_ERR;
    }
#else            

    res = move_window(fs, dp->sect);
    if (res == FR_OK) {
        dp->dir[DIR_Name] = DDEM;    
        fs->wflag = 1;
    }
#endif

    return res;
}

#endif  



#if FF_FS_MINIMIZE <= 1 || FF_FS_RPATH >= 2
 
 
 

static void get_fileinfo (
    DIR* dp,             
    FILINFO* fno         
)
{
    UINT si, di;
#if FF_USE_LFN
    BYTE lcf;
    WCHAR wc, hs;
    FATFS *fs = dp->obj.fs;
#else
    TCHAR c;
#endif


    fno->fname[0] = 0;           
    if (dp->sect == 0) return;   

#if FF_USE_LFN       
#if FF_FS_EXFAT
    if (fs->fs_type == FS_EXFAT) {   
        get_xfileinfo(fs->dirbuf, fno);
        return;
    } else
#endif
    {    
        if (dp->blk_ofs != 0xFFFFFFFF) {     
            si = di = hs = 0;
            while (fs->lfnbuf[si] != 0) {
                wc = fs->lfnbuf[si++];       
                if (hs == 0 && IsSurrogate(wc)) {    
                    hs = wc; continue;       
                }
                wc = put_utf((DWORD)hs << 16 | wc, &fno->fname[di], FF_LFN_BUF - di);    
                if (wc == 0) { di = 0; break; }  
                di += wc;
                hs = 0;
            }
            if (hs != 0) di = 0;     
            fno->fname[di] = 0;      
        }
    }

    si = di = 0;
    while (si < 11) {        
        wc = dp->dir[si++];          
        if (wc == ' ') continue;     
        if (wc == RDDEM) wc = DDEM;  
        if (si == 9 && di < FF_SFN_BUF) fno->altname[di++] = '.';    
#if FF_LFN_UNICODE >= 1  
        if (dbc_1st((BYTE)wc) && si != 8 && si != 11 && dbc_2nd(dp->dir[si])) {  
            wc = wc << 8 | dp->dir[si++];
        }
        wc = ff_oem2uni(wc, CODEPAGE);       
        if (wc == 0) { di = 0; break; }      
        wc = put_utf(wc, &fno->altname[di], FF_SFN_BUF - di);    
        if (wc == 0) { di = 0; break; }      
        di += wc;
#else                    
        fno->altname[di++] = (TCHAR)wc;  
#endif
    }
    fno->altname[di] = 0;    

    if (fno->fname[0] == 0) {    
        if (di == 0) {   
            fno->fname[di++] = '?';
        } else {
            for (si = di = 0, lcf = NS_BODY; fno->altname[si]; si++, di++) {  
                wc = (WCHAR)fno->altname[si];
                if (wc == '.') lcf = NS_EXT;
                if (IsUpper(wc) && (dp->dir[DIR_NTres] & lcf)) wc += 0x20;
                fno->fname[di] = (TCHAR)wc;
            }
        }
        fno->fname[di] = 0;  
        if (!dp->dir[DIR_NTres]) fno->altname[0] = 0;    
    }

#else    
    si = di = 0;
    while (si < 11) {        
        c = (TCHAR)dp->dir[si++];
        if (c == ' ') continue;      
        if (c == RDDEM) c = DDEM;    
        if (si == 9) fno->fname[di++] = '.'; 
        fno->fname[di++] = c;
    }
    fno->fname[di] = 0;
#endif

    fno->fattrib = dp->dir[DIR_Attr];                    
    fno->fsize = ld_dword(dp->dir + DIR_FileSize);       
    fno->ftime = ld_word(dp->dir + DIR_ModTime + 0);     
    fno->fdate = ld_word(dp->dir + DIR_ModTime + 2);     
}

#endif  



#if FF_USE_FIND && FF_FS_MINIMIZE <= 1
 
 
 

static DWORD get_achar (     
    const TCHAR** ptr        
)
{
    DWORD chr;


#if FF_USE_LFN && FF_LFN_UNICODE >= 1    
    chr = tchar2uni(ptr);
    if (chr == 0xFFFFFFFF) chr = 0;      
    chr = ff_wtoupper(chr);

#else                                    
    chr = (BYTE)*(*ptr)++;               
    if (IsLower(chr)) chr -= 0x20;       
#if FF_CODE_PAGE == 0
    if (ExCvt && chr >= 0x80) chr = ExCvt[chr - 0x80];   
#elif FF_CODE_PAGE < 900
    if (chr >= 0x80) chr = ExCvt[chr - 0x80];    
#endif
#if FF_CODE_PAGE == 0 || FF_CODE_PAGE >= 900
    if (dbc_1st((BYTE)chr)) {    
        chr = dbc_2nd((BYTE)**ptr) ? chr << 8 | (BYTE)*(*ptr)++ : 0;
    }
#endif

#endif
    return chr;
}


static int pattern_matching (    
    const TCHAR* pat,    
    const TCHAR* nam,    
    int skip,            
    int inf              
)
{
    const TCHAR *pp, *np;
    DWORD pc, nc;
    int nm, nx;


    while (skip--) {                 
        if (!get_achar(&nam)) return 0;  
    }
    if (*pat == 0 && inf) return 1;  

    do {
        pp = pat; np = nam;          
        for (;;) {
            if (*pp == '?' || *pp == '*') {  
                nm = nx = 0;
                do {                 
                    if (*pp++ == '?') nm++; else nx = 1;
                } while (*pp == '?' || *pp == '*');
                if (pattern_matching(pp, np, nm, nx)) return 1;  
                nc = *np; break;     
            }
            pc = get_achar(&pp);     
            nc = get_achar(&np);     
            if (pc != nc) break;     
            if (pc == 0) return 1;   
        }
        get_achar(&nam);             
    } while (inf && nc);             

    return 0;
}

#endif  



 
 
 

static FRESULT create_name (     
    DIR* dp,                     
    const TCHAR** path           
)
{
#if FF_USE_LFN       
    BYTE b, cf;
    WCHAR wc, *lfn;
    DWORD uc;
    UINT i, ni, si, di;
    const TCHAR *p;


     
    p = *path; lfn = dp->obj.fs->lfnbuf; di = 0;
    for (;;) {
        uc = tchar2uni(&p);          
        if (uc == 0xFFFFFFFF) return FR_INVALID_NAME;        
        if (uc >= 0x10000) lfn[di++] = (WCHAR)(uc >> 16);    
        wc = (WCHAR)uc;
        if (wc < ' ' || wc == '/' || wc == '\\') break;  
        if (wc < 0x80 && chk_chr("\"*:<>\?|\x7F", wc)) return FR_INVALID_NAME;   
        if (di >= FF_MAX_LFN) return FR_INVALID_NAME;    
        lfn[di++] = wc;                  
    }
    if (wc == '/' || wc == '\\') while (*p == '/' || *p == '\\') p++;     
    *path = p;                           
    cf = (wc < ' ') ? NS_LAST : 0;       

#if FF_FS_RPATH != 0
    if ((di == 1 && lfn[di - 1] == '.') ||
        (di == 2 && lfn[di - 1] == '.' && lfn[di - 2] == '.')) {     
        lfn[di] = 0;
        for (i = 0; i < 11; i++) {       
            dp->fn[i] = (i < di) ? '.' : ' ';
        }
        dp->fn[i] = cf | NS_DOT;         
        return FR_OK;
    }
#endif
    while (di) {                         
        wc = lfn[di - 1];
        if (wc != ' ' && wc != '.') break;
        di--;
    }
    lfn[di] = 0;                             
    if (di == 0) return FR_INVALID_NAME;     

     
    for (si = 0; lfn[si] == ' '; si++) ;     
    if (si > 0 || lfn[si] == '.') cf |= NS_LOSS | NS_LFN;    
    while (di > 0 && lfn[di - 1] != '.') di--;   

    mem_set(dp->fn, ' ', 11);
    i = b = 0; ni = 8;
    for (;;) {
        wc = lfn[si++];                  
        if (wc == 0) break;              
        if (wc == ' ' || (wc == '.' && si != di)) {  
            cf |= NS_LOSS | NS_LFN;
            continue;
        }

        if (i >= ni || si == di) {       
            if (ni == 11) {              
                cf |= NS_LOSS | NS_LFN;
                break;
            }
            if (si != di) cf |= NS_LOSS | NS_LFN;    
            if (si > di) break;                      
            si = di; i = 8; ni = 11; b <<= 2;        
            continue;
        }

        if (wc >= 0x80) {    
            cf |= NS_LFN;    
#if FF_CODE_PAGE == 0
            if (ExCvt) {     
                wc = ff_uni2oem(wc, CODEPAGE);           
                if (wc & 0x80) wc = ExCvt[wc & 0x7F];    
            } else {         
                wc = ff_uni2oem(ff_wtoupper(wc), CODEPAGE);  
            }
#elif FF_CODE_PAGE < 900     
            wc = ff_uni2oem(wc, CODEPAGE);           
            if (wc & 0x80) wc = ExCvt[wc & 0x7F];    
#else                        
            wc = ff_uni2oem(ff_wtoupper(wc), CODEPAGE);  
#endif
        }

        if (wc >= 0x100) {               
            if (i >= ni - 1) {           
                cf |= NS_LOSS | NS_LFN;
                i = ni; continue;        
            }
            dp->fn[i++] = (BYTE)(wc >> 8);   
        } else {                         
            if (wc == 0 || chk_chr("+,;=[]", wc)) {  
                wc = '_'; cf |= NS_LOSS | NS_LFN; 
            } else {
                if (IsUpper(wc)) {       
                    b |= 2;
                }
                if (IsLower(wc)) {       
                    b |= 1; wc -= 0x20;
                }
            }
        }
        dp->fn[i++] = (BYTE)wc;
    }

    if (dp->fn[0] == DDEM) dp->fn[0] = RDDEM;    

    if (ni == 8) b <<= 2;                
    if ((b & 0x0C) == 0x0C || (b & 0x03) == 0x03) cf |= NS_LFN;  
    if (!(cf & NS_LFN)) {                
        if (b & 0x01) cf |= NS_EXT;      
        if (b & 0x04) cf |= NS_BODY;     
    }

    dp->fn[NSFLAG] = cf;     

    return FR_OK;


#else    
    BYTE c, d, *sfn;
    UINT ni, si, i;
    const char *p;

     
    p = *path; sfn = dp->fn;
    mem_set(sfn, ' ', 11);
    si = i = 0; ni = 8;
#if FF_FS_RPATH != 0
    if (p[si] == '.') {  
        for (;;) {
            c = (BYTE)p[si++];
            if (c != '.' || si >= 3) break;
            sfn[i++] = c;
        }
        if (c != '/' && c != '\\' && c > ' ') return FR_INVALID_NAME;
        *path = p + si;                              
        sfn[NSFLAG] = (c <= ' ') ? NS_LAST | NS_DOT : NS_DOT;    
        return FR_OK;
    }
#endif
    for (;;) {
        c = (BYTE)p[si++];               
        if (c <= ' ') break;             
        if (c == '/' || c == '\\') {     
            while (p[si] == '/' || p[si] == '\\') si++;  
            break;
        }
        if (c == '.' || i >= ni) {       
            if (ni == 11 || c != '.') return FR_INVALID_NAME;    
            i = 8; ni = 11;              
            continue;
        }
#if FF_CODE_PAGE == 0
        if (ExCvt && c >= 0x80) {        
            c = ExCvt[c & 0x7F];         
        }
#elif FF_CODE_PAGE < 900
        if (c >= 0x80) {                 
            c = ExCvt[c & 0x7F];         
        }
#endif
        if (dbc_1st(c)) {                
            d = (BYTE)p[si++];           
            if (!dbc_2nd(d) || i >= ni - 1) return FR_INVALID_NAME;  
            sfn[i++] = c;
            sfn[i++] = d;
        } else {                         
            if (chk_chr("\"*+,:;<=>\?[]|\x7F", c)) return FR_INVALID_NAME;   
            if (IsLower(c)) c -= 0x20;   
            sfn[i++] = c;
        }
    }
    *path = p + si;                      
    if (i == 0) return FR_INVALID_NAME;  

    if (sfn[0] == DDEM) sfn[0] = RDDEM;  
    sfn[NSFLAG] = (c <= ' ') ? NS_LAST : 0;      

    return FR_OK;
#endif  
}




 
 
 

static FRESULT follow_path (     
    DIR* dp,                     
    const TCHAR* path            
)
{
    FRESULT res;
    BYTE ns;
    FATFS *fs = dp->obj.fs;


#if FF_FS_RPATH != 0
    if (*path != '/' && *path != '\\') {     
        dp->obj.sclust = fs->cdir;               
    } else
#endif
    {                                        
        while (*path == '/' || *path == '\\') path++;    
        dp->obj.sclust = 0;                  
    }
#if FF_FS_EXFAT
    dp->obj.n_frag = 0;  
#if FF_FS_RPATH != 0
    if (fs->fs_type == FS_EXFAT && dp->obj.sclust) {     
        DIR dj;

        dp->obj.c_scl = fs->cdc_scl;
        dp->obj.c_size = fs->cdc_size;
        dp->obj.c_ofs = fs->cdc_ofs;
        res = load_obj_xdir(&dj, &dp->obj);
        if (res != FR_OK) return res;
        dp->obj.objsize = ld_dword(fs->dirbuf + XDIR_FileSize);
        dp->obj.stat = fs->dirbuf[XDIR_GenFlags] & 2;
    }
#endif
#endif

    if ((UINT)*path < ' ') {                 
        dp->fn[NSFLAG] = NS_NONAME;
        res = dir_sdi(dp, 0);

    } else {                                 
        for (;;) {
            res = create_name(dp, &path);    
            if (res != FR_OK) break;
            res = dir_find(dp);              
            ns = dp->fn[NSFLAG];
            if (res != FR_OK) {              
                if (res == FR_NO_FILE) {     
                    if (FF_FS_RPATH && (ns & NS_DOT)) {  
                        if (!(ns & NS_LAST)) continue;   
                        dp->fn[NSFLAG] = NS_NONAME;
                        res = FR_OK;
                    } else {                             
                        if (!(ns & NS_LAST)) res = FR_NO_PATH;   
                    }
                }
                break;
            }
            if (ns & NS_LAST) break;             
             
            if (!(dp->obj.attr & AM_DIR)) {      
                res = FR_NO_PATH; break;
            }
#if FF_FS_EXFAT
            if (fs->fs_type == FS_EXFAT) {       
                dp->obj.c_scl = dp->obj.sclust;
                dp->obj.c_size = ((DWORD)dp->obj.objsize & 0xFFFFFF00) | dp->obj.stat;
                dp->obj.c_ofs = dp->blk_ofs;
                init_alloc_info(fs, &dp->obj);   
            } else
#endif
            {
                dp->obj.sclust = ld_clust(fs, fs->win + dp->dptr % SS(fs));  
            }
        }
    }

    return res;
}




 
 
 

static BYTE check_fs (   
    FATFS* fs,           
    DWORD sect           
)
{
    fs->wflag = 0; fs->winsect = 0xFFFFFFFF;         
    if (move_window(fs, sect) != FR_OK) return 4;    

    if (ld_word(fs->win + BS_55AA) != 0xAA55) return 3;  

#if FF_FS_EXFAT
    if (!mem_cmp(fs->win + BS_JmpBoot, "\xEB\x76\x90" "EXFAT   ", 11)) return 1;     
#endif
    if (fs->win[BS_JmpBoot] == 0xE9 || fs->win[BS_JmpBoot] == 0xEB || fs->win[BS_JmpBoot] == 0xE8) {     
        if (!mem_cmp(fs->win + BS_FilSysType, "FAT", 3)) return 0;       
        if (!mem_cmp(fs->win + BS_FilSysType32, "FAT32", 5)) return 0;   
    }
    return 2;    
}




 
 
 

static FRESULT find_volume (     
    FATFS *fs,                   
    BYTE mode                    
)
{
    BYTE fmt, *pt;
    DSTATUS stat;
    DWORD bsect, fasize, tsect, sysect, nclst, szbfat, br[4];
    WORD nrsv;
    UINT i;


#if FF_FS_REENTRANT
    if (!lock_fs(fs)) return FR_TIMEOUT;     
#endif

    mode &= (BYTE)~FA_READ;              
    if (fs->fs_type != 0) {              
        disk_ioctl(fs->drv, IOCTL_STATUS, &stat);
        if (!(stat & STA_NOINIT)) {      
            if (!FF_FS_READONLY && mode && (stat & STA_PROTECT)) {   
                return FR_WRITE_PROTECTED;
            }
            return FR_OK;                
        }
    }

     
     

    fs->fs_type = 0;                     
    disk_ioctl(fs->drv, IOCTL_INIT, &stat);  
    if (stat & STA_NOINIT) {             
        return FR_NOT_READY;             
    }
    if (!FF_FS_READONLY && mode && (stat & STA_PROTECT)) {  
        return FR_WRITE_PROTECTED;
    }
#if FF_MAX_SS != FF_MIN_SS               
    if (disk_ioctl(fs->drv, GET_SECTOR_SIZE, &SS(fs)) != RES_OK) return FR_DISK_ERR;
    if (SS(fs) > FF_MAX_SS || SS(fs) < FF_MIN_SS || (SS(fs) & (SS(fs) - 1))) return FR_DISK_ERR;
#endif

     
    bsect = 0;
    fmt = check_fs(fs, bsect);           
    if (fmt == 2 || (fmt < 2 && LD2PT(fs) != 0)) {  
        for (i = 0; i < 4; i++) {        
            pt = fs->win + (MBR_Table + i * SZ_PTE);
            br[i] = pt[PTE_System] ? ld_dword(pt + PTE_StLba) : 0;
        }
        i = LD2PT(fs);                   
        if (i != 0) i--;
        do {                             
            bsect = br[i];
            fmt = bsect ? check_fs(fs, bsect) : 3;   
        } while (LD2PT(fs) == 0 && fmt >= 2 && ++i < 4);
    }
    if (fmt == 4) return FR_DISK_ERR;        
    if (fmt >= 2) return FR_NO_FILESYSTEM;   

     

#if FF_FS_EXFAT
    if (fmt == 1) {
        QWORD maxlba;
        DWORD so, cv, bcl;

        for (i = BPB_ZeroedEx; i < BPB_ZeroedEx + 53 && fs->win[i] == 0; i++) ;  
        if (i < BPB_ZeroedEx + 53) return FR_NO_FILESYSTEM;

        if (ld_word(fs->win + BPB_FSVerEx) != 0x100) return FR_NO_FILESYSTEM;    

        if (1 << fs->win[BPB_BytsPerSecEx] != SS(fs)) {  
            return FR_NO_FILESYSTEM;
        }

        maxlba = ld_qword(fs->win + BPB_TotSecEx) + bsect;   
        if (maxlba >= 0x100000000) return FR_NO_FILESYSTEM;  

        fs->fsize = ld_dword(fs->win + BPB_FatSzEx);     

        fs->n_fats = fs->win[BPB_NumFATsEx];             
        if (fs->n_fats != 1) return FR_NO_FILESYSTEM;    

        fs->csize = 1 << fs->win[BPB_SecPerClusEx];      
        if (fs->csize == 0) return FR_NO_FILESYSTEM;     

        nclst = ld_dword(fs->win + BPB_NumClusEx);       
        if (nclst > MAX_EXFAT) return FR_NO_FILESYSTEM;  
        fs->n_fatent = nclst + 2;

         
        fs->volbase = bsect;
        fs->database = bsect + ld_dword(fs->win + BPB_DataOfsEx);
        fs->fatbase = bsect + ld_dword(fs->win + BPB_FatOfsEx);
        if (maxlba < (QWORD)fs->database + nclst * fs->csize) return FR_NO_FILESYSTEM;   
        fs->dirbase = ld_dword(fs->win + BPB_RootClusEx);

         
        so = i = 0;
        for (;;) {   
            if (i == 0) {
                if (so >= fs->csize) return FR_NO_FILESYSTEM;    
                if (move_window(fs, clst2sect(fs, fs->dirbase) + so) != FR_OK) return FR_DISK_ERR;
                so++;
            }
            if (fs->win[i] == ET_BITMAP) break;              
            i = (i + SZDIRE) % SS(fs);   
        }
        bcl = ld_dword(fs->win + i + 20);                    
        if (bcl < 2 || bcl >= fs->n_fatent) return FR_NO_FILESYSTEM;
        fs->bitbase = fs->database + fs->csize * (bcl - 2);  
        for (;;) {   
            if (move_window(fs, fs->fatbase + bcl / (SS(fs) / 4)) != FR_OK) return FR_DISK_ERR;
            cv = ld_dword(fs->win + bcl % (SS(fs) / 4) * 4);
            if (cv == 0xFFFFFFFF) break;                 
            if (cv != ++bcl) return FR_NO_FILESYSTEM;    
        }

#if !FF_FS_READONLY
        fs->last_clst = fs->free_clst = 0xFFFFFFFF;      
#endif
        fmt = FS_EXFAT;          
    } else
#endif   
    {
        if (ld_word(fs->win + BPB_BytsPerSec) != SS(fs)) return FR_NO_FILESYSTEM;    

        fasize = ld_word(fs->win + BPB_FATSz16);         
        if (fasize == 0) fasize = ld_dword(fs->win + BPB_FATSz32);
        fs->fsize = fasize;

        fs->n_fats = fs->win[BPB_NumFATs];               
        if (fs->n_fats != 1 && fs->n_fats != 2) return FR_NO_FILESYSTEM;     
        fasize *= fs->n_fats;                            

        fs->csize = fs->win[BPB_SecPerClus];             
        if (fs->csize == 0 || (fs->csize & (fs->csize - 1))) return FR_NO_FILESYSTEM;    

        fs->n_rootdir = ld_word(fs->win + BPB_RootEntCnt);   
        if (fs->n_rootdir % (SS(fs) / SZDIRE)) return FR_NO_FILESYSTEM;  

        tsect = ld_word(fs->win + BPB_TotSec16);         
        if (tsect == 0) tsect = ld_dword(fs->win + BPB_TotSec32);

        nrsv = ld_word(fs->win + BPB_RsvdSecCnt);        
        if (nrsv == 0) return FR_NO_FILESYSTEM;          

         
        sysect = nrsv + fasize + fs->n_rootdir / (SS(fs) / SZDIRE);  
        if (tsect < sysect) return FR_NO_FILESYSTEM;     
        nclst = (tsect - sysect) / fs->csize;            
        if (nclst == 0) return FR_NO_FILESYSTEM;         
        fmt = 0;
        if (nclst <= MAX_FAT32) fmt = FS_FAT32;
        if (nclst <= MAX_FAT16) fmt = FS_FAT16;
        if (nclst <= MAX_FAT12) fmt = FS_FAT12;
        if (fmt == 0) return FR_NO_FILESYSTEM;

         
        fs->n_fatent = nclst + 2;                        
        fs->volbase = bsect;                             
        fs->fatbase = bsect + nrsv;                      
        fs->database = bsect + sysect;                   
        if (fmt == FS_FAT32) {
            if (ld_word(fs->win + BPB_FSVer32) != 0) return FR_NO_FILESYSTEM;    
            if (fs->n_rootdir != 0) return FR_NO_FILESYSTEM;     
            fs->dirbase = ld_dword(fs->win + BPB_RootClus32);    
            szbfat = fs->n_fatent * 4;                   
        } else {
            if (fs->n_rootdir == 0) return FR_NO_FILESYSTEM;     
            fs->dirbase = fs->fatbase + fasize;          
            szbfat = (fmt == FS_FAT16) ?                 
                fs->n_fatent * 2 : fs->n_fatent * 3 / 2 + (fs->n_fatent & 1);
        }
        if (fs->fsize < (szbfat + (SS(fs) - 1)) / SS(fs)) return FR_NO_FILESYSTEM;   

#if !FF_FS_READONLY
         
        fs->last_clst = fs->free_clst = 0xFFFFFFFF;      
        fs->fsi_flag = 0x80;
#if (FF_FS_NOFSINFO & 3) != 3
        if (fmt == FS_FAT32              
            && ld_word(fs->win + BPB_FSInfo32) == 1
            && move_window(fs, bsect + 1) == FR_OK)
        {
            fs->fsi_flag = 0;
            if (ld_word(fs->win + BS_55AA) == 0xAA55     
                && ld_dword(fs->win + FSI_LeadSig) == 0x41615252
                && ld_dword(fs->win + FSI_StrucSig) == 0x61417272)
            {
#if (FF_FS_NOFSINFO & 1) == 0
                fs->free_clst = ld_dword(fs->win + FSI_Free_Count);
#endif
#if (FF_FS_NOFSINFO & 2) == 0
                fs->last_clst = ld_dword(fs->win + FSI_Nxt_Free);
#endif
            }
        }
#endif   
#endif   
    }

    fs->fs_type = fmt;       
    fs->id = ++Fsid;         
#if FF_USE_LFN == 1
    fs->lfnbuf = LfnBuf;     
#if FF_FS_EXFAT
    fs->dirbuf = DirBuf;     
#endif
#endif
#if FF_FS_RPATH != 0
    fs->cdir = 0;            
#endif
#if FF_FS_LOCK != 0          
    clear_lock(fs);
#endif
    return FR_OK;
}




 
 
 

static FRESULT validate (    
    FFOBJID* obj,            
    FATFS** rfs              
)
{
    FRESULT res = FR_INVALID_OBJECT;
    DSTATUS stat;


    if (obj && obj->fs && obj->fs->fs_type && obj->id == obj->fs->id) {  
#if FF_FS_REENTRANT
        if (lock_fs(obj->fs)) {  
            if (disk_ioctl(obj->fs->drv, IOCTL_STATUS, &stat) == RES_OK && !(stat & STA_NOINIT)) {  
                res = FR_OK;
            } else {
                unlock_fs(obj->fs, FR_OK);
            }
        } else {
            res = FR_TIMEOUT;
        }
#else
        if (disk_ioctl(obj->fs->drv, IOCTL_STATUS, &stat) == RES_OK && !(stat & STA_NOINIT)) {  
            res = FR_OK;
        }
#endif
    }
    *rfs = (res == FR_OK) ? obj->fs : 0;     
    return res;
}




 



 
 
 

FRESULT f_mount (
    FATFS* fs            
)
{
    FRESULT res;

    fs->fs_type = 0;                     
#if FF_FS_REENTRANT                      
    if (!ff_cre_syncobj(fs, &fs->sobj)) return FR_INT_ERR;
#endif

    res = find_volume(fs, 0);            
    LEAVE_FF(fs, res);
}


FRESULT f_umount (
    FATFS* fs                    
)
{
#if FF_FS_LOCK
    clear_lock(fs);
#endif
#if FF_FS_REENTRANT              
    if (!ff_del_syncobj(fs->sobj)) return FR_INT_ERR;
#endif
    fs->fs_type = 0;             

    return FR_OK;
}


 
 
 

FRESULT f_open (
    FATFS *fs,
    FIL* fp,             
    const TCHAR* path,   
    BYTE mode            
)
{
    FRESULT res;
    DIR dj;
#if !FF_FS_READONLY
    DWORD dw, cl, bcs, clst, sc;
    FSIZE_t ofs;
#endif
    DEF_NAMBUF


    if (!fp) return FR_INVALID_OBJECT;

     
    mode &= FF_FS_READONLY ? FA_READ : FA_READ | FA_WRITE | FA_CREATE_ALWAYS | FA_CREATE_NEW | FA_OPEN_ALWAYS | FA_OPEN_APPEND;
    res = find_volume(fs, mode);
    if (res == FR_OK) {
        dj.obj.fs = fs;
        INIT_NAMBUF(fs);
        res = follow_path(&dj, path);    
#if !FF_FS_READONLY  
        if (res == FR_OK) {
            if (dj.fn[NSFLAG] & NS_NONAME) {     
                res = FR_INVALID_NAME;
            }
#if FF_FS_LOCK != 0
            else {
                res = chk_lock(&dj, (mode & ~FA_READ) ? 1 : 0);      
            }
#endif
        }
         
        if (mode & (FA_CREATE_ALWAYS | FA_OPEN_ALWAYS | FA_CREATE_NEW)) {
            if (res != FR_OK) {                  
                if (res == FR_NO_FILE) {         
#if FF_FS_LOCK != 0
                    res = enq_lock() ? dir_register(&dj) : FR_TOO_MANY_OPEN_FILES;
#else
                    res = dir_register(&dj);
#endif
                }
                mode |= FA_CREATE_ALWAYS;        
            }
            else {                               
                if (dj.obj.attr & (AM_RDO | AM_DIR)) {   
                    res = FR_DENIED;
                } else {
                    if (mode & FA_CREATE_NEW) res = FR_EXIST;    
                }
            }
            if (res == FR_OK && (mode & FA_CREATE_ALWAYS)) {     
#if FF_FS_EXFAT
                if (fs->fs_type == FS_EXFAT) {
                     
                    fp->obj.fs = fs;
                    init_alloc_info(fs, &fp->obj);
                     
                    mem_set(fs->dirbuf + 2, 0, 30);      
                    mem_set(fs->dirbuf + 38, 0, 26);     
                    fs->dirbuf[XDIR_Attr] = AM_ARC;
                    st_dword(fs->dirbuf + XDIR_CrtTime, GET_FATTIME());
                    fs->dirbuf[XDIR_GenFlags] = 1;
                    res = store_xdir(&dj);
                    if (res == FR_OK && fp->obj.sclust != 0) {   
                        res = remove_chain(&fp->obj, fp->obj.sclust, 0);
                        fs->last_clst = fp->obj.sclust - 1;      
                    }
                } else
#endif
                {
                     
                    cl = ld_clust(fs, dj.dir);           
                    st_dword(dj.dir + DIR_CrtTime, GET_FATTIME());   
                    dj.dir[DIR_Attr] = AM_ARC;           
                    st_clust(fs, dj.dir, 0);             
                    st_dword(dj.dir + DIR_FileSize, 0);
                    fs->wflag = 1;
                    if (cl != 0) {                       
                        dw = fs->winsect;
                        res = remove_chain(&dj.obj, cl, 0);
                        if (res == FR_OK) {
                            res = move_window(fs, dw);
                            fs->last_clst = cl - 1;      
                        }
                    }
                }
            }
        }
        else {   
            if (res == FR_OK) {                  
                if (dj.obj.attr & AM_DIR) {      
                    res = FR_NO_FILE;
                } else {
                    if ((mode & FA_WRITE) && (dj.obj.attr & AM_RDO)) {  
                        res = FR_DENIED;
                    }
                }
            }
        }
        if (res == FR_OK) {
            if (mode & FA_CREATE_ALWAYS) mode |= FA_MODIFIED;    
            fp->dir_sect = fs->winsect;          
            fp->dir_ptr = dj.dir;
#if FF_FS_LOCK != 0
            fp->obj.lockid = inc_lock(&dj, (mode & ~FA_READ) ? 1 : 0);   
            if (fp->obj.lockid == 0) res = FR_INT_ERR;
#endif
        }
#else        
        if (res == FR_OK) {
            if (dj.fn[NSFLAG] & NS_NONAME) {     
                res = FR_INVALID_NAME;
            } else {
                if (dj.obj.attr & AM_DIR) {      
                    res = FR_NO_FILE;
                }
            }
        }
#endif

        if (res == FR_OK) {
#if FF_FS_EXFAT
            if (fs->fs_type == FS_EXFAT) {
                fp->obj.c_scl = dj.obj.sclust;                           
                fp->obj.c_size = ((DWORD)dj.obj.objsize & 0xFFFFFF00) | dj.obj.stat;
                fp->obj.c_ofs = dj.blk_ofs;
                init_alloc_info(fs, &fp->obj);
            } else
#endif
            {
                fp->obj.sclust = ld_clust(fs, dj.dir);                   
                fp->obj.objsize = ld_dword(dj.dir + DIR_FileSize);
            }
#if FF_USE_FASTSEEK
            fp->cltbl = 0;           
#endif
            fp->obj.fs = fs;         
            fp->obj.id = fs->id;
            fp->flag = mode;         
            fp->err = 0;             
            fp->sect = 0;            
            fp->fptr = 0;            
#if !FF_FS_READONLY
#if !FF_FS_TINY
            mem_set(fp->buf, 0, sizeof fp->buf);     
#endif
            if ((mode & FA_SEEKEND) && fp->obj.objsize > 0) {    
                fp->fptr = fp->obj.objsize;          
                bcs = (DWORD)fs->csize * SS(fs);     
                clst = fp->obj.sclust;               
                for (ofs = fp->obj.objsize; res == FR_OK && ofs > bcs; ofs -= bcs) {
                    clst = get_fat(&fp->obj, clst);
                    if (clst <= 1) res = FR_INT_ERR;
                    if (clst == 0xFFFFFFFF) res = FR_DISK_ERR;
                }
                fp->clust = clst;
                if (res == FR_OK && ofs % SS(fs)) {  
                    if ((sc = clst2sect(fs, clst)) == 0) {
                        res = FR_INT_ERR;
                    } else {
                        fp->sect = sc + (DWORD)(ofs / SS(fs));
#if !FF_FS_TINY
                        if (disk_read(fs->drv, fp->buf, fp->sect, 1) != RES_OK) res = FR_DISK_ERR;
#endif
                    }
                }
            }
#endif
        }

        FREE_NAMBUF();
    }

    if (res != FR_OK) fp->obj.fs = 0;    

    LEAVE_FF(fs, res);
}




 
 
 

FRESULT f_read (
    FIL* fp,     
    void* buff,  
    UINT btr,    
    UINT* br     
)
{
    FRESULT res;
    FATFS *fs;
    DWORD clst, sect;
    FSIZE_t remain;
    UINT rcnt, cc, csect;
    BYTE *rbuff = (BYTE*)buff;


    *br = 0;     
    res = validate(&fp->obj, &fs);               
    if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);    
    if (!(fp->flag & FA_READ)) LEAVE_FF(fs, FR_DENIED);  
    remain = fp->obj.objsize - fp->fptr;
    if (btr > remain) btr = (UINT)remain;        

    for ( ;  btr;                                
        btr -= rcnt, *br += rcnt, rbuff += rcnt, fp->fptr += rcnt) {
        if (fp->fptr % SS(fs) == 0) {            
            csect = (UINT)(fp->fptr / SS(fs) & (fs->csize - 1));     
            if (csect == 0) {                    
                if (fp->fptr == 0) {             
                    clst = fp->obj.sclust;       
                } else {                         
#if FF_USE_FASTSEEK
                    if (fp->cltbl) {
                        clst = clmt_clust(fp, fp->fptr);     
                    } else
#endif
                    {
                        clst = get_fat(&fp->obj, fp->clust);     
                    }
                }
                if (clst < 2) ABORT(fs, FR_INT_ERR);
                if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
                fp->clust = clst;                
            }
            sect = clst2sect(fs, fp->clust);     
            if (sect == 0) ABORT(fs, FR_INT_ERR);
            sect += csect;
            cc = btr / SS(fs);                   
            if (cc > 0) {                        
                if (csect + cc > fs->csize) {    
                    cc = fs->csize - csect;
                }
                if (disk_read(fs->drv, rbuff, sect, cc) != RES_OK) ABORT(fs, FR_DISK_ERR);
#if !FF_FS_READONLY && FF_FS_MINIMIZE <= 2       
#if FF_FS_TINY
                if (fs->wflag && fs->winsect - sect < cc) {
                    mem_cpy(rbuff + ((fs->winsect - sect) * SS(fs)), fs->win, SS(fs));
                }
#else
                if ((fp->flag & FA_DIRTY) && fp->sect - sect < cc) {
                    mem_cpy(rbuff + ((fp->sect - sect) * SS(fs)), fp->buf, SS(fs));
                }
#endif
#endif
                rcnt = SS(fs) * cc;              
                continue;
            }
#if !FF_FS_TINY
            if (fp->sect != sect) {          
#if !FF_FS_READONLY
                if (fp->flag & FA_DIRTY) {       
                    if (disk_write(fs->drv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
                    fp->flag &= (BYTE)~FA_DIRTY;
                }
#endif
                if (disk_read(fs->drv, fp->buf, sect, 1) != RES_OK)    ABORT(fs, FR_DISK_ERR);  
            }
#endif
            fp->sect = sect;
        }
        rcnt = SS(fs) - (UINT)fp->fptr % SS(fs);     
        if (rcnt > btr) rcnt = btr;                  
#if FF_FS_TINY
        if (move_window(fs, fp->sect) != FR_OK) ABORT(fs, FR_DISK_ERR);  
        mem_cpy(rbuff, fs->win + fp->fptr % SS(fs), rcnt);   
#else
        mem_cpy(rbuff, fp->buf + fp->fptr % SS(fs), rcnt);   
#endif
    }

    LEAVE_FF(fs, FR_OK);
}




#if !FF_FS_READONLY
 
 
 

FRESULT f_write (
    FIL* fp,             
    const void* buff,    
    UINT btw,            
    UINT* bw             
)
{
    FRESULT res;
    FATFS *fs;
    DWORD clst, sect;
    UINT wcnt, cc, csect;
    const BYTE *wbuff = (const BYTE*)buff;


    *bw = 0;     
    res = validate(&fp->obj, &fs);           
    if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);    
    if (!(fp->flag & FA_WRITE)) LEAVE_FF(fs, FR_DENIED);     

     
    if ((!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) && (DWORD)(fp->fptr + btw) < (DWORD)fp->fptr) {
        btw = (UINT)(0xFFFFFFFF - (DWORD)fp->fptr);
    }

    for ( ;  btw;                            
        btw -= wcnt, *bw += wcnt, wbuff += wcnt, fp->fptr += wcnt, fp->obj.objsize = (fp->fptr > fp->obj.objsize) ? fp->fptr : fp->obj.objsize) {
        if (fp->fptr % SS(fs) == 0) {        
            csect = (UINT)(fp->fptr / SS(fs)) & (fs->csize - 1);     
            if (csect == 0) {                
                if (fp->fptr == 0) {         
                    clst = fp->obj.sclust;   
                    if (clst == 0) {         
                        clst = create_chain(&fp->obj, 0);    
                    }
                } else {                     
#if FF_USE_FASTSEEK
                    if (fp->cltbl) {
                        clst = clmt_clust(fp, fp->fptr);     
                    } else
#endif
                    {
                        clst = create_chain(&fp->obj, fp->clust);    
                    }
                }
                if (clst == 0) break;        
                if (clst == 1) ABORT(fs, FR_INT_ERR);
                if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
                fp->clust = clst;            
                if (fp->obj.sclust == 0) fp->obj.sclust = clst;  
            }
#if FF_FS_TINY
            if (fs->winsect == fp->sect && sync_window(fs) != FR_OK) ABORT(fs, FR_DISK_ERR);     
#else
            if (fp->flag & FA_DIRTY) {       
                if (disk_write(fs->drv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
                fp->flag &= (BYTE)~FA_DIRTY;
            }
#endif
            sect = clst2sect(fs, fp->clust);     
            if (sect == 0) ABORT(fs, FR_INT_ERR);
            sect += csect;
            cc = btw / SS(fs);               
            if (cc > 0) {                    
                if (csect + cc > fs->csize) {    
                    cc = fs->csize - csect;
                }
                if (disk_write(fs->drv, wbuff, sect, cc) != RES_OK) ABORT(fs, FR_DISK_ERR);
#if FF_FS_MINIMIZE <= 2
#if FF_FS_TINY
                if (fs->winsect - sect < cc) {   
                    mem_cpy(fs->win, wbuff + ((fs->winsect - sect) * SS(fs)), SS(fs));
                    fs->wflag = 0;
                }
#else
                if (fp->sect - sect < cc) {  
                    mem_cpy(fp->buf, wbuff + ((fp->sect - sect) * SS(fs)), SS(fs));
                    fp->flag &= (BYTE)~FA_DIRTY;
                }
#endif
#endif
                wcnt = SS(fs) * cc;      
                continue;
            }
#if FF_FS_TINY
            if (fp->fptr >= fp->obj.objsize) {   
                if (sync_window(fs) != FR_OK) ABORT(fs, FR_DISK_ERR);
                fs->winsect = sect;
            }
#else
            if (fp->sect != sect &&          
                fp->fptr < fp->obj.objsize &&
                disk_read(fs->drv, fp->buf, sect, 1) != RES_OK) {
                    ABORT(fs, FR_DISK_ERR);
            }
#endif
            fp->sect = sect;
        }
        wcnt = SS(fs) - (UINT)fp->fptr % SS(fs);     
        if (wcnt > btw) wcnt = btw;                  
#if FF_FS_TINY
        if (move_window(fs, fp->sect) != FR_OK) ABORT(fs, FR_DISK_ERR);  
        mem_cpy(fs->win + fp->fptr % SS(fs), wbuff, wcnt);   
        fs->wflag = 1;
#else
        mem_cpy(fp->buf + fp->fptr % SS(fs), wbuff, wcnt);   
        fp->flag |= FA_DIRTY;
#endif
    }

    fp->flag |= FA_MODIFIED;                 

    LEAVE_FF(fs, FR_OK);
}




 
 
 

FRESULT f_sync (
    FIL* fp      
)
{
    FRESULT res;
    FATFS *fs;
    DWORD tm;
    BYTE *dir;


    res = validate(&fp->obj, &fs);   
    if (res == FR_OK) {
        if (fp->flag & FA_MODIFIED) {    
#if !FF_FS_TINY
            if (fp->flag & FA_DIRTY) {   
                if (disk_write(fs->drv, fp->buf, fp->sect, 1) != RES_OK) LEAVE_FF(fs, FR_DISK_ERR);
                fp->flag &= (BYTE)~FA_DIRTY;
            }
#endif
             
            tm = GET_FATTIME();              
#if FF_FS_EXFAT
            if (fs->fs_type == FS_EXFAT) {
                res = fill_first_frag(&fp->obj);     
                if (res == FR_OK) {
                    res = fill_last_frag(&fp->obj, fp->clust, 0xFFFFFFFF);   
                }
                if (res == FR_OK) {
                    DIR dj;
                    DEF_NAMBUF

                    INIT_NAMBUF(fs);
                    res = load_obj_xdir(&dj, &fp->obj);  
                    if (res == FR_OK) {
                        fs->dirbuf[XDIR_Attr] |= AM_ARC;                 
                        fs->dirbuf[XDIR_GenFlags] = fp->obj.stat | 1;    
                        st_dword(fs->dirbuf + XDIR_FstClus, fp->obj.sclust);
                        st_qword(fs->dirbuf + XDIR_FileSize, fp->obj.objsize);
                        st_qword(fs->dirbuf + XDIR_ValidFileSize, fp->obj.objsize);
                        st_dword(fs->dirbuf + XDIR_ModTime, tm);         
                        fs->dirbuf[XDIR_ModTime10] = 0;
                        st_dword(fs->dirbuf + XDIR_AccTime, 0);
                        res = store_xdir(&dj);   
                        if (res == FR_OK) {
                            res = sync_fs(fs);
                            fp->flag &= (BYTE)~FA_MODIFIED;
                        }
                    }
                    FREE_NAMBUF();
                }
            } else
#endif
            {
                res = move_window(fs, fp->dir_sect);
                if (res == FR_OK) {
                    dir = fp->dir_ptr;
                    dir[DIR_Attr] |= AM_ARC;                         
                    st_clust(fp->obj.fs, dir, fp->obj.sclust);       
                    st_dword(dir + DIR_FileSize, (DWORD)fp->obj.objsize);    
                    st_dword(dir + DIR_ModTime, tm);                 
                    st_word(dir + DIR_LstAccDate, 0);
                    fs->wflag = 1;
                    res = sync_fs(fs);                   
                    fp->flag &= (BYTE)~FA_MODIFIED;
                }
            }
        }
    }

    LEAVE_FF(fs, res);
}

#endif  




 
 
 

FRESULT f_close (
    FIL* fp      
)
{
    FRESULT res;
    FATFS *fs;

#if !FF_FS_READONLY
    res = f_sync(fp);                    
    if (res == FR_OK)
#endif
    {
        res = validate(&fp->obj, &fs);   
        if (res == FR_OK) {
#if FF_FS_LOCK != 0
            res = dec_lock(fp->obj.lockid);      
            if (res == FR_OK) fp->obj.fs = 0;    
#else
            fp->obj.fs = 0;  
#endif
#if FF_FS_REENTRANT
            unlock_fs(fs, FR_OK);        
#endif
        }
    }
    return res;
}




#if FF_FS_RPATH >= 1
 
 
 

FRESULT f_chdir (
    FATFS *fs,
    const TCHAR* path    
)
{
#if FF_STR_VOLUME_ID == 2
    UINT i;
#endif
    FRESULT res;
    DIR dj;
    DEF_NAMBUF


     
    res = find_volume(fs, 0);
    if (res == FR_OK) {
        dj.obj.fs = fs;
        INIT_NAMBUF(fs);
        res = follow_path(&dj, path);        
        if (res == FR_OK) {                  
            if (dj.fn[NSFLAG] & NS_NONAME) {     
                fs->cdir = dj.obj.sclust;
#if FF_FS_EXFAT
                if (fs->fs_type == FS_EXFAT) {
                    fs->cdc_scl = dj.obj.c_scl;
                    fs->cdc_size = dj.obj.c_size;
                    fs->cdc_ofs = dj.obj.c_ofs;
                }
#endif
            } else {
                if (dj.obj.attr & AM_DIR) {  
#if FF_FS_EXFAT
                    if (fs->fs_type == FS_EXFAT) {
                        fs->cdir = ld_dword(fs->dirbuf + XDIR_FstClus);      
                        fs->cdc_scl = dj.obj.sclust;                         
                        fs->cdc_size = ((DWORD)dj.obj.objsize & 0xFFFFFF00) | dj.obj.stat;
                        fs->cdc_ofs = dj.blk_ofs;
                    } else
#endif
                    {
                        fs->cdir = ld_clust(fs, dj.dir);                     
                    }
                } else {
                    res = FR_NO_PATH;        
                }
            }
        }
        FREE_NAMBUF();
        if (res == FR_NO_FILE) res = FR_NO_PATH;
#if FF_STR_VOLUME_ID == 2    
        if (res == FR_OK) {
            for (i = FF_VOLUMES - 1; i && fs != FatFs[i]; i--) ;     
            CurrVol = (BYTE)i;
        }
#endif
    }

    LEAVE_FF(fs, res);
}


#if FF_FS_RPATH >= 2
FRESULT f_getcwd (
    FATFS *fs,
    TCHAR* buff,     
    UINT len         
)
{
    FRESULT res;
    DIR dj;
    UINT i, n;
    DWORD ccl;
    TCHAR *tp = buff;
#if FF_VOLUMES >= 2
    UINT vl;
#endif
#if FF_STR_VOLUME_ID
    const char *vp;
#endif
    FILINFO fno;
    DEF_NAMBUF


     
    buff[0] = 0;     
    res = find_volume(fs, 0);     
    if (res == FR_OK) {
        dj.obj.fs = fs;
        INIT_NAMBUF(fs);

         
        i = len;             
        if (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) {   
            dj.obj.sclust = fs->cdir;                
            while ((ccl = dj.obj.sclust) != 0) {     
                res = dir_sdi(&dj, 1 * SZDIRE);  
                if (res != FR_OK) break;
                res = move_window(fs, dj.sect);
                if (res != FR_OK) break;
                dj.obj.sclust = ld_clust(fs, dj.dir);    
                res = dir_sdi(&dj, 0);
                if (res != FR_OK) break;
                do {                             
                    res = DIR_READ_FILE(&dj);
                    if (res != FR_OK) break;
                    if (ccl == ld_clust(fs, dj.dir)) break;  
                    res = dir_next(&dj, 0);
                } while (res == FR_OK);
                if (res == FR_NO_FILE) res = FR_INT_ERR; 
                if (res != FR_OK) break;
                get_fileinfo(&dj, &fno);         
                for (n = 0; fno.fname[n]; n++) ;     
                if (i < n + 1) {     
                    res = FR_NOT_ENOUGH_CORE; break;
                }
                while (n) buff[--i] = fno.fname[--n];    
                buff[--i] = '/';
            }
        }
        if (res == FR_OK) {
            if (i == len) buff[--i] = '/';   
#if FF_VOLUMES >= 2          
            vl = 0;
#if FF_STR_VOLUME_ID >= 1    
            for (n = 0, vp = (const char*)VolumeStr[CurrVol]; vp[n]; n++) ;
            if (i >= n + 2) {
                if (FF_STR_VOLUME_ID == 2) *tp++ = (TCHAR)'/';
                for (vl = 0; vl < n; *tp++ = (TCHAR)vp[vl], vl++) ;
                if (FF_STR_VOLUME_ID == 1) *tp++ = (TCHAR)':';
                vl++;
            }
#else                        
            if (i >= 3) {
                *tp++ = (TCHAR)'0' + CurrVol;
                *tp++ = (TCHAR)':';
                vl = 2;
            }
#endif
            if (vl == 0) res = FR_NOT_ENOUGH_CORE;
#endif
             
            if (res == FR_OK) {
                do *tp++ = buff[i++]; while (i < len);   
            }
        }
        FREE_NAMBUF();
    }

    *tp = 0;
    LEAVE_FF(fs, res);
}

#endif  
#endif  



#if FF_FS_MINIMIZE <= 2
 
 
 

FRESULT f_lseek (
    FIL* fp,         
    FSIZE_t ofs      
)
{
    FRESULT res;
    FATFS *fs;
    DWORD clst, bcs, nsect;
    FSIZE_t ifptr;
#if FF_USE_FASTSEEK
    DWORD cl, pcl, ncl, tcl, dsc, tlen, ulen, *tbl;
#endif

    res = validate(&fp->obj, &fs);       
    if (res == FR_OK) res = (FRESULT)fp->err;
#if FF_FS_EXFAT && !FF_FS_READONLY
    if (res == FR_OK && fs->fs_type == FS_EXFAT) {
        res = fill_last_frag(&fp->obj, fp->clust, 0xFFFFFFFF);   
    }
#endif
    if (res != FR_OK) LEAVE_FF(fs, res);

#if FF_USE_FASTSEEK
    if (fp->cltbl) {     
        if (ofs == CREATE_LINKMAP) {     
            tbl = fp->cltbl;
            tlen = *tbl++; ulen = 2;     
            cl = fp->obj.sclust;         
            if (cl != 0) {
                do {
                     
                    tcl = cl; ncl = 0; ulen += 2;    
                    do {
                        pcl = cl; ncl++;
                        cl = get_fat(&fp->obj, cl);
                        if (cl <= 1) ABORT(fs, FR_INT_ERR);
                        if (cl == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
                    } while (cl == pcl + 1);
                    if (ulen <= tlen) {      
                        *tbl++ = ncl; *tbl++ = tcl;
                    }
                } while (cl < fs->n_fatent);     
            }
            *fp->cltbl = ulen;   
            if (ulen <= tlen) {
                *tbl = 0;        
            } else {
                res = FR_NOT_ENOUGH_CORE;    
            }
        } else {                         
            if (ofs > fp->obj.objsize) ofs = fp->obj.objsize;    
            fp->fptr = ofs;              
            if (ofs > 0) {
                fp->clust = clmt_clust(fp, ofs - 1);
                dsc = clst2sect(fs, fp->clust);
                if (dsc == 0) ABORT(fs, FR_INT_ERR);
                dsc += (DWORD)((ofs - 1) / SS(fs)) & (fs->csize - 1);
                if (fp->fptr % SS(fs) && dsc != fp->sect) {  
#if !FF_FS_TINY
#if !FF_FS_READONLY
                    if (fp->flag & FA_DIRTY) {       
                        if (disk_write(fs->drv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
                        fp->flag &= (BYTE)~FA_DIRTY;
                    }
#endif
                    if (disk_read(fs->drv, fp->buf, dsc, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);  
#endif
                    fp->sect = dsc;
                }
            }
        }
    } else
#endif

     
    {
#if FF_FS_EXFAT
        if (fs->fs_type != FS_EXFAT && ofs >= 0x100000000) ofs = 0xFFFFFFFF;     
#endif
        if (ofs > fp->obj.objsize && (FF_FS_READONLY || !(fp->flag & FA_WRITE))) {   
            ofs = fp->obj.objsize;
        }
        ifptr = fp->fptr;
        fp->fptr = nsect = 0;
        if (ofs > 0) {
            bcs = (DWORD)fs->csize * SS(fs);     
            if (ifptr > 0 &&
                (ofs - 1) / bcs >= (ifptr - 1) / bcs) {  
                fp->fptr = (ifptr - 1) & ~(FSIZE_t)(bcs - 1);    
                ofs -= fp->fptr;
                clst = fp->clust;
            } else {                                     
                clst = fp->obj.sclust;                   
#if !FF_FS_READONLY
                if (clst == 0) {                         
                    clst = create_chain(&fp->obj, 0);
                    if (clst == 1) ABORT(fs, FR_INT_ERR);
                    if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
                    fp->obj.sclust = clst;
                }
#endif
                fp->clust = clst;
            }
            if (clst != 0) {
                while (ofs > bcs) {                      
                    ofs -= bcs; fp->fptr += bcs;
#if !FF_FS_READONLY
                    if (fp->flag & FA_WRITE) {           
                        if (FF_FS_EXFAT && fp->fptr > fp->obj.objsize) {     
                            fp->obj.objsize = fp->fptr;
                            fp->flag |= FA_MODIFIED;
                        }
                        clst = create_chain(&fp->obj, clst);     
                        if (clst == 0) {                 
                            ofs = 0; break;
                        }
                    } else
#endif
                    {
                        clst = get_fat(&fp->obj, clst);  
                    }
                    if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
                    if (clst <= 1 || clst >= fs->n_fatent) ABORT(fs, FR_INT_ERR);
                    fp->clust = clst;
                }
                fp->fptr += ofs;
                if (ofs % SS(fs)) {
                    nsect = clst2sect(fs, clst);     
                    if (nsect == 0) ABORT(fs, FR_INT_ERR);
                    nsect += (DWORD)(ofs / SS(fs));
                }
            }
        }
        if (!FF_FS_READONLY && fp->fptr > fp->obj.objsize) {     
            fp->obj.objsize = fp->fptr;
            fp->flag |= FA_MODIFIED;
        }
        if (fp->fptr % SS(fs) && nsect != fp->sect) {    
#if !FF_FS_TINY
#if !FF_FS_READONLY
            if (fp->flag & FA_DIRTY) {           
                if (disk_write(fs->drv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
                fp->flag &= (BYTE)~FA_DIRTY;
            }
#endif
            if (disk_read(fs->drv, fp->buf, nsect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);    
#endif
            fp->sect = nsect;
        }
    }

    LEAVE_FF(fs, res);
}



#if FF_FS_MINIMIZE <= 1
 
 
 

FRESULT f_opendir (
    FATFS *fs,
    DIR* dp,             
    const TCHAR* path    
)
{
    FRESULT res;
    DEF_NAMBUF


    if (!dp) return FR_INVALID_OBJECT;

     
    res = find_volume(fs, 0);
    if (res == FR_OK) {
        dp->obj.fs = fs;
        INIT_NAMBUF(fs);
        res = follow_path(dp, path);             
        if (res == FR_OK) {                      
            if (!(dp->fn[NSFLAG] & NS_NONAME)) {     
                if (dp->obj.attr & AM_DIR) {         
#if FF_FS_EXFAT
                    if (fs->fs_type == FS_EXFAT) {
                        dp->obj.c_scl = dp->obj.sclust;                          
                        dp->obj.c_size = ((DWORD)dp->obj.objsize & 0xFFFFFF00) | dp->obj.stat;
                        dp->obj.c_ofs = dp->blk_ofs;
                        init_alloc_info(fs, &dp->obj);   
                    } else
#endif
                    {
                        dp->obj.sclust = ld_clust(fs, dp->dir);  
                    }
                } else {                         
                    res = FR_NO_PATH;
                }
            }
            if (res == FR_OK) {
                dp->obj.id = fs->id;
                res = dir_sdi(dp, 0);            
#if FF_FS_LOCK != 0
                if (res == FR_OK) {
                    if (dp->obj.sclust != 0) {
                        dp->obj.lockid = inc_lock(dp, 0);    
                        if (!dp->obj.lockid) res = FR_TOO_MANY_OPEN_FILES;
                    } else {
                        dp->obj.lockid = 0;  
                    }
                }
#endif
            }
        }
        FREE_NAMBUF();
        if (res == FR_NO_FILE) res = FR_NO_PATH;
    }
    if (res != FR_OK) dp->obj.fs = 0;        

    LEAVE_FF(fs, res);
}




 
 
 

FRESULT f_closedir (
    DIR *dp      
)
{
    FRESULT res;
    FATFS *fs;


    res = validate(&dp->obj, &fs);   
    if (res == FR_OK) {
#if FF_FS_LOCK != 0
        if (dp->obj.lockid) res = dec_lock(dp->obj.lockid);  
        if (res == FR_OK) dp->obj.fs = 0;    
#else
        dp->obj.fs = 0;  
#endif
#if FF_FS_REENTRANT
        unlock_fs(fs, FR_OK);        
#endif
    }
    return res;
}




 
 
 

FRESULT f_readdir (
    DIR* dp,             
    FILINFO* fno         
)
{
    FRESULT res;
    FATFS *fs;
    DEF_NAMBUF


    res = validate(&dp->obj, &fs);   
    if (res == FR_OK) {
        if (!fno) {
            res = dir_sdi(dp, 0);            
        } else {
            INIT_NAMBUF(fs);
            res = DIR_READ_FILE(dp);         
            if (res == FR_NO_FILE) res = FR_OK;  
            if (res == FR_OK) {              
                get_fileinfo(dp, fno);       
                res = dir_next(dp, 0);       
                if (res == FR_NO_FILE) res = FR_OK;  
            }
            FREE_NAMBUF();
        }
    }
    LEAVE_FF(fs, res);
}



#if FF_USE_FIND
 
 
 

FRESULT f_findnext (
    DIR* dp,         
    FILINFO* fno     
)
{
    FRESULT res;


    for (;;) {
        res = f_readdir(dp, fno);        
        if (res != FR_OK || !fno || !fno->fname[0]) break;   
        if (pattern_matching(dp->pat, fno->fname, 0, 0)) break;      
#if FF_USE_LFN && FF_USE_FIND == 2
        if (pattern_matching(dp->pat, fno->altname, 0, 0)) break;    
#endif
    }
    return res;
}



 
 
 

FRESULT f_findfirst (
    DIR* dp,                 
    FILINFO* fno,            
    const TCHAR* path,       
    const TCHAR* pattern     
)
{
    FRESULT res;


    dp->pat = pattern;       
    res = f_opendir(dp, path);       
    if (res == FR_OK) {
        res = f_findnext(dp, fno);   
    }
    return res;
}

#endif   



#if FF_FS_MINIMIZE == 0
 
 
 

FRESULT f_stat (
    FATFS *fs,
    const TCHAR* path,   
    FILINFO* fno         
)
{
    FRESULT res;
    DIR dj;
    DEF_NAMBUF


     
    res = find_volume(fs, 0);
    dj.obj.fs = fs;
    if (res == FR_OK) {
        INIT_NAMBUF(dj.obj.fs);
        res = follow_path(&dj, path);    
        if (res == FR_OK) {              
            if (dj.fn[NSFLAG] & NS_NONAME) {     
                res = FR_INVALID_NAME;
            } else {                             
                if (fno) get_fileinfo(&dj, fno);
            }
        }
        FREE_NAMBUF();
    }

    LEAVE_FF(dj.obj.fs, res);
}



#if !FF_FS_READONLY
 
 
 

FRESULT f_getfree (
    FATFS *fs,
    DWORD* nclst         
)
{
    FRESULT res;
    DWORD nfree, clst, sect, stat;
    UINT i;
    FFOBJID obj;


     
    res = find_volume(fs, 0);
    if (res == FR_OK) {
         
        if (fs->free_clst <= fs->n_fatent - 2) {
            *nclst = fs->free_clst;
        } else {
             
            nfree = 0;
            if (fs->fs_type == FS_FAT12) {   
                clst = 2; obj.fs = fs;
                do {
                    stat = get_fat(&obj, clst);
                    if (stat == 0xFFFFFFFF) { res = FR_DISK_ERR; break; }
                    if (stat == 1) { res = FR_INT_ERR; break; }
                    if (stat == 0) nfree++;
                } while (++clst < fs->n_fatent);
            } else {
#if FF_FS_EXFAT
                if (fs->fs_type == FS_EXFAT) {   
                    BYTE bm;
                    UINT b;

                    clst = fs->n_fatent - 2;     
                    sect = fs->bitbase;          
                    i = 0;                       
                    do {     
                        if (i == 0) {
                            res = move_window(fs, sect++);
                            if (res != FR_OK) break;
                        }
                        for (b = 8, bm = fs->win[i]; b && clst; b--, clst--) {
                            if (!(bm & 1)) nfree++;
                            bm >>= 1;
                        }
                        i = (i + 1) % SS(fs);
                    } while (clst);
                } else
#endif
                {    
                    clst = fs->n_fatent;     
                    sect = fs->fatbase;      
                    i = 0;                   
                    do {     
                        if (i == 0) {
                            res = move_window(fs, sect++);
                            if (res != FR_OK) break;
                        }
                        if (fs->fs_type == FS_FAT16) {
                            if (ld_word(fs->win + i) == 0) nfree++;
                            i += 2;
                        } else {
                            if ((ld_dword(fs->win + i) & 0x0FFFFFFF) == 0) nfree++;
                            i += 4;
                        }
                        i %= SS(fs);
                    } while (--clst);
                }
            }
            *nclst = nfree;          
            fs->free_clst = nfree;   
            fs->fsi_flag |= 1;       
        }
    }

    LEAVE_FF(fs, res);
}




 
 
 

FRESULT f_truncate (
    FIL* fp      
)
{
    FRESULT res;
    FATFS *fs;
    DWORD ncl;


    res = validate(&fp->obj, &fs);   
    if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);
    if (!(fp->flag & FA_WRITE)) LEAVE_FF(fs, FR_DENIED);     

    if (fp->fptr < fp->obj.objsize) {    
        if (fp->fptr == 0) {     
            res = remove_chain(&fp->obj, fp->obj.sclust, 0);
            fp->obj.sclust = 0;
        } else {                 
            ncl = get_fat(&fp->obj, fp->clust);
            res = FR_OK;
            if (ncl == 0xFFFFFFFF) res = FR_DISK_ERR;
            if (ncl == 1) res = FR_INT_ERR;
            if (res == FR_OK && ncl < fs->n_fatent) {
                res = remove_chain(&fp->obj, ncl, fp->clust);
            }
        }
        fp->obj.objsize = fp->fptr;  
        fp->flag |= FA_MODIFIED;
#if !FF_FS_TINY
        if (res == FR_OK && (fp->flag & FA_DIRTY)) {
            if (disk_write(fs->drv, fp->buf, fp->sect, 1) != RES_OK) {
                res = FR_DISK_ERR;
            } else {
                fp->flag &= (BYTE)~FA_DIRTY;
            }
        }
#endif
        if (res != FR_OK) ABORT(fs, res);
    }

    LEAVE_FF(fs, res);
}




 
 
 

FRESULT f_unlink (
    FATFS *fs,
    const TCHAR* path        
)
{
    FRESULT res;
    DIR dj, sdj;
    DWORD dclst = 0;
#if FF_FS_EXFAT
    FFOBJID obj;
#endif
    DEF_NAMBUF


     
    res = find_volume(fs, FA_WRITE);
    if (res == FR_OK) {
        dj.obj.fs = fs;
        INIT_NAMBUF(fs);
        res = follow_path(&dj, path);        
        if (FF_FS_RPATH && res == FR_OK && (dj.fn[NSFLAG] & NS_DOT)) {
            res = FR_INVALID_NAME;           
        }
#if FF_FS_LOCK != 0
        if (res == FR_OK) res = chk_lock(&dj, 2);    
#endif
        if (res == FR_OK) {                  
            if (dj.fn[NSFLAG] & NS_NONAME) {
                res = FR_INVALID_NAME;       
            } else {
                if (dj.obj.attr & AM_RDO) {
                    res = FR_DENIED;         
                }
            }
            if (res == FR_OK) {
#if FF_FS_EXFAT
                obj.fs = fs;
                if (fs->fs_type == FS_EXFAT) {
                    init_alloc_info(fs, &obj);
                    dclst = obj.sclust;
                } else
#endif
                {
                    dclst = ld_clust(fs, dj.dir);
                }
                if (dj.obj.attr & AM_DIR) {          
#if FF_FS_RPATH != 0
                    if (dclst == fs->cdir) {             
                        res = FR_DENIED;
                    } else
#endif
                    {
                        sdj.obj.fs = fs;                 
                        sdj.obj.sclust = dclst;
#if FF_FS_EXFAT
                        if (fs->fs_type == FS_EXFAT) {
                            sdj.obj.objsize = obj.objsize;
                            sdj.obj.stat = obj.stat;
                        }
#endif
                        res = dir_sdi(&sdj, 0);
                        if (res == FR_OK) {
                            res = DIR_READ_FILE(&sdj);           
                            if (res == FR_OK) res = FR_DENIED;   
                            if (res == FR_NO_FILE) res = FR_OK;  
                        }
                    }
                }
            }
            if (res == FR_OK) {
                res = dir_remove(&dj);           
                if (res == FR_OK && dclst != 0) {    
#if FF_FS_EXFAT
                    res = remove_chain(&obj, dclst, 0);
#else
                    res = remove_chain(&dj.obj, dclst, 0);
#endif
                }
                if (res == FR_OK) res = sync_fs(fs);
            }
        }
        FREE_NAMBUF();
    }

    LEAVE_FF(fs, res);
}




 
 
 

FRESULT f_mkdir (
    FATFS *fs,
    const TCHAR* path        
)
{
    FRESULT res;
    DIR dj;
    FFOBJID sobj;
    DWORD dcl, pcl, tm;
    DEF_NAMBUF


    res = find_volume(fs, FA_WRITE);     
    if (res == FR_OK) {
        dj.obj.fs = fs;
        INIT_NAMBUF(fs);
        res = follow_path(&dj, path);            
        if (res == FR_OK) res = FR_EXIST;        
        if (FF_FS_RPATH && res == FR_NO_FILE && (dj.fn[NSFLAG] & NS_DOT)) {  
            res = FR_INVALID_NAME;
        }
        if (res == FR_NO_FILE) {                 
            sobj.fs = fs;                        
            dcl = create_chain(&sobj, 0);        
            res = FR_OK;
            if (dcl == 0) res = FR_DENIED;       
            if (dcl == 1) res = FR_INT_ERR;      
            if (dcl == 0xFFFFFFFF) res = FR_DISK_ERR;    
            tm = GET_FATTIME();
            if (res == FR_OK) {
                res = dir_clear(fs, dcl);        
                if (res == FR_OK) {
                    if (!FF_FS_EXFAT || fs->fs_type != FS_EXFAT) {   
                        mem_set(fs->win + DIR_Name, ' ', 11);    
                        fs->win[DIR_Name] = '.';
                        fs->win[DIR_Attr] = AM_DIR;
                        st_dword(fs->win + DIR_ModTime, tm);
                        st_clust(fs, fs->win, dcl);
                        mem_cpy(fs->win + SZDIRE, fs->win, SZDIRE);  
                        fs->win[SZDIRE + 1] = '.'; pcl = dj.obj.sclust;
                        st_clust(fs, fs->win + SZDIRE, pcl);
                        fs->wflag = 1;
                    }
                    res = dir_register(&dj);     
                }
            }
            if (res == FR_OK) {
#if FF_FS_EXFAT
                if (fs->fs_type == FS_EXFAT) {   
                    st_dword(fs->dirbuf + XDIR_ModTime, tm);     
                    st_dword(fs->dirbuf + XDIR_FstClus, dcl);    
                    st_dword(fs->dirbuf + XDIR_FileSize, (DWORD)fs->csize * SS(fs));     
                    st_dword(fs->dirbuf + XDIR_ValidFileSize, (DWORD)fs->csize * SS(fs));
                    fs->dirbuf[XDIR_GenFlags] = 3;               
                    fs->dirbuf[XDIR_Attr] = AM_DIR;              
                    res = store_xdir(&dj);
                } else
#endif
                {
                    st_dword(dj.dir + DIR_ModTime, tm);  
                    st_clust(fs, dj.dir, dcl);           
                    dj.dir[DIR_Attr] = AM_DIR;           
                    fs->wflag = 1;
                }
                if (res == FR_OK) {
                    res = sync_fs(fs);
                }
            } else {
                remove_chain(&sobj, dcl, 0);         
            }
        }
        FREE_NAMBUF();
    }

    LEAVE_FF(fs, res);
}




 
 
 

FRESULT f_rename (
    FATFS *fs,
    const TCHAR* path_old,   
    const TCHAR* path_new    
)
{
    FRESULT res;
    DIR djo, djn;
    BYTE buf[FF_FS_EXFAT ? SZDIRE * 2 : SZDIRE], *dir;
    DWORD dw;
    DEF_NAMBUF


    res = find_volume(fs, FA_WRITE);     
    if (res == FR_OK) {
        djo.obj.fs = fs;
        INIT_NAMBUF(fs);
        res = follow_path(&djo, path_old);       
        if (res == FR_OK && (djo.fn[NSFLAG] & (NS_DOT | NS_NONAME))) res = FR_INVALID_NAME;  
#if FF_FS_LOCK != 0
        if (res == FR_OK) {
            res = chk_lock(&djo, 2);
        }
#endif
        if (res == FR_OK) {                      
#if FF_FS_EXFAT
            if (fs->fs_type == FS_EXFAT) {   
                BYTE nf, nn;
                WORD nh;

                mem_cpy(buf, fs->dirbuf, SZDIRE * 2);    
                mem_cpy(&djn, &djo, sizeof djo);
                res = follow_path(&djn, path_new);       
                if (res == FR_OK) {                      
                    res = (djn.obj.sclust == djo.obj.sclust && djn.dptr == djo.dptr) ? FR_NO_FILE : FR_EXIST;
                }
                if (res == FR_NO_FILE) {                 
                    res = dir_register(&djn);            
                    if (res == FR_OK) {
                        nf = fs->dirbuf[XDIR_NumSec]; nn = fs->dirbuf[XDIR_NumName];
                        nh = ld_word(fs->dirbuf + XDIR_NameHash);
                        mem_cpy(fs->dirbuf, buf, SZDIRE * 2);    
                        fs->dirbuf[XDIR_NumSec] = nf; fs->dirbuf[XDIR_NumName] = nn;
                        st_word(fs->dirbuf + XDIR_NameHash, nh);
                        if (!(fs->dirbuf[XDIR_Attr] & AM_DIR)) fs->dirbuf[XDIR_Attr] |= AM_ARC;  
 
                        res = store_xdir(&djn);
                    }
                }
            } else
#endif
            {    
                mem_cpy(buf, djo.dir, SZDIRE);           
                mem_cpy(&djn, &djo, sizeof (DIR));       
                res = follow_path(&djn, path_new);       
                if (res == FR_OK) {                      
                    res = (djn.obj.sclust == djo.obj.sclust && djn.dptr == djo.dptr) ? FR_NO_FILE : FR_EXIST;
                }
                if (res == FR_NO_FILE) {                 
                    res = dir_register(&djn);            
                    if (res == FR_OK) {
                        dir = djn.dir;                   
                        mem_cpy(dir + 13, buf + 13, SZDIRE - 13);
                        dir[DIR_Attr] = buf[DIR_Attr];
                        if (!(dir[DIR_Attr] & AM_DIR)) dir[DIR_Attr] |= AM_ARC;  
                        fs->wflag = 1;
                        if ((dir[DIR_Attr] & AM_DIR) && djo.obj.sclust != djn.obj.sclust) {  
                            dw = clst2sect(fs, ld_clust(fs, dir));
                            if (dw == 0) {
                                res = FR_INT_ERR;
                            } else {
 
                                res = move_window(fs, dw);
                                dir = fs->win + SZDIRE * 1;  
                                if (res == FR_OK && dir[1] == '.') {
                                    st_clust(fs, dir, djn.obj.sclust);
                                    fs->wflag = 1;
                                }
                            }
                        }
                    }
                }
            }
            if (res == FR_OK) {
                res = dir_remove(&djo);      
                if (res == FR_OK) {
                    res = sync_fs(fs);
                }
            }
 
        }
        FREE_NAMBUF();
    }

    LEAVE_FF(fs, res);
}

#endif  
#endif  
#endif  
#endif  



#if FF_USE_CHMOD && !FF_FS_READONLY
 
 
 

FRESULT f_chmod (
    FATFS *fs,
    const TCHAR* path,   
    BYTE attr,           
    BYTE mask            
)
{
    FRESULT res;
    DIR dj;
    DEF_NAMBUF


    res = find_volume(fs, FA_WRITE);     
    if (res == FR_OK) {
        dj.obj.fs = fs;
        INIT_NAMBUF(fs);
        res = follow_path(&dj, path);    
        if (res == FR_OK && (dj.fn[NSFLAG] & (NS_DOT | NS_NONAME))) res = FR_INVALID_NAME;   
        if (res == FR_OK) {
            mask &= AM_RDO|AM_HID|AM_SYS|AM_ARC;     
#if FF_FS_EXFAT
            if (fs->fs_type == FS_EXFAT) {
                fs->dirbuf[XDIR_Attr] = (attr & mask) | (fs->dirbuf[XDIR_Attr] & (BYTE)~mask);   
                res = store_xdir(&dj);
            } else
#endif
            {
                dj.dir[DIR_Attr] = (attr & mask) | (dj.dir[DIR_Attr] & (BYTE)~mask);     
                fs->wflag = 1;
            }
            if (res == FR_OK) {
                res = sync_fs(fs);
            }
        }
        FREE_NAMBUF();
    }

    LEAVE_FF(fs, res);
}




 
 
 

FRESULT f_utime (
    FATFS *fs,
    const TCHAR* path,   
    const FILINFO* fno   
)
{
    FRESULT res;
    DIR dj;
    DEF_NAMBUF


    res = find_volume(fs, FA_WRITE);     
    if (res == FR_OK) {
        dj.obj.fs = fs;
        INIT_NAMBUF(fs);
        res = follow_path(&dj, path);    
        if (res == FR_OK && (dj.fn[NSFLAG] & (NS_DOT | NS_NONAME))) res = FR_INVALID_NAME;   
        if (res == FR_OK) {
#if FF_FS_EXFAT
            if (fs->fs_type == FS_EXFAT) {
                st_dword(fs->dirbuf + XDIR_ModTime, (DWORD)fno->fdate << 16 | fno->ftime);
                res = store_xdir(&dj);
            } else
#endif
            {
                st_dword(dj.dir + DIR_ModTime, (DWORD)fno->fdate << 16 | fno->ftime);
                fs->wflag = 1;
            }
            if (res == FR_OK) {
                res = sync_fs(fs);
            }
        }
        FREE_NAMBUF();
    }

    LEAVE_FF(fs, res);
}

#endif   



#if FF_USE_LABEL
 
 
 

FRESULT f_getlabel (
    FATFS *fs,
    TCHAR* label,        
    DWORD* vsn           
)
{
    FRESULT res;
    DIR dj;
    UINT si, di;
    WCHAR wc;

     
    res = find_volume(fs, 0);

     
    if (res == FR_OK && label) {
        dj.obj.fs = fs; dj.obj.sclust = 0;   
        res = dir_sdi(&dj, 0);
        if (res == FR_OK) {
            res = DIR_READ_LABEL(&dj);       
            if (res == FR_OK) {
#if FF_FS_EXFAT
                if (fs->fs_type == FS_EXFAT) {
                    WCHAR hs;

                    for (si = di = hs = 0; si < dj.dir[XDIR_NumLabel]; si++) {   
                        wc = ld_word(dj.dir + XDIR_Label + si * 2);
                        if (hs == 0 && IsSurrogate(wc)) {    
                            hs = wc; continue;
                        }
                        wc = put_utf((DWORD)hs << 16 | wc, &label[di], 4);
                        if (wc == 0) { di = 0; break; }
                        di += wc;
                        hs = 0;
                    }
                    if (hs != 0) di = 0;     
                    label[di] = 0;
                } else
#endif
                {
                    si = di = 0;         
                    while (si < 11) {
                        wc = dj.dir[si++];
#if FF_USE_LFN && FF_LFN_UNICODE >= 1    
                        if (dbc_1st((BYTE)wc) && si < 11) wc = wc << 8 | dj.dir[si++];   
                        wc = ff_oem2uni(wc, CODEPAGE);                   
                        if (wc != 0) wc = put_utf(wc, &label[di], 4);    
                        if (wc == 0) { di = 0; break; }
                        di += wc;
#else                                    
                        label[di++] = (TCHAR)wc;
#endif
                    }
                    do {                 
                        label[di] = 0;
                        if (di == 0) break;
                    } while (label[--di] == ' ');
                }
            }
        }
        if (res == FR_NO_FILE) {     
            label[0] = 0;
            res = FR_OK;
        }
    }

     
    if (res == FR_OK && vsn) {
        res = move_window(fs, fs->volbase);
        if (res == FR_OK) {
            switch (fs->fs_type) {
            case FS_EXFAT:
                di = BPB_VolIDEx; break;

            case FS_FAT32:
                di = BS_VolID32; break;

            default:
                di = BS_VolID;
            }
            *vsn = ld_dword(fs->win + di);
        }
    }

    LEAVE_FF(fs, res);
}



#if !FF_FS_READONLY
 
 
 

FRESULT f_setlabel (
    FATFS *fs,
    const TCHAR* label   
)
{
    FRESULT res;
    DIR dj;
    BYTE dirvn[22];
    UINT di;
    WCHAR wc;
    static const char badchr[] = "+.,;=[]/\\\"*:<>\?|\x7F";  
#if FF_USE_LFN
    DWORD dc;
#endif

     
    res = find_volume(fs, FA_WRITE);
    if (res != FR_OK) LEAVE_FF(fs, res);

#if FF_FS_EXFAT
    if (fs->fs_type == FS_EXFAT) {   
        mem_set(dirvn, 0, 22);
        di = 0;
        while ((UINT)*label >= ' ') {    
            dc = tchar2uni(&label);  
            if (dc >= 0x10000) {
                if (dc == 0xFFFFFFFF || di >= 10) {  
                    dc = 0;
                } else {
                    st_word(dirvn + di * 2, (WCHAR)(dc >> 16)); di++;
                }
            }
            if (dc == 0 || chk_chr(badchr + 7, (int)dc) || di >= 11) {   
                LEAVE_FF(fs, FR_INVALID_NAME);
            }
            st_word(dirvn + di * 2, (WCHAR)dc); di++;
        }
    } else
#endif
    {    
        mem_set(dirvn, ' ', 11);
        di = 0;
        while ((UINT)*label >= ' ') {    
#if FF_USE_LFN
            dc = tchar2uni(&label);
            wc = (dc < 0x10000) ? ff_uni2oem(ff_wtoupper(dc), CODEPAGE) : 0;
#else                                    
            wc = (BYTE)*label++;
            if (dbc_1st((BYTE)wc)) wc = dbc_2nd((BYTE)*label) ? wc << 8 | (BYTE)*label++ : 0;
            if (IsLower(wc)) wc -= 0x20;         
#if FF_CODE_PAGE == 0
            if (ExCvt && wc >= 0x80) wc = ExCvt[wc - 0x80];  
#elif FF_CODE_PAGE < 900
            if (wc >= 0x80) wc = ExCvt[wc - 0x80];   
#endif
#endif
            if (wc == 0 || chk_chr(badchr + 0, (int)wc) || di >= (UINT)((wc >= 0x100) ? 10 : 11)) {  
                LEAVE_FF(fs, FR_INVALID_NAME);
            }
            if (wc >= 0x100) dirvn[di++] = (BYTE)(wc >> 8);
            dirvn[di++] = (BYTE)wc;
        }
        if (dirvn[0] == DDEM) LEAVE_FF(fs, FR_INVALID_NAME);     
        while (di && dirvn[di - 1] == ' ') di--;                 
    }

     
    dj.obj.fs = fs; dj.obj.sclust = 0;   
    res = dir_sdi(&dj, 0);
    if (res == FR_OK) {
        res = DIR_READ_LABEL(&dj);   
        if (res == FR_OK) {
            if (FF_FS_EXFAT && fs->fs_type == FS_EXFAT) {
                dj.dir[XDIR_NumLabel] = (BYTE)di;    
                mem_cpy(dj.dir + XDIR_Label, dirvn, 22);
            } else {
                if (di != 0) {
                    mem_cpy(dj.dir, dirvn, 11);  
                } else {
                    dj.dir[DIR_Name] = DDEM;     
                }
            }
            fs->wflag = 1;
            res = sync_fs(fs);
        } else {             
            if (res == FR_NO_FILE) {
                res = FR_OK;
                if (di != 0) {   
                    res = dir_alloc(&dj, 1);     
                    if (res == FR_OK) {
                        mem_set(dj.dir, 0, SZDIRE);  
                        if (FF_FS_EXFAT && fs->fs_type == FS_EXFAT) {
                            dj.dir[XDIR_Type] = ET_VLABEL;   
                            dj.dir[XDIR_NumLabel] = (BYTE)di;
                            mem_cpy(dj.dir + XDIR_Label, dirvn, 22);
                        } else {
                            dj.dir[DIR_Attr] = AM_VOL;       
                            mem_cpy(dj.dir, dirvn, 11);
                        }
                        fs->wflag = 1;
                        res = sync_fs(fs);
                    }
                }
            }
        }
    }

    LEAVE_FF(fs, res);
}

#endif  
#endif  



#if FF_USE_EXPAND && !FF_FS_READONLY
 
 
 

FRESULT f_expand (
    FIL* fp,         
    FSIZE_t fsz,     
    BYTE opt         
)
{
    FRESULT res;
    FATFS *fs;
    DWORD n, clst, stcl, scl, ncl, tcl, lclst;


    res = validate(&fp->obj, &fs);       
    if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);
    if (fsz == 0 || fp->obj.objsize != 0 || !(fp->flag & FA_WRITE)) LEAVE_FF(fs, FR_DENIED);
#if FF_FS_EXFAT
    if (fs->fs_type != FS_EXFAT && fsz >= 0x100000000) LEAVE_FF(fs, FR_DENIED);  
#endif
    n = (DWORD)fs->csize * SS(fs);   
    tcl = (DWORD)(fsz / n) + ((fsz & (n - 1)) ? 1 : 0);  
    stcl = fs->last_clst; lclst = 0;
    if (stcl < 2 || stcl >= fs->n_fatent) stcl = 2;

#if FF_FS_EXFAT
    if (fs->fs_type == FS_EXFAT) {
        scl = find_bitmap(fs, stcl, tcl);            
        if (scl == 0) res = FR_DENIED;               
        if (scl == 0xFFFFFFFF) res = FR_DISK_ERR;
        if (res == FR_OK) {  
            if (opt) {       
                res = change_bitmap(fs, scl, tcl, 1);    
                lclst = scl + tcl - 1;
            } else {         
                lclst = scl - 1;
            }
        }
    } else
#endif
    {
        scl = clst = stcl; ncl = 0;
        for (;;) {   
            n = get_fat(&fp->obj, clst);
            if (++clst >= fs->n_fatent) clst = 2;
            if (n == 1) { res = FR_INT_ERR; break; }
            if (n == 0xFFFFFFFF) { res = FR_DISK_ERR; break; }
            if (n == 0) {    
                if (++ncl == tcl) break;     
            } else {
                scl = clst; ncl = 0;         
            }
            if (clst == stcl) { res = FR_DENIED; break; }    
        }
        if (res == FR_OK) {  
            if (opt) {       
                for (clst = scl, n = tcl; n; clst++, n--) {  
                    res = put_fat(fs, clst, (n == 1) ? 0xFFFFFFFF : clst + 1);
                    if (res != FR_OK) break;
                    lclst = clst;
                }
            } else {         
                lclst = scl - 1;
            }
        }
    }

    if (res == FR_OK) {
        fs->last_clst = lclst;       
        if (opt) {   
            fp->obj.sclust = scl;        
            fp->obj.objsize = fsz;
            if (FF_FS_EXFAT) fp->obj.stat = 2;   
            fp->flag |= FA_MODIFIED;
            if (fs->free_clst <= fs->n_fatent - 2) {     
                fs->free_clst -= tcl;
                fs->fsi_flag |= 1;
            }
        }
    }

    LEAVE_FF(fs, res);
}

#endif  



#if FF_USE_FORWARD
 
 
 

FRESULT f_forward (
    FIL* fp,                         
    UINT (*func)(const BYTE*,UINT),  
    UINT btf,                        
    UINT* bf                         
)
{
    FRESULT res;
    FATFS *fs;
    DWORD clst, sect;
    FSIZE_t remain;
    UINT rcnt, csect;
    BYTE *dbuf;


    *bf = 0;     
    res = validate(&fp->obj, &fs);       
    if (res != FR_OK || (res = (FRESULT)fp->err) != FR_OK) LEAVE_FF(fs, res);
    if (!(fp->flag & FA_READ)) LEAVE_FF(fs, FR_DENIED);  

    remain = fp->obj.objsize - fp->fptr;
    if (btf > remain) btf = (UINT)remain;            

    for ( ;  btf && (*func)(0, 0);                   
        fp->fptr += rcnt, *bf += rcnt, btf -= rcnt) {
        csect = (UINT)(fp->fptr / SS(fs) & (fs->csize - 1));     
        if (fp->fptr % SS(fs) == 0) {                
            if (csect == 0) {                        
                clst = (fp->fptr == 0) ?             
                    fp->obj.sclust : get_fat(&fp->obj, fp->clust);
                if (clst <= 1) ABORT(fs, FR_INT_ERR);
                if (clst == 0xFFFFFFFF) ABORT(fs, FR_DISK_ERR);
                fp->clust = clst;                    
            }
        }
        sect = clst2sect(fs, fp->clust);             
        if (sect == 0) ABORT(fs, FR_INT_ERR);
        sect += csect;
#if FF_FS_TINY
        if (move_window(fs, sect) != FR_OK) ABORT(fs, FR_DISK_ERR);  
        dbuf = fs->win;
#else
        if (fp->sect != sect) {      
#if !FF_FS_READONLY
            if (fp->flag & FA_DIRTY) {       
                if (disk_write(fs->drv, fp->buf, fp->sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
                fp->flag &= (BYTE)~FA_DIRTY;
            }
#endif
            if (disk_read(fs->drv, fp->buf, sect, 1) != RES_OK) ABORT(fs, FR_DISK_ERR);
        }
        dbuf = fp->buf;
#endif
        fp->sect = sect;
        rcnt = SS(fs) - (UINT)fp->fptr % SS(fs);     
        if (rcnt > btf) rcnt = btf;                  
        rcnt = (*func)(dbuf + ((UINT)fp->fptr % SS(fs)), rcnt);  
        if (rcnt == 0) ABORT(fs, FR_INT_ERR);
    }

    LEAVE_FF(fs, FR_OK);
}
#endif  



#if FF_USE_MKFS && !FF_FS_READONLY
 
 
 

FRESULT f_mkfs (
    FATFS *fs,
    BYTE opt,            
    DWORD au,            
    void* work,          
    UINT len             
)
{
    const UINT n_fats = 1;       
    const UINT n_rootdir = 512;  
    static const WORD cst[] = {1, 4, 16, 64, 256, 512, 0};   
    static const WORD cst32[] = {1, 2, 4, 8, 16, 32, 0};     
    BYTE fmt, sys, *buf, *pte, part; void *pdrv;
    WORD ss;     
    DWORD szb_buf, sz_buf, sz_blk, n_clst, pau, sect, nsect, n;
    DWORD b_vol, b_fat, b_data;              
    DWORD sz_vol, sz_rsv, sz_fat, sz_dir;    
    UINT i;
    DSTATUS stat;
#if FF_USE_TRIM || FF_FS_EXFAT
    DWORD tbl[3];
#endif


     
    fs->fs_type = 0;     
    pdrv = fs->drv;      
    part = LD2PT(fs);    

     
    disk_ioctl(pdrv, IOCTL_INIT, &stat);
    if (stat & STA_NOINIT) return FR_NOT_READY;
    if (stat & STA_PROTECT) return FR_WRITE_PROTECTED;
    if (disk_ioctl(pdrv, GET_BLOCK_SIZE, &sz_blk) != RES_OK || !sz_blk || sz_blk > 32768 || (sz_blk & (sz_blk - 1))) sz_blk = 1;     
#if FF_MAX_SS != FF_MIN_SS       
    if (disk_ioctl(pdrv, GET_SECTOR_SIZE, &ss) != RES_OK) return FR_DISK_ERR;
    if (ss > FF_MAX_SS || ss < FF_MIN_SS || (ss & (ss - 1))) return FR_DISK_ERR;
#else
    ss = FF_MAX_SS;
#endif
    if ((au != 0 && au < ss) || au > 0x1000000 || (au & (au - 1))) return FR_INVALID_PARAMETER;  
    au /= ss;    

     
#if FF_USE_LFN == 3
    if (!work) {     
        for (szb_buf = MAX_MALLOC, buf = 0; szb_buf >= ss && (buf = ff_memalloc(szb_buf)) == 0; szb_buf /= 2) ;
        sz_buf = szb_buf / ss;       
    } else
#endif
    {
        buf = (BYTE*)work;       
        sz_buf = len / ss;       
        szb_buf = sz_buf * ss;   
    }
    if (!buf || sz_buf == 0) return FR_NOT_ENOUGH_CORE;

     
    if (FF_MULTI_PARTITION && part != 0) {
         
        if (disk_read(pdrv, buf, 0, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);   
        if (ld_word(buf + BS_55AA) != 0xAA55) LEAVE_MKFS(FR_MKFS_ABORTED);   
        pte = buf + (MBR_Table + (part - 1) * SZ_PTE);
        if (pte[PTE_System] == 0) LEAVE_MKFS(FR_MKFS_ABORTED);   
        b_vol = ld_dword(pte + PTE_StLba);       
        sz_vol = ld_dword(pte + PTE_SizLba);     
    } else {
         
        if (disk_ioctl(pdrv, GET_SECTOR_COUNT, &sz_vol) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
        b_vol = (opt & FM_SFD) ? 0 : 63;         
        if (sz_vol < b_vol) LEAVE_MKFS(FR_MKFS_ABORTED);
        sz_vol -= b_vol;                         
    }
    if (sz_vol < 22) LEAVE_MKFS(FR_MKFS_ABORTED);   

     
    do {
        if (FF_FS_EXFAT && (opt & FM_EXFAT)) {   
            if ((opt & FM_ANY) == FM_EXFAT || sz_vol >= 0x4000000 || au > 128) {     
                fmt = FS_EXFAT; break;
            }
        }
        if (au > 128) LEAVE_MKFS(FR_INVALID_PARAMETER);  
        if (opt & FM_FAT32) {    
            if ((opt & FM_ANY) == FM_FAT32 || !(opt & FM_FAT)) {     
                fmt = FS_FAT32; break;
            }
        }
        if (!(opt & FM_FAT)) LEAVE_MKFS(FR_INVALID_PARAMETER);   
        fmt = FS_FAT16;
    } while (0);

#if FF_FS_EXFAT
    if (fmt == FS_EXFAT) {   
        DWORD szb_bit, szb_case, sum, nb, cl;
        WCHAR ch, si;
        UINT j, st;
        BYTE b;

        if (sz_vol < 0x1000) LEAVE_MKFS(FR_MKFS_ABORTED);    
#if FF_USE_TRIM
        tbl[0] = b_vol; tbl[1] = b_vol + sz_vol - 1;     
        disk_ioctl(pdrv, CTRL_TRIM, tbl);
#endif
         
        if (au == 0) {   
            au = 8;
            if (sz_vol >= 0x80000) au = 64;      
            if (sz_vol >= 0x4000000) au = 256;   
        }
        b_fat = b_vol + 32;                                      
        sz_fat = ((sz_vol / au + 2) * 4 + ss - 1) / ss;          
        b_data = (b_fat + sz_fat + sz_blk - 1) & ~(sz_blk - 1);  
        if (b_data - b_vol >= sz_vol / 2) LEAVE_MKFS(FR_MKFS_ABORTED);  
        n_clst = (sz_vol - (b_data - b_vol)) / au;               
        if (n_clst <16) LEAVE_MKFS(FR_MKFS_ABORTED);             
        if (n_clst > MAX_EXFAT) LEAVE_MKFS(FR_MKFS_ABORTED);     

        szb_bit = (n_clst + 7) / 8;                      
        tbl[0] = (szb_bit + au * ss - 1) / (au * ss);    

         
        sect = b_data + au * tbl[0];     
        sum = 0;                         
        st = 0; si = 0; i = 0; j = 0; szb_case = 0;
        do {
            switch (st) {
            case 0:
                ch = (WCHAR)ff_wtoupper(si);     
                if (ch != si) {
                    si++; break;         
                }
                for (j = 1; (WCHAR)(si + j) && (WCHAR)(si + j) == ff_wtoupper((WCHAR)(si + j)); j++) ;   
                if (j >= 128) {
                    ch = 0xFFFF; st = 2; break;  
                }
                st = 1;          
                 
            case 1:
                ch = si++;       
                if (--j == 0) st = 0;
                break;

            default:
                ch = (WCHAR)j; si += (WCHAR)j;   
                st = 0;
            }
            sum = xsum32(buf[i + 0] = (BYTE)ch, sum);        
            sum = xsum32(buf[i + 1] = (BYTE)(ch >> 8), sum);
            i += 2; szb_case += 2;
            if (si == 0 || i == szb_buf) {       
                n = (i + ss - 1) / ss;
                if (disk_write(pdrv, buf, sect, n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
                sect += n; i = 0;
            }
        } while (si);
        tbl[1] = (szb_case + au * ss - 1) / (au * ss);   
        tbl[2] = 1;                                      

         
        sect = b_data; nsect = (szb_bit + ss - 1) / ss;  
        nb = tbl[0] + tbl[1] + tbl[2];                   
        do {
            mem_set(buf, 0, szb_buf);
            for (i = 0; nb >= 8 && i < szb_buf; buf[i++] = 0xFF, nb -= 8) ;
            for (b = 1; nb != 0 && i < szb_buf; buf[i] |= b, b <<= 1, nb--) ;
            n = (nsect > sz_buf) ? sz_buf : nsect;       
            if (disk_write(pdrv, buf, sect, n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
            sect += n; nsect -= n;
        } while (nsect);

         
        sect = b_fat; nsect = sz_fat;    
        j = nb = cl = 0;
        do {
            mem_set(buf, 0, szb_buf); i = 0;     
            if (cl == 0) {   
                st_dword(buf + i, 0xFFFFFFF8); i += 4; cl++;
                st_dword(buf + i, 0xFFFFFFFF); i += 4; cl++;
            }
            do {             
                while (nb != 0 && i < szb_buf) {             
                    st_dword(buf + i, (nb > 1) ? cl + 1 : 0xFFFFFFFF);
                    i += 4; cl++; nb--;
                }
                if (nb == 0 && j < 3) nb = tbl[j++];     
            } while (nb != 0 && i < szb_buf);
            n = (nsect > sz_buf) ? sz_buf : nsect;   
            if (disk_write(pdrv, buf, sect, n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
            sect += n; nsect -= n;
        } while (nsect);

         
        mem_set(buf, 0, szb_buf);
        buf[SZDIRE * 0 + 0] = ET_VLABEL;         
        buf[SZDIRE * 1 + 0] = ET_BITMAP;         
        st_dword(buf + SZDIRE * 1 + 20, 2);              
        st_dword(buf + SZDIRE * 1 + 24, szb_bit);        
        buf[SZDIRE * 2 + 0] = ET_UPCASE;         
        st_dword(buf + SZDIRE * 2 + 4, sum);             
        st_dword(buf + SZDIRE * 2 + 20, 2 + tbl[0]);     
        st_dword(buf + SZDIRE * 2 + 24, szb_case);       
        sect = b_data + au * (tbl[0] + tbl[1]); nsect = au;  
        do {     
            n = (nsect > sz_buf) ? sz_buf : nsect;
            if (disk_write(pdrv, buf, sect, n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
            mem_set(buf, 0, ss);
            sect += n; nsect -= n;
        } while (nsect);

         
        sect = b_vol;
        for (n = 0; n < 2; n++) {
             
            mem_set(buf, 0, ss);
            mem_cpy(buf + BS_JmpBoot, "\xEB\x76\x90" "EXFAT   ", 11);    
            st_dword(buf + BPB_VolOfsEx, b_vol);                     
            st_dword(buf + BPB_TotSecEx, sz_vol);                    
            st_dword(buf + BPB_FatOfsEx, b_fat - b_vol);             
            st_dword(buf + BPB_FatSzEx, sz_fat);                     
            st_dword(buf + BPB_DataOfsEx, b_data - b_vol);           
            st_dword(buf + BPB_NumClusEx, n_clst);                   
            st_dword(buf + BPB_RootClusEx, 2 + tbl[0] + tbl[1]);     
            st_dword(buf + BPB_VolIDEx, GET_FATTIME());              
            st_word(buf + BPB_FSVerEx, 0x100);                       
            for (buf[BPB_BytsPerSecEx] = 0, i = ss; i >>= 1; buf[BPB_BytsPerSecEx]++) ;  
            for (buf[BPB_SecPerClusEx] = 0, i = au; i >>= 1; buf[BPB_SecPerClusEx]++) ;  
            buf[BPB_NumFATsEx] = 1;                  
            buf[BPB_DrvNumEx] = 0x80;                
            st_word(buf + BS_BootCodeEx, 0xFEEB);    
            st_word(buf + BS_55AA, 0xAA55);          
            for (i = sum = 0; i < ss; i++) {         
                if (i != BPB_VolFlagEx && i != BPB_VolFlagEx + 1 && i != BPB_PercInUseEx) sum = xsum32(buf[i], sum);
            }
            if (disk_write(pdrv, buf, sect++, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
             
            mem_set(buf, 0, ss);
            st_word(buf + ss - 2, 0xAA55);   
            for (j = 1; j < 9; j++) {
                for (i = 0; i < ss; sum = xsum32(buf[i++], sum)) ;   
                if (disk_write(pdrv, buf, sect++, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
            }
             
            mem_set(buf, 0, ss);
            for ( ; j < 11; j++) {
                for (i = 0; i < ss; sum = xsum32(buf[i++], sum)) ;   
                if (disk_write(pdrv, buf, sect++, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
            }
             
            for (i = 0; i < ss; i += 4) st_dword(buf + i, sum);      
            if (disk_write(pdrv, buf, sect++, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
        }

    } else
#endif   
    {    
        do {
            pau = au;
             
            if (fmt == FS_FAT32) {   
                if (pau == 0) {  
                    n = sz_vol / 0x20000;    
                    for (i = 0, pau = 1; cst32[i] && cst32[i] <= n; i++, pau <<= 1) ;    
                }
                n_clst = sz_vol / pau;   
                sz_fat = (n_clst * 4 + 8 + ss - 1) / ss;     
                sz_rsv = 32;     
                sz_dir = 0;      
                if (n_clst <= MAX_FAT16 || n_clst > MAX_FAT32) LEAVE_MKFS(FR_MKFS_ABORTED);
            } else {                 
                if (pau == 0) {  
                    n = sz_vol / 0x1000;     
                    for (i = 0, pau = 1; cst[i] && cst[i] <= n; i++, pau <<= 1) ;    
                }
                n_clst = sz_vol / pau;
                if (n_clst > MAX_FAT12) {
                    n = n_clst * 2 + 4;      
                } else {
                    fmt = FS_FAT12;
                    n = (n_clst * 3 + 1) / 2 + 3;    
                }
                sz_fat = (n + ss - 1) / ss;      
                sz_rsv = 1;                      
                sz_dir = (DWORD)n_rootdir * SZDIRE / ss;     
            }
            b_fat = b_vol + sz_rsv;                      
            b_data = b_fat + sz_fat * n_fats + sz_dir;   

             
            n = ((b_data + sz_blk - 1) & ~(sz_blk - 1)) - b_data;    
            if (fmt == FS_FAT32) {       
                sz_rsv += n; b_fat += n;
            } else {                     
                sz_fat += n / n_fats;
            }

             
            if (sz_vol < b_data + pau * 16 - b_vol) LEAVE_MKFS(FR_MKFS_ABORTED);     
            n_clst = (sz_vol - sz_rsv - sz_fat * n_fats - sz_dir) / pau;
            if (fmt == FS_FAT32) {
                if (n_clst <= MAX_FAT16) {   
                    if (au == 0 && (au = pau / 2) != 0) continue;    
                    LEAVE_MKFS(FR_MKFS_ABORTED);
                }
            }
            if (fmt == FS_FAT16) {
                if (n_clst > MAX_FAT16) {    
                    if (au == 0 && (pau * 2) <= 64) {
                        au = pau * 2; continue;      
                    }
                    if ((opt & FM_FAT32)) {
                        fmt = FS_FAT32; continue;    
                    }
                    if (au == 0 && (au = pau * 2) <= 128) continue;  
                    LEAVE_MKFS(FR_MKFS_ABORTED);
                }
                if  (n_clst <= MAX_FAT12) {  
                    if (au == 0 && (au = pau * 2) <= 128) continue;  
                    LEAVE_MKFS(FR_MKFS_ABORTED);
                }
            }
            if (fmt == FS_FAT12 && n_clst > MAX_FAT12) LEAVE_MKFS(FR_MKFS_ABORTED);  

             
            break;
        } while (1);

#if FF_USE_TRIM
        tbl[0] = b_vol; tbl[1] = b_vol + sz_vol - 1;     
        disk_ioctl(pdrv, CTRL_TRIM, tbl);
#endif
         
        mem_set(buf, 0, ss);
        mem_cpy(buf + BS_JmpBoot, "\xEB\xFE\x90" "MSDOS5.0", 11); 
        st_word(buf + BPB_BytsPerSec, ss);               
        buf[BPB_SecPerClus] = (BYTE)pau;                 
        st_word(buf + BPB_RsvdSecCnt, (WORD)sz_rsv);     
        buf[BPB_NumFATs] = (BYTE)n_fats;                 
        st_word(buf + BPB_RootEntCnt, (WORD)((fmt == FS_FAT32) ? 0 : n_rootdir));    
        if (sz_vol < 0x10000) {
            st_word(buf + BPB_TotSec16, (WORD)sz_vol);   
        } else {
            st_dword(buf + BPB_TotSec32, sz_vol);        
        }
        buf[BPB_Media] = 0xF8;                           
        st_word(buf + BPB_SecPerTrk, 63);                
        st_word(buf + BPB_NumHeads, 255);                
        st_dword(buf + BPB_HiddSec, b_vol);              
        if (fmt == FS_FAT32) {
            st_dword(buf + BS_VolID32, GET_FATTIME());   
            st_dword(buf + BPB_FATSz32, sz_fat);         
            st_dword(buf + BPB_RootClus32, 2);           
            st_word(buf + BPB_FSInfo32, 1);              
            st_word(buf + BPB_BkBootSec32, 6);           
            buf[BS_DrvNum32] = 0x80;                     
            buf[BS_BootSig32] = 0x29;                    
            mem_cpy(buf + BS_VolLab32, "NO NAME    " "FAT32   ", 19);    
        } else {
            st_dword(buf + BS_VolID, GET_FATTIME());     
            st_word(buf + BPB_FATSz16, (WORD)sz_fat);    
            buf[BS_DrvNum] = 0x80;                       
            buf[BS_BootSig] = 0x29;                      
            mem_cpy(buf + BS_VolLab, "NO NAME    " "FAT     ", 19);  
        }
        st_word(buf + BS_55AA, 0xAA55);                  
        if (disk_write(pdrv, buf, b_vol, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);  

         
        if (fmt == FS_FAT32) {
            disk_write(pdrv, buf, b_vol + 6, 1);         
            mem_set(buf, 0, ss);
            st_dword(buf + FSI_LeadSig, 0x41615252);
            st_dword(buf + FSI_StrucSig, 0x61417272);
            st_dword(buf + FSI_Free_Count, n_clst - 1);  
            st_dword(buf + FSI_Nxt_Free, 2);             
            st_word(buf + BS_55AA, 0xAA55);
            disk_write(pdrv, buf, b_vol + 7, 1);         
            disk_write(pdrv, buf, b_vol + 1, 1);         
        }

         
        mem_set(buf, 0, (UINT)szb_buf);
        sect = b_fat;        
        for (i = 0; i < n_fats; i++) {           
            if (fmt == FS_FAT32) {
                st_dword(buf + 0, 0xFFFFFFF8);   
                st_dword(buf + 4, 0xFFFFFFFF);   
                st_dword(buf + 8, 0x0FFFFFFF);   
            } else {
                st_dword(buf + 0, (fmt == FS_FAT12) ? 0xFFFFF8 : 0xFFFFFFF8);    
            }
            nsect = sz_fat;      
            do {     
                n = (nsect > sz_buf) ? sz_buf : nsect;
                if (disk_write(pdrv, buf, sect, (UINT)n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
                mem_set(buf, 0, ss);
                sect += n; nsect -= n;
            } while (nsect);
        }

         
        nsect = (fmt == FS_FAT32) ? pau : sz_dir;    
        do {
            n = (nsect > sz_buf) ? sz_buf : nsect;
            if (disk_write(pdrv, buf, sect, (UINT)n) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);
            sect += n; nsect -= n;
        } while (nsect);
    }

     
    if (FF_FS_EXFAT && fmt == FS_EXFAT) {
        sys = 0x07;          
    } else {
        if (fmt == FS_FAT32) {
            sys = 0x0C;      
        } else {
            if (sz_vol >= 0x10000) {
                sys = 0x06;  
            } else {
                sys = (fmt == FS_FAT16) ? 0x04 : 0x01;   
            }
        }
    }

     
    if (FF_MULTI_PARTITION && part != 0) {   
         
        if (disk_read(pdrv, buf, 0, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);   
        buf[MBR_Table + (part - 1) * SZ_PTE + PTE_System] = sys;         
        if (disk_write(pdrv, buf, 0, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);  
    } else {                                 
        if (!(opt & FM_SFD)) {   
            mem_set(buf, 0, ss);
            st_word(buf + BS_55AA, 0xAA55);      
            pte = buf + MBR_Table;               
            pte[PTE_Boot] = 0;                   
            pte[PTE_StHead] = 1;                 
            pte[PTE_StSec] = 1;                  
            pte[PTE_StCyl] = 0;                  
            pte[PTE_System] = sys;               
            n = (b_vol + sz_vol) / (63 * 255);   
            pte[PTE_EdHead] = 254;               
            pte[PTE_EdSec] = (BYTE)(((n >> 2) & 0xC0) | 63);     
            pte[PTE_EdCyl] = (BYTE)n;            
            st_dword(pte + PTE_StLba, b_vol);    
            st_dword(pte + PTE_SizLba, sz_vol);  
            if (disk_write(pdrv, buf, 0, 1) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);  
        }
    }

    if (disk_ioctl(pdrv, CTRL_SYNC, 0) != RES_OK) LEAVE_MKFS(FR_DISK_ERR);

    LEAVE_MKFS(FR_OK);
}



#if FF_MULTI_PARTITION
 
 
 

FRESULT f_fdisk (
    void *pdrv,          
    const DWORD* szt,    
    void* work           
)
{
    UINT i, n, sz_cyl, tot_cyl, b_cyl, e_cyl, p_cyl;
    BYTE s_hd, e_hd, *p, *buf = (BYTE*)work;
    DSTATUS stat;
    DWORD sz_disk, sz_part, s_part;
    FRESULT res;


    disk_ioctl(pdrv, IOCTL_INIT, &stat);
    if (stat & STA_NOINIT) return FR_NOT_READY;
    if (stat & STA_PROTECT) return FR_WRITE_PROTECTED;
    if (disk_ioctl(pdrv, GET_SECTOR_COUNT, &sz_disk)) return FR_DISK_ERR;

    buf = (BYTE*)work;
#if FF_USE_LFN == 3
    if (!buf) buf = ff_memalloc(FF_MAX_SS);  
#endif
    if (!buf) return FR_NOT_ENOUGH_CORE;

     
    for (n = 16; n < 256 && sz_disk / n / 63 > 1024; n *= 2) ;
    if (n == 256) n--;
    e_hd = (BYTE)(n - 1);
    sz_cyl = 63 * n;
    tot_cyl = sz_disk / sz_cyl;

     
    mem_set(buf, 0, FF_MAX_SS);
    p = buf + MBR_Table; b_cyl = 0;
    for (i = 0; i < 4; i++, p += SZ_PTE) {
        p_cyl = (szt[i] <= 100U) ? (DWORD)tot_cyl * szt[i] / 100 : szt[i] / sz_cyl;  
        if (p_cyl == 0) continue;
        s_part = (DWORD)sz_cyl * b_cyl;
        sz_part = (DWORD)sz_cyl * p_cyl;
        if (i == 0) {    
            s_hd = 1;
            s_part += 63; sz_part -= 63;
        } else {
            s_hd = 0;
        }
        e_cyl = b_cyl + p_cyl - 1;   
        if (e_cyl >= tot_cyl) LEAVE_MKFS(FR_INVALID_PARAMETER);

         
        p[1] = s_hd;                         
        p[2] = (BYTE)(((b_cyl >> 2) & 0xC0) | 1);    
        p[3] = (BYTE)b_cyl;                  
        p[4] = 0x07;                         
        p[5] = e_hd;                         
        p[6] = (BYTE)(((e_cyl >> 2) & 0xC0) | 63);   
        p[7] = (BYTE)e_cyl;                  
        st_dword(p + 8, s_part);             
        st_dword(p + 12, sz_part);           

         
        b_cyl += p_cyl;
    }
    st_word(p, 0xAA55);      

     
    res = (disk_write(pdrv, buf, 0, 1) == RES_OK && disk_ioctl(pdrv, CTRL_SYNC, 0) == RES_OK) ? FR_OK : FR_DISK_ERR;
    LEAVE_MKFS(res);
}

#endif  
#endif  



#if FF_CODE_PAGE == 0
 
 
 

FRESULT f_setcp (
    WORD cp      
)
{
    static const WORD       validcp[] = {  437,   720,   737,   771,   775,   850,   852,   857,   860,   861,   862,   863,   864,   865,   866,   869,   932,   936,   949,   950, 0};
    static const BYTE* const tables[] = {Ct437, Ct720, Ct737, Ct771, Ct775, Ct850, Ct852, Ct857, Ct860, Ct861, Ct862, Ct863, Ct864, Ct865, Ct866, Ct869, Dc932, Dc936, Dc949, Dc950, 0};
    UINT i;


    for (i = 0; validcp[i] != 0 && validcp[i] != cp; i++) ;  
    if (validcp[i] != cp) return FR_INVALID_PARAMETER;   

    CodePage = cp;
    if (cp >= 900) {     
        ExCvt = 0;
        DbcTbl = tables[i];
    } else {             
        ExCvt = tables[i];
        DbcTbl = 0;
    }
    return FR_OK;
}
#endif   

