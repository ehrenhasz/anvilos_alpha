 
#ifndef LYNXDRV_H_
#define LYNXDRV_H_

#define FB_ACCEL_SMI 0xab

#define MHZ(x) ((x) * 1000000)

#define DEFAULT_SM750_CHIP_CLOCK	290
#define DEFAULT_SM750LE_CHIP_CLOCK	333
#ifndef SM750LE_REVISION_ID
#define SM750LE_REVISION_ID ((unsigned char)0xfe)
#endif

enum sm750_pnltype {
	sm750_24TFT = 0,	 
	sm750_dualTFT = 2,	 
	sm750_doubleTFT = 1,	 
};

 
enum sm750_dataflow {
	sm750_simul_pri,	 
	sm750_simul_sec,	 
	sm750_dual_normal,	 
	sm750_dual_swap,	 
};

enum sm750_channel {
	sm750_primary = 0,
	 
	sm750_secondary = 1,
};

enum sm750_path {
	sm750_panel = 1,
	sm750_crt = 2,
	sm750_pnc = 3,	 
};

struct init_status {
	ushort powerMode;
	 
	ushort chip_clk;
	ushort mem_clk;
	ushort master_clk;
	ushort setAllEngOff;
	ushort resetMemory;
};

struct lynx_accel {
	 
	volatile unsigned char __iomem *dprBase;
	 
	volatile unsigned char __iomem *dpPortBase;

	 
	void (*de_init)(struct lynx_accel *accel);

	int (*de_wait)(void); 

	int (*de_fillrect)(struct lynx_accel *accel,
			   u32 base, u32 pitch, u32 bpp,
			   u32 x, u32 y, u32 width, u32 height,
			   u32 color, u32 rop);

	int (*de_copyarea)(struct lynx_accel *accel,
			   u32 s_base, u32 s_pitch,
			   u32 sx, u32 sy,
			   u32 d_base, u32 d_pitch,
			   u32 bpp, u32 dx, u32 dy,
			   u32 width, u32 height,
			   u32 rop2);

	int (*de_imageblit)(struct lynx_accel *accel, const char *p_srcbuf,
			    u32 src_delta, u32 start_bit, u32 d_base, u32 d_pitch,
			    u32 byte_per_pixel, u32 dx, u32 dy, u32 width,
			    u32 height, u32 f_color, u32 b_color, u32 rop2);

};

struct sm750_dev {
	 
	u16 devid;
	u8 revid;
	struct pci_dev *pdev;
	struct fb_info *fbinfo[2];
	struct lynx_accel accel;
	int accel_off;
	int fb_count;
	int mtrr_off;
	struct{
		int vram;
	} mtrr;
	 
	unsigned long vidmem_start;
	unsigned long vidreg_start;
	__u32 vidmem_size;
	__u32 vidreg_size;
	void __iomem *pvReg;
	unsigned char __iomem *pvMem;
	 
	spinlock_t slock;

	struct init_status initParm;
	enum sm750_pnltype pnltype;
	enum sm750_dataflow dataflow;
	int nocrt;

	 
	int hwCursor;
};

struct lynx_cursor {
	 
	int w;
	int h;
	int size;
	 
	int max_w;
	int max_h;
	 
	char __iomem *vstart;
	int offset;
	 
	volatile char __iomem *mmio;
};

struct lynxfb_crtc {
	unsigned char __iomem *v_cursor;  
	unsigned char __iomem *v_screen;  
	int o_cursor;  
	int o_screen;  
	int channel; 
	resource_size_t vidmem_size; 

	 
	u16 line_pad; 
	u16 xpanstep;
	u16 ypanstep;
	u16 ywrapstep;

	void *priv;

	 
	struct lynx_cursor cursor;
};

struct lynxfb_output {
	int dpms;
	int paths;
	 

	int *channel;
	 
	void *priv;

	int (*proc_setBLANK)(struct lynxfb_output *output, int blank);
};

struct lynxfb_par {
	 
	int index;
	unsigned int pseudo_palette[256];
	struct lynxfb_crtc crtc;
	struct lynxfb_output output;
	struct fb_info *info;
	struct sm750_dev *dev;
};

static inline unsigned long ps_to_hz(unsigned int psvalue)
{
	unsigned long long numerator = 1000 * 1000 * 1000 * 1000ULL;
	 
	do_div(numerator, psvalue);
	return (unsigned long)numerator;
}

int hw_sm750_map(struct sm750_dev *sm750_dev, struct pci_dev *pdev);
int hw_sm750_inithw(struct sm750_dev *sm750_dev, struct pci_dev *pdev);
void hw_sm750_initAccel(struct sm750_dev *sm750_dev);
int hw_sm750_deWait(void);
int hw_sm750le_deWait(void);

int hw_sm750_output_setMode(struct lynxfb_output *output,
			    struct fb_var_screeninfo *var,
			    struct fb_fix_screeninfo *fix);

int hw_sm750_crtc_checkMode(struct lynxfb_crtc *crtc,
			    struct fb_var_screeninfo *var);

int hw_sm750_crtc_setMode(struct lynxfb_crtc *crtc,
			  struct fb_var_screeninfo *var,
			  struct fb_fix_screeninfo *fix);

int hw_sm750_setColReg(struct lynxfb_crtc *crtc, ushort index,
		       ushort red, ushort green, ushort blue);

int hw_sm750_setBLANK(struct lynxfb_output *output, int blank);
int hw_sm750le_setBLANK(struct lynxfb_output *output, int blank);
int hw_sm750_pan_display(struct lynxfb_crtc *crtc,
			 const struct fb_var_screeninfo *var,
			 const struct fb_info *info);

#endif
