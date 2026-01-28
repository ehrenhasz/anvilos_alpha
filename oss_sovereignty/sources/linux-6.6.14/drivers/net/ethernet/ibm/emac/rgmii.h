


#ifndef __IBM_NEWEMAC_RGMII_H
#define __IBM_NEWEMAC_RGMII_H


#define RGMII_STANDARD		0
#define RGMII_AXON		1


struct rgmii_regs {
	u32 fer;		
	u32 ssr;		
};


struct rgmii_instance {
	struct rgmii_regs __iomem	*base;

	
	int				flags;
#define EMAC_RGMII_FLAG_HAS_MDIO	0x00000001

	
	struct mutex			lock;

	
	int				users;

	
	struct platform_device		*ofdev;
};

#ifdef CONFIG_IBM_EMAC_RGMII

int rgmii_init(void);
void rgmii_exit(void);
int rgmii_attach(struct platform_device *ofdev, int input, int mode);
void rgmii_detach(struct platform_device *ofdev, int input);
void rgmii_get_mdio(struct platform_device *ofdev, int input);
void rgmii_put_mdio(struct platform_device *ofdev, int input);
void rgmii_set_speed(struct platform_device *ofdev, int input, int speed);
int rgmii_get_regs_len(struct platform_device *ofdev);
void *rgmii_dump_regs(struct platform_device *ofdev, void *buf);

#else

# define rgmii_init()		0
# define rgmii_exit()		do { } while(0)
# define rgmii_attach(x,y,z)	(-ENXIO)
# define rgmii_detach(x,y)	do { } while(0)
# define rgmii_get_mdio(o,i)	do { } while (0)
# define rgmii_put_mdio(o,i)	do { } while (0)
# define rgmii_set_speed(x,y,z)	do { } while(0)
# define rgmii_get_regs_len(x)	0
# define rgmii_dump_regs(x,buf)	(buf)
#endif				

#endif 
