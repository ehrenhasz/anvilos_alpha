 
 
#ifndef __IBM_NEWEMAC_ZMII_H
#define __IBM_NEWEMAC_ZMII_H

 
struct zmii_regs {
	u32 fer;		 
	u32 ssr;		 
	u32 smiirs;		 
};

 
struct zmii_instance {
	struct zmii_regs __iomem	*base;

	 
	struct mutex			lock;

	 
	int				mode;

	 
	int				users;

	 
	u32				fer_save;

	 
	struct platform_device		*ofdev;
};

#ifdef CONFIG_IBM_EMAC_ZMII

int zmii_init(void);
void zmii_exit(void);
int zmii_attach(struct platform_device *ofdev, int input,
		phy_interface_t *mode);
void zmii_detach(struct platform_device *ofdev, int input);
void zmii_get_mdio(struct platform_device *ofdev, int input);
void zmii_put_mdio(struct platform_device *ofdev, int input);
void zmii_set_speed(struct platform_device *ofdev, int input, int speed);
int zmii_get_regs_len(struct platform_device *ocpdev);
void *zmii_dump_regs(struct platform_device *ofdev, void *buf);

#else
# define zmii_init()		0
# define zmii_exit()		do { } while(0)
# define zmii_attach(x,y,z)	(-ENXIO)
# define zmii_detach(x,y)	do { } while(0)
# define zmii_get_mdio(x,y)	do { } while(0)
# define zmii_put_mdio(x,y)	do { } while(0)
# define zmii_set_speed(x,y,z)	do { } while(0)
# define zmii_get_regs_len(x)	0
# define zmii_dump_regs(x,buf)	(buf)
#endif				 

#endif  
