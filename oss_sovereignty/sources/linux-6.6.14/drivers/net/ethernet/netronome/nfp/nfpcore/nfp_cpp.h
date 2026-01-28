



#ifndef __NFP_CPP_H__
#define __NFP_CPP_H__

#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/sizes.h>

#ifndef NFP_SUBSYS
#define NFP_SUBSYS "nfp"
#endif

#define nfp_err(cpp, fmt, args...) \
	dev_err(nfp_cpp_device(cpp)->parent, NFP_SUBSYS ": " fmt, ## args)
#define nfp_warn(cpp, fmt, args...) \
	dev_warn(nfp_cpp_device(cpp)->parent, NFP_SUBSYS ": " fmt, ## args)
#define nfp_info(cpp, fmt, args...) \
	dev_info(nfp_cpp_device(cpp)->parent, NFP_SUBSYS ": " fmt, ## args)
#define nfp_dbg(cpp, fmt, args...) \
	dev_dbg(nfp_cpp_device(cpp)->parent, NFP_SUBSYS ": " fmt, ## args)
#define nfp_printk(level, cpp, fmt, args...) \
	dev_printk(level, nfp_cpp_device(cpp)->parent,	\
		   NFP_SUBSYS ": " fmt,	## args)

#define PCI_64BIT_BAR_COUNT             3

#define NFP_CPP_NUM_TARGETS             16

#define NFP_CPP_SAFE_AREA_SIZE		SZ_2M


#define NFP_MUTEX_WAIT_FIRST_WARN	15
#define NFP_MUTEX_WAIT_NEXT_WARN	5
#define NFP_MUTEX_WAIT_ERROR		60

struct device;

struct nfp_cpp_area;
struct nfp_cpp;
struct resource;


#define NFP_CPP_ACTION_RW               32

#define NFP_CPP_TARGET_ID_MASK          0x1f

#define NFP_CPP_ATOMIC_RD(target, island) \
	NFP_CPP_ISLAND_ID((target), 3, 0, (island))
#define NFP_CPP_ATOMIC_WR(target, island) \
	NFP_CPP_ISLAND_ID((target), 4, 0, (island))


#define NFP_CPP_ID(target, action, token)			 \
	((((target) & 0x7f) << 24) | (((token)  & 0xff) << 16) | \
	 (((action) & 0xff) <<  8))


#define NFP_CPP_ISLAND_ID(target, action, token, island)	 \
	((((target) & 0x7f) << 24) | (((token)  & 0xff) << 16) | \
	 (((action) & 0xff) <<  8) | (((island) & 0xff) << 0))


static inline u8 NFP_CPP_ID_TARGET_of(u32 id)
{
	return (id >> 24) & NFP_CPP_TARGET_ID_MASK;
}


static inline u8 NFP_CPP_ID_TOKEN_of(u32 id)
{
	return (id >> 16) & 0xff;
}


static inline u8 NFP_CPP_ID_ACTION_of(u32 id)
{
	return (id >> 8) & 0xff;
}


static inline u8 NFP_CPP_ID_ISLAND_of(u32 id)
{
	return (id >> 0) & 0xff;
}


#define NFP_CPP_INTERFACE_TYPE_INVALID      0x0
#define NFP_CPP_INTERFACE_TYPE_PCI          0x1
#define NFP_CPP_INTERFACE_TYPE_ARM          0x2
#define NFP_CPP_INTERFACE_TYPE_RPC          0x3
#define NFP_CPP_INTERFACE_TYPE_ILA          0x4


#define NFP_CPP_INTERFACE(type, unit, channel)	\
	((((type) & 0xf) << 12) |		\
	 (((unit) & 0xf) <<  8) |		\
	 (((channel) & 0xff) << 0))


#define NFP_CPP_INTERFACE_TYPE_of(interface)   (((interface) >> 12) & 0xf)


#define NFP_CPP_INTERFACE_UNIT_of(interface)   (((interface) >>  8) & 0xf)


#define NFP_CPP_INTERFACE_CHANNEL_of(interface)   (((interface) >>  0) & 0xff)


void nfp_cpp_free(struct nfp_cpp *cpp);
u32 nfp_cpp_model(struct nfp_cpp *cpp);
u16 nfp_cpp_interface(struct nfp_cpp *cpp);
int nfp_cpp_serial(struct nfp_cpp *cpp, const u8 **serial);
unsigned int nfp_cpp_mu_locality_lsb(struct nfp_cpp *cpp);

struct nfp_cpp_area *nfp_cpp_area_alloc_with_name(struct nfp_cpp *cpp,
						  u32 cpp_id,
						  const char *name,
						  unsigned long long address,
						  unsigned long size);
struct nfp_cpp_area *nfp_cpp_area_alloc(struct nfp_cpp *cpp, u32 cpp_id,
					unsigned long long address,
					unsigned long size);
struct nfp_cpp_area *
nfp_cpp_area_alloc_acquire(struct nfp_cpp *cpp, const char *name, u32 cpp_id,
			   unsigned long long address, unsigned long size);
void nfp_cpp_area_free(struct nfp_cpp_area *area);
int nfp_cpp_area_acquire(struct nfp_cpp_area *area);
int nfp_cpp_area_acquire_nonblocking(struct nfp_cpp_area *area);
void nfp_cpp_area_release(struct nfp_cpp_area *area);
void nfp_cpp_area_release_free(struct nfp_cpp_area *area);
int nfp_cpp_area_read(struct nfp_cpp_area *area, unsigned long offset,
		      void *buffer, size_t length);
int nfp_cpp_area_write(struct nfp_cpp_area *area, unsigned long offset,
		       const void *buffer, size_t length);
size_t nfp_cpp_area_size(struct nfp_cpp_area *area);
const char *nfp_cpp_area_name(struct nfp_cpp_area *cpp_area);
void *nfp_cpp_area_priv(struct nfp_cpp_area *cpp_area);
struct nfp_cpp *nfp_cpp_area_cpp(struct nfp_cpp_area *cpp_area);
struct resource *nfp_cpp_area_resource(struct nfp_cpp_area *area);
phys_addr_t nfp_cpp_area_phys(struct nfp_cpp_area *area);
void __iomem *nfp_cpp_area_iomem(struct nfp_cpp_area *area);

int nfp_cpp_area_readl(struct nfp_cpp_area *area, unsigned long offset,
		       u32 *value);
int nfp_cpp_area_writel(struct nfp_cpp_area *area, unsigned long offset,
			u32 value);
int nfp_cpp_area_readq(struct nfp_cpp_area *area, unsigned long offset,
		       u64 *value);
int nfp_cpp_area_writeq(struct nfp_cpp_area *area, unsigned long offset,
			u64 value);
int nfp_cpp_area_fill(struct nfp_cpp_area *area, unsigned long offset,
		      u32 value, size_t length);

int nfp_xpb_readl(struct nfp_cpp *cpp, u32 xpb_tgt, u32 *value);
int nfp_xpb_writel(struct nfp_cpp *cpp, u32 xpb_tgt, u32 value);
int nfp_xpb_writelm(struct nfp_cpp *cpp, u32 xpb_tgt, u32 mask, u32 value);


int nfp_cpp_read(struct nfp_cpp *cpp, u32 cpp_id,
		 unsigned long long address, void *kernel_vaddr, size_t length);
int nfp_cpp_write(struct nfp_cpp *cpp, u32 cpp_id,
		  unsigned long long address, const void *kernel_vaddr,
		  size_t length);
int nfp_cpp_readl(struct nfp_cpp *cpp, u32 cpp_id,
		  unsigned long long address, u32 *value);
int nfp_cpp_writel(struct nfp_cpp *cpp, u32 cpp_id,
		   unsigned long long address, u32 value);
int nfp_cpp_readq(struct nfp_cpp *cpp, u32 cpp_id,
		  unsigned long long address, u64 *value);
int nfp_cpp_writeq(struct nfp_cpp *cpp, u32 cpp_id,
		   unsigned long long address, u64 value);

u8 __iomem *
nfp_cpp_map_area(struct nfp_cpp *cpp, const char *name, u32 cpp_id, u64 addr,
		 unsigned long size, struct nfp_cpp_area **area);

struct nfp_cpp_mutex;

int nfp_cpp_mutex_init(struct nfp_cpp *cpp, int target,
		       unsigned long long address, u32 key_id);
struct nfp_cpp_mutex *nfp_cpp_mutex_alloc(struct nfp_cpp *cpp, int target,
					  unsigned long long address,
					  u32 key_id);
void nfp_cpp_mutex_free(struct nfp_cpp_mutex *mutex);
int nfp_cpp_mutex_lock(struct nfp_cpp_mutex *mutex);
int nfp_cpp_mutex_unlock(struct nfp_cpp_mutex *mutex);
int nfp_cpp_mutex_trylock(struct nfp_cpp_mutex *mutex);
int nfp_cpp_mutex_reclaim(struct nfp_cpp *cpp, int target,
			  unsigned long long address);


static inline u8 nfp_cppcore_pcie_unit(struct nfp_cpp *cpp)
{
	return NFP_CPP_INTERFACE_UNIT_of(nfp_cpp_interface(cpp));
}

struct nfp_cpp_explicit;

struct nfp_cpp_explicit_command {
	u32 cpp_id;
	u16 data_ref;
	u8  data_master;
	u8  len;
	u8  byte_mask;
	u8  signal_master;
	u8  signal_ref;
	u8  posted;
	u8  siga;
	u8  sigb;
	s8   siga_mode;
	s8   sigb_mode;
};

#define NFP_SERIAL_LEN		6


struct nfp_cpp_operations {
	size_t area_priv_size;
	struct module *owner;

	int (*init)(struct nfp_cpp *cpp);
	void (*free)(struct nfp_cpp *cpp);

	int (*read_serial)(struct device *dev, u8 *serial);
	int (*get_interface)(struct device *dev);

	int (*area_init)(struct nfp_cpp_area *area,
			 u32 dest, unsigned long long address,
			 unsigned long size);
	void (*area_cleanup)(struct nfp_cpp_area *area);
	int (*area_acquire)(struct nfp_cpp_area *area);
	void (*area_release)(struct nfp_cpp_area *area);
	struct resource *(*area_resource)(struct nfp_cpp_area *area);
	phys_addr_t (*area_phys)(struct nfp_cpp_area *area);
	void __iomem *(*area_iomem)(struct nfp_cpp_area *area);
	int (*area_read)(struct nfp_cpp_area *area, void *kernel_vaddr,
			 unsigned long offset, unsigned int length);
	int (*area_write)(struct nfp_cpp_area *area, const void *kernel_vaddr,
			  unsigned long offset, unsigned int length);

	size_t explicit_priv_size;
	int (*explicit_acquire)(struct nfp_cpp_explicit *expl);
	void (*explicit_release)(struct nfp_cpp_explicit *expl);
	int (*explicit_put)(struct nfp_cpp_explicit *expl,
			    const void *buff, size_t len);
	int (*explicit_get)(struct nfp_cpp_explicit *expl,
			    void *buff, size_t len);
	int (*explicit_do)(struct nfp_cpp_explicit *expl,
			   const struct nfp_cpp_explicit_command *cmd,
			   u64 address);
};

struct nfp_cpp *
nfp_cpp_from_operations(const struct nfp_cpp_operations *ops,
			struct device *parent, void *priv);
void *nfp_cpp_priv(struct nfp_cpp *priv);

int nfp_cpp_area_cache_add(struct nfp_cpp *cpp, size_t size);




#define NFP_CPP_INTERFACE_CHANNEL_PEROPENER	255
struct device *nfp_cpp_device(struct nfp_cpp *cpp);


#define NFP_SIGNAL_MASK_A	BIT(0)	
#define NFP_SIGNAL_MASK_B	BIT(1)	

enum nfp_cpp_explicit_signal_mode {
	NFP_SIGNAL_NONE = 0,
	NFP_SIGNAL_PUSH = 1,
	NFP_SIGNAL_PUSH_OPTIONAL = -1,
	NFP_SIGNAL_PULL = 2,
	NFP_SIGNAL_PULL_OPTIONAL = -2,
};

struct nfp_cpp_explicit *nfp_cpp_explicit_acquire(struct nfp_cpp *cpp);
int nfp_cpp_explicit_set_target(struct nfp_cpp_explicit *expl, u32 cpp_id,
				u8 len, u8 mask);
int nfp_cpp_explicit_set_data(struct nfp_cpp_explicit *expl,
			      u8 data_master, u16 data_ref);
int nfp_cpp_explicit_set_signal(struct nfp_cpp_explicit *expl,
				u8 signal_master, u8 signal_ref);
int nfp_cpp_explicit_set_posted(struct nfp_cpp_explicit *expl, int posted,
				u8 siga,
				enum nfp_cpp_explicit_signal_mode siga_mode,
				u8 sigb,
				enum nfp_cpp_explicit_signal_mode sigb_mode);
int nfp_cpp_explicit_put(struct nfp_cpp_explicit *expl,
			 const void *buff, size_t len);
int nfp_cpp_explicit_do(struct nfp_cpp_explicit *expl, u64 address);
int nfp_cpp_explicit_get(struct nfp_cpp_explicit *expl, void *buff, size_t len);
void nfp_cpp_explicit_release(struct nfp_cpp_explicit *expl);
struct nfp_cpp *nfp_cpp_explicit_cpp(struct nfp_cpp_explicit *expl);
void *nfp_cpp_explicit_priv(struct nfp_cpp_explicit *cpp_explicit);



int nfp_cpp_model_autodetect(struct nfp_cpp *cpp, u32 *model);

int nfp_cpp_explicit_read(struct nfp_cpp *cpp, u32 cpp_id,
			  u64 addr, void *buff, size_t len,
			  int width_read);

int nfp_cpp_explicit_write(struct nfp_cpp *cpp, u32 cpp_id,
			   u64 addr, const void *buff, size_t len,
			   int width_write);

#endif 
