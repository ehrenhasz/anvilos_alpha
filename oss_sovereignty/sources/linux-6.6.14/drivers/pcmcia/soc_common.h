

#ifndef _ASM_ARCH_PCMCIA
#define _ASM_ARCH_PCMCIA


#include <linux/clk.h>
#include <linux/cpufreq.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/soc_common.h>

struct device;
struct gpio_desc;
struct pcmcia_low_level;
struct regulator;

struct skt_dev_info {
	int nskt;
	struct soc_pcmcia_socket skt[];
};

struct soc_pcmcia_timing {
	unsigned short io;
	unsigned short mem;
	unsigned short attr;
};

extern void soc_common_pcmcia_get_timing(struct soc_pcmcia_socket *, struct soc_pcmcia_timing *);

void soc_pcmcia_init_one(struct soc_pcmcia_socket *skt,
	const struct pcmcia_low_level *ops, struct device *dev);
void soc_pcmcia_remove_one(struct soc_pcmcia_socket *skt);
int soc_pcmcia_add_one(struct soc_pcmcia_socket *skt);
int soc_pcmcia_request_gpiods(struct soc_pcmcia_socket *skt);

void soc_common_cf_socket_state(struct soc_pcmcia_socket *skt,
	struct pcmcia_state *state);

int soc_pcmcia_regulator_set(struct soc_pcmcia_socket *skt,
	struct soc_pcmcia_regulator *r, int v);

#ifdef CONFIG_PCMCIA_DEBUG

extern void soc_pcmcia_debug(struct soc_pcmcia_socket *skt, const char *func,
			     int lvl, const char *fmt, ...);

#define debug(skt, lvl, fmt, arg...) \
	soc_pcmcia_debug(skt, __func__, lvl, fmt , ## arg)

#else
#define debug(skt, lvl, fmt, arg...) do { } while (0)
#endif



#define SOC_PCMCIA_IO_ACCESS		(165)
#define SOC_PCMCIA_5V_MEM_ACCESS	(150)
#define SOC_PCMCIA_3V_MEM_ACCESS	(300)
#define SOC_PCMCIA_ATTR_MEM_ACCESS	(300)


#define SOC_PCMCIA_POLL_PERIOD    (2*HZ)



#define iostschg bvd1
#define iospkr   bvd2

#endif
