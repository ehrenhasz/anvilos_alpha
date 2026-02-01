
 

#include <linux/device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/dma-direction.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/comedi/comedidev.h>
#include "comedi_internal.h"

struct comedi_driver *comedi_drivers;
 
DEFINE_MUTEX(comedi_drivers_list_lock);

 
int comedi_set_hw_dev(struct comedi_device *dev, struct device *hw_dev)
{
	if (hw_dev == dev->hw_dev)
		return 0;
	if (dev->hw_dev)
		return -EEXIST;
	dev->hw_dev = get_device(hw_dev);
	return 0;
}
EXPORT_SYMBOL_GPL(comedi_set_hw_dev);

static void comedi_clear_hw_dev(struct comedi_device *dev)
{
	put_device(dev->hw_dev);
	dev->hw_dev = NULL;
}

 
void *comedi_alloc_devpriv(struct comedi_device *dev, size_t size)
{
	dev->private = kzalloc(size, GFP_KERNEL);
	return dev->private;
}
EXPORT_SYMBOL_GPL(comedi_alloc_devpriv);

 
int comedi_alloc_subdevices(struct comedi_device *dev, int num_subdevices)
{
	struct comedi_subdevice *s;
	int i;

	if (num_subdevices < 1)
		return -EINVAL;

	s = kcalloc(num_subdevices, sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;
	dev->subdevices = s;
	dev->n_subdevices = num_subdevices;

	for (i = 0; i < num_subdevices; ++i) {
		s = &dev->subdevices[i];
		s->device = dev;
		s->index = i;
		s->async_dma_dir = DMA_NONE;
		spin_lock_init(&s->spin_lock);
		s->minor = -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(comedi_alloc_subdevices);

 
int comedi_alloc_subdev_readback(struct comedi_subdevice *s)
{
	if (!s->n_chan)
		return -EINVAL;

	s->readback = kcalloc(s->n_chan, sizeof(*s->readback), GFP_KERNEL);
	if (!s->readback)
		return -ENOMEM;

	if (!s->insn_read)
		s->insn_read = comedi_readback_insn_read;

	return 0;
}
EXPORT_SYMBOL_GPL(comedi_alloc_subdev_readback);

static void comedi_device_detach_cleanup(struct comedi_device *dev)
{
	int i;
	struct comedi_subdevice *s;

	lockdep_assert_held(&dev->attach_lock);
	lockdep_assert_held(&dev->mutex);
	if (dev->subdevices) {
		for (i = 0; i < dev->n_subdevices; i++) {
			s = &dev->subdevices[i];
			if (comedi_can_auto_free_spriv(s))
				kfree(s->private);
			comedi_free_subdevice_minor(s);
			if (s->async) {
				comedi_buf_alloc(dev, s, 0);
				kfree(s->async);
			}
			kfree(s->readback);
		}
		kfree(dev->subdevices);
		dev->subdevices = NULL;
		dev->n_subdevices = 0;
	}
	kfree(dev->private);
	kfree(dev->pacer);
	dev->private = NULL;
	dev->pacer = NULL;
	dev->driver = NULL;
	dev->board_name = NULL;
	dev->board_ptr = NULL;
	dev->mmio = NULL;
	dev->iobase = 0;
	dev->iolen = 0;
	dev->ioenabled = false;
	dev->irq = 0;
	dev->read_subdev = NULL;
	dev->write_subdev = NULL;
	dev->open = NULL;
	dev->close = NULL;
	comedi_clear_hw_dev(dev);
}

void comedi_device_detach(struct comedi_device *dev)
{
	lockdep_assert_held(&dev->mutex);
	comedi_device_cancel_all(dev);
	down_write(&dev->attach_lock);
	dev->attached = false;
	dev->detach_count++;
	if (dev->driver)
		dev->driver->detach(dev);
	comedi_device_detach_cleanup(dev);
	up_write(&dev->attach_lock);
}

static int poll_invalid(struct comedi_device *dev, struct comedi_subdevice *s)
{
	return -EINVAL;
}

static int insn_device_inval(struct comedi_device *dev,
			     struct comedi_insn *insn, unsigned int *data)
{
	return -EINVAL;
}

static unsigned int get_zero_valid_routes(struct comedi_device *dev,
					  unsigned int n_pairs,
					  unsigned int *pair_data)
{
	return 0;
}

int insn_inval(struct comedi_device *dev, struct comedi_subdevice *s,
	       struct comedi_insn *insn, unsigned int *data)
{
	return -EINVAL;
}

 
int comedi_readback_insn_read(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn,
			      unsigned int *data)
{
	unsigned int chan = CR_CHAN(insn->chanspec);
	int i;

	if (!s->readback)
		return -EINVAL;

	for (i = 0; i < insn->n; i++)
		data[i] = s->readback[chan];

	return insn->n;
}
EXPORT_SYMBOL_GPL(comedi_readback_insn_read);

 
int comedi_timeout(struct comedi_device *dev,
		   struct comedi_subdevice *s,
		   struct comedi_insn *insn,
		   int (*cb)(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn,
			     unsigned long context),
		   unsigned long context)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(COMEDI_TIMEOUT_MS);
	int ret;

	while (time_before(jiffies, timeout)) {
		ret = cb(dev, s, insn, context);
		if (ret != -EBUSY)
			return ret;	 
		cpu_relax();
	}
	return -ETIMEDOUT;
}
EXPORT_SYMBOL_GPL(comedi_timeout);

 
int comedi_dio_insn_config(struct comedi_device *dev,
			   struct comedi_subdevice *s,
			   struct comedi_insn *insn,
			   unsigned int *data,
			   unsigned int mask)
{
	unsigned int chan_mask = 1 << CR_CHAN(insn->chanspec);

	if (!mask)
		mask = chan_mask;

	switch (data[0]) {
	case INSN_CONFIG_DIO_INPUT:
		s->io_bits &= ~mask;
		break;

	case INSN_CONFIG_DIO_OUTPUT:
		s->io_bits |= mask;
		break;

	case INSN_CONFIG_DIO_QUERY:
		data[1] = (s->io_bits & mask) ? COMEDI_OUTPUT : COMEDI_INPUT;
		return insn->n;

	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(comedi_dio_insn_config);

 
unsigned int comedi_dio_update_state(struct comedi_subdevice *s,
				     unsigned int *data)
{
	unsigned int chanmask = (s->n_chan < 32) ? ((1 << s->n_chan) - 1)
						 : 0xffffffff;
	unsigned int mask = data[0] & chanmask;
	unsigned int bits = data[1];

	if (mask) {
		s->state &= ~mask;
		s->state |= (bits & mask);
	}

	return mask;
}
EXPORT_SYMBOL_GPL(comedi_dio_update_state);

 
unsigned int comedi_bytes_per_scan_cmd(struct comedi_subdevice *s,
				       struct comedi_cmd *cmd)
{
	unsigned int num_samples;
	unsigned int bits_per_sample;

	switch (s->type) {
	case COMEDI_SUBD_DI:
	case COMEDI_SUBD_DO:
	case COMEDI_SUBD_DIO:
		bits_per_sample = 8 * comedi_bytes_per_sample(s);
		num_samples = DIV_ROUND_UP(cmd->scan_end_arg, bits_per_sample);
		break;
	default:
		num_samples = cmd->scan_end_arg;
		break;
	}
	return comedi_samples_to_bytes(s, num_samples);
}
EXPORT_SYMBOL_GPL(comedi_bytes_per_scan_cmd);

 
unsigned int comedi_bytes_per_scan(struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;

	return comedi_bytes_per_scan_cmd(s, cmd);
}
EXPORT_SYMBOL_GPL(comedi_bytes_per_scan);

static unsigned int __comedi_nscans_left(struct comedi_subdevice *s,
					 unsigned int nscans)
{
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;

	if (cmd->stop_src == TRIG_COUNT) {
		unsigned int scans_left = 0;

		if (async->scans_done < cmd->stop_arg)
			scans_left = cmd->stop_arg - async->scans_done;

		if (nscans > scans_left)
			nscans = scans_left;
	}
	return nscans;
}

 
unsigned int comedi_nscans_left(struct comedi_subdevice *s,
				unsigned int nscans)
{
	if (nscans == 0) {
		unsigned int nbytes = comedi_buf_read_n_available(s);

		nscans = nbytes / comedi_bytes_per_scan(s);
	}
	return __comedi_nscans_left(s, nscans);
}
EXPORT_SYMBOL_GPL(comedi_nscans_left);

 
unsigned int comedi_nsamples_left(struct comedi_subdevice *s,
				  unsigned int nsamples)
{
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned long long scans_left;
	unsigned long long samples_left;

	if (cmd->stop_src != TRIG_COUNT)
		return nsamples;

	scans_left = __comedi_nscans_left(s, cmd->stop_arg);
	if (!scans_left)
		return 0;

	samples_left = scans_left * cmd->scan_end_arg -
		comedi_bytes_to_samples(s, async->scan_progress);

	if (samples_left < nsamples)
		return samples_left;
	return nsamples;
}
EXPORT_SYMBOL_GPL(comedi_nsamples_left);

 
void comedi_inc_scan_progress(struct comedi_subdevice *s,
			      unsigned int num_bytes)
{
	struct comedi_async *async = s->async;
	struct comedi_cmd *cmd = &async->cmd;
	unsigned int scan_length = comedi_bytes_per_scan(s);

	 
	if (!(s->subdev_flags & SDF_PACKED)) {
		async->cur_chan += comedi_bytes_to_samples(s, num_bytes);
		async->cur_chan %= cmd->chanlist_len;
	}

	async->scan_progress += num_bytes;
	if (async->scan_progress >= scan_length) {
		unsigned int nscans = async->scan_progress / scan_length;

		if (async->scans_done < (UINT_MAX - nscans))
			async->scans_done += nscans;
		else
			async->scans_done = UINT_MAX;

		async->scan_progress %= scan_length;
		async->events |= COMEDI_CB_EOS;
	}
}
EXPORT_SYMBOL_GPL(comedi_inc_scan_progress);

 
unsigned int comedi_handle_events(struct comedi_device *dev,
				  struct comedi_subdevice *s)
{
	unsigned int events = s->async->events;

	if (events == 0)
		return events;

	if ((events & COMEDI_CB_CANCEL_MASK) && s->cancel)
		s->cancel(dev, s);

	comedi_event(dev, s);

	return events;
}
EXPORT_SYMBOL_GPL(comedi_handle_events);

static int insn_rw_emulate_bits(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn,
				unsigned int *data)
{
	struct comedi_insn _insn;
	unsigned int chan = CR_CHAN(insn->chanspec);
	unsigned int base_chan = (chan < 32) ? 0 : chan;
	unsigned int _data[2];
	int ret;

	memset(_data, 0, sizeof(_data));
	memset(&_insn, 0, sizeof(_insn));
	_insn.insn = INSN_BITS;
	_insn.chanspec = base_chan;
	_insn.n = 2;
	_insn.subdev = insn->subdev;

	if (insn->insn == INSN_WRITE) {
		if (!(s->subdev_flags & SDF_WRITABLE))
			return -EINVAL;
		_data[0] = 1 << (chan - base_chan);		     
		_data[1] = data[0] ? (1 << (chan - base_chan)) : 0;  
	}

	ret = s->insn_bits(dev, s, &_insn, _data);
	if (ret < 0)
		return ret;

	if (insn->insn == INSN_READ)
		data[0] = (_data[1] >> (chan - base_chan)) & 1;

	return 1;
}

static int __comedi_device_postconfig_async(struct comedi_device *dev,
					    struct comedi_subdevice *s)
{
	struct comedi_async *async;
	unsigned int buf_size;
	int ret;

	lockdep_assert_held(&dev->mutex);
	if ((s->subdev_flags & (SDF_CMD_READ | SDF_CMD_WRITE)) == 0) {
		dev_warn(dev->class_dev,
			 "async subdevices must support SDF_CMD_READ or SDF_CMD_WRITE\n");
		return -EINVAL;
	}
	if (!s->do_cmdtest) {
		dev_warn(dev->class_dev,
			 "async subdevices must have a do_cmdtest() function\n");
		return -EINVAL;
	}
	if (!s->cancel)
		dev_warn(dev->class_dev,
			 "async subdevices should have a cancel() function\n");

	async = kzalloc(sizeof(*async), GFP_KERNEL);
	if (!async)
		return -ENOMEM;

	init_waitqueue_head(&async->wait_head);
	s->async = async;

	async->max_bufsize = comedi_default_buf_maxsize_kb * 1024;
	buf_size = comedi_default_buf_size_kb * 1024;
	if (buf_size > async->max_bufsize)
		buf_size = async->max_bufsize;

	if (comedi_buf_alloc(dev, s, buf_size) < 0) {
		dev_warn(dev->class_dev, "Buffer allocation failed\n");
		return -ENOMEM;
	}
	if (s->buf_change) {
		ret = s->buf_change(dev, s);
		if (ret < 0)
			return ret;
	}

	comedi_alloc_subdevice_minor(s);

	return 0;
}

static int __comedi_device_postconfig(struct comedi_device *dev)
{
	struct comedi_subdevice *s;
	int ret;
	int i;

	lockdep_assert_held(&dev->mutex);
	if (!dev->insn_device_config)
		dev->insn_device_config = insn_device_inval;

	if (!dev->get_valid_routes)
		dev->get_valid_routes = get_zero_valid_routes;

	for (i = 0; i < dev->n_subdevices; i++) {
		s = &dev->subdevices[i];

		if (s->type == COMEDI_SUBD_UNUSED)
			continue;

		if (s->type == COMEDI_SUBD_DO) {
			if (s->n_chan < 32)
				s->io_bits = (1 << s->n_chan) - 1;
			else
				s->io_bits = 0xffffffff;
		}

		if (s->len_chanlist == 0)
			s->len_chanlist = 1;

		if (s->do_cmd) {
			ret = __comedi_device_postconfig_async(dev, s);
			if (ret)
				return ret;
		}

		if (!s->range_table && !s->range_table_list)
			s->range_table = &range_unknown;

		if (!s->insn_read && s->insn_bits)
			s->insn_read = insn_rw_emulate_bits;
		if (!s->insn_write && s->insn_bits)
			s->insn_write = insn_rw_emulate_bits;

		if (!s->insn_read)
			s->insn_read = insn_inval;
		if (!s->insn_write)
			s->insn_write = insn_inval;
		if (!s->insn_bits)
			s->insn_bits = insn_inval;
		if (!s->insn_config)
			s->insn_config = insn_inval;

		if (!s->poll)
			s->poll = poll_invalid;
	}

	return 0;
}

 
static int comedi_device_postconfig(struct comedi_device *dev)
{
	int ret;

	lockdep_assert_held(&dev->mutex);
	ret = __comedi_device_postconfig(dev);
	if (ret < 0)
		return ret;
	down_write(&dev->attach_lock);
	dev->attached = true;
	up_write(&dev->attach_lock);
	return 0;
}

 
static void *comedi_recognize(struct comedi_driver *driv, const char *name)
{
	char **name_ptr = (char **)driv->board_name;
	int i;

	for (i = 0; i < driv->num_names; i++) {
		if (strcmp(*name_ptr, name) == 0)
			return name_ptr;
		name_ptr = (void *)name_ptr + driv->offset;
	}

	return NULL;
}

static void comedi_report_boards(struct comedi_driver *driv)
{
	unsigned int i;
	const char *const *name_ptr;

	pr_info("comedi: valid board names for %s driver are:\n",
		driv->driver_name);

	name_ptr = driv->board_name;
	for (i = 0; i < driv->num_names; i++) {
		pr_info(" %s\n", *name_ptr);
		name_ptr = (const char **)((char *)name_ptr + driv->offset);
	}

	if (driv->num_names == 0)
		pr_info(" %s\n", driv->driver_name);
}

 
int comedi_load_firmware(struct comedi_device *dev,
			 struct device *device,
			 const char *name,
			 int (*cb)(struct comedi_device *dev,
				   const u8 *data, size_t size,
				   unsigned long context),
			 unsigned long context)
{
	const struct firmware *fw;
	int ret;

	if (!cb)
		return -EINVAL;

	ret = request_firmware(&fw, name, device);
	if (ret == 0) {
		ret = cb(dev, fw->data, fw->size, context);
		release_firmware(fw);
	}

	return min(ret, 0);
}
EXPORT_SYMBOL_GPL(comedi_load_firmware);

 
int __comedi_request_region(struct comedi_device *dev,
			    unsigned long start, unsigned long len)
{
	if (!start) {
		dev_warn(dev->class_dev,
			 "%s: a I/O base address must be specified\n",
			 dev->board_name);
		return -EINVAL;
	}

	if (!request_region(start, len, dev->board_name)) {
		dev_warn(dev->class_dev, "%s: I/O port conflict (%#lx,%lu)\n",
			 dev->board_name, start, len);
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(__comedi_request_region);

 
int comedi_request_region(struct comedi_device *dev,
			  unsigned long start, unsigned long len)
{
	int ret;

	ret = __comedi_request_region(dev, start, len);
	if (ret == 0) {
		dev->iobase = start;
		dev->iolen = len;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(comedi_request_region);

 
void comedi_legacy_detach(struct comedi_device *dev)
{
	if (dev->irq) {
		free_irq(dev->irq, dev);
		dev->irq = 0;
	}
	if (dev->iobase && dev->iolen) {
		release_region(dev->iobase, dev->iolen);
		dev->iobase = 0;
		dev->iolen = 0;
	}
}
EXPORT_SYMBOL_GPL(comedi_legacy_detach);

int comedi_device_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	struct comedi_driver *driv;
	int ret;

	lockdep_assert_held(&dev->mutex);
	if (dev->attached)
		return -EBUSY;

	mutex_lock(&comedi_drivers_list_lock);
	for (driv = comedi_drivers; driv; driv = driv->next) {
		if (!try_module_get(driv->module))
			continue;
		if (driv->num_names) {
			dev->board_ptr = comedi_recognize(driv, it->board_name);
			if (dev->board_ptr)
				break;
		} else if (strcmp(driv->driver_name, it->board_name) == 0) {
			break;
		}
		module_put(driv->module);
	}
	if (!driv) {
		 
		 
		for (driv = comedi_drivers; driv; driv = driv->next) {
			if (!try_module_get(driv->module))
				continue;
			comedi_report_boards(driv);
			module_put(driv->module);
		}
		ret = -EIO;
		goto out;
	}
	if (!driv->attach) {
		 
		dev_warn(dev->class_dev,
			 "driver '%s' does not support attach using comedi_config\n",
			 driv->driver_name);
		module_put(driv->module);
		ret = -EIO;
		goto out;
	}
	dev->driver = driv;
	dev->board_name = dev->board_ptr ? *(const char **)dev->board_ptr
					 : dev->driver->driver_name;
	ret = driv->attach(dev, it);
	if (ret >= 0)
		ret = comedi_device_postconfig(dev);
	if (ret < 0) {
		comedi_device_detach(dev);
		module_put(driv->module);
	}
	 
out:
	mutex_unlock(&comedi_drivers_list_lock);
	return ret;
}

 
int comedi_auto_config(struct device *hardware_device,
		       struct comedi_driver *driver, unsigned long context)
{
	struct comedi_device *dev;
	int ret;

	if (!hardware_device) {
		pr_warn("BUG! %s called with NULL hardware_device\n", __func__);
		return -EINVAL;
	}
	if (!driver) {
		dev_warn(hardware_device,
			 "BUG! %s called with NULL comedi driver\n", __func__);
		return -EINVAL;
	}

	if (!driver->auto_attach) {
		dev_warn(hardware_device,
			 "BUG! comedi driver '%s' has no auto_attach handler\n",
			 driver->driver_name);
		return -EINVAL;
	}

	dev = comedi_alloc_board_minor(hardware_device);
	if (IS_ERR(dev)) {
		dev_warn(hardware_device,
			 "driver '%s' could not create device.\n",
			 driver->driver_name);
		return PTR_ERR(dev);
	}
	 
	lockdep_assert_held(&dev->mutex);

	dev->driver = driver;
	dev->board_name = dev->driver->driver_name;
	ret = driver->auto_attach(dev, context);
	if (ret >= 0)
		ret = comedi_device_postconfig(dev);

	if (ret < 0) {
		dev_warn(hardware_device,
			 "driver '%s' failed to auto-configure device.\n",
			 driver->driver_name);
		mutex_unlock(&dev->mutex);
		comedi_release_hardware_device(hardware_device);
	} else {
		 
		dev_info(dev->class_dev,
			 "driver '%s' has successfully auto-configured '%s'.\n",
			 driver->driver_name, dev->board_name);
		mutex_unlock(&dev->mutex);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(comedi_auto_config);

 
void comedi_auto_unconfig(struct device *hardware_device)
{
	if (!hardware_device)
		return;
	comedi_release_hardware_device(hardware_device);
}
EXPORT_SYMBOL_GPL(comedi_auto_unconfig);

 
int comedi_driver_register(struct comedi_driver *driver)
{
	mutex_lock(&comedi_drivers_list_lock);
	driver->next = comedi_drivers;
	comedi_drivers = driver;
	mutex_unlock(&comedi_drivers_list_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(comedi_driver_register);

 
void comedi_driver_unregister(struct comedi_driver *driver)
{
	struct comedi_driver *prev;
	int i;

	 
	mutex_lock(&comedi_drivers_list_lock);
	if (comedi_drivers == driver) {
		comedi_drivers = driver->next;
	} else {
		for (prev = comedi_drivers; prev->next; prev = prev->next) {
			if (prev->next == driver) {
				prev->next = driver->next;
				break;
			}
		}
	}
	mutex_unlock(&comedi_drivers_list_lock);

	 
	for (i = 0; i < COMEDI_NUM_BOARD_MINORS; i++) {
		struct comedi_device *dev = comedi_dev_get_from_minor(i);

		if (!dev)
			continue;

		mutex_lock(&dev->mutex);
		if (dev->attached && dev->driver == driver) {
			if (dev->use_count)
				dev_warn(dev->class_dev,
					 "BUG! detaching device with use_count=%d\n",
					 dev->use_count);
			comedi_device_detach(dev);
		}
		mutex_unlock(&dev->mutex);
		comedi_dev_put(dev);
	}
}
EXPORT_SYMBOL_GPL(comedi_driver_unregister);
