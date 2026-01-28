


#ifndef EF4_PHY_H
#define EF4_PHY_H


extern const struct ef4_phy_operations falcon_sfx7101_phy_ops;

void tenxpress_set_id_led(struct ef4_nic *efx, enum ef4_led_mode mode);


extern const struct ef4_phy_operations falcon_qt202x_phy_ops;


#define QUAKE_LED_LINK_INVAL	(0)
#define QUAKE_LED_LINK_STAT	(1)
#define QUAKE_LED_LINK_ACT	(2)
#define QUAKE_LED_LINK_ACTSTAT	(3)
#define QUAKE_LED_OFF		(4)
#define QUAKE_LED_ON		(5)
#define QUAKE_LED_LINK_INPUT	(6)	

#define QUAKE_LED_TXLINK	(0)
#define QUAKE_LED_RXLINK	(8)

void falcon_qt202x_set_led(struct ef4_nic *p, int led, int state);


extern const struct ef4_phy_operations falcon_txc_phy_ops;

#define TXC_GPIO_DIR_INPUT	0
#define TXC_GPIO_DIR_OUTPUT	1

void falcon_txc_set_gpio_dir(struct ef4_nic *efx, int pin, int dir);
void falcon_txc_set_gpio_val(struct ef4_nic *efx, int pin, int val);

#endif
