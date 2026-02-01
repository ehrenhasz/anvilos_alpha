 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/pci.h>
#include "ath5k.h"

#define ATH_SDEVICE(subv, subd) \
	.vendor = PCI_ANY_ID, .device = PCI_ANY_ID, \
	.subvendor = (subv), .subdevice = (subd)

#define ATH_LED(pin, polarity) .driver_data = (((pin) << 8) | (polarity))
#define ATH_PIN(data) ((data) >> 8)
#define ATH_POLARITY(data) ((data) & 0xff)

 
static const struct pci_device_id ath5k_led_devices[] = {
	 
	{ PCI_VDEVICE(ATHEROS, PCI_DEVICE_ID_ATHEROS_AR5211), ATH_LED(0, 0) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_COMPAQ, PCI_ANY_ID), ATH_LED(1, 1) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_FOXCONN, 0xe008), ATH_LED(3, 0) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_FOXCONN, 0xe00d), ATH_LED(3, 0) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_AMBIT, 0x0422), ATH_LED(1, 1) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_AMBIT, 0x0428), ATH_LED(3, 0) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_QMI, 0x0100), ATH_LED(1, 0) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_QMI, 0x0105), ATH_LED(3, 0) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_AZWAVE, 0x1026), ATH_LED(3, 0) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_IBM, 0x058a), ATH_LED(1, 0) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_HP, 0x0137a), ATH_LED(3, 1) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_HP, 0x0137b), ATH_LED(3, 0) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_ATHEROS, 0x3067), ATH_LED(3, 0) },
	 
	{ PCI_VDEVICE(ATHEROS, PCI_DEVICE_ID_ATHEROS_AR5212_IBM), ATH_LED(0, 0) },
	 
	{ ATH_SDEVICE(PCI_VENDOR_ID_QMI, 0x0112), ATH_LED(3, 0) },
	{ }
};

void ath5k_led_enable(struct ath5k_hw *ah)
{
	if (IS_ENABLED(CONFIG_MAC80211_LEDS) &&
	    test_bit(ATH_STAT_LEDSOFT, ah->status)) {
		ath5k_hw_set_gpio_output(ah, ah->led_pin);
		ath5k_led_off(ah);
	}
}

static void ath5k_led_on(struct ath5k_hw *ah)
{
	if (!test_bit(ATH_STAT_LEDSOFT, ah->status))
		return;
	ath5k_hw_set_gpio(ah, ah->led_pin, ah->led_on);
}

void ath5k_led_off(struct ath5k_hw *ah)
{
	if (!IS_ENABLED(CONFIG_MAC80211_LEDS) ||
	    !test_bit(ATH_STAT_LEDSOFT, ah->status))
		return;
	ath5k_hw_set_gpio(ah, ah->led_pin, !ah->led_on);
}

static void
ath5k_led_brightness_set(struct led_classdev *led_dev,
	enum led_brightness brightness)
{
	struct ath5k_led *led = container_of(led_dev, struct ath5k_led,
		led_dev);

	if (brightness == LED_OFF)
		ath5k_led_off(led->ah);
	else
		ath5k_led_on(led->ah);
}

static int
ath5k_register_led(struct ath5k_hw *ah, struct ath5k_led *led,
		   const char *name, const char *trigger)
{
	int err;

	led->ah = ah;
	strncpy(led->name, name, sizeof(led->name));
	led->name[sizeof(led->name)-1] = 0;
	led->led_dev.name = led->name;
	led->led_dev.default_trigger = trigger;
	led->led_dev.brightness_set = ath5k_led_brightness_set;

	err = led_classdev_register(ah->dev, &led->led_dev);
	if (err) {
		ATH5K_WARN(ah, "could not register LED %s\n", name);
		led->ah = NULL;
	}
	return err;
}

static void
ath5k_unregister_led(struct ath5k_led *led)
{
	if (!IS_ENABLED(CONFIG_MAC80211_LEDS) || !led->ah)
		return;
	led_classdev_unregister(&led->led_dev);
	ath5k_led_off(led->ah);
	led->ah = NULL;
}

void ath5k_unregister_leds(struct ath5k_hw *ah)
{
	ath5k_unregister_led(&ah->rx_led);
	ath5k_unregister_led(&ah->tx_led);
}

int ath5k_init_leds(struct ath5k_hw *ah)
{
	int ret = 0;
	struct ieee80211_hw *hw = ah->hw;
#ifndef CONFIG_ATH5K_AHB
	struct pci_dev *pdev = ah->pdev;
#endif
	char name[ATH5K_LED_MAX_NAME_LEN + 1];
	const struct pci_device_id *match;

	if (!IS_ENABLED(CONFIG_MAC80211_LEDS) || !ah->pdev)
		return 0;

#ifdef CONFIG_ATH5K_AHB
	match = NULL;
#else
	match = pci_match_id(&ath5k_led_devices[0], pdev);
#endif
	if (match) {
		__set_bit(ATH_STAT_LEDSOFT, ah->status);
		ah->led_pin = ATH_PIN(match->driver_data);
		ah->led_on = ATH_POLARITY(match->driver_data);
	}

	if (!test_bit(ATH_STAT_LEDSOFT, ah->status))
		goto out;

	ath5k_led_enable(ah);

	snprintf(name, sizeof(name), "ath5k-%s::rx", wiphy_name(hw->wiphy));
	ret = ath5k_register_led(ah, &ah->rx_led, name,
		ieee80211_get_rx_led_name(hw));
	if (ret)
		goto out;

	snprintf(name, sizeof(name), "ath5k-%s::tx", wiphy_name(hw->wiphy));
	ret = ath5k_register_led(ah, &ah->tx_led, name,
		ieee80211_get_tx_led_name(hw));
out:
	return ret;
}

