
 

#define pr_fmt(fmt)  "[nandsim]" fmt

#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/math64.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

 
#if !defined(CONFIG_NANDSIM_FIRST_ID_BYTE)  || \
    !defined(CONFIG_NANDSIM_SECOND_ID_BYTE) || \
    !defined(CONFIG_NANDSIM_THIRD_ID_BYTE)  || \
    !defined(CONFIG_NANDSIM_FOURTH_ID_BYTE)
#define CONFIG_NANDSIM_FIRST_ID_BYTE  0x98
#define CONFIG_NANDSIM_SECOND_ID_BYTE 0x39
#define CONFIG_NANDSIM_THIRD_ID_BYTE  0xFF  
#define CONFIG_NANDSIM_FOURTH_ID_BYTE 0xFF  
#endif

#ifndef CONFIG_NANDSIM_ACCESS_DELAY
#define CONFIG_NANDSIM_ACCESS_DELAY 25
#endif
#ifndef CONFIG_NANDSIM_PROGRAMM_DELAY
#define CONFIG_NANDSIM_PROGRAMM_DELAY 200
#endif
#ifndef CONFIG_NANDSIM_ERASE_DELAY
#define CONFIG_NANDSIM_ERASE_DELAY 2
#endif
#ifndef CONFIG_NANDSIM_OUTPUT_CYCLE
#define CONFIG_NANDSIM_OUTPUT_CYCLE 40
#endif
#ifndef CONFIG_NANDSIM_INPUT_CYCLE
#define CONFIG_NANDSIM_INPUT_CYCLE  50
#endif
#ifndef CONFIG_NANDSIM_BUS_WIDTH
#define CONFIG_NANDSIM_BUS_WIDTH  8
#endif
#ifndef CONFIG_NANDSIM_DO_DELAYS
#define CONFIG_NANDSIM_DO_DELAYS  0
#endif
#ifndef CONFIG_NANDSIM_LOG
#define CONFIG_NANDSIM_LOG        0
#endif
#ifndef CONFIG_NANDSIM_DBG
#define CONFIG_NANDSIM_DBG        0
#endif
#ifndef CONFIG_NANDSIM_MAX_PARTS
#define CONFIG_NANDSIM_MAX_PARTS  32
#endif

static uint access_delay   = CONFIG_NANDSIM_ACCESS_DELAY;
static uint programm_delay = CONFIG_NANDSIM_PROGRAMM_DELAY;
static uint erase_delay    = CONFIG_NANDSIM_ERASE_DELAY;
static uint output_cycle   = CONFIG_NANDSIM_OUTPUT_CYCLE;
static uint input_cycle    = CONFIG_NANDSIM_INPUT_CYCLE;
static uint bus_width      = CONFIG_NANDSIM_BUS_WIDTH;
static uint do_delays      = CONFIG_NANDSIM_DO_DELAYS;
static uint log            = CONFIG_NANDSIM_LOG;
static uint dbg            = CONFIG_NANDSIM_DBG;
static unsigned long parts[CONFIG_NANDSIM_MAX_PARTS];
static unsigned int parts_num;
static char *badblocks = NULL;
static char *weakblocks = NULL;
static char *weakpages = NULL;
static unsigned int bitflips = 0;
static char *gravepages = NULL;
static unsigned int overridesize = 0;
static char *cache_file = NULL;
static unsigned int bbt;
static unsigned int bch;
static u_char id_bytes[8] = {
	[0] = CONFIG_NANDSIM_FIRST_ID_BYTE,
	[1] = CONFIG_NANDSIM_SECOND_ID_BYTE,
	[2] = CONFIG_NANDSIM_THIRD_ID_BYTE,
	[3] = CONFIG_NANDSIM_FOURTH_ID_BYTE,
	[4 ... 7] = 0xFF,
};

module_param_array(id_bytes, byte, NULL, 0400);
module_param_named(first_id_byte, id_bytes[0], byte, 0400);
module_param_named(second_id_byte, id_bytes[1], byte, 0400);
module_param_named(third_id_byte, id_bytes[2], byte, 0400);
module_param_named(fourth_id_byte, id_bytes[3], byte, 0400);
module_param(access_delay,   uint, 0400);
module_param(programm_delay, uint, 0400);
module_param(erase_delay,    uint, 0400);
module_param(output_cycle,   uint, 0400);
module_param(input_cycle,    uint, 0400);
module_param(bus_width,      uint, 0400);
module_param(do_delays,      uint, 0400);
module_param(log,            uint, 0400);
module_param(dbg,            uint, 0400);
module_param_array(parts, ulong, &parts_num, 0400);
module_param(badblocks,      charp, 0400);
module_param(weakblocks,     charp, 0400);
module_param(weakpages,      charp, 0400);
module_param(bitflips,       uint, 0400);
module_param(gravepages,     charp, 0400);
module_param(overridesize,   uint, 0400);
module_param(cache_file,     charp, 0400);
module_param(bbt,	     uint, 0400);
module_param(bch,	     uint, 0400);

MODULE_PARM_DESC(id_bytes,       "The ID bytes returned by NAND Flash 'read ID' command");
MODULE_PARM_DESC(first_id_byte,  "The first byte returned by NAND Flash 'read ID' command (manufacturer ID) (obsolete)");
MODULE_PARM_DESC(second_id_byte, "The second byte returned by NAND Flash 'read ID' command (chip ID) (obsolete)");
MODULE_PARM_DESC(third_id_byte,  "The third byte returned by NAND Flash 'read ID' command (obsolete)");
MODULE_PARM_DESC(fourth_id_byte, "The fourth byte returned by NAND Flash 'read ID' command (obsolete)");
MODULE_PARM_DESC(access_delay,   "Initial page access delay (microseconds)");
MODULE_PARM_DESC(programm_delay, "Page programm delay (microseconds");
MODULE_PARM_DESC(erase_delay,    "Sector erase delay (milliseconds)");
MODULE_PARM_DESC(output_cycle,   "Word output (from flash) time (nanoseconds)");
MODULE_PARM_DESC(input_cycle,    "Word input (to flash) time (nanoseconds)");
MODULE_PARM_DESC(bus_width,      "Chip's bus width (8- or 16-bit)");
MODULE_PARM_DESC(do_delays,      "Simulate NAND delays using busy-waits if not zero");
MODULE_PARM_DESC(log,            "Perform logging if not zero");
MODULE_PARM_DESC(dbg,            "Output debug information if not zero");
MODULE_PARM_DESC(parts,          "Partition sizes (in erase blocks) separated by commas");
 
MODULE_PARM_DESC(badblocks,      "Erase blocks that are initially marked bad, separated by commas");
MODULE_PARM_DESC(weakblocks,     "Weak erase blocks [: remaining erase cycles (defaults to 3)]"
				 " separated by commas e.g. 113:2 means eb 113"
				 " can be erased only twice before failing");
MODULE_PARM_DESC(weakpages,      "Weak pages [: maximum writes (defaults to 3)]"
				 " separated by commas e.g. 1401:2 means page 1401"
				 " can be written only twice before failing");
MODULE_PARM_DESC(bitflips,       "Maximum number of random bit flips per page (zero by default)");
MODULE_PARM_DESC(gravepages,     "Pages that lose data [: maximum reads (defaults to 3)]"
				 " separated by commas e.g. 1401:2 means page 1401"
				 " can be read only twice before failing");
MODULE_PARM_DESC(overridesize,   "Specifies the NAND Flash size overriding the ID bytes. "
				 "The size is specified in erase blocks and as the exponent of a power of two"
				 " e.g. 5 means a size of 32 erase blocks");
MODULE_PARM_DESC(cache_file,     "File to use to cache nand pages instead of memory");
MODULE_PARM_DESC(bbt,		 "0 OOB, 1 BBT with marker in OOB, 2 BBT with marker in data area");
MODULE_PARM_DESC(bch,		 "Enable BCH ecc and set how many bits should "
				 "be correctable in 512-byte blocks");

 
#define NS_LARGEST_PAGE_SIZE	4096

 
#define NS_LOG(args...) \
	do { if (log) pr_debug(" log: " args); } while(0)
#define NS_DBG(args...) \
	do { if (dbg) pr_debug(" debug: " args); } while(0)
#define NS_WARN(args...) \
	do { pr_warn(" warning: " args); } while(0)
#define NS_ERR(args...) \
	do { pr_err(" error: " args); } while(0)
#define NS_INFO(args...) \
	do { pr_info(" " args); } while(0)

 
#define NS_UDELAY(us) \
        do { if (do_delays) udelay(us); } while(0)
#define NS_MDELAY(us) \
        do { if (do_delays) mdelay(us); } while(0)

 
#define NS_IS_INITIALIZED(ns) ((ns)->geom.totsz != 0)

 
#define NS_STATUS_OK(ns) (NAND_STATUS_READY | (NAND_STATUS_WP * ((ns)->lines.wp == 0)))

 
#define NS_STATUS_FAILED(ns) (NAND_STATUS_FAIL | NS_STATUS_OK(ns))

 
#define NS_RAW_OFFSET(ns) \
	(((ns)->regs.row * (ns)->geom.pgszoob) + (ns)->regs.column)

 
#define NS_RAW_OFFSET_OOB(ns) (NS_RAW_OFFSET(ns) + ns->geom.pgsz)

 
#define NS_PAGE_BYTE_SHIFT(ns) ((ns)->regs.column + (ns)->regs.off)

 
#define STATE_CMD_READ0        0x00000001  
#define STATE_CMD_READ1        0x00000002  
#define STATE_CMD_READSTART    0x00000003  
#define STATE_CMD_PAGEPROG     0x00000004  
#define STATE_CMD_READOOB      0x00000005  
#define STATE_CMD_ERASE1       0x00000006  
#define STATE_CMD_STATUS       0x00000007  
#define STATE_CMD_SEQIN        0x00000009  
#define STATE_CMD_READID       0x0000000A  
#define STATE_CMD_ERASE2       0x0000000B  
#define STATE_CMD_RESET        0x0000000C  
#define STATE_CMD_RNDOUT       0x0000000D  
#define STATE_CMD_RNDOUTSTART  0x0000000E  
#define STATE_CMD_MASK         0x0000000F  

 
#define STATE_ADDR_PAGE        0x00000010  
#define STATE_ADDR_SEC         0x00000020  
#define STATE_ADDR_COLUMN      0x00000030  
#define STATE_ADDR_ZERO        0x00000040  
#define STATE_ADDR_MASK        0x00000070  

 
#define STATE_DATAIN           0x00000100  
#define STATE_DATAIN_MASK      0x00000100  

#define STATE_DATAOUT          0x00001000  
#define STATE_DATAOUT_ID       0x00002000  
#define STATE_DATAOUT_STATUS   0x00003000  
#define STATE_DATAOUT_MASK     0x00007000  

 
#define STATE_READY            0x00000000

 
#define STATE_UNKNOWN          0x10000000

 
#define ACTION_CPY       0x00100000  
#define ACTION_PRGPAGE   0x00200000  
#define ACTION_SECERASE  0x00300000  
#define ACTION_ZEROOFF   0x00400000  
#define ACTION_HALFOFF   0x00500000  
#define ACTION_OOBOFF    0x00600000  
#define ACTION_MASK      0x00700000  

#define NS_OPER_NUM      13  
#define NS_OPER_STATES   6   

#define OPT_ANY          0xFFFFFFFF  
#define OPT_PAGE512      0x00000002  
#define OPT_PAGE2048     0x00000008  
#define OPT_PAGE512_8BIT 0x00000040  
#define OPT_PAGE4096     0x00000080  
#define OPT_LARGEPAGE    (OPT_PAGE2048 | OPT_PAGE4096)  
#define OPT_SMALLPAGE    (OPT_PAGE512)  

 
#define NS_STATE(x) ((x) & ~ACTION_MASK)

 
#define NS_MAX_PREVSTATES 1

 
#define NS_MAX_HELD_PAGES 16

 
union ns_mem {
	u_char *byte;     
	uint16_t *word;   
};

 
struct nandsim {
	struct nand_chip chip;
	struct nand_controller base;
	struct mtd_partition partitions[CONFIG_NANDSIM_MAX_PARTS];
	unsigned int nbparts;

	uint busw;               
	u_char ids[8];           
	uint32_t options;        
	uint32_t state;          
	uint32_t nxstate;        

	uint32_t *op;            
	uint32_t pstates[NS_MAX_PREVSTATES];  
	uint16_t npstates;       
	uint16_t stateidx;       

	 
	union ns_mem *pages;

	 
	struct kmem_cache *nand_pages_slab;

	 
	union ns_mem buf;

	 
	struct {
		uint64_t totsz;      
		uint32_t secsz;      
		uint pgsz;           
		uint oobsz;          
		uint64_t totszoob;   
		uint pgszoob;        
		uint secszoob;       
		uint pgnum;          
		uint pgsec;          
		uint secshift;       
		uint pgshift;        
		uint pgaddrbytes;    
		uint secaddrbytes;   
		uint idbytes;        
	} geom;

	 
	struct {
		unsigned command;  
		u_char   status;   
		uint     row;      
		uint     column;   
		uint     count;    
		uint     num;      
		uint     off;      
	} regs;

	 
        struct {
                int ce;   
                int cle;  
                int ale;  
                int wp;   
        } lines;

	 
	struct file *cfile;  
	unsigned long *pages_written;  
	void *file_buf;
	struct page *held_pages[NS_MAX_HELD_PAGES];
	int held_cnt;

	 
	struct dentry *dent;
};

 
static struct nandsim_operations {
	uint32_t reqopts;   
	uint32_t states[NS_OPER_STATES];  
} ops[NS_OPER_NUM] = {
	 
	{OPT_SMALLPAGE, {STATE_CMD_READ0 | ACTION_ZEROOFF, STATE_ADDR_PAGE | ACTION_CPY,
			STATE_DATAOUT, STATE_READY}},
	 
	{OPT_PAGE512_8BIT, {STATE_CMD_READ1 | ACTION_HALFOFF, STATE_ADDR_PAGE | ACTION_CPY,
			STATE_DATAOUT, STATE_READY}},
	 
	{OPT_SMALLPAGE, {STATE_CMD_READOOB | ACTION_OOBOFF, STATE_ADDR_PAGE | ACTION_CPY,
			STATE_DATAOUT, STATE_READY}},
	 
	{OPT_ANY, {STATE_CMD_SEQIN, STATE_ADDR_PAGE, STATE_DATAIN,
			STATE_CMD_PAGEPROG | ACTION_PRGPAGE, STATE_READY}},
	 
	{OPT_SMALLPAGE, {STATE_CMD_READ0, STATE_CMD_SEQIN | ACTION_ZEROOFF, STATE_ADDR_PAGE,
			      STATE_DATAIN, STATE_CMD_PAGEPROG | ACTION_PRGPAGE, STATE_READY}},
	 
	{OPT_PAGE512, {STATE_CMD_READ1, STATE_CMD_SEQIN | ACTION_HALFOFF, STATE_ADDR_PAGE,
			      STATE_DATAIN, STATE_CMD_PAGEPROG | ACTION_PRGPAGE, STATE_READY}},
	 
	{OPT_SMALLPAGE, {STATE_CMD_READOOB, STATE_CMD_SEQIN | ACTION_OOBOFF, STATE_ADDR_PAGE,
			      STATE_DATAIN, STATE_CMD_PAGEPROG | ACTION_PRGPAGE, STATE_READY}},
	 
	{OPT_ANY, {STATE_CMD_ERASE1, STATE_ADDR_SEC, STATE_CMD_ERASE2 | ACTION_SECERASE, STATE_READY}},
	 
	{OPT_ANY, {STATE_CMD_STATUS, STATE_DATAOUT_STATUS, STATE_READY}},
	 
	{OPT_ANY, {STATE_CMD_READID, STATE_ADDR_ZERO, STATE_DATAOUT_ID, STATE_READY}},
	 
	{OPT_LARGEPAGE, {STATE_CMD_READ0, STATE_ADDR_PAGE, STATE_CMD_READSTART | ACTION_CPY,
			       STATE_DATAOUT, STATE_READY}},
	 
	{OPT_LARGEPAGE, {STATE_CMD_RNDOUT, STATE_ADDR_COLUMN, STATE_CMD_RNDOUTSTART | ACTION_CPY,
			       STATE_DATAOUT, STATE_READY}},
};

struct weak_block {
	struct list_head list;
	unsigned int erase_block_no;
	unsigned int max_erases;
	unsigned int erases_done;
};

static LIST_HEAD(weak_blocks);

struct weak_page {
	struct list_head list;
	unsigned int page_no;
	unsigned int max_writes;
	unsigned int writes_done;
};

static LIST_HEAD(weak_pages);

struct grave_page {
	struct list_head list;
	unsigned int page_no;
	unsigned int max_reads;
	unsigned int reads_done;
};

static LIST_HEAD(grave_pages);

static unsigned long *erase_block_wear = NULL;
static unsigned int wear_eb_count = 0;
static unsigned long total_wear = 0;

 
static struct mtd_info *nsmtd;

static int ns_show(struct seq_file *m, void *private)
{
	unsigned long wmin = -1, wmax = 0, avg;
	unsigned long deciles[10], decile_max[10], tot = 0;
	unsigned int i;

	 
	for (i = 0; i < wear_eb_count; ++i) {
		unsigned long wear = erase_block_wear[i];
		if (wear < wmin)
			wmin = wear;
		if (wear > wmax)
			wmax = wear;
		tot += wear;
	}

	for (i = 0; i < 9; ++i) {
		deciles[i] = 0;
		decile_max[i] = (wmax * (i + 1) + 5) / 10;
	}
	deciles[9] = 0;
	decile_max[9] = wmax;
	for (i = 0; i < wear_eb_count; ++i) {
		int d;
		unsigned long wear = erase_block_wear[i];
		for (d = 0; d < 10; ++d)
			if (wear <= decile_max[d]) {
				deciles[d] += 1;
				break;
			}
	}
	avg = tot / wear_eb_count;

	 
	seq_printf(m, "Total numbers of erases:  %lu\n", tot);
	seq_printf(m, "Number of erase blocks:   %u\n", wear_eb_count);
	seq_printf(m, "Average number of erases: %lu\n", avg);
	seq_printf(m, "Maximum number of erases: %lu\n", wmax);
	seq_printf(m, "Minimum number of erases: %lu\n", wmin);
	for (i = 0; i < 10; ++i) {
		unsigned long from = (i ? decile_max[i - 1] + 1 : 0);
		if (from > decile_max[i])
			continue;
		seq_printf(m, "Number of ebs with erase counts from %lu to %lu : %lu\n",
			from,
			decile_max[i],
			deciles[i]);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(ns);

 
static int ns_debugfs_create(struct nandsim *ns)
{
	struct dentry *root = nsmtd->dbg.dfs_dir;

	 
	if (IS_ERR_OR_NULL(root)) {
		if (IS_ENABLED(CONFIG_DEBUG_FS) &&
		    !IS_ENABLED(CONFIG_MTD_PARTITIONED_MASTER))
			NS_WARN("CONFIG_MTD_PARTITIONED_MASTER must be enabled to expose debugfs stuff\n");
		return 0;
	}

	ns->dent = debugfs_create_file("nandsim_wear_report", 0400, root, ns,
				       &ns_fops);
	if (IS_ERR_OR_NULL(ns->dent)) {
		NS_ERR("cannot create \"nandsim_wear_report\" debugfs entry\n");
		return -1;
	}

	return 0;
}

static void ns_debugfs_remove(struct nandsim *ns)
{
	debugfs_remove_recursive(ns->dent);
}

 
static int __init ns_alloc_device(struct nandsim *ns)
{
	struct file *cfile;
	int i, err;

	if (cache_file) {
		cfile = filp_open(cache_file, O_CREAT | O_RDWR | O_LARGEFILE, 0600);
		if (IS_ERR(cfile))
			return PTR_ERR(cfile);
		if (!(cfile->f_mode & FMODE_CAN_READ)) {
			NS_ERR("alloc_device: cache file not readable\n");
			err = -EINVAL;
			goto err_close_filp;
		}
		if (!(cfile->f_mode & FMODE_CAN_WRITE)) {
			NS_ERR("alloc_device: cache file not writeable\n");
			err = -EINVAL;
			goto err_close_filp;
		}
		ns->pages_written =
			vzalloc(array_size(sizeof(unsigned long),
					   BITS_TO_LONGS(ns->geom.pgnum)));
		if (!ns->pages_written) {
			NS_ERR("alloc_device: unable to allocate pages written array\n");
			err = -ENOMEM;
			goto err_close_filp;
		}
		ns->file_buf = kmalloc(ns->geom.pgszoob, GFP_KERNEL);
		if (!ns->file_buf) {
			NS_ERR("alloc_device: unable to allocate file buf\n");
			err = -ENOMEM;
			goto err_free_pw;
		}
		ns->cfile = cfile;

		return 0;

err_free_pw:
		vfree(ns->pages_written);
err_close_filp:
		filp_close(cfile, NULL);

		return err;
	}

	ns->pages = vmalloc(array_size(sizeof(union ns_mem), ns->geom.pgnum));
	if (!ns->pages) {
		NS_ERR("alloc_device: unable to allocate page array\n");
		return -ENOMEM;
	}
	for (i = 0; i < ns->geom.pgnum; i++) {
		ns->pages[i].byte = NULL;
	}
	ns->nand_pages_slab = kmem_cache_create("nandsim",
						ns->geom.pgszoob, 0, 0, NULL);
	if (!ns->nand_pages_slab) {
		NS_ERR("cache_create: unable to create kmem_cache\n");
		err = -ENOMEM;
		goto err_free_pg;
	}

	return 0;

err_free_pg:
	vfree(ns->pages);

	return err;
}

 
static void ns_free_device(struct nandsim *ns)
{
	int i;

	if (ns->cfile) {
		kfree(ns->file_buf);
		vfree(ns->pages_written);
		filp_close(ns->cfile, NULL);
		return;
	}

	if (ns->pages) {
		for (i = 0; i < ns->geom.pgnum; i++) {
			if (ns->pages[i].byte)
				kmem_cache_free(ns->nand_pages_slab,
						ns->pages[i].byte);
		}
		kmem_cache_destroy(ns->nand_pages_slab);
		vfree(ns->pages);
	}
}

static char __init *ns_get_partition_name(int i)
{
	return kasprintf(GFP_KERNEL, "NAND simulator partition %d", i);
}

 
static int __init ns_init(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	struct nandsim   *ns   = nand_get_controller_data(chip);
	int i, ret = 0;
	uint64_t remains;
	uint64_t next_offset;

	if (NS_IS_INITIALIZED(ns)) {
		NS_ERR("init_nandsim: nandsim is already initialized\n");
		return -EIO;
	}

	 
	ns->busw = chip->options & NAND_BUSWIDTH_16 ? 16 : 8;
	ns->geom.totsz    = mtd->size;
	ns->geom.pgsz     = mtd->writesize;
	ns->geom.oobsz    = mtd->oobsize;
	ns->geom.secsz    = mtd->erasesize;
	ns->geom.pgszoob  = ns->geom.pgsz + ns->geom.oobsz;
	ns->geom.pgnum    = div_u64(ns->geom.totsz, ns->geom.pgsz);
	ns->geom.totszoob = ns->geom.totsz + (uint64_t)ns->geom.pgnum * ns->geom.oobsz;
	ns->geom.secshift = ffs(ns->geom.secsz) - 1;
	ns->geom.pgshift  = chip->page_shift;
	ns->geom.pgsec    = ns->geom.secsz / ns->geom.pgsz;
	ns->geom.secszoob = ns->geom.secsz + ns->geom.oobsz * ns->geom.pgsec;
	ns->options = 0;

	if (ns->geom.pgsz == 512) {
		ns->options |= OPT_PAGE512;
		if (ns->busw == 8)
			ns->options |= OPT_PAGE512_8BIT;
	} else if (ns->geom.pgsz == 2048) {
		ns->options |= OPT_PAGE2048;
	} else if (ns->geom.pgsz == 4096) {
		ns->options |= OPT_PAGE4096;
	} else {
		NS_ERR("init_nandsim: unknown page size %u\n", ns->geom.pgsz);
		return -EIO;
	}

	if (ns->options & OPT_SMALLPAGE) {
		if (ns->geom.totsz <= (32 << 20)) {
			ns->geom.pgaddrbytes  = 3;
			ns->geom.secaddrbytes = 2;
		} else {
			ns->geom.pgaddrbytes  = 4;
			ns->geom.secaddrbytes = 3;
		}
	} else {
		if (ns->geom.totsz <= (128 << 20)) {
			ns->geom.pgaddrbytes  = 4;
			ns->geom.secaddrbytes = 2;
		} else {
			ns->geom.pgaddrbytes  = 5;
			ns->geom.secaddrbytes = 3;
		}
	}

	 
	if (parts_num > ARRAY_SIZE(ns->partitions)) {
		NS_ERR("too many partitions.\n");
		return -EINVAL;
	}
	remains = ns->geom.totsz;
	next_offset = 0;
	for (i = 0; i < parts_num; ++i) {
		uint64_t part_sz = (uint64_t)parts[i] * ns->geom.secsz;

		if (!part_sz || part_sz > remains) {
			NS_ERR("bad partition size.\n");
			return -EINVAL;
		}
		ns->partitions[i].name = ns_get_partition_name(i);
		if (!ns->partitions[i].name) {
			NS_ERR("unable to allocate memory.\n");
			return -ENOMEM;
		}
		ns->partitions[i].offset = next_offset;
		ns->partitions[i].size   = part_sz;
		next_offset += ns->partitions[i].size;
		remains -= ns->partitions[i].size;
	}
	ns->nbparts = parts_num;
	if (remains) {
		if (parts_num + 1 > ARRAY_SIZE(ns->partitions)) {
			NS_ERR("too many partitions.\n");
			ret = -EINVAL;
			goto free_partition_names;
		}
		ns->partitions[i].name = ns_get_partition_name(i);
		if (!ns->partitions[i].name) {
			NS_ERR("unable to allocate memory.\n");
			ret = -ENOMEM;
			goto free_partition_names;
		}
		ns->partitions[i].offset = next_offset;
		ns->partitions[i].size   = remains;
		ns->nbparts += 1;
	}

	if (ns->busw == 16)
		NS_WARN("16-bit flashes support wasn't tested\n");

	printk("flash size: %llu MiB\n",
			(unsigned long long)ns->geom.totsz >> 20);
	printk("page size: %u bytes\n",         ns->geom.pgsz);
	printk("OOB area size: %u bytes\n",     ns->geom.oobsz);
	printk("sector size: %u KiB\n",         ns->geom.secsz >> 10);
	printk("pages number: %u\n",            ns->geom.pgnum);
	printk("pages per sector: %u\n",        ns->geom.pgsec);
	printk("bus width: %u\n",               ns->busw);
	printk("bits in sector size: %u\n",     ns->geom.secshift);
	printk("bits in page size: %u\n",       ns->geom.pgshift);
	printk("bits in OOB size: %u\n",	ffs(ns->geom.oobsz) - 1);
	printk("flash size with OOB: %llu KiB\n",
			(unsigned long long)ns->geom.totszoob >> 10);
	printk("page address bytes: %u\n",      ns->geom.pgaddrbytes);
	printk("sector address bytes: %u\n",    ns->geom.secaddrbytes);
	printk("options: %#x\n",                ns->options);

	ret = ns_alloc_device(ns);
	if (ret)
		goto free_partition_names;

	 
	ns->buf.byte = kmalloc(ns->geom.pgszoob, GFP_KERNEL);
	if (!ns->buf.byte) {
		NS_ERR("init_nandsim: unable to allocate %u bytes for the internal buffer\n",
			ns->geom.pgszoob);
		ret = -ENOMEM;
		goto free_device;
	}
	memset(ns->buf.byte, 0xFF, ns->geom.pgszoob);

	return 0;

free_device:
	ns_free_device(ns);
free_partition_names:
	for (i = 0; i < ARRAY_SIZE(ns->partitions); ++i)
		kfree(ns->partitions[i].name);

	return ret;
}

 
static void ns_free(struct nandsim *ns)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ns->partitions); ++i)
		kfree(ns->partitions[i].name);

	kfree(ns->buf.byte);
	ns_free_device(ns);

	return;
}

static int ns_parse_badblocks(struct nandsim *ns, struct mtd_info *mtd)
{
	char *w;
	int zero_ok;
	unsigned int erase_block_no;
	loff_t offset;

	if (!badblocks)
		return 0;
	w = badblocks;
	do {
		zero_ok = (*w == '0' ? 1 : 0);
		erase_block_no = simple_strtoul(w, &w, 0);
		if (!zero_ok && !erase_block_no) {
			NS_ERR("invalid badblocks.\n");
			return -EINVAL;
		}
		offset = (loff_t)erase_block_no * ns->geom.secsz;
		if (mtd_block_markbad(mtd, offset)) {
			NS_ERR("invalid badblocks.\n");
			return -EINVAL;
		}
		if (*w == ',')
			w += 1;
	} while (*w);
	return 0;
}

static int ns_parse_weakblocks(void)
{
	char *w;
	int zero_ok;
	unsigned int erase_block_no;
	unsigned int max_erases;
	struct weak_block *wb;

	if (!weakblocks)
		return 0;
	w = weakblocks;
	do {
		zero_ok = (*w == '0' ? 1 : 0);
		erase_block_no = simple_strtoul(w, &w, 0);
		if (!zero_ok && !erase_block_no) {
			NS_ERR("invalid weakblocks.\n");
			return -EINVAL;
		}
		max_erases = 3;
		if (*w == ':') {
			w += 1;
			max_erases = simple_strtoul(w, &w, 0);
		}
		if (*w == ',')
			w += 1;
		wb = kzalloc(sizeof(*wb), GFP_KERNEL);
		if (!wb) {
			NS_ERR("unable to allocate memory.\n");
			return -ENOMEM;
		}
		wb->erase_block_no = erase_block_no;
		wb->max_erases = max_erases;
		list_add(&wb->list, &weak_blocks);
	} while (*w);
	return 0;
}

static int ns_erase_error(unsigned int erase_block_no)
{
	struct weak_block *wb;

	list_for_each_entry(wb, &weak_blocks, list)
		if (wb->erase_block_no == erase_block_no) {
			if (wb->erases_done >= wb->max_erases)
				return 1;
			wb->erases_done += 1;
			return 0;
		}
	return 0;
}

static int ns_parse_weakpages(void)
{
	char *w;
	int zero_ok;
	unsigned int page_no;
	unsigned int max_writes;
	struct weak_page *wp;

	if (!weakpages)
		return 0;
	w = weakpages;
	do {
		zero_ok = (*w == '0' ? 1 : 0);
		page_no = simple_strtoul(w, &w, 0);
		if (!zero_ok && !page_no) {
			NS_ERR("invalid weakpages.\n");
			return -EINVAL;
		}
		max_writes = 3;
		if (*w == ':') {
			w += 1;
			max_writes = simple_strtoul(w, &w, 0);
		}
		if (*w == ',')
			w += 1;
		wp = kzalloc(sizeof(*wp), GFP_KERNEL);
		if (!wp) {
			NS_ERR("unable to allocate memory.\n");
			return -ENOMEM;
		}
		wp->page_no = page_no;
		wp->max_writes = max_writes;
		list_add(&wp->list, &weak_pages);
	} while (*w);
	return 0;
}

static int ns_write_error(unsigned int page_no)
{
	struct weak_page *wp;

	list_for_each_entry(wp, &weak_pages, list)
		if (wp->page_no == page_no) {
			if (wp->writes_done >= wp->max_writes)
				return 1;
			wp->writes_done += 1;
			return 0;
		}
	return 0;
}

static int ns_parse_gravepages(void)
{
	char *g;
	int zero_ok;
	unsigned int page_no;
	unsigned int max_reads;
	struct grave_page *gp;

	if (!gravepages)
		return 0;
	g = gravepages;
	do {
		zero_ok = (*g == '0' ? 1 : 0);
		page_no = simple_strtoul(g, &g, 0);
		if (!zero_ok && !page_no) {
			NS_ERR("invalid gravepagess.\n");
			return -EINVAL;
		}
		max_reads = 3;
		if (*g == ':') {
			g += 1;
			max_reads = simple_strtoul(g, &g, 0);
		}
		if (*g == ',')
			g += 1;
		gp = kzalloc(sizeof(*gp), GFP_KERNEL);
		if (!gp) {
			NS_ERR("unable to allocate memory.\n");
			return -ENOMEM;
		}
		gp->page_no = page_no;
		gp->max_reads = max_reads;
		list_add(&gp->list, &grave_pages);
	} while (*g);
	return 0;
}

static int ns_read_error(unsigned int page_no)
{
	struct grave_page *gp;

	list_for_each_entry(gp, &grave_pages, list)
		if (gp->page_no == page_no) {
			if (gp->reads_done >= gp->max_reads)
				return 1;
			gp->reads_done += 1;
			return 0;
		}
	return 0;
}

static int ns_setup_wear_reporting(struct mtd_info *mtd)
{
	wear_eb_count = div_u64(mtd->size, mtd->erasesize);
	erase_block_wear = kcalloc(wear_eb_count, sizeof(unsigned long), GFP_KERNEL);
	if (!erase_block_wear) {
		NS_ERR("Too many erase blocks for wear reporting\n");
		return -ENOMEM;
	}
	return 0;
}

static void ns_update_wear(unsigned int erase_block_no)
{
	if (!erase_block_wear)
		return;
	total_wear += 1;
	 
	if (total_wear == 0)
		NS_ERR("Erase counter total overflow\n");
	erase_block_wear[erase_block_no] += 1;
	if (erase_block_wear[erase_block_no] == 0)
		NS_ERR("Erase counter overflow for erase block %u\n", erase_block_no);
}

 
static char *ns_get_state_name(uint32_t state)
{
	switch (NS_STATE(state)) {
		case STATE_CMD_READ0:
			return "STATE_CMD_READ0";
		case STATE_CMD_READ1:
			return "STATE_CMD_READ1";
		case STATE_CMD_PAGEPROG:
			return "STATE_CMD_PAGEPROG";
		case STATE_CMD_READOOB:
			return "STATE_CMD_READOOB";
		case STATE_CMD_READSTART:
			return "STATE_CMD_READSTART";
		case STATE_CMD_ERASE1:
			return "STATE_CMD_ERASE1";
		case STATE_CMD_STATUS:
			return "STATE_CMD_STATUS";
		case STATE_CMD_SEQIN:
			return "STATE_CMD_SEQIN";
		case STATE_CMD_READID:
			return "STATE_CMD_READID";
		case STATE_CMD_ERASE2:
			return "STATE_CMD_ERASE2";
		case STATE_CMD_RESET:
			return "STATE_CMD_RESET";
		case STATE_CMD_RNDOUT:
			return "STATE_CMD_RNDOUT";
		case STATE_CMD_RNDOUTSTART:
			return "STATE_CMD_RNDOUTSTART";
		case STATE_ADDR_PAGE:
			return "STATE_ADDR_PAGE";
		case STATE_ADDR_SEC:
			return "STATE_ADDR_SEC";
		case STATE_ADDR_ZERO:
			return "STATE_ADDR_ZERO";
		case STATE_ADDR_COLUMN:
			return "STATE_ADDR_COLUMN";
		case STATE_DATAIN:
			return "STATE_DATAIN";
		case STATE_DATAOUT:
			return "STATE_DATAOUT";
		case STATE_DATAOUT_ID:
			return "STATE_DATAOUT_ID";
		case STATE_DATAOUT_STATUS:
			return "STATE_DATAOUT_STATUS";
		case STATE_READY:
			return "STATE_READY";
		case STATE_UNKNOWN:
			return "STATE_UNKNOWN";
	}

	NS_ERR("get_state_name: unknown state, BUG\n");
	return NULL;
}

 
static int ns_check_command(int cmd)
{
	switch (cmd) {

	case NAND_CMD_READ0:
	case NAND_CMD_READ1:
	case NAND_CMD_READSTART:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_READOOB:
	case NAND_CMD_ERASE1:
	case NAND_CMD_STATUS:
	case NAND_CMD_SEQIN:
	case NAND_CMD_READID:
	case NAND_CMD_ERASE2:
	case NAND_CMD_RESET:
	case NAND_CMD_RNDOUT:
	case NAND_CMD_RNDOUTSTART:
		return 0;

	default:
		return 1;
	}
}

 
static uint32_t ns_get_state_by_command(unsigned command)
{
	switch (command) {
		case NAND_CMD_READ0:
			return STATE_CMD_READ0;
		case NAND_CMD_READ1:
			return STATE_CMD_READ1;
		case NAND_CMD_PAGEPROG:
			return STATE_CMD_PAGEPROG;
		case NAND_CMD_READSTART:
			return STATE_CMD_READSTART;
		case NAND_CMD_READOOB:
			return STATE_CMD_READOOB;
		case NAND_CMD_ERASE1:
			return STATE_CMD_ERASE1;
		case NAND_CMD_STATUS:
			return STATE_CMD_STATUS;
		case NAND_CMD_SEQIN:
			return STATE_CMD_SEQIN;
		case NAND_CMD_READID:
			return STATE_CMD_READID;
		case NAND_CMD_ERASE2:
			return STATE_CMD_ERASE2;
		case NAND_CMD_RESET:
			return STATE_CMD_RESET;
		case NAND_CMD_RNDOUT:
			return STATE_CMD_RNDOUT;
		case NAND_CMD_RNDOUTSTART:
			return STATE_CMD_RNDOUTSTART;
	}

	NS_ERR("get_state_by_command: unknown command, BUG\n");
	return 0;
}

 
static inline void ns_accept_addr_byte(struct nandsim *ns, u_char bt)
{
	uint byte = (uint)bt;

	if (ns->regs.count < (ns->geom.pgaddrbytes - ns->geom.secaddrbytes))
		ns->regs.column |= (byte << 8 * ns->regs.count);
	else {
		ns->regs.row |= (byte << 8 * (ns->regs.count -
						ns->geom.pgaddrbytes +
						ns->geom.secaddrbytes));
	}

	return;
}

 
static inline void ns_switch_to_ready_state(struct nandsim *ns, u_char status)
{
	NS_DBG("switch_to_ready_state: switch to %s state\n",
	       ns_get_state_name(STATE_READY));

	ns->state       = STATE_READY;
	ns->nxstate     = STATE_UNKNOWN;
	ns->op          = NULL;
	ns->npstates    = 0;
	ns->stateidx    = 0;
	ns->regs.num    = 0;
	ns->regs.count  = 0;
	ns->regs.off    = 0;
	ns->regs.row    = 0;
	ns->regs.column = 0;
	ns->regs.status = status;
}

 
static int ns_find_operation(struct nandsim *ns, uint32_t flag)
{
	int opsfound = 0;
	int i, j, idx = 0;

	for (i = 0; i < NS_OPER_NUM; i++) {

		int found = 1;

		if (!(ns->options & ops[i].reqopts))
			 
			continue;

		if (flag) {
			if (!(ops[i].states[ns->npstates] & STATE_ADDR_MASK))
				continue;
		} else {
			if (NS_STATE(ns->state) != NS_STATE(ops[i].states[ns->npstates]))
				continue;
		}

		for (j = 0; j < ns->npstates; j++)
			if (NS_STATE(ops[i].states[j]) != NS_STATE(ns->pstates[j])
				&& (ns->options & ops[idx].reqopts)) {
				found = 0;
				break;
			}

		if (found) {
			idx = i;
			opsfound += 1;
		}
	}

	if (opsfound == 1) {
		 
		ns->op = &ops[idx].states[0];
		if (flag) {
			 
			ns->stateidx = ns->npstates - 1;
		} else {
			ns->stateidx = ns->npstates;
		}
		ns->npstates = 0;
		ns->state = ns->op[ns->stateidx];
		ns->nxstate = ns->op[ns->stateidx + 1];
		NS_DBG("find_operation: operation found, index: %d, state: %s, nxstate %s\n",
		       idx, ns_get_state_name(ns->state),
		       ns_get_state_name(ns->nxstate));
		return 0;
	}

	if (opsfound == 0) {
		 
		if (ns->npstates != 0) {
			NS_DBG("find_operation: no operation found, try again with state %s\n",
			       ns_get_state_name(ns->state));
			ns->npstates = 0;
			return ns_find_operation(ns, 0);

		}
		NS_DBG("find_operation: no operations found\n");
		ns_switch_to_ready_state(ns, NS_STATUS_FAILED(ns));
		return -2;
	}

	if (flag) {
		 
		NS_DBG("find_operation: BUG, operation must be known if address is input\n");
		return -2;
	}

	NS_DBG("find_operation: there is still ambiguity\n");

	ns->pstates[ns->npstates++] = ns->state;

	return -1;
}

static void ns_put_pages(struct nandsim *ns)
{
	int i;

	for (i = 0; i < ns->held_cnt; i++)
		put_page(ns->held_pages[i]);
}

 
static int ns_get_pages(struct nandsim *ns, struct file *file, size_t count,
			loff_t pos)
{
	pgoff_t index, start_index, end_index;
	struct page *page;
	struct address_space *mapping = file->f_mapping;

	start_index = pos >> PAGE_SHIFT;
	end_index = (pos + count - 1) >> PAGE_SHIFT;
	if (end_index - start_index + 1 > NS_MAX_HELD_PAGES)
		return -EINVAL;
	ns->held_cnt = 0;
	for (index = start_index; index <= end_index; index++) {
		page = find_get_page(mapping, index);
		if (page == NULL) {
			page = find_or_create_page(mapping, index, GFP_NOFS);
			if (page == NULL) {
				write_inode_now(mapping->host, 1);
				page = find_or_create_page(mapping, index, GFP_NOFS);
			}
			if (page == NULL) {
				ns_put_pages(ns);
				return -ENOMEM;
			}
			unlock_page(page);
		}
		ns->held_pages[ns->held_cnt++] = page;
	}
	return 0;
}

static ssize_t ns_read_file(struct nandsim *ns, struct file *file, void *buf,
			    size_t count, loff_t pos)
{
	ssize_t tx;
	int err;
	unsigned int noreclaim_flag;

	err = ns_get_pages(ns, file, count, pos);
	if (err)
		return err;
	noreclaim_flag = memalloc_noreclaim_save();
	tx = kernel_read(file, buf, count, &pos);
	memalloc_noreclaim_restore(noreclaim_flag);
	ns_put_pages(ns);
	return tx;
}

static ssize_t ns_write_file(struct nandsim *ns, struct file *file, void *buf,
			     size_t count, loff_t pos)
{
	ssize_t tx;
	int err;
	unsigned int noreclaim_flag;

	err = ns_get_pages(ns, file, count, pos);
	if (err)
		return err;
	noreclaim_flag = memalloc_noreclaim_save();
	tx = kernel_write(file, buf, count, &pos);
	memalloc_noreclaim_restore(noreclaim_flag);
	ns_put_pages(ns);
	return tx;
}

 
static inline union ns_mem *NS_GET_PAGE(struct nandsim *ns)
{
	return &(ns->pages[ns->regs.row]);
}

 
static inline u_char *NS_PAGE_BYTE_OFF(struct nandsim *ns)
{
	return NS_GET_PAGE(ns)->byte + NS_PAGE_BYTE_SHIFT(ns);
}

static int ns_do_read_error(struct nandsim *ns, int num)
{
	unsigned int page_no = ns->regs.row;

	if (ns_read_error(page_no)) {
		get_random_bytes(ns->buf.byte, num);
		NS_WARN("simulating read error in page %u\n", page_no);
		return 1;
	}
	return 0;
}

static void ns_do_bit_flips(struct nandsim *ns, int num)
{
	if (bitflips && get_random_u16() < (1 << 6)) {
		int flips = 1;
		if (bitflips > 1)
			flips = get_random_u32_inclusive(1, bitflips);
		while (flips--) {
			int pos = get_random_u32_below(num * 8);
			ns->buf.byte[pos / 8] ^= (1 << (pos % 8));
			NS_WARN("read_page: flipping bit %d in page %d "
				"reading from %d ecc: corrected=%u failed=%u\n",
				pos, ns->regs.row, NS_PAGE_BYTE_SHIFT(ns),
				nsmtd->ecc_stats.corrected, nsmtd->ecc_stats.failed);
		}
	}
}

 
static void ns_read_page(struct nandsim *ns, int num)
{
	union ns_mem *mypage;

	if (ns->cfile) {
		if (!test_bit(ns->regs.row, ns->pages_written)) {
			NS_DBG("read_page: page %d not written\n", ns->regs.row);
			memset(ns->buf.byte, 0xFF, num);
		} else {
			loff_t pos;
			ssize_t tx;

			NS_DBG("read_page: page %d written, reading from %d\n",
				ns->regs.row, NS_PAGE_BYTE_SHIFT(ns));
			if (ns_do_read_error(ns, num))
				return;
			pos = (loff_t)NS_RAW_OFFSET(ns) + ns->regs.off;
			tx = ns_read_file(ns, ns->cfile, ns->buf.byte, num,
					  pos);
			if (tx != num) {
				NS_ERR("read_page: read error for page %d ret %ld\n", ns->regs.row, (long)tx);
				return;
			}
			ns_do_bit_flips(ns, num);
		}
		return;
	}

	mypage = NS_GET_PAGE(ns);
	if (mypage->byte == NULL) {
		NS_DBG("read_page: page %d not allocated\n", ns->regs.row);
		memset(ns->buf.byte, 0xFF, num);
	} else {
		NS_DBG("read_page: page %d allocated, reading from %d\n",
			ns->regs.row, NS_PAGE_BYTE_SHIFT(ns));
		if (ns_do_read_error(ns, num))
			return;
		memcpy(ns->buf.byte, NS_PAGE_BYTE_OFF(ns), num);
		ns_do_bit_flips(ns, num);
	}
}

 
static void ns_erase_sector(struct nandsim *ns)
{
	union ns_mem *mypage;
	int i;

	if (ns->cfile) {
		for (i = 0; i < ns->geom.pgsec; i++)
			if (__test_and_clear_bit(ns->regs.row + i,
						 ns->pages_written)) {
				NS_DBG("erase_sector: freeing page %d\n", ns->regs.row + i);
			}
		return;
	}

	mypage = NS_GET_PAGE(ns);
	for (i = 0; i < ns->geom.pgsec; i++) {
		if (mypage->byte != NULL) {
			NS_DBG("erase_sector: freeing page %d\n", ns->regs.row+i);
			kmem_cache_free(ns->nand_pages_slab, mypage->byte);
			mypage->byte = NULL;
		}
		mypage++;
	}
}

 
static int ns_prog_page(struct nandsim *ns, int num)
{
	int i;
	union ns_mem *mypage;
	u_char *pg_off;

	if (ns->cfile) {
		loff_t off;
		ssize_t tx;
		int all;

		NS_DBG("prog_page: writing page %d\n", ns->regs.row);
		pg_off = ns->file_buf + NS_PAGE_BYTE_SHIFT(ns);
		off = (loff_t)NS_RAW_OFFSET(ns) + ns->regs.off;
		if (!test_bit(ns->regs.row, ns->pages_written)) {
			all = 1;
			memset(ns->file_buf, 0xff, ns->geom.pgszoob);
		} else {
			all = 0;
			tx = ns_read_file(ns, ns->cfile, pg_off, num, off);
			if (tx != num) {
				NS_ERR("prog_page: read error for page %d ret %ld\n", ns->regs.row, (long)tx);
				return -1;
			}
		}
		for (i = 0; i < num; i++)
			pg_off[i] &= ns->buf.byte[i];
		if (all) {
			loff_t pos = (loff_t)ns->regs.row * ns->geom.pgszoob;
			tx = ns_write_file(ns, ns->cfile, ns->file_buf,
					   ns->geom.pgszoob, pos);
			if (tx != ns->geom.pgszoob) {
				NS_ERR("prog_page: write error for page %d ret %ld\n", ns->regs.row, (long)tx);
				return -1;
			}
			__set_bit(ns->regs.row, ns->pages_written);
		} else {
			tx = ns_write_file(ns, ns->cfile, pg_off, num, off);
			if (tx != num) {
				NS_ERR("prog_page: write error for page %d ret %ld\n", ns->regs.row, (long)tx);
				return -1;
			}
		}
		return 0;
	}

	mypage = NS_GET_PAGE(ns);
	if (mypage->byte == NULL) {
		NS_DBG("prog_page: allocating page %d\n", ns->regs.row);
		 
		mypage->byte = kmem_cache_alloc(ns->nand_pages_slab, GFP_NOFS);
		if (mypage->byte == NULL) {
			NS_ERR("prog_page: error allocating memory for page %d\n", ns->regs.row);
			return -1;
		}
		memset(mypage->byte, 0xFF, ns->geom.pgszoob);
	}

	pg_off = NS_PAGE_BYTE_OFF(ns);
	for (i = 0; i < num; i++)
		pg_off[i] &= ns->buf.byte[i];

	return 0;
}

 
static int ns_do_state_action(struct nandsim *ns, uint32_t action)
{
	int num;
	int busdiv = ns->busw == 8 ? 1 : 2;
	unsigned int erase_block_no, page_no;

	action &= ACTION_MASK;

	 
	if (action != ACTION_SECERASE && ns->regs.row >= ns->geom.pgnum) {
		NS_WARN("do_state_action: wrong page number (%#x)\n", ns->regs.row);
		return -1;
	}

	switch (action) {

	case ACTION_CPY:
		 

		 
		if (ns->regs.column >= (ns->geom.pgszoob - ns->regs.off)) {
			NS_ERR("do_state_action: column number is too large\n");
			break;
		}
		num = ns->geom.pgszoob - NS_PAGE_BYTE_SHIFT(ns);
		ns_read_page(ns, num);

		NS_DBG("do_state_action: (ACTION_CPY:) copy %d bytes to int buf, raw offset %d\n",
			num, NS_RAW_OFFSET(ns) + ns->regs.off);

		if (ns->regs.off == 0)
			NS_LOG("read page %d\n", ns->regs.row);
		else if (ns->regs.off < ns->geom.pgsz)
			NS_LOG("read page %d (second half)\n", ns->regs.row);
		else
			NS_LOG("read OOB of page %d\n", ns->regs.row);

		NS_UDELAY(access_delay);
		NS_UDELAY(input_cycle * ns->geom.pgsz / 1000 / busdiv);

		break;

	case ACTION_SECERASE:
		 

		if (ns->lines.wp) {
			NS_ERR("do_state_action: device is write-protected, ignore sector erase\n");
			return -1;
		}

		if (ns->regs.row >= ns->geom.pgnum - ns->geom.pgsec
			|| (ns->regs.row & ~(ns->geom.secsz - 1))) {
			NS_ERR("do_state_action: wrong sector address (%#x)\n", ns->regs.row);
			return -1;
		}

		ns->regs.row = (ns->regs.row <<
				8 * (ns->geom.pgaddrbytes - ns->geom.secaddrbytes)) | ns->regs.column;
		ns->regs.column = 0;

		erase_block_no = ns->regs.row >> (ns->geom.secshift - ns->geom.pgshift);

		NS_DBG("do_state_action: erase sector at address %#x, off = %d\n",
				ns->regs.row, NS_RAW_OFFSET(ns));
		NS_LOG("erase sector %u\n", erase_block_no);

		ns_erase_sector(ns);

		NS_MDELAY(erase_delay);

		if (erase_block_wear)
			ns_update_wear(erase_block_no);

		if (ns_erase_error(erase_block_no)) {
			NS_WARN("simulating erase failure in erase block %u\n", erase_block_no);
			return -1;
		}

		break;

	case ACTION_PRGPAGE:
		 

		if (ns->lines.wp) {
			NS_WARN("do_state_action: device is write-protected, programm\n");
			return -1;
		}

		num = ns->geom.pgszoob - NS_PAGE_BYTE_SHIFT(ns);
		if (num != ns->regs.count) {
			NS_ERR("do_state_action: too few bytes were input (%d instead of %d)\n",
					ns->regs.count, num);
			return -1;
		}

		if (ns_prog_page(ns, num) == -1)
			return -1;

		page_no = ns->regs.row;

		NS_DBG("do_state_action: copy %d bytes from int buf to (%#x, %#x), raw off = %d\n",
			num, ns->regs.row, ns->regs.column, NS_RAW_OFFSET(ns) + ns->regs.off);
		NS_LOG("programm page %d\n", ns->regs.row);

		NS_UDELAY(programm_delay);
		NS_UDELAY(output_cycle * ns->geom.pgsz / 1000 / busdiv);

		if (ns_write_error(page_no)) {
			NS_WARN("simulating write failure in page %u\n", page_no);
			return -1;
		}

		break;

	case ACTION_ZEROOFF:
		NS_DBG("do_state_action: set internal offset to 0\n");
		ns->regs.off = 0;
		break;

	case ACTION_HALFOFF:
		if (!(ns->options & OPT_PAGE512_8BIT)) {
			NS_ERR("do_state_action: BUG! can't skip half of page for non-512"
				"byte page size 8x chips\n");
			return -1;
		}
		NS_DBG("do_state_action: set internal offset to %d\n", ns->geom.pgsz/2);
		ns->regs.off = ns->geom.pgsz/2;
		break;

	case ACTION_OOBOFF:
		NS_DBG("do_state_action: set internal offset to %d\n", ns->geom.pgsz);
		ns->regs.off = ns->geom.pgsz;
		break;

	default:
		NS_DBG("do_state_action: BUG! unknown action\n");
	}

	return 0;
}

 
static void ns_switch_state(struct nandsim *ns)
{
	if (ns->op) {
		 

		ns->stateidx += 1;
		ns->state = ns->nxstate;
		ns->nxstate = ns->op[ns->stateidx + 1];

		NS_DBG("switch_state: operation is known, switch to the next state, "
			"state: %s, nxstate: %s\n",
		       ns_get_state_name(ns->state),
		       ns_get_state_name(ns->nxstate));
	} else {
		 

		 
		ns->state = ns_get_state_by_command(ns->regs.command);

		NS_DBG("switch_state: operation is unknown, try to find it\n");

		if (ns_find_operation(ns, 0))
			return;
	}

	 
	if ((ns->state & ACTION_MASK) &&
	    ns_do_state_action(ns, ns->state) < 0) {
		ns_switch_to_ready_state(ns, NS_STATUS_FAILED(ns));
		return;
	}

	 
	if ((ns->nxstate & STATE_ADDR_MASK) && ns->busw == 16) {
		NS_DBG("switch_state: double the column number for 16x device\n");
		ns->regs.column <<= 1;
	}

	if (NS_STATE(ns->nxstate) == STATE_READY) {
		 

		u_char status = NS_STATUS_OK(ns);

		 
		if ((ns->state & (STATE_DATAIN_MASK | STATE_DATAOUT_MASK))
			&& ns->regs.count != ns->regs.num) {
			NS_WARN("switch_state: not all bytes were processed, %d left\n",
					ns->regs.num - ns->regs.count);
			status = NS_STATUS_FAILED(ns);
		}

		NS_DBG("switch_state: operation complete, switch to STATE_READY state\n");

		ns_switch_to_ready_state(ns, status);

		return;
	} else if (ns->nxstate & (STATE_DATAIN_MASK | STATE_DATAOUT_MASK)) {
		 

		ns->state      = ns->nxstate;
		ns->nxstate    = ns->op[++ns->stateidx + 1];
		ns->regs.num   = ns->regs.count = 0;

		NS_DBG("switch_state: the next state is data I/O, switch, "
			"state: %s, nxstate: %s\n",
		       ns_get_state_name(ns->state),
		       ns_get_state_name(ns->nxstate));

		 
		switch (NS_STATE(ns->state)) {
			case STATE_DATAIN:
			case STATE_DATAOUT:
				ns->regs.num = ns->geom.pgszoob - NS_PAGE_BYTE_SHIFT(ns);
				break;

			case STATE_DATAOUT_ID:
				ns->regs.num = ns->geom.idbytes;
				break;

			case STATE_DATAOUT_STATUS:
				ns->regs.count = ns->regs.num = 0;
				break;

			default:
				NS_ERR("switch_state: BUG! unknown data state\n");
		}

	} else if (ns->nxstate & STATE_ADDR_MASK) {
		 

		ns->regs.count = 0;

		switch (NS_STATE(ns->nxstate)) {
			case STATE_ADDR_PAGE:
				ns->regs.num = ns->geom.pgaddrbytes;

				break;
			case STATE_ADDR_SEC:
				ns->regs.num = ns->geom.secaddrbytes;
				break;

			case STATE_ADDR_ZERO:
				ns->regs.num = 1;
				break;

			case STATE_ADDR_COLUMN:
				 
				ns->regs.num = ns->geom.pgaddrbytes - ns->geom.secaddrbytes;
				break;

			default:
				NS_ERR("switch_state: BUG! unknown address state\n");
		}
	} else {
		 

		ns->regs.num = 0;
		ns->regs.count = 0;
	}
}

static u_char ns_nand_read_byte(struct nand_chip *chip)
{
	struct nandsim *ns = nand_get_controller_data(chip);
	u_char outb = 0x00;

	 
	if (!ns->lines.ce) {
		NS_ERR("read_byte: chip is disabled, return %#x\n", (uint)outb);
		return outb;
	}
	if (ns->lines.ale || ns->lines.cle) {
		NS_ERR("read_byte: ALE or CLE pin is high, return %#x\n", (uint)outb);
		return outb;
	}
	if (!(ns->state & STATE_DATAOUT_MASK)) {
		NS_WARN("read_byte: unexpected data output cycle, state is %s return %#x\n",
			ns_get_state_name(ns->state), (uint)outb);
		return outb;
	}

	 
	if (NS_STATE(ns->state) == STATE_DATAOUT_STATUS) {
		NS_DBG("read_byte: return %#x status\n", ns->regs.status);
		return ns->regs.status;
	}

	 
	if (ns->regs.count == ns->regs.num) {
		NS_WARN("read_byte: no more data to output, return %#x\n", (uint)outb);
		return outb;
	}

	switch (NS_STATE(ns->state)) {
		case STATE_DATAOUT:
			if (ns->busw == 8) {
				outb = ns->buf.byte[ns->regs.count];
				ns->regs.count += 1;
			} else {
				outb = (u_char)cpu_to_le16(ns->buf.word[ns->regs.count >> 1]);
				ns->regs.count += 2;
			}
			break;
		case STATE_DATAOUT_ID:
			NS_DBG("read_byte: read ID byte %d, total = %d\n", ns->regs.count, ns->regs.num);
			outb = ns->ids[ns->regs.count];
			ns->regs.count += 1;
			break;
		default:
			BUG();
	}

	if (ns->regs.count == ns->regs.num) {
		NS_DBG("read_byte: all bytes were read\n");

		if (NS_STATE(ns->nxstate) == STATE_READY)
			ns_switch_state(ns);
	}

	return outb;
}

static void ns_nand_write_byte(struct nand_chip *chip, u_char byte)
{
	struct nandsim *ns = nand_get_controller_data(chip);

	 
	if (!ns->lines.ce) {
		NS_ERR("write_byte: chip is disabled, ignore write\n");
		return;
	}
	if (ns->lines.ale && ns->lines.cle) {
		NS_ERR("write_byte: ALE and CLE pins are high simultaneously, ignore write\n");
		return;
	}

	if (ns->lines.cle == 1) {
		 

		if (byte == NAND_CMD_RESET) {
			NS_LOG("reset chip\n");
			ns_switch_to_ready_state(ns, NS_STATUS_OK(ns));
			return;
		}

		 
		if (ns_check_command(byte)) {
			NS_ERR("write_byte: unknown command %#x\n", (uint)byte);
			return;
		}

		if (NS_STATE(ns->state) == STATE_DATAOUT_STATUS
			|| NS_STATE(ns->state) == STATE_DATAOUT) {
			int row = ns->regs.row;

			ns_switch_state(ns);
			if (byte == NAND_CMD_RNDOUT)
				ns->regs.row = row;
		}

		 
		if (NS_STATE(ns->nxstate) != STATE_UNKNOWN && !(ns->nxstate & STATE_CMD_MASK)) {
			 
			if (!(ns->regs.command == NAND_CMD_READID &&
			    NS_STATE(ns->state) == STATE_DATAOUT_ID && ns->regs.count == 2)) {
				 
				NS_WARN("write_byte: command (%#x) wasn't expected, expected state is %s, ignore previous states\n",
					(uint)byte,
					ns_get_state_name(ns->nxstate));
			}
			ns_switch_to_ready_state(ns, NS_STATUS_FAILED(ns));
		}

		NS_DBG("command byte corresponding to %s state accepted\n",
			ns_get_state_name(ns_get_state_by_command(byte)));
		ns->regs.command = byte;
		ns_switch_state(ns);

	} else if (ns->lines.ale == 1) {
		 

		if (NS_STATE(ns->nxstate) == STATE_UNKNOWN) {

			NS_DBG("write_byte: operation isn't known yet, identify it\n");

			if (ns_find_operation(ns, 1) < 0)
				return;

			if ((ns->state & ACTION_MASK) &&
			    ns_do_state_action(ns, ns->state) < 0) {
				ns_switch_to_ready_state(ns,
							 NS_STATUS_FAILED(ns));
				return;
			}

			ns->regs.count = 0;
			switch (NS_STATE(ns->nxstate)) {
				case STATE_ADDR_PAGE:
					ns->regs.num = ns->geom.pgaddrbytes;
					break;
				case STATE_ADDR_SEC:
					ns->regs.num = ns->geom.secaddrbytes;
					break;
				case STATE_ADDR_ZERO:
					ns->regs.num = 1;
					break;
				default:
					BUG();
			}
		}

		 
		if (!(ns->nxstate & STATE_ADDR_MASK)) {
			NS_ERR("write_byte: address (%#x) isn't expected, expected state is %s, switch to STATE_READY\n",
			       (uint)byte, ns_get_state_name(ns->nxstate));
			ns_switch_to_ready_state(ns, NS_STATUS_FAILED(ns));
			return;
		}

		 
		if (ns->regs.count == ns->regs.num) {
			NS_ERR("write_byte: no more address bytes expected\n");
			ns_switch_to_ready_state(ns, NS_STATUS_FAILED(ns));
			return;
		}

		ns_accept_addr_byte(ns, byte);

		ns->regs.count += 1;

		NS_DBG("write_byte: address byte %#x was accepted (%d bytes input, %d expected)\n",
				(uint)byte, ns->regs.count, ns->regs.num);

		if (ns->regs.count == ns->regs.num) {
			NS_DBG("address (%#x, %#x) is accepted\n", ns->regs.row, ns->regs.column);
			ns_switch_state(ns);
		}

	} else {
		 

		 
		if (!(ns->state & STATE_DATAIN_MASK)) {
			NS_ERR("write_byte: data input (%#x) isn't expected, state is %s, switch to %s\n",
			       (uint)byte, ns_get_state_name(ns->state),
			       ns_get_state_name(STATE_READY));
			ns_switch_to_ready_state(ns, NS_STATUS_FAILED(ns));
			return;
		}

		 
		if (ns->regs.count == ns->regs.num) {
			NS_WARN("write_byte: %u input bytes has already been accepted, ignore write\n",
					ns->regs.num);
			return;
		}

		if (ns->busw == 8) {
			ns->buf.byte[ns->regs.count] = byte;
			ns->regs.count += 1;
		} else {
			ns->buf.word[ns->regs.count >> 1] = cpu_to_le16((uint16_t)byte);
			ns->regs.count += 2;
		}
	}

	return;
}

static void ns_nand_write_buf(struct nand_chip *chip, const u_char *buf,
			      int len)
{
	struct nandsim *ns = nand_get_controller_data(chip);

	 
	if (!(ns->state & STATE_DATAIN_MASK)) {
		NS_ERR("write_buf: data input isn't expected, state is %s, switch to STATE_READY\n",
		       ns_get_state_name(ns->state));
		ns_switch_to_ready_state(ns, NS_STATUS_FAILED(ns));
		return;
	}

	 
	if (ns->regs.count + len > ns->regs.num) {
		NS_ERR("write_buf: too many input bytes\n");
		ns_switch_to_ready_state(ns, NS_STATUS_FAILED(ns));
		return;
	}

	memcpy(ns->buf.byte + ns->regs.count, buf, len);
	ns->regs.count += len;

	if (ns->regs.count == ns->regs.num) {
		NS_DBG("write_buf: %d bytes were written\n", ns->regs.count);
	}
}

static void ns_nand_read_buf(struct nand_chip *chip, u_char *buf, int len)
{
	struct nandsim *ns = nand_get_controller_data(chip);

	 
	if (!ns->lines.ce) {
		NS_ERR("read_buf: chip is disabled\n");
		return;
	}
	if (ns->lines.ale || ns->lines.cle) {
		NS_ERR("read_buf: ALE or CLE pin is high\n");
		return;
	}
	if (!(ns->state & STATE_DATAOUT_MASK)) {
		NS_WARN("read_buf: unexpected data output cycle, current state is %s\n",
			ns_get_state_name(ns->state));
		return;
	}

	if (NS_STATE(ns->state) != STATE_DATAOUT) {
		int i;

		for (i = 0; i < len; i++)
			buf[i] = ns_nand_read_byte(chip);

		return;
	}

	 
	if (ns->regs.count + len > ns->regs.num) {
		NS_ERR("read_buf: too many bytes to read\n");
		ns_switch_to_ready_state(ns, NS_STATUS_FAILED(ns));
		return;
	}

	memcpy(buf, ns->buf.byte + ns->regs.count, len);
	ns->regs.count += len;

	if (ns->regs.count == ns->regs.num) {
		if (NS_STATE(ns->nxstate) == STATE_READY)
			ns_switch_state(ns);
	}

	return;
}

static int ns_exec_op(struct nand_chip *chip, const struct nand_operation *op,
		      bool check_only)
{
	int i;
	unsigned int op_id;
	const struct nand_op_instr *instr = NULL;
	struct nandsim *ns = nand_get_controller_data(chip);

	if (check_only) {
		 
		for (op_id = 0; op_id < op->ninstrs; op_id++) {
			instr = &op->instrs[op_id];
			if (instr->type == NAND_OP_CMD_INSTR &&
			    (instr->ctx.cmd.opcode == NAND_CMD_READCACHEEND ||
			     instr->ctx.cmd.opcode == NAND_CMD_READCACHESEQ))
				return -EOPNOTSUPP;
		}

		return 0;
	}

	ns->lines.ce = 1;

	for (op_id = 0; op_id < op->ninstrs; op_id++) {
		instr = &op->instrs[op_id];
		ns->lines.cle = 0;
		ns->lines.ale = 0;

		switch (instr->type) {
		case NAND_OP_CMD_INSTR:
			ns->lines.cle = 1;
			ns_nand_write_byte(chip, instr->ctx.cmd.opcode);
			break;
		case NAND_OP_ADDR_INSTR:
			ns->lines.ale = 1;
			for (i = 0; i < instr->ctx.addr.naddrs; i++)
				ns_nand_write_byte(chip, instr->ctx.addr.addrs[i]);
			break;
		case NAND_OP_DATA_IN_INSTR:
			ns_nand_read_buf(chip, instr->ctx.data.buf.in, instr->ctx.data.len);
			break;
		case NAND_OP_DATA_OUT_INSTR:
			ns_nand_write_buf(chip, instr->ctx.data.buf.out, instr->ctx.data.len);
			break;
		case NAND_OP_WAITRDY_INSTR:
			 
			break;
		}
	}

	return 0;
}

static int ns_attach_chip(struct nand_chip *chip)
{
	unsigned int eccsteps, eccbytes;

	chip->ecc.engine_type = NAND_ECC_ENGINE_TYPE_SOFT;
	chip->ecc.algo = bch ? NAND_ECC_ALGO_BCH : NAND_ECC_ALGO_HAMMING;

	if (!bch)
		return 0;

	if (!IS_ENABLED(CONFIG_MTD_NAND_ECC_SW_BCH)) {
		NS_ERR("BCH ECC support is disabled\n");
		return -EINVAL;
	}

	 
	eccsteps = nsmtd->writesize / 512;
	eccbytes = ((bch * 13) + 7) / 8;

	 
	if (nsmtd->oobsize < 64 || !eccsteps) {
		NS_ERR("BCH not available on small page devices\n");
		return -EINVAL;
	}

	if (((eccbytes * eccsteps) + 2) > nsmtd->oobsize) {
		NS_ERR("Invalid BCH value %u\n", bch);
		return -EINVAL;
	}

	chip->ecc.size = 512;
	chip->ecc.strength = bch;
	chip->ecc.bytes = eccbytes;

	NS_INFO("Using %u-bit/%u bytes BCH ECC\n", bch, chip->ecc.size);

	return 0;
}

static const struct nand_controller_ops ns_controller_ops = {
	.attach_chip = ns_attach_chip,
	.exec_op = ns_exec_op,
};

 
static int __init ns_init_module(void)
{
	struct list_head *pos, *n;
	struct nand_chip *chip;
	struct nandsim *ns;
	int ret;

	if (bus_width != 8 && bus_width != 16) {
		NS_ERR("wrong bus width (%d), use only 8 or 16\n", bus_width);
		return -EINVAL;
	}

	ns = kzalloc(sizeof(struct nandsim), GFP_KERNEL);
	if (!ns) {
		NS_ERR("unable to allocate core structures.\n");
		return -ENOMEM;
	}
	chip	    = &ns->chip;
	nsmtd       = nand_to_mtd(chip);
	nand_set_controller_data(chip, (void *)ns);

	 
	 
	chip->options   |= NAND_SKIP_BBTSCAN;

	switch (bbt) {
	case 2:
		chip->bbt_options |= NAND_BBT_NO_OOB;
		fallthrough;
	case 1:
		chip->bbt_options |= NAND_BBT_USE_FLASH;
		fallthrough;
	case 0:
		break;
	default:
		NS_ERR("bbt has to be 0..2\n");
		ret = -EINVAL;
		goto free_ns_struct;
	}
	 
	if (id_bytes[6] != 0xFF || id_bytes[7] != 0xFF)
		ns->geom.idbytes = 8;
	else if (id_bytes[4] != 0xFF || id_bytes[5] != 0xFF)
		ns->geom.idbytes = 6;
	else if (id_bytes[2] != 0xFF || id_bytes[3] != 0xFF)
		ns->geom.idbytes = 4;
	else
		ns->geom.idbytes = 2;
	ns->regs.status = NS_STATUS_OK(ns);
	ns->nxstate = STATE_UNKNOWN;
	ns->options |= OPT_PAGE512;  
	memcpy(ns->ids, id_bytes, sizeof(ns->ids));
	if (bus_width == 16) {
		ns->busw = 16;
		chip->options |= NAND_BUSWIDTH_16;
	}

	nsmtd->owner = THIS_MODULE;

	ret = ns_parse_weakblocks();
	if (ret)
		goto free_ns_struct;

	ret = ns_parse_weakpages();
	if (ret)
		goto free_wb_list;

	ret = ns_parse_gravepages();
	if (ret)
		goto free_wp_list;

	nand_controller_init(&ns->base);
	ns->base.ops = &ns_controller_ops;
	chip->controller = &ns->base;

	ret = nand_scan(chip, 1);
	if (ret) {
		NS_ERR("Could not scan NAND Simulator device\n");
		goto free_gp_list;
	}

	if (overridesize) {
		uint64_t new_size = (uint64_t)nsmtd->erasesize << overridesize;
		struct nand_memory_organization *memorg;
		u64 targetsize;

		memorg = nanddev_get_memorg(&chip->base);

		if (new_size >> overridesize != nsmtd->erasesize) {
			NS_ERR("overridesize is too big\n");
			ret = -EINVAL;
			goto cleanup_nand;
		}

		 
		nsmtd->size = new_size;
		memorg->eraseblocks_per_lun = 1 << overridesize;
		targetsize = nanddev_target_size(&chip->base);
		chip->chip_shift = ffs(nsmtd->erasesize) + overridesize - 1;
		chip->pagemask = (targetsize >> chip->page_shift) - 1;
	}

	ret = ns_setup_wear_reporting(nsmtd);
	if (ret)
		goto cleanup_nand;

	ret = ns_init(nsmtd);
	if (ret)
		goto free_ebw;

	ret = nand_create_bbt(chip);
	if (ret)
		goto free_ns_object;

	ret = ns_parse_badblocks(ns, nsmtd);
	if (ret)
		goto free_ns_object;

	 
	ret = mtd_device_register(nsmtd, &ns->partitions[0], ns->nbparts);
	if (ret)
		goto free_ns_object;

	ret = ns_debugfs_create(ns);
	if (ret)
		goto unregister_mtd;

        return 0;

unregister_mtd:
	WARN_ON(mtd_device_unregister(nsmtd));
free_ns_object:
	ns_free(ns);
free_ebw:
	kfree(erase_block_wear);
cleanup_nand:
	nand_cleanup(chip);
free_gp_list:
	list_for_each_safe(pos, n, &grave_pages) {
		list_del(pos);
		kfree(list_entry(pos, struct grave_page, list));
	}
free_wp_list:
	list_for_each_safe(pos, n, &weak_pages) {
		list_del(pos);
		kfree(list_entry(pos, struct weak_page, list));
	}
free_wb_list:
	list_for_each_safe(pos, n, &weak_blocks) {
		list_del(pos);
		kfree(list_entry(pos, struct weak_block, list));
	}
free_ns_struct:
	kfree(ns);

	return ret;
}

module_init(ns_init_module);

 
static void __exit ns_cleanup_module(void)
{
	struct nand_chip *chip = mtd_to_nand(nsmtd);
	struct nandsim *ns = nand_get_controller_data(chip);
	struct list_head *pos, *n;

	ns_debugfs_remove(ns);
	WARN_ON(mtd_device_unregister(nsmtd));
	ns_free(ns);
	kfree(erase_block_wear);
	nand_cleanup(chip);

	list_for_each_safe(pos, n, &grave_pages) {
		list_del(pos);
		kfree(list_entry(pos, struct grave_page, list));
	}

	list_for_each_safe(pos, n, &weak_pages) {
		list_del(pos);
		kfree(list_entry(pos, struct weak_page, list));
	}

	list_for_each_safe(pos, n, &weak_blocks) {
		list_del(pos);
		kfree(list_entry(pos, struct weak_block, list));
	}

	kfree(ns);
}

module_exit(ns_cleanup_module);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Artem B. Bityuckiy");
MODULE_DESCRIPTION ("The NAND flash simulator");
