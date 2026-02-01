
 

#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/ctype.h>

#include "internal.h"

static const struct acpi_device_id acpi_pnp_device_ids[] = {
	 
	{"PNP0600"},		 
	 
	{"PNP0700"},
	 
	{"IFX0101"},		 
	{"IFX0102"},		 
	 
	{"PNP0C31"},		 
	{"ATM1200"},		 
	{"IFX0102"},		 
	{"BCM0101"},		 
	{"BCM0102"},		 
	{"NSC1200"},		 
	{"ICO0102"},		 
	 
	{"PNP0600"},		 
	 
	{"ASB16fd"},		 
	{"AZT3001"},		 
	{"CDC0001"},		 
	{"CSC0001"},		 
	{"CSC000f"},		 
	{"CSC0101"},		 
	{"CTL7001"},		 
	{"CTL7002"},		 
	{"CTL7005"},		 
	{"ENS2020"},		 
	{"ESS0001"},		 
	{"ESS0005"},		 
	{"ESS6880"},		 
	{"IBM0012"},		 
	{"OPT0001"},		 
	{"YMH0006"},		 
	{"YMH0022"},		 
	{"PNPb02f"},		 
	 
	{"PNP0300"},
	{"PNP0301"},
	{"PNP0302"},
	{"PNP0303"},
	{"PNP0304"},
	{"PNP0305"},
	{"PNP0306"},
	{"PNP0309"},
	{"PNP030a"},
	{"PNP030b"},
	{"PNP0320"},
	{"PNP0343"},
	{"PNP0344"},
	{"PNP0345"},
	{"CPQA0D7"},
	 
	{"AUI0200"},
	{"FJC6000"},
	{"FJC6001"},
	{"PNP0f03"},
	{"PNP0f0b"},
	{"PNP0f0e"},
	{"PNP0f12"},
	{"PNP0f13"},
	{"PNP0f19"},
	{"PNP0f1c"},
	{"SYN0801"},
	 
	{"AVM0900"},
	 
	{"MSM0c24"},		 
	 
	{"ADS7183"},		 
	 
	{"MFRad13"},		 
	 
	{"ENE0100"},
	{"ENE0200"},
	{"ENE0201"},
	{"ENE0202"},
	 
	{"FIT0002"},		 
	 
	{"ITE8704"},		 
	{"ITE8713"},		 
	{"ITE8708"},		 
	{"ITE8709"},		 
	 
	{"WEC0530"},		 
	{"NTN0530"},		 
	 
	{"WEC1022"},
	 
	{"WEC0517"},
	{"WEC0518"},
	 
	{"TCM5090"},		 
	{"TCM5091"},		 
	{"TCM5094"},		 
	{"TCM5095"},		 
	{"TCM5098"},		 
	{"PNP80f7"},		 
	{"PNP80f8"},		 
	 
	{"NSC6001"},
	{"HWPC224"},
	{"IBM0071"},
	 
	{"SMCf010"},
	 
	{"GIC1000"},
	 
	{"PNP0400"},		 
	{"PNP0401"},		 
	 
	{"APP000B"},
	 
	{"PNP0c02"},		 
	{"PNP0c01"},		 
	 
	{"PNP0b00"},
	{"PNP0b01"},
	{"PNP0b02"},
	 
	{"PNP0400"},		 
	{"PNP0401"},		 
	 
	{"NIC1900"},
	{"NIC2400"},
	{"NIC2500"},
	{"NIC2600"},
	{"NIC2700"},
	 
	{"AAC000F"},		 
	{"ADC0001"},		 
	{"ADC0002"},		 
	{"AEI0250"},		 
	{"AEI1240"},		 
	{"AKY1021"},		 
	{"ALI5123"},		 
	{"AZT4001"},		 
	{"BDP3336"},		 
	{"BRI0A49"},		 
	{"BRI1400"},		 
	{"BRI3400"},		 
	{"CPI4050"},		 
	{"CTL3001"},		 
	{"CTL3011"},		 
	{"DAV0336"},		 
	{"DMB1032"},		 
	{"DMB2001"},		 
	{"ETT0002"},		 
	{"FUJ0202"},		 
	{"FUJ0205"},		 
	{"FUJ0206"},		 
	{"FUJ0209"},		 
	{"GVC000F"},		 
	{"GVC0303"},		 
	{"HAY0001"},		 
	{"HAY000C"},		 
	{"HAY000D"},		 
	{"HAY5670"},		 
	{"HAY5674"},		 
	{"HAY5675"},		 
	{"HAYF000"},		 
	{"HAYF001"},		 
	{"IBM0033"},		 
	{"PNP4972"},		 
	{"IXDC801"},		 
	{"IXDC901"},		 
	{"IXDD801"},		 
	{"IXDD901"},		 
	{"IXDF401"},		 
	{"IXDF801"},		 
	{"IXDF901"},		 
	{"KOR4522"},		 
	{"KORF661"},		 
	{"LAS4040"},		 
	{"LAS4540"},		 
	{"LAS5440"},		 
	{"MNP0281"},		 
	{"MNP0336"},		 
	{"MNP0339"},		 
	{"MNP0342"},		 
	{"MNP0500"},		 
	{"MNP0501"},		 
	{"MNP0502"},		 
	{"MOT1105"},		 
	{"MOT1111"},		 
	{"MOT1114"},		 
	{"MOT1115"},		 
	{"MOT1190"},		 
	{"MOT1501"},		 
	{"MOT1502"},		 
	{"MOT1505"},		 
	{"MOT1509"},		 
	{"MOT150A"},		 
	{"MOT150F"},		 
	{"MOT1510"},		 
	{"MOT1550"},		 
	{"MOT1560"},		 
	{"MOT1580"},		 
	{"MOT15B0"},		 
	{"MOT15F0"},		 
	{"MVX00A1"},		 
	{"MVX00F2"},		 
	{"nEC8241"},		 
	{"PMC2430"},		 
	{"PNP0500"},		 
	{"PNP0501"},		 
	{"PNPC000"},		 
	{"PNPC001"},		 
	{"PNPC031"},		 
	{"PNPC032"},		 
	{"PNPC100"},		 
	{"PNPC101"},		 
	{"PNPC102"},		 
	{"PNPC103"},		 
	{"PNPC104"},		 
	{"PNPC105"},		 
	{"PNPC106"},		 
	{"PNPC107"},		 
	{"PNPC108"},		 
	{"PNPC109"},		 
	{"PNPC10A"},		 
	{"PNPC10B"},		 
	{"PNPC10C"},		 
	{"PNPC10D"},		 
	{"PNPC10E"},		 
	{"PNPC10F"},		 
	{"PNP2000"},		 
	{"ROK0030"},		 
	{"ROK0100"},		 
	{"ROK4120"},		 
	{"ROK4920"},		 
	{"RSS00A0"},		 
	{"RSS0262"},		 
	{"RSS0250"},		 
	{"SUP1310"},		 
	{"SUP1381"},		 
	{"SUP1421"},		 
	{"SUP1590"},		 
	{"SUP1620"},		 
	{"SUP1760"},		 
	{"SUP2171"},		 
	{"TEX0011"},		 
	{"UAC000F"},		 
	{"USR0000"},		 
	{"USR0002"},		 
	{"USR0004"},		 
	{"USR0006"},		 
	{"USR0007"},		 
	{"USR0009"},		 
	{"USR2002"},		 
	{"USR2070"},		 
	{"USR2080"},		 
	{"USR3031"},		 
	{"USR3050"},		 
	{"USR3070"},		 
	{"USR3080"},		 
	{"USR3090"},		 
	{"USR9100"},		 
	{"USR9160"},		 
	{"USR9170"},		 
	{"USR9180"},		 
	{"USR9190"},		 
	{"WACFXXX"},		 
	{"FPI2002"},		 
	{"FUJ02B2"},		 
	{"FUJ02B3"},
	{"FUJ02B4"},		 
	{"FUJ02B6"},		 
	{"FUJ02B7"},
	{"FUJ02B8"},
	{"FUJ02B9"},
	{"FUJ02BC"},
	{"FUJ02E5"},		 
	{"FUJ02E6"},		 
	{"FUJ02E7"},		 
	{"FUJ02E9"},		 
	{"LTS0001"},		 
	{"WCI0003"},		 
	{"WEC1022"},		 
	 
	{"NSC0800"},		 
	 
	{"PNPb006"},
	 
	{"CSC0100"},
	{"CSC0103"},
	{"CSC0110"},
	{"CSC0000"},
	{"GIM0100"},		 
	 
	{"ESS1869"},
	{"ESS1879"},
	 
	{"YMH0021"},
	{"NMX2210"},		 
	{""},
};

static bool matching_id(const char *idstr, const char *list_id)
{
	int i;

	if (strlen(idstr) != strlen(list_id))
		return false;

	if (memcmp(idstr, list_id, 3))
		return false;

	for (i = 3; i < 7; i++) {
		char c = toupper(idstr[i]);

		if (!isxdigit(c)
		    || (list_id[i] != 'X' && c != toupper(list_id[i])))
			return false;
	}
	return true;
}

static bool acpi_pnp_match(const char *idstr, const struct acpi_device_id **matchid)
{
	const struct acpi_device_id *devid;

	for (devid = acpi_pnp_device_ids; devid->id[0]; devid++)
		if (matching_id(idstr, (char *)devid->id)) {
			if (matchid)
				*matchid = devid;

			return true;
		}

	return false;
}

 
static const struct acpi_device_id acpi_nonpnp_device_ids[] = {
	{"INTC1080"},
	{"INTC1081"},
	{""},
};

static int acpi_pnp_attach(struct acpi_device *adev,
			   const struct acpi_device_id *id)
{
	return !!acpi_match_device_ids(adev, acpi_nonpnp_device_ids);
}

static struct acpi_scan_handler acpi_pnp_handler = {
	.ids = acpi_pnp_device_ids,
	.match = acpi_pnp_match,
	.attach = acpi_pnp_attach,
};

 
static int is_cmos_rtc_device(struct acpi_device *adev)
{
	static const struct acpi_device_id ids[] = {
		{ "PNP0B00" },
		{ "PNP0B01" },
		{ "PNP0B02" },
		{""},
	};
	return !acpi_match_device_ids(adev, ids);
}

bool acpi_is_pnp_device(struct acpi_device *adev)
{
	return adev->handler == &acpi_pnp_handler || is_cmos_rtc_device(adev);
}
EXPORT_SYMBOL_GPL(acpi_is_pnp_device);

void __init acpi_pnp_init(void)
{
	acpi_scan_add_handler(&acpi_pnp_handler);
}
