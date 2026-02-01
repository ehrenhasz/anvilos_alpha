
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <asm/mach-types.h>

#include "sa1111_generic.h"
#include "max1600.h"

 
static int neponset_pcmcia_hw_init(struct soc_pcmcia_socket *skt)
{
	struct max1600 *m;
	int ret;

	ret = max1600_init(skt->socket.dev.parent, &m,
			   skt->nr ? MAX1600_CHAN_B : MAX1600_CHAN_A,
			   MAX1600_CODE_LOW);
	if (ret == 0)
		skt->driver_data = m;

	return ret;
}

static int
neponset_pcmcia_configure_socket(struct soc_pcmcia_socket *skt, const socket_state_t *state)
{
	struct max1600 *m = skt->driver_data;
	int ret;

	ret = sa1111_pcmcia_configure_socket(skt, state);
	if (ret == 0)
		ret = max1600_configure(m, state->Vcc, state->Vpp);

	return ret;
}

static struct pcmcia_low_level neponset_pcmcia_ops = {
	.owner			= THIS_MODULE,
	.hw_init		= neponset_pcmcia_hw_init,
	.configure_socket	= neponset_pcmcia_configure_socket,
	.first			= 0,
	.nr			= 2,
};

int pcmcia_neponset_init(struct sa1111_dev *sadev)
{
	sa11xx_drv_pcmcia_ops(&neponset_pcmcia_ops);
	return sa1111_pcmcia_add(sadev, &neponset_pcmcia_ops,
				 sa11xx_drv_pcmcia_add_one);
}
