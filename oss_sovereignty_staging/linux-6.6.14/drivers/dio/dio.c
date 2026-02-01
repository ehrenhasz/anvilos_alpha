
 
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/dio.h>
#include <linux/slab.h>                          
#include <linux/uaccess.h>
#include <linux/io.h>                            

struct dio_bus dio_bus = {
	.resources = {
		 
		{ .name = "DIO mem", .start = 0x00600000, .end = 0x007fffff },
		 
		{ .name = "DIO-II mem", .start = 0x01000000, .end = 0x1fffffff }
	},
	.name = "DIO bus"
};

 
#define CONFIG_DIO_CONSTANTS

#ifdef CONFIG_DIO_CONSTANTS
 
struct dioname {
	int id;
	const char *name;
};

 
#define DIONAME(x) { DIO_ID_##x, DIO_DESC_##x }
#define DIOFBNAME(x) { DIO_ENCODE_ID(DIO_ID_FBUFFER, DIO_ID2_##x), DIO_DESC2_##x }

static struct dioname names[] = {
	DIONAME(DCA0), DIONAME(DCA0REM), DIONAME(DCA1), DIONAME(DCA1REM),
	DIONAME(DCM), DIONAME(DCMREM),
	DIONAME(LAN),
	DIONAME(FHPIB), DIONAME(NHPIB),
	DIONAME(SCSI0), DIONAME(SCSI1), DIONAME(SCSI2), DIONAME(SCSI3),
	DIONAME(FBUFFER),
	DIONAME(PARALLEL), DIONAME(VME), DIONAME(DCL), DIONAME(DCLREM),
	DIONAME(MISC0), DIONAME(MISC1), DIONAME(MISC2), DIONAME(MISC3),
	DIONAME(MISC4), DIONAME(MISC5), DIONAME(MISC6), DIONAME(MISC7),
	DIONAME(MISC8), DIONAME(MISC9), DIONAME(MISC10), DIONAME(MISC11),
	DIONAME(MISC12), DIONAME(MISC13),
	DIOFBNAME(GATORBOX), DIOFBNAME(TOPCAT), DIOFBNAME(RENAISSANCE),
	DIOFBNAME(LRCATSEYE), DIOFBNAME(HRCCATSEYE), DIOFBNAME(HRMCATSEYE),
	DIOFBNAME(DAVINCI), DIOFBNAME(XXXCATSEYE), DIOFBNAME(HYPERION),
	DIOFBNAME(XGENESIS), DIOFBNAME(TIGER), DIOFBNAME(YGENESIS)
};

#undef DIONAME
#undef DIOFBNAME

static const char unknowndioname[]
	= "unknown DIO board, please email linux-m68k@lists.linux-m68k.org";

static const char *dio_getname(int id)
{
	 
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(names); i++)
		if (names[i].id == id)
			return names[i].name;

	return unknowndioname;
}

#else

static char dio_no_name[] = { 0 };
#define dio_getname(_id)	(dio_no_name)

#endif  

static void dio_dev_release(struct device *dev)
{
	struct dio_dev *ddev = container_of(dev, typeof(struct dio_dev), dev);
	kfree(ddev);
}

int __init dio_find(int deviceid)
{
	 
	int scode, id;
	u_char prid, secid, i;

	for (scode = 0; scode < DIO_SCMAX; scode++) {
		void *va;
		unsigned long pa;

		if (DIO_SCINHOLE(scode))
			continue;

		pa = dio_scodetophysaddr(scode);

		if (!pa)
			continue;

		if (scode < DIOII_SCBASE)
			va = (void *)(pa + DIO_VIRADDRBASE);
		else
			va = ioremap(pa, PAGE_SIZE);

		if (copy_from_kernel_nofault(&i,
				(unsigned char *)va + DIO_IDOFF, 1)) {
			if (scode >= DIOII_SCBASE)
				iounmap(va);
			continue;	      
		}

		prid = DIO_ID(va);

		if (DIO_NEEDSSECID(prid)) {
			secid = DIO_SECID(va);
			id = DIO_ENCODE_ID(prid, secid);
		} else
			id = prid;

		if (id == deviceid) {
			if (scode >= DIOII_SCBASE)
				iounmap(va);
			return scode;
		}
	}

	return -1;
}

 
static int __init dio_init(void)
{
	int scode;
	int i;
	struct dio_dev *dev;
	int error;

	if (!MACH_IS_HP300)
		return 0;

        printk(KERN_INFO "Scanning for DIO devices...\n");

	 
	INIT_LIST_HEAD(&dio_bus.devices);
	dev_set_name(&dio_bus.dev, "dio");
	error = device_register(&dio_bus.dev);
	if (error) {
		pr_err("DIO: Error registering dio_bus\n");
		return error;
	}

	 
	dio_bus.num_resources = (hp300_model == HP_320 ? 1 : 2);
	for (i = 0; i < dio_bus.num_resources; i++)
		request_resource(&iomem_resource, &dio_bus.resources[i]);

	 
	for (scode = 0; scode < DIO_SCMAX; ++scode) {
		u_char prid, secid = 0;	 
		u_char *va;
		unsigned long pa;

		if (DIO_SCINHOLE(scode))
			continue;

		pa = dio_scodetophysaddr(scode);

		if (!pa)
			continue;

		if (scode < DIOII_SCBASE)
			va = (void *)(pa + DIO_VIRADDRBASE);
		else
			va = ioremap(pa, PAGE_SIZE);

		if (copy_from_kernel_nofault(&i,
				(unsigned char *)va + DIO_IDOFF, 1)) {
			if (scode >= DIOII_SCBASE)
				iounmap(va);
			continue;	       
		}

		 
		dev = kzalloc(sizeof(struct dio_dev), GFP_KERNEL);
		if (!dev) {
			if (scode >= DIOII_SCBASE)
				iounmap(va);
			return -ENOMEM;
		}

		dev->bus = &dio_bus;
		dev->dev.parent = &dio_bus.dev;
		dev->dev.bus = &dio_bus_type;
		dev->dev.release = dio_dev_release;
		dev->scode = scode;
		dev->resource.start = pa;
		dev->resource.end = pa + DIO_SIZE(scode, va);
		dev_set_name(&dev->dev, "%02x", scode);

		 
		prid = DIO_ID(va);

		if (DIO_NEEDSSECID(prid)) {
			secid = DIO_SECID(va);
			dev->id = DIO_ENCODE_ID(prid, secid);
		} else
			dev->id = prid;

		dev->ipl = DIO_IPL(va);
		strcpy(dev->name, dio_getname(dev->id));
                printk(KERN_INFO "select code %3d: ipl %d: ID %02X", dev->scode, dev->ipl, prid);
		if (DIO_NEEDSSECID(prid))
                        printk(":%02X", secid);
                printk(": %s\n", dev->name);

		if (scode >= DIOII_SCBASE)
			iounmap(va);
		error = device_register(&dev->dev);
		if (error) {
			pr_err("DIO: Error registering device %s\n",
			       dev->name);
			put_device(&dev->dev);
			continue;
		}
		error = dio_create_sysfs_dev_files(dev);
		if (error)
			dev_err(&dev->dev, "Error creating sysfs files\n");
	}
	return 0;
}

subsys_initcall(dio_init);

 
unsigned long dio_scodetophysaddr(int scode)
{
	if (scode >= DIOII_SCBASE)
		return (DIOII_BASE + (scode - 132) * DIOII_DEVSIZE);
	else if (scode > DIO_SCMAX || scode < 0)
		return 0;
	else if (DIO_SCINHOLE(scode))
		return 0;

	return (DIO_BASE + scode * DIO_DEVSIZE);
}
