



#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mux/consumer.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>

#define SPI_MUX_NO_CS	((unsigned int)-1)

 

 
struct spi_mux_priv {
	struct spi_device	*spi;
	unsigned int		current_cs;

	void			(*child_msg_complete)(void *context);
	void			*child_msg_context;
	struct spi_device	*child_msg_dev;
	struct mux_control	*mux;
};

 
static int spi_mux_select(struct spi_device *spi)
{
	struct spi_mux_priv *priv = spi_controller_get_devdata(spi->controller);
	int ret;

	ret = mux_control_select(priv->mux, spi_get_chipselect(spi, 0));
	if (ret)
		return ret;

	if (priv->current_cs == spi_get_chipselect(spi, 0))
		return 0;

	dev_dbg(&priv->spi->dev, "setting up the mux for cs %d\n",
		spi_get_chipselect(spi, 0));

	 
	priv->spi->max_speed_hz = spi->max_speed_hz;
	priv->spi->mode = spi->mode;
	priv->spi->bits_per_word = spi->bits_per_word;

	priv->current_cs = spi_get_chipselect(spi, 0);

	return 0;
}

static int spi_mux_setup(struct spi_device *spi)
{
	struct spi_mux_priv *priv = spi_controller_get_devdata(spi->controller);

	 
	return spi_setup(priv->spi);
}

static void spi_mux_complete_cb(void *context)
{
	struct spi_mux_priv *priv = (struct spi_mux_priv *)context;
	struct spi_controller *ctlr = spi_get_drvdata(priv->spi);
	struct spi_message *m = ctlr->cur_msg;

	m->complete = priv->child_msg_complete;
	m->context = priv->child_msg_context;
	m->spi = priv->child_msg_dev;
	spi_finalize_current_message(ctlr);
	mux_control_deselect(priv->mux);
}

static int spi_mux_transfer_one_message(struct spi_controller *ctlr,
						struct spi_message *m)
{
	struct spi_mux_priv *priv = spi_controller_get_devdata(ctlr);
	struct spi_device *spi = m->spi;
	int ret;

	ret = spi_mux_select(spi);
	if (ret)
		return ret;

	 
	priv->child_msg_complete = m->complete;
	priv->child_msg_context = m->context;
	priv->child_msg_dev = m->spi;

	m->complete = spi_mux_complete_cb;
	m->context = priv;
	m->spi = priv->spi;

	 
	return spi_async(priv->spi, m);
}

static int spi_mux_probe(struct spi_device *spi)
{
	struct spi_controller *ctlr;
	struct spi_mux_priv *priv;
	int ret;

	ctlr = spi_alloc_master(&spi->dev, sizeof(*priv));
	if (!ctlr)
		return -ENOMEM;

	spi_set_drvdata(spi, ctlr);
	priv = spi_controller_get_devdata(ctlr);
	priv->spi = spi;

	 
	lockdep_set_subclass(&ctlr->io_mutex, 1);
	lockdep_set_subclass(&ctlr->add_lock, 1);

	priv->mux = devm_mux_control_get(&spi->dev, NULL);
	if (IS_ERR(priv->mux)) {
		ret = dev_err_probe(&spi->dev, PTR_ERR(priv->mux),
				    "failed to get control-mux\n");
		goto err_put_ctlr;
	}

	priv->current_cs = SPI_MUX_NO_CS;

	 
	ctlr->mode_bits = spi->controller->mode_bits;
	ctlr->flags = spi->controller->flags;
	ctlr->transfer_one_message = spi_mux_transfer_one_message;
	ctlr->setup = spi_mux_setup;
	ctlr->num_chipselect = mux_control_states(priv->mux);
	ctlr->bus_num = -1;
	ctlr->dev.of_node = spi->dev.of_node;
	ctlr->must_async = true;

	ret = devm_spi_register_controller(&spi->dev, ctlr);
	if (ret)
		goto err_put_ctlr;

	return 0;

err_put_ctlr:
	spi_controller_put(ctlr);

	return ret;
}

static const struct spi_device_id spi_mux_id[] = {
	{ "spi-mux" },
	{ }
};
MODULE_DEVICE_TABLE(spi, spi_mux_id);

static const struct of_device_id spi_mux_of_match[] = {
	{ .compatible = "spi-mux" },
	{ }
};
MODULE_DEVICE_TABLE(of, spi_mux_of_match);

static struct spi_driver spi_mux_driver = {
	.probe  = spi_mux_probe,
	.driver = {
		.name   = "spi-mux",
		.of_match_table = spi_mux_of_match,
	},
	.id_table = spi_mux_id,
};

module_spi_driver(spi_mux_driver);

MODULE_DESCRIPTION("SPI multiplexer");
MODULE_LICENSE("GPL");
