


#ifndef __IBM_NEWEMAC_TAH_H
#define __IBM_NEWEMAC_TAH_H


struct tah_regs {
	u32 revid;
	u32 pad[3];
	u32 mr;
	u32 ssr0;
	u32 ssr1;
	u32 ssr2;
	u32 ssr3;
	u32 ssr4;
	u32 ssr5;
	u32 tsr;
};



struct tah_instance {
	struct tah_regs __iomem		*base;

	
	struct mutex			lock;

	
	int				users;

	
	struct platform_device		*ofdev;
};



#define TAH_MR_CVR		0x80000000
#define TAH_MR_SR		0x40000000
#define TAH_MR_ST_256		0x01000000
#define TAH_MR_ST_512		0x02000000
#define TAH_MR_ST_768		0x03000000
#define TAH_MR_ST_1024		0x04000000
#define TAH_MR_ST_1280		0x05000000
#define TAH_MR_ST_1536		0x06000000
#define TAH_MR_TFS_16KB		0x00000000
#define TAH_MR_TFS_2KB		0x00200000
#define TAH_MR_TFS_4KB		0x00400000
#define TAH_MR_TFS_6KB		0x00600000
#define TAH_MR_TFS_8KB		0x00800000
#define TAH_MR_TFS_10KB		0x00a00000
#define TAH_MR_DTFP		0x00100000
#define TAH_MR_DIG		0x00080000

#ifdef CONFIG_IBM_EMAC_TAH

int tah_init(void);
void tah_exit(void);
int tah_attach(struct platform_device *ofdev, int channel);
void tah_detach(struct platform_device *ofdev, int channel);
void tah_reset(struct platform_device *ofdev);
int tah_get_regs_len(struct platform_device *ofdev);
void *tah_dump_regs(struct platform_device *ofdev, void *buf);

#else

# define tah_init()		0
# define tah_exit()		do { } while(0)
# define tah_attach(x,y)	(-ENXIO)
# define tah_detach(x,y)	do { } while(0)
# define tah_reset(x)		do { } while(0)
# define tah_get_regs_len(x)	0
# define tah_dump_regs(x,buf)	(buf)

#endif				

#endif 
