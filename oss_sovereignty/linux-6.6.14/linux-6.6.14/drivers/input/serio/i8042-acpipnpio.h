#ifndef _I8042_ACPIPNPIO_H
#define _I8042_ACPIPNPIO_H
#include <linux/acpi.h>
#ifdef CONFIG_X86
#include <asm/x86_init.h>
#endif
#define I8042_KBD_PHYS_DESC "isa0060/serio0"
#define I8042_AUX_PHYS_DESC "isa0060/serio1"
#define I8042_MUX_PHYS_DESC "isa0060/serio%d"
#if defined(__ia64__)
# define I8042_MAP_IRQ(x)	isa_irq_to_vector((x))
#else
# define I8042_MAP_IRQ(x)	(x)
#endif
#define I8042_KBD_IRQ	i8042_kbd_irq
#define I8042_AUX_IRQ	i8042_aux_irq
static int i8042_kbd_irq;
static int i8042_aux_irq;
#define I8042_COMMAND_REG	i8042_command_reg
#define I8042_STATUS_REG	i8042_command_reg
#define I8042_DATA_REG		i8042_data_reg
static int i8042_command_reg = 0x64;
static int i8042_data_reg = 0x60;
static inline int i8042_read_data(void)
{
	return inb(I8042_DATA_REG);
}
static inline int i8042_read_status(void)
{
	return inb(I8042_STATUS_REG);
}
static inline void i8042_write_data(int val)
{
	outb(val, I8042_DATA_REG);
}
static inline void i8042_write_command(int val)
{
	outb(val, I8042_COMMAND_REG);
}
#ifdef CONFIG_X86
#include <linux/dmi.h>
#define SERIO_QUIRK_NOKBD		BIT(0)
#define SERIO_QUIRK_NOAUX		BIT(1)
#define SERIO_QUIRK_NOMUX		BIT(2)
#define SERIO_QUIRK_FORCEMUX		BIT(3)
#define SERIO_QUIRK_UNLOCK		BIT(4)
#define SERIO_QUIRK_PROBE_DEFER		BIT(5)
#define SERIO_QUIRK_RESET_ALWAYS	BIT(6)
#define SERIO_QUIRK_RESET_NEVER		BIT(7)
#define SERIO_QUIRK_DIECT		BIT(8)
#define SERIO_QUIRK_DUMBKBD		BIT(9)
#define SERIO_QUIRK_NOLOOP		BIT(10)
#define SERIO_QUIRK_NOTIMEOUT		BIT(11)
#define SERIO_QUIRK_KBDRESET		BIT(12)
#define SERIO_QUIRK_DRITEK		BIT(13)
#define SERIO_QUIRK_NOPNP		BIT(14)
static const struct dmi_system_id i8042_dmi_quirk_table[] __initconst = {
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ALIENWARE"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Sentia"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "X750LN"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "X450LCP"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_NEVER)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "ZenBook UX425"),
		},
		.driver_data = (void *)(SERIO_QUIRK_PROBE_DEFER | SERIO_QUIRK_RESET_NEVER)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "ZenBook UX325"),
		},
		.driver_data = (void *)(SERIO_QUIRK_PROBE_DEFER | SERIO_QUIRK_RESET_NEVER)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_CHASSIS_TYPE, "10"),  
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_NEVER)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK COMPUTER INC."),
			DMI_MATCH(DMI_CHASSIS_TYPE, "31"),  
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_NEVER)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "ASUSTeK Computer INC."),
			DMI_MATCH(DMI_BOARD_NAME, "P/I-P65UP5"),
			DMI_MATCH(DMI_BOARD_VERSION, "REV 2.X"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "ASUSTeK Computer Inc."),
			DMI_MATCH(DMI_BOARD_NAME, "G1S"),
			DMI_MATCH(DMI_BOARD_VERSION, "1.0"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 1360"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5710"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 7738"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5536"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "0100"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5738"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "AOA150"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "AO532h"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire A114-31"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire A314-31"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire A315-31"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire ES1-132"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire ES1-332"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire ES1-432"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate Spin B118-RN"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5100"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5610"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5630"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5650"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5680"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5720"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 9110"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate 660"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate 2490"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate 4280"),
		},
		.driver_data = (void *)(SERIO_QUIRK_DRITEK)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate P459-G2-M"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Amoi Electronics CO.,LTD."),
			DMI_MATCH(DMI_PRODUCT_NAME, "M636/A737 platform"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ByteSpeed LLC"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ByteSpeed Laptop C15B"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "COMPAL"),
			DMI_MATCH(DMI_PRODUCT_NAME, "HEL80I"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Compaq"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "8500"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Compaq"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "DL760"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "DIXONSXP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Advent 4211"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Embedded Box PC 3000"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "XPS M1530"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Vostro1510"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Vostro V13"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_NOTIMEOUT)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Vostro 1320"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Vostro 1520"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Vostro 1720"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Entroware"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Proteus"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "EL07R4"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "P7010"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LifeBook P Series"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LifeBook S Series"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LifeBook S6230"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK T725"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_NOTIMEOUT)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK U745"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "FMVLT70H"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK A544"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOTIMEOUT)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK AH544"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOTIMEOUT)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK U574"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOTIMEOUT)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK UH544"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOTIMEOUT)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "0000000000"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK T3010"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK E4010"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "AMILO Pro V2010"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "AMILO PRO V2030"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU"),
			DMI_MATCH(DMI_PRODUCT_NAME, "FMVA0501PZ"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU CLIENT COMPUTING LIMITED"),
			DMI_MATCH(DMI_PRODUCT_NAME, "LIFEBOOK E5411"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOAUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "GIGABYTE"),
			DMI_MATCH(DMI_PRODUCT_NAME, "M912"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "01"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "GIGABYTE"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Spring Peak"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "GIGABYTE"),
			DMI_MATCH(DMI_PRODUCT_NAME, "T1005"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "GIGABYTE"),
			DMI_MATCH(DMI_PRODUCT_NAME, "T1005M/P"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "GIGABYTE"),
			DMI_MATCH(DMI_PRODUCT_NAME, "P35V2"),
		},
		.driver_data = (void *)(SERIO_QUIRK_KBDRESET)
	},
		{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "GIGABYTE"),
			DMI_MATCH(DMI_PRODUCT_NAME, "X3"),
		},
		.driver_data = (void *)(SERIO_QUIRK_KBDRESET)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "GIGABYTE"),
			DMI_MATCH(DMI_PRODUCT_NAME, "P34"),
		},
		.driver_data = (void *)(SERIO_QUIRK_KBDRESET)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "GIGABYTE"),
			DMI_MATCH(DMI_PRODUCT_NAME, "P57"),
		},
		.driver_data = (void *)(SERIO_QUIRK_KBDRESET)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Gericom"),
			DMI_MATCH(DMI_PRODUCT_NAME, "N34AS6"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Gigabyte Technology Co.,Ltd."),
			DMI_MATCH(DMI_BOARD_NAME, "M1022E"),
			DMI_MATCH(DMI_BOARD_VERSION, "1.02"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
			DMI_MATCH(DMI_PRODUCT_NAME, "HP Pavilion dv9700"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "Rev 1"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Pavilion dv4000 (EA032EA#ABF)"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
			DMI_MATCH(DMI_PRODUCT_NAME, "HP Pavilion Notebook PC"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "HP Pavilion Notebook ZT1000"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Pavilion dv4000 (EH476UA#ABL)"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Hewlett-Packard"),
			DMI_MATCH(DMI_PRODUCT_NAME, "HP Pavilion dv4 Notebook PC"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_NOTIMEOUT)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "IBM"),
			DMI_MATCH(DMI_PRODUCT_NAME, "2656"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Intel"),
			DMI_MATCH(DMI_PRODUCT_NAME, "IC4I"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Intel Corporation"),
			DMI_MATCH(DMI_BOARD_NAME, "D845PESV"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Intel Corporation"),
			DMI_MATCH(DMI_BOARD_NAME, "D54250WYK"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_NAME, "076804U"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_NAME, "80UN"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "Lenovo LaVie Z"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_NAME, "20046"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "ThinkPad L460"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_NAME, "33474HU"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "LG Electronics Inc."),
			DMI_MATCH(DMI_BOARD_NAME, "X110"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "MEDION"),
			DMI_MATCH(DMI_PRODUCT_NAME, "E1210"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "MEDION"),
			DMI_MATCH(DMI_PRODUCT_NAME, "E122X"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "MICRO-STAR INTERNATIONAL CO., LTD"),
			DMI_MATCH(DMI_BOARD_NAME, "U-100"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Matsushita"),
			DMI_MATCH(DMI_PRODUCT_NAME, "CF-29"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Medion"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Akoya E7225"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "1.0"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Microsoft Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Virtual Machine"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "VS2005R2"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Notebook"),
			DMI_MATCH(DMI_PRODUCT_NAME, "MAM 2070"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "5a"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Notebook"),
			DMI_MATCH(DMI_PRODUCT_NAME, "N24_25BU"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "OQO"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ZEPTO"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "00"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "PEGATRON CORPORATION"),
			DMI_MATCH(DMI_PRODUCT_NAME, "C15B"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "PK"),
			DMI_MATCH(DMI_BOARD_NAME, "Grumpy_PK"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ULI"),
			DMI_MATCH(DMI_PRODUCT_NAME, "EV4873"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "5a"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "RIOWORKS"),
			DMI_MATCH(DMI_BOARD_NAME, "HDAMB"),
			DMI_MATCH(DMI_BOARD_VERSION, "Rev E"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "SHARP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PC-MM20 Series"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "VGN-FZ240E"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
			DMI_MATCH(DMI_BOARD_NAME, "VAIO"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "VGN-FS115B"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Sony Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "VGN-CS"),
		},
		.driver_data = (void *)(SERIO_QUIRK_FORCEMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Satellite P10"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
			DMI_MATCH(DMI_PRODUCT_NAME, "EQUIUM A110"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
			DMI_MATCH(DMI_PRODUCT_NAME, "SATELLITE C850D"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "TUXEDO"),
			DMI_MATCH(DMI_BOARD_NAME, "AURA1501"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "TUXEDO"),
			DMI_MATCH(DMI_BOARD_NAME, "EDUBOOK1502"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "VIOOO"),
			DMI_MATCH(DMI_PRODUCT_NAME, "N10"),
		},
		.driver_data = (void *)(SERIO_QUIRK_RESET_ALWAYS)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "XMG"),
			DMI_MATCH(DMI_PRODUCT_NAME, "C504"),
		},
		.driver_data = (void *)(SERIO_QUIRK_KBDRESET)
	},
	{
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "blue"),
			DMI_MATCH(DMI_PRODUCT_NAME, "FB5601"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "M606"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOLOOP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "LAPQC71A"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "LAPQC71B"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "N140CU"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "N141CU"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "N150CU"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "NH5xAx"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "NHxxRZQ"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "NL5xRU"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "NS50MU"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOAUX | SERIO_QUIRK_NOMUX |
					SERIO_QUIRK_RESET_ALWAYS | SERIO_QUIRK_NOLOOP |
					SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "NS50_70MU"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOAUX | SERIO_QUIRK_NOMUX |
					SERIO_QUIRK_RESET_ALWAYS | SERIO_QUIRK_NOLOOP |
					SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "NJ50_70CU"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "P65xH"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "P65xRP"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "P65_P67H"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "P65_67RP"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "P65_67RS"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_PRODUCT_NAME, "P67xRP"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "PB50_70DFx,DDx"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "PCX0DX"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "PD5x_7xPNP_PNR_PNN_PNT"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOAUX)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "X170SM"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "X170KM-G"),
		},
		.driver_data = (void *)(SERIO_QUIRK_NOMUX | SERIO_QUIRK_RESET_ALWAYS |
					SERIO_QUIRK_NOLOOP | SERIO_QUIRK_NOPNP)
	},
	{ }
};
#ifdef CONFIG_PNP
static const struct dmi_system_id i8042_dmi_laptop_table[] __initconst = {
	{
		.matches = {
			DMI_MATCH(DMI_CHASSIS_TYPE, "8"),  
		},
	},
	{
		.matches = {
			DMI_MATCH(DMI_CHASSIS_TYPE, "9"),  
		},
	},
	{
		.matches = {
			DMI_MATCH(DMI_CHASSIS_TYPE, "10"),  
		},
	},
	{
		.matches = {
			DMI_MATCH(DMI_CHASSIS_TYPE, "14"),  
		},
	},
	{ }
};
#endif
#endif  
#ifdef CONFIG_PNP
#include <linux/pnp.h>
static bool i8042_pnp_kbd_registered;
static unsigned int i8042_pnp_kbd_devices;
static bool i8042_pnp_aux_registered;
static unsigned int i8042_pnp_aux_devices;
static int i8042_pnp_command_reg;
static int i8042_pnp_data_reg;
static int i8042_pnp_kbd_irq;
static int i8042_pnp_aux_irq;
static char i8042_pnp_kbd_name[32];
static char i8042_pnp_aux_name[32];
static void i8042_pnp_id_to_string(struct pnp_id *id, char *dst, int dst_size)
{
	strscpy(dst, "PNP:", dst_size);
	while (id) {
		strlcat(dst, " ", dst_size);
		strlcat(dst, id->id, dst_size);
		id = id->next;
	}
}
static int i8042_pnp_kbd_probe(struct pnp_dev *dev, const struct pnp_device_id *did)
{
	if (pnp_port_valid(dev, 0) && pnp_port_len(dev, 0) == 1)
		i8042_pnp_data_reg = pnp_port_start(dev,0);
	if (pnp_port_valid(dev, 1) && pnp_port_len(dev, 1) == 1)
		i8042_pnp_command_reg = pnp_port_start(dev, 1);
	if (pnp_irq_valid(dev,0))
		i8042_pnp_kbd_irq = pnp_irq(dev, 0);
	strscpy(i8042_pnp_kbd_name, did->id, sizeof(i8042_pnp_kbd_name));
	if (strlen(pnp_dev_name(dev))) {
		strlcat(i8042_pnp_kbd_name, ":", sizeof(i8042_pnp_kbd_name));
		strlcat(i8042_pnp_kbd_name, pnp_dev_name(dev), sizeof(i8042_pnp_kbd_name));
	}
	i8042_pnp_id_to_string(dev->id, i8042_kbd_firmware_id,
			       sizeof(i8042_kbd_firmware_id));
	i8042_kbd_fwnode = dev_fwnode(&dev->dev);
	device_set_wakeup_enable(&dev->dev, true);
	i8042_pnp_kbd_devices++;
	return 0;
}
static int i8042_pnp_aux_probe(struct pnp_dev *dev, const struct pnp_device_id *did)
{
	if (pnp_port_valid(dev, 0) && pnp_port_len(dev, 0) == 1)
		i8042_pnp_data_reg = pnp_port_start(dev,0);
	if (pnp_port_valid(dev, 1) && pnp_port_len(dev, 1) == 1)
		i8042_pnp_command_reg = pnp_port_start(dev, 1);
	if (pnp_irq_valid(dev, 0))
		i8042_pnp_aux_irq = pnp_irq(dev, 0);
	strscpy(i8042_pnp_aux_name, did->id, sizeof(i8042_pnp_aux_name));
	if (strlen(pnp_dev_name(dev))) {
		strlcat(i8042_pnp_aux_name, ":", sizeof(i8042_pnp_aux_name));
		strlcat(i8042_pnp_aux_name, pnp_dev_name(dev), sizeof(i8042_pnp_aux_name));
	}
	i8042_pnp_id_to_string(dev->id, i8042_aux_firmware_id,
			       sizeof(i8042_aux_firmware_id));
	i8042_pnp_aux_devices++;
	return 0;
}
static const struct pnp_device_id pnp_kbd_devids[] = {
	{ .id = "PNP0300", .driver_data = 0 },
	{ .id = "PNP0301", .driver_data = 0 },
	{ .id = "PNP0302", .driver_data = 0 },
	{ .id = "PNP0303", .driver_data = 0 },
	{ .id = "PNP0304", .driver_data = 0 },
	{ .id = "PNP0305", .driver_data = 0 },
	{ .id = "PNP0306", .driver_data = 0 },
	{ .id = "PNP0309", .driver_data = 0 },
	{ .id = "PNP030a", .driver_data = 0 },
	{ .id = "PNP030b", .driver_data = 0 },
	{ .id = "PNP0320", .driver_data = 0 },
	{ .id = "PNP0343", .driver_data = 0 },
	{ .id = "PNP0344", .driver_data = 0 },
	{ .id = "PNP0345", .driver_data = 0 },
	{ .id = "CPQA0D7", .driver_data = 0 },
	{ .id = "", },
};
MODULE_DEVICE_TABLE(pnp, pnp_kbd_devids);
static struct pnp_driver i8042_pnp_kbd_driver = {
	.name           = "i8042 kbd",
	.id_table       = pnp_kbd_devids,
	.probe          = i8042_pnp_kbd_probe,
	.driver         = {
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
		.suppress_bind_attrs = true,
	},
};
static const struct pnp_device_id pnp_aux_devids[] = {
	{ .id = "AUI0200", .driver_data = 0 },
	{ .id = "FJC6000", .driver_data = 0 },
	{ .id = "FJC6001", .driver_data = 0 },
	{ .id = "PNP0f03", .driver_data = 0 },
	{ .id = "PNP0f0b", .driver_data = 0 },
	{ .id = "PNP0f0e", .driver_data = 0 },
	{ .id = "PNP0f12", .driver_data = 0 },
	{ .id = "PNP0f13", .driver_data = 0 },
	{ .id = "PNP0f19", .driver_data = 0 },
	{ .id = "PNP0f1c", .driver_data = 0 },
	{ .id = "SYN0801", .driver_data = 0 },
	{ .id = "", },
};
MODULE_DEVICE_TABLE(pnp, pnp_aux_devids);
static struct pnp_driver i8042_pnp_aux_driver = {
	.name           = "i8042 aux",
	.id_table       = pnp_aux_devids,
	.probe          = i8042_pnp_aux_probe,
	.driver         = {
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
		.suppress_bind_attrs = true,
	},
};
static void i8042_pnp_exit(void)
{
	if (i8042_pnp_kbd_registered) {
		i8042_pnp_kbd_registered = false;
		pnp_unregister_driver(&i8042_pnp_kbd_driver);
	}
	if (i8042_pnp_aux_registered) {
		i8042_pnp_aux_registered = false;
		pnp_unregister_driver(&i8042_pnp_aux_driver);
	}
}
static int __init i8042_pnp_init(void)
{
	char kbd_irq_str[4] = { 0 }, aux_irq_str[4] = { 0 };
	bool pnp_data_busted = false;
	int err;
	if (i8042_nopnp) {
		pr_info("PNP detection disabled\n");
		return 0;
	}
	err = pnp_register_driver(&i8042_pnp_kbd_driver);
	if (!err)
		i8042_pnp_kbd_registered = true;
	err = pnp_register_driver(&i8042_pnp_aux_driver);
	if (!err)
		i8042_pnp_aux_registered = true;
	if (!i8042_pnp_kbd_devices && !i8042_pnp_aux_devices) {
		i8042_pnp_exit();
#if defined(__ia64__)
		return -ENODEV;
#else
		pr_info("PNP: No PS/2 controller found.\n");
#if defined(__loongarch__)
		if (acpi_disabled == 0)
			return -ENODEV;
#else
		if (x86_platform.legacy.i8042 !=
				X86_LEGACY_I8042_EXPECTED_PRESENT)
			return -ENODEV;
#endif
		pr_info("Probing ports directly.\n");
		return 0;
#endif
	}
	if (i8042_pnp_kbd_devices)
		snprintf(kbd_irq_str, sizeof(kbd_irq_str),
			"%d", i8042_pnp_kbd_irq);
	if (i8042_pnp_aux_devices)
		snprintf(aux_irq_str, sizeof(aux_irq_str),
			"%d", i8042_pnp_aux_irq);
	pr_info("PNP: PS/2 Controller [%s%s%s] at %#x,%#x irq %s%s%s\n",
		i8042_pnp_kbd_name, (i8042_pnp_kbd_devices && i8042_pnp_aux_devices) ? "," : "",
		i8042_pnp_aux_name,
		i8042_pnp_data_reg, i8042_pnp_command_reg,
		kbd_irq_str, (i8042_pnp_kbd_devices && i8042_pnp_aux_devices) ? "," : "",
		aux_irq_str);
#if defined(__ia64__)
	if (!i8042_pnp_kbd_devices)
		i8042_nokbd = true;
	if (!i8042_pnp_aux_devices)
		i8042_noaux = true;
#endif
	if (((i8042_pnp_data_reg & ~0xf) == (i8042_data_reg & ~0xf) &&
	      i8042_pnp_data_reg != i8042_data_reg) ||
	    !i8042_pnp_data_reg) {
		pr_warn("PNP: PS/2 controller has invalid data port %#x; using default %#x\n",
			i8042_pnp_data_reg, i8042_data_reg);
		i8042_pnp_data_reg = i8042_data_reg;
		pnp_data_busted = true;
	}
	if (((i8042_pnp_command_reg & ~0xf) == (i8042_command_reg & ~0xf) &&
	      i8042_pnp_command_reg != i8042_command_reg) ||
	    !i8042_pnp_command_reg) {
		pr_warn("PNP: PS/2 controller has invalid command port %#x; using default %#x\n",
			i8042_pnp_command_reg, i8042_command_reg);
		i8042_pnp_command_reg = i8042_command_reg;
		pnp_data_busted = true;
	}
	if (!i8042_nokbd && !i8042_pnp_kbd_irq) {
		pr_warn("PNP: PS/2 controller doesn't have KBD irq; using default %d\n",
			i8042_kbd_irq);
		i8042_pnp_kbd_irq = i8042_kbd_irq;
		pnp_data_busted = true;
	}
	if (!i8042_noaux && !i8042_pnp_aux_irq) {
		if (!pnp_data_busted && i8042_pnp_kbd_irq) {
			pr_warn("PNP: PS/2 appears to have AUX port disabled, "
				"if this is incorrect please boot with i8042.nopnp\n");
			i8042_noaux = true;
		} else {
			pr_warn("PNP: PS/2 controller doesn't have AUX irq; using default %d\n",
				i8042_aux_irq);
			i8042_pnp_aux_irq = i8042_aux_irq;
		}
	}
	i8042_data_reg = i8042_pnp_data_reg;
	i8042_command_reg = i8042_pnp_command_reg;
	i8042_kbd_irq = i8042_pnp_kbd_irq;
	i8042_aux_irq = i8042_pnp_aux_irq;
#ifdef CONFIG_X86
	i8042_bypass_aux_irq_test = !pnp_data_busted &&
				    dmi_check_system(i8042_dmi_laptop_table);
#endif
	return 0;
}
#else   
static inline int i8042_pnp_init(void) { return 0; }
static inline void i8042_pnp_exit(void) { }
#endif  
#ifdef CONFIG_X86
static void __init i8042_check_quirks(void)
{
	const struct dmi_system_id *device_quirk_info;
	uintptr_t quirks;
	device_quirk_info = dmi_first_match(i8042_dmi_quirk_table);
	if (!device_quirk_info)
		return;
	quirks = (uintptr_t)device_quirk_info->driver_data;
	if (quirks & SERIO_QUIRK_NOKBD)
		i8042_nokbd = true;
	if (quirks & SERIO_QUIRK_NOAUX)
		i8042_noaux = true;
	if (quirks & SERIO_QUIRK_NOMUX)
		i8042_nomux = true;
	if (quirks & SERIO_QUIRK_FORCEMUX)
		i8042_nomux = false;
	if (quirks & SERIO_QUIRK_UNLOCK)
		i8042_unlock = true;
	if (quirks & SERIO_QUIRK_PROBE_DEFER)
		i8042_probe_defer = true;
	if (i8042_reset == I8042_RESET_DEFAULT) {
		if (quirks & SERIO_QUIRK_RESET_ALWAYS)
			i8042_reset = I8042_RESET_ALWAYS;
		if (quirks & SERIO_QUIRK_RESET_NEVER)
			i8042_reset = I8042_RESET_NEVER;
	}
	if (quirks & SERIO_QUIRK_DIECT)
		i8042_direct = true;
	if (quirks & SERIO_QUIRK_DUMBKBD)
		i8042_dumbkbd = true;
	if (quirks & SERIO_QUIRK_NOLOOP)
		i8042_noloop = true;
	if (quirks & SERIO_QUIRK_NOTIMEOUT)
		i8042_notimeout = true;
	if (quirks & SERIO_QUIRK_KBDRESET)
		i8042_kbdreset = true;
	if (quirks & SERIO_QUIRK_DRITEK)
		i8042_dritek = true;
#ifdef CONFIG_PNP
	if (quirks & SERIO_QUIRK_NOPNP)
		i8042_nopnp = true;
#endif
}
#else
static inline void i8042_check_quirks(void) {}
#endif
static int __init i8042_platform_init(void)
{
	int retval;
#ifdef CONFIG_X86
	u8 a20_on = 0xdf;
	if (x86_platform.legacy.i8042 == X86_LEGACY_I8042_PLATFORM_ABSENT)
		return -ENODEV;
#endif
	i8042_kbd_irq = I8042_MAP_IRQ(1);
	i8042_aux_irq = I8042_MAP_IRQ(12);
#if defined(__ia64__)
	i8042_reset = I8042_RESET_ALWAYS;
#endif
	i8042_check_quirks();
	pr_debug("Active quirks (empty means none):%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		i8042_nokbd ? " nokbd" : "",
		i8042_noaux ? " noaux" : "",
		i8042_nomux ? " nomux" : "",
		i8042_unlock ? " unlock" : "",
		i8042_probe_defer ? "probe_defer" : "",
		i8042_reset == I8042_RESET_DEFAULT ?
			"" : i8042_reset == I8042_RESET_ALWAYS ?
				" reset_always" : " reset_never",
		i8042_direct ? " direct" : "",
		i8042_dumbkbd ? " dumbkbd" : "",
		i8042_noloop ? " noloop" : "",
		i8042_notimeout ? " notimeout" : "",
		i8042_kbdreset ? " kbdreset" : "",
#ifdef CONFIG_X86
		i8042_dritek ? " dritek" : "",
#else
		"",
#endif
#ifdef CONFIG_PNP
		i8042_nopnp ? " nopnp" : "");
#else
		"");
#endif
	retval = i8042_pnp_init();
	if (retval)
		return retval;
#ifdef CONFIG_X86
	i8042_command(&a20_on, 0x10d1);
	i8042_command(NULL, 0x00ff);	 
#endif  
	return retval;
}
static inline void i8042_platform_exit(void)
{
	i8042_pnp_exit();
}
#endif  
