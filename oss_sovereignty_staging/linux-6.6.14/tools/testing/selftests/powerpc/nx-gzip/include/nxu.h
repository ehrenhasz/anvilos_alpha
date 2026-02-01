 
 

#ifndef _NXU_H
#define _NXU_H

#include <stdint.h>
#include <endian.h>
#include "nx.h"

 
#define LLSZ   286
#define DSZ    30

 
#define DHTSZ  18
#define DHT_MAXSZ 288
#define MAX_DDE_COUNT 256

 
#ifdef NXDBG
#define NXPRT(X)	X
#else
#define NXPRT(X)
#endif

#ifdef NXTIMER
#include <sys/platform/ppc.h>
#define NX_CLK(X)	X
#define nx_get_time()	__ppc_get_timebase()
#define nx_get_freq()	__ppc_get_timebase_freq()
#else
#define NX_CLK(X)
#define nx_get_time()  (-1)
#define nx_get_freq()  (-1)
#endif

#define NX_MAX_FAULTS  500

 

union nx_qw_t {
	uint32_t word[4];
	uint64_t dword[2];
} __aligned(16);

 

struct nx_dde_t {
	 
	union {
		uint32_t dde_count;
		 
	};
	uint32_t ddebc;  
	uint64_t ddead;  
} __aligned(16);

struct nx_csb_t {
	 
	union {
		uint32_t csb_v;
		 

		uint32_t csb_f;
		 

		uint32_t csb_cs;
		 

		uint32_t csb_cc;
		 

		uint32_t csb_ce;
		 

	};
	uint32_t tpbc;
	 

	uint64_t fsaddr;
	 
} __aligned(16);

struct nx_ccb_t {
	 

	uint32_t reserved[3];
	union {
		 

		uint32_t ccb_cm;
		 

		uint32_t word;
		 
	};
} __aligned(16);

struct vas_stamped_crb_t {
	 

	union {
		uint32_t vas_buf_num;
		 

		uint32_t send_wc_id;
		 

	};
	union {
		uint32_t recv_wc_id;
		 

	};
	uint32_t reserved2;
	union {
		uint32_t vas_invalid;
		 

	};
};

struct nx_stamped_fault_crb_t {
	 
	uint64_t fsa;
	union {
		uint32_t nxsf_t;
		uint32_t nxsf_fs;
	};
	uint32_t pswid;
};

union stamped_crb_t {
	struct vas_stamped_crb_t      vas;
	struct nx_stamped_fault_crb_t nx;
};

struct nx_gzip_cpb_t {
	 

	 

	struct {
		union {
		union nx_qw_t qw0;
			struct {
				uint32_t in_adler;             
				uint32_t in_crc;               
				union {
					uint32_t in_histlen;   
					uint32_t in_subc;      
				};
				union {
					 
					uint32_t in_sfbt;
					 
					uint32_t in_rembytecnt;
					 
					uint32_t in_dhtlen;
				};
			};
		};
		union {
			union nx_qw_t  in_dht[DHTSZ];	 
			char in_dht_char[DHT_MAXSZ];	 
		};
		union nx_qw_t  reserved[5];		 
	};

	 

	volatile struct {
		union {
			union nx_qw_t qw24;
			struct {
				uint32_t out_adler;     
				uint32_t out_crc;       
				union {
					 
					uint32_t out_tebc;
					 
					uint32_t out_subc;
				};
				union {
					 
					uint32_t out_sfbt;
					 
					uint32_t out_rembytecnt;
					 
					uint32_t out_dhtlen;
				};
			};
		};
		union {
			union nx_qw_t  qw25[79];         
			 
			uint32_t out_spbc_comp_wrap;
			uint32_t out_spbc_wrap;          
			 
			uint32_t out_spbc_comp;
			  
			uint32_t out_lzcount[LLSZ+DSZ];
			struct {
				union nx_qw_t  out_dht[DHTSZ];   
				 
				uint32_t out_spbc_decomp;
			};
		};
		 
		uint32_t out_spbc_comp_with_count;
	};
} __aligned(128);

struct nx_gzip_crb_t {
	union {                    
		uint32_t gzip_fc;      
	};
	uint32_t reserved1;        
	union {
		uint64_t csb_address;  
		struct {
			uint32_t reserved2;
			union {
				uint32_t crb_c;
				 

				uint32_t crb_at;
				 

			};
		};
	};
	struct nx_dde_t source_dde;            
	struct nx_dde_t target_dde;            
	volatile struct nx_ccb_t ccb;          
	volatile union {
		 
		union nx_qw_t reserved64[11];
		union stamped_crb_t stamp;        
	};
	volatile struct nx_csb_t csb;
} __aligned(128);

struct nx_gzip_crb_cpb_t {
	struct nx_gzip_crb_t crb;
	struct nx_gzip_cpb_t cpb;
} __aligned(2048);


 

#define size_mask(x)          ((1U<<(x))-1)

 

#define dde_count_mask        size_mask(8)
#define dde_count_offset      23

 

#define csb_v_mask            size_mask(1)
#define csb_v_offset          0
#define csb_f_mask            size_mask(1)
#define csb_f_offset          6
#define csb_cs_mask           size_mask(8)
#define csb_cs_offset         15
#define csb_cc_mask           size_mask(8)
#define csb_cc_offset         23
#define csb_ce_mask           size_mask(8)
#define csb_ce_offset         31

 

#define ccb_cm_mask           size_mask(3)
#define ccb_cm_offset         31

 

#define vas_buf_num_mask      size_mask(6)
#define vas_buf_num_offset    5
#define send_wc_id_mask       size_mask(16)
#define send_wc_id_offset     31
#define recv_wc_id_mask       size_mask(16)
#define recv_wc_id_offset     31
#define vas_invalid_mask      size_mask(1)
#define vas_invalid_offset    31

 

#define nxsf_t_mask           size_mask(1)
#define nxsf_t_offset         23
#define nxsf_fs_mask          size_mask(8)
#define nxsf_fs_offset        31

 

#define in_histlen_mask       size_mask(12)
#define in_histlen_offset     11
#define in_dhtlen_mask        size_mask(12)
#define in_dhtlen_offset      31
#define in_subc_mask          size_mask(3)
#define in_subc_offset        31
#define in_sfbt_mask          size_mask(4)
#define in_sfbt_offset        15
#define in_rembytecnt_mask    size_mask(16)
#define in_rembytecnt_offset  31

 

#define out_tebc_mask         size_mask(3)
#define out_tebc_offset       15
#define out_subc_mask         size_mask(16)
#define out_subc_offset       31
#define out_sfbt_mask         size_mask(4)
#define out_sfbt_offset       15
#define out_rembytecnt_mask   size_mask(16)
#define out_rembytecnt_offset 31
#define out_dhtlen_mask       size_mask(12)
#define out_dhtlen_offset     31

 

#define gzip_fc_mask          size_mask(8)
#define gzip_fc_offset        31
#define crb_c_mask            size_mask(1)
#define crb_c_offset          28
#define crb_at_mask           size_mask(1)
#define crb_at_offset         30
#define csb_address_mask      ~(15UL)  

 

#define getnn(ST, REG)      ((be32toh(ST.REG) >> (31-REG##_offset)) \
				 & REG##_mask)
#define getpnn(ST, REG)     ((be32toh((ST)->REG) >> (31-REG##_offset)) \
				 & REG##_mask)
#define get32(ST, REG)      (be32toh(ST.REG))
#define getp32(ST, REG)     (be32toh((ST)->REG))
#define get64(ST, REG)      (be64toh(ST.REG))
#define getp64(ST, REG)     (be64toh((ST)->REG))

#define unget32(ST, REG)    (get32(ST, REG) & ~((REG##_mask) \
				<< (31-REG##_offset)))
 

#define ungetp32(ST, REG)   (getp32(ST, REG) & ~((REG##_mask) \
				<< (31-REG##_offset)))
 

#define clear_regs(ST)      memset((void *)(&(ST)), 0, sizeof(ST))
#define clear_dde(ST)       do { ST.dde_count = ST.ddebc = 0; ST.ddead = 0; \
				} while (0)
#define clearp_dde(ST)      do { (ST)->dde_count = (ST)->ddebc = 0; \
				 (ST)->ddead = 0; \
				} while (0)
#define clear_struct(ST)    memset((void *)(&(ST)), 0, sizeof(ST))
#define putnn(ST, REG, X)   (ST.REG = htobe32(unget32(ST, REG) | (((X) \
				 & REG##_mask) << (31-REG##_offset))))
#define putpnn(ST, REG, X)  ((ST)->REG = htobe32(ungetp32(ST, REG) \
				| (((X) & REG##_mask) << (31-REG##_offset))))

#define put32(ST, REG, X)   (ST.REG = htobe32(X))
#define putp32(ST, REG, X)  ((ST)->REG = htobe32(X))
#define put64(ST, REG, X)   (ST.REG = htobe64(X))
#define putp64(ST, REG, X)  ((ST)->REG = htobe64(X))

 

#define get_csb_ce(ST) ((uint32_t)getnn(ST, csb_ce))
#define get_csb_ce_ms3b(ST) (get_csb_ce(ST) >> 5)
#define put_csb_ce_ms3b(ST, X) putnn(ST, csb_ce, ((uint32_t)(X) << 5))

#define CSB_CE_PARTIAL         0x4
#define CSB_CE_TERMINATE       0x2
#define CSB_CE_TPBC_VALID      0x1

#define csb_ce_termination(X)         (!!((X) & CSB_CE_TERMINATE))
 

#define csb_ce_check_completion(X)    (!csb_ce_termination(X))
 

#define csb_ce_partial_completion(X)  (!!((X) & CSB_CE_PARTIAL))
#define csb_ce_full_completion(X)     (!csb_ce_partial_completion(X))
#define csb_ce_tpbc_valid(X)          (!!((X) & CSB_CE_TPBC_VALID))
 

#define csb_ce_default_err(X)         csb_ce_termination(X)
 

#define csb_ce_cc3_partial(X)         csb_ce_partial_completion(X)
 

#define csb_ce_cc64(X)                ((X)&(CSB_CE_PARTIAL \
					| CSB_CE_TERMINATE) == 0)
 

 

#define SFBT_BFINAL 0x1
#define SFBT_LIT    0x4
#define SFBT_FHT    0x5
#define SFBT_DHT    0x6
#define SFBT_HDR    0x7

 

#define GZIP_FC_LIMIT_MASK                               0x01
#define GZIP_FC_COMPRESS_FHT                             0x00
#define GZIP_FC_COMPRESS_DHT                             0x02
#define GZIP_FC_COMPRESS_FHT_COUNT                       0x04
#define GZIP_FC_COMPRESS_DHT_COUNT                       0x06
#define GZIP_FC_COMPRESS_RESUME_FHT                      0x08
#define GZIP_FC_COMPRESS_RESUME_DHT                      0x0a
#define GZIP_FC_COMPRESS_RESUME_FHT_COUNT                0x0c
#define GZIP_FC_COMPRESS_RESUME_DHT_COUNT                0x0e
#define GZIP_FC_DECOMPRESS                               0x10
#define GZIP_FC_DECOMPRESS_SINGLE_BLK_N_SUSPEND          0x12
#define GZIP_FC_DECOMPRESS_RESUME                        0x14
#define GZIP_FC_DECOMPRESS_RESUME_SINGLE_BLK_N_SUSPEND   0x16
#define GZIP_FC_WRAP                                     0x1e

#define fc_is_compress(fc)  (((fc) & 0x10) == 0)
#define fc_has_count(fc)    (fc_is_compress(fc) && (((fc) & 0x4) != 0))

 

#define ERR_NX_OK             0
#define ERR_NX_ALIGNMENT      1
#define ERR_NX_OPOVERLAP      2
#define ERR_NX_DATA_LENGTH    3
#define ERR_NX_TRANSLATION    5
#define ERR_NX_PROTECTION     6
#define ERR_NX_EXTERNAL_UE7   7
#define ERR_NX_INVALID_OP     8
#define ERR_NX_PRIVILEGE      9
#define ERR_NX_INTERNAL_UE   10
#define ERR_NX_EXTERN_UE_WR  12
#define ERR_NX_TARGET_SPACE  13
#define ERR_NX_EXCESSIVE_DDE 14
#define ERR_NX_TRANSL_WR     15
#define ERR_NX_PROTECT_WR    16
#define ERR_NX_SUBFUNCTION   17
#define ERR_NX_FUNC_ABORT    18
#define ERR_NX_BYTE_MAX      19
#define ERR_NX_CORRUPT_CRB   20
#define ERR_NX_INVALID_CRB   21
#define ERR_NX_INVALID_DDE   30
#define ERR_NX_SEGMENTED_DDL 31
#define ERR_NX_DDE_OVERFLOW  33
#define ERR_NX_TPBC_GT_SPBC  64
#define ERR_NX_MISSING_CODE  66
#define ERR_NX_INVALID_DIST  67
#define ERR_NX_INVALID_DHT   68
#define ERR_NX_EXTERNAL_UE90 90
#define ERR_NX_WDOG_TIMER   224
#define ERR_NX_AT_FAULT     250
#define ERR_NX_INTR_SERVER  252
#define ERR_NX_UE253        253
#define ERR_NX_NO_HW        254
#define ERR_NX_HUNG_OP      255
#define ERR_NX_END          256

 
#define INIT_CRC   0   
#define INIT_ADLER 1   

 
int nxu_submit_job(struct nx_gzip_crb_cpb_t *c, void *handle);

extern void nxu_sigsegv_handler(int sig, siginfo_t *info, void *ctx);
extern int nxu_touch_pages(void *buf, long buf_len, long page_len, int wr);

 

char *nx_crb_str(struct nx_gzip_crb_t *crb, char *prbuf);
char *nx_cpb_str(struct nx_gzip_cpb_t *cpb, char *prbuf);
char *nx_prt_hex(void *cp, int sz, char *prbuf);
char *nx_lzcount_str(struct nx_gzip_cpb_t *cpb, char *prbuf);
char *nx_strerror(int e);

#ifdef NX_SIM
#include <stdio.h>
int nx_sim_init(void *ctx);
int nx_sim_end(void *ctx);
int nxu_run_sim_job(struct nx_gzip_crb_cpb_t *c, void *ctx);
#endif  

 

#define set_final_bit(x)	(x |= (unsigned char)1)
#define clr_final_bit(x)	(x &= ~(unsigned char)1)

#define append_empty_fh_blk(p, b) do { *(p) = (2 | (1&(b))); *((p)+1) = 0; \
					} while (0)
 


#ifdef NX_842

 

struct nx_eft_crb_t {
	union {                    
		uint32_t eft_fc;       
	};
	uint32_t reserved1;        
	union {
		uint64_t csb_address;  
		struct {
			uint32_t reserved2;
			union {
				uint32_t crb_c;
				 

				uint32_t crb_at;
				 

			};
		};
	};
	struct nx_dde_t source_dde;            
	struct nx_dde_t target_dde;            
	struct nx_ccb_t ccb;                   
	union {
		union nx_qw_t reserved64[3];      
	};
	struct nx_csb_t csb;
} __aligned(128);

 

#define EFT_FC_MASK                 size_mask(3)
#define EFT_FC_OFFSET               31
#define EFT_FC_COMPRESS             0x0
#define EFT_FC_COMPRESS_WITH_CRC    0x1
#define EFT_FC_DECOMPRESS           0x2
#define EFT_FC_DECOMPRESS_WITH_CRC  0x3
#define EFT_FC_BLK_DATA_MOVE        0x4
#endif  

#endif  
