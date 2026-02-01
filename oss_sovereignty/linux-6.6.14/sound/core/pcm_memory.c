
 

#include <linux/io.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <linux/vmalloc.h>
#include <linux/export.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/info.h>
#include <sound/initval.h>
#include "pcm_local.h"

static int preallocate_dma = 1;
module_param(preallocate_dma, int, 0444);
MODULE_PARM_DESC(preallocate_dma, "Preallocate DMA memory when the PCM devices are initialized.");

static int maximum_substreams = 4;
module_param(maximum_substreams, int, 0444);
MODULE_PARM_DESC(maximum_substreams, "Maximum substreams with preallocated DMA memory.");

static const size_t snd_minimum_buffer = 16384;

static unsigned long max_alloc_per_card = 32UL * 1024UL * 1024UL;
module_param(max_alloc_per_card, ulong, 0644);
MODULE_PARM_DESC(max_alloc_per_card, "Max total allocation bytes per card.");

static void __update_allocated_size(struct snd_card *card, ssize_t bytes)
{
	card->total_pcm_alloc_bytes += bytes;
}

static void update_allocated_size(struct snd_card *card, ssize_t bytes)
{
	mutex_lock(&card->memory_mutex);
	__update_allocated_size(card, bytes);
	mutex_unlock(&card->memory_mutex);
}

static void decrease_allocated_size(struct snd_card *card, size_t bytes)
{
	mutex_lock(&card->memory_mutex);
	WARN_ON(card->total_pcm_alloc_bytes < bytes);
	__update_allocated_size(card, -(ssize_t)bytes);
	mutex_unlock(&card->memory_mutex);
}

static int do_alloc_pages(struct snd_card *card, int type, struct device *dev,
			  int str, size_t size, struct snd_dma_buffer *dmab)
{
	enum dma_data_direction dir;
	int err;

	 
	mutex_lock(&card->memory_mutex);
	if (max_alloc_per_card &&
	    card->total_pcm_alloc_bytes + size > max_alloc_per_card) {
		mutex_unlock(&card->memory_mutex);
		return -ENOMEM;
	}
	__update_allocated_size(card, size);
	mutex_unlock(&card->memory_mutex);

	if (str == SNDRV_PCM_STREAM_PLAYBACK)
		dir = DMA_TO_DEVICE;
	else
		dir = DMA_FROM_DEVICE;
	err = snd_dma_alloc_dir_pages(type, dev, dir, size, dmab);
	if (!err) {
		 
		if (dmab->bytes != size)
			update_allocated_size(card, dmab->bytes - size);
	} else {
		 
		decrease_allocated_size(card, size);
	}
	return err;
}

static void do_free_pages(struct snd_card *card, struct snd_dma_buffer *dmab)
{
	if (!dmab->area)
		return;
	decrease_allocated_size(card, dmab->bytes);
	snd_dma_free_pages(dmab);
	dmab->area = NULL;
}

 
static int preallocate_pcm_pages(struct snd_pcm_substream *substream,
				 size_t size, bool no_fallback)
{
	struct snd_dma_buffer *dmab = &substream->dma_buffer;
	struct snd_card *card = substream->pcm->card;
	size_t orig_size = size;
	int err;

	do {
		err = do_alloc_pages(card, dmab->dev.type, dmab->dev.dev,
				     substream->stream, size, dmab);
		if (err != -ENOMEM)
			return err;
		if (no_fallback)
			break;
		size >>= 1;
	} while (size >= snd_minimum_buffer);
	dmab->bytes = 0;  
	pr_warn("ALSA pcmC%dD%d%c,%d:%s: cannot preallocate for size %zu\n",
		substream->pcm->card->number, substream->pcm->device,
		substream->stream ? 'c' : 'p', substream->number,
		substream->pcm->name, orig_size);
	return -ENOMEM;
}

 
void snd_pcm_lib_preallocate_free(struct snd_pcm_substream *substream)
{
	do_free_pages(substream->pcm->card, &substream->dma_buffer);
}

 
void snd_pcm_lib_preallocate_free_for_all(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	int stream;

	for_each_pcm_substream(pcm, stream, substream)
		snd_pcm_lib_preallocate_free(substream);
}
EXPORT_SYMBOL(snd_pcm_lib_preallocate_free_for_all);

#ifdef CONFIG_SND_VERBOSE_PROCFS
 
static void snd_pcm_lib_preallocate_proc_read(struct snd_info_entry *entry,
					      struct snd_info_buffer *buffer)
{
	struct snd_pcm_substream *substream = entry->private_data;
	snd_iprintf(buffer, "%lu\n", (unsigned long) substream->dma_buffer.bytes / 1024);
}

 
static void snd_pcm_lib_preallocate_max_proc_read(struct snd_info_entry *entry,
						  struct snd_info_buffer *buffer)
{
	struct snd_pcm_substream *substream = entry->private_data;
	snd_iprintf(buffer, "%lu\n", (unsigned long) substream->dma_max / 1024);
}

 
static void snd_pcm_lib_preallocate_proc_write(struct snd_info_entry *entry,
					       struct snd_info_buffer *buffer)
{
	struct snd_pcm_substream *substream = entry->private_data;
	struct snd_card *card = substream->pcm->card;
	char line[64], str[64];
	size_t size;
	struct snd_dma_buffer new_dmab;

	mutex_lock(&substream->pcm->open_mutex);
	if (substream->runtime) {
		buffer->error = -EBUSY;
		goto unlock;
	}
	if (!snd_info_get_line(buffer, line, sizeof(line))) {
		snd_info_get_str(str, line, sizeof(str));
		size = simple_strtoul(str, NULL, 10) * 1024;
		if ((size != 0 && size < 8192) || size > substream->dma_max) {
			buffer->error = -EINVAL;
			goto unlock;
		}
		if (substream->dma_buffer.bytes == size)
			goto unlock;
		memset(&new_dmab, 0, sizeof(new_dmab));
		new_dmab.dev = substream->dma_buffer.dev;
		if (size > 0) {
			if (do_alloc_pages(card,
					   substream->dma_buffer.dev.type,
					   substream->dma_buffer.dev.dev,
					   substream->stream,
					   size, &new_dmab) < 0) {
				buffer->error = -ENOMEM;
				pr_debug("ALSA pcmC%dD%d%c,%d:%s: cannot preallocate for size %zu\n",
					 substream->pcm->card->number, substream->pcm->device,
					 substream->stream ? 'c' : 'p', substream->number,
					 substream->pcm->name, size);
				goto unlock;
			}
			substream->buffer_bytes_max = size;
		} else {
			substream->buffer_bytes_max = UINT_MAX;
		}
		if (substream->dma_buffer.area)
			do_free_pages(card, &substream->dma_buffer);
		substream->dma_buffer = new_dmab;
	} else {
		buffer->error = -EINVAL;
	}
 unlock:
	mutex_unlock(&substream->pcm->open_mutex);
}

static inline void preallocate_info_init(struct snd_pcm_substream *substream)
{
	struct snd_info_entry *entry;

	entry = snd_info_create_card_entry(substream->pcm->card, "prealloc",
					   substream->proc_root);
	if (entry) {
		snd_info_set_text_ops(entry, substream,
				      snd_pcm_lib_preallocate_proc_read);
		entry->c.text.write = snd_pcm_lib_preallocate_proc_write;
		entry->mode |= 0200;
	}
	entry = snd_info_create_card_entry(substream->pcm->card, "prealloc_max",
					   substream->proc_root);
	if (entry)
		snd_info_set_text_ops(entry, substream,
				      snd_pcm_lib_preallocate_max_proc_read);
}

#else  
static inline void preallocate_info_init(struct snd_pcm_substream *substream)
{
}
#endif  

 
static int preallocate_pages(struct snd_pcm_substream *substream,
			      int type, struct device *data,
			      size_t size, size_t max, bool managed)
{
	int err;

	if (snd_BUG_ON(substream->dma_buffer.dev.type))
		return -EINVAL;

	substream->dma_buffer.dev.type = type;
	substream->dma_buffer.dev.dev = data;

	if (size > 0) {
		if (!max) {
			 
			err = preallocate_pcm_pages(substream, size, true);
			if (err < 0)
				return err;
		} else if (preallocate_dma &&
			   substream->number < maximum_substreams) {
			err = preallocate_pcm_pages(substream, size, false);
			if (err < 0 && err != -ENOMEM)
				return err;
		}
	}

	if (substream->dma_buffer.bytes > 0)
		substream->buffer_bytes_max = substream->dma_buffer.bytes;
	substream->dma_max = max;
	if (max > 0)
		preallocate_info_init(substream);
	if (managed)
		substream->managed_buffer_alloc = 1;
	return 0;
}

static int preallocate_pages_for_all(struct snd_pcm *pcm, int type,
				      void *data, size_t size, size_t max,
				      bool managed)
{
	struct snd_pcm_substream *substream;
	int stream, err;

	for_each_pcm_substream(pcm, stream, substream) {
		err = preallocate_pages(substream, type, data, size, max, managed);
		if (err < 0)
			return err;
	}
	return 0;
}

 
void snd_pcm_lib_preallocate_pages(struct snd_pcm_substream *substream,
				  int type, struct device *data,
				  size_t size, size_t max)
{
	preallocate_pages(substream, type, data, size, max, false);
}
EXPORT_SYMBOL(snd_pcm_lib_preallocate_pages);

 
void snd_pcm_lib_preallocate_pages_for_all(struct snd_pcm *pcm,
					  int type, void *data,
					  size_t size, size_t max)
{
	preallocate_pages_for_all(pcm, type, data, size, max, false);
}
EXPORT_SYMBOL(snd_pcm_lib_preallocate_pages_for_all);

 
int snd_pcm_set_managed_buffer(struct snd_pcm_substream *substream, int type,
				struct device *data, size_t size, size_t max)
{
	return preallocate_pages(substream, type, data, size, max, true);
}
EXPORT_SYMBOL(snd_pcm_set_managed_buffer);

 
int snd_pcm_set_managed_buffer_all(struct snd_pcm *pcm, int type,
				   struct device *data,
				   size_t size, size_t max)
{
	return preallocate_pages_for_all(pcm, type, data, size, max, true);
}
EXPORT_SYMBOL(snd_pcm_set_managed_buffer_all);

 
int snd_pcm_lib_malloc_pages(struct snd_pcm_substream *substream, size_t size)
{
	struct snd_card *card;
	struct snd_pcm_runtime *runtime;
	struct snd_dma_buffer *dmab = NULL;

	if (PCM_RUNTIME_CHECK(substream))
		return -EINVAL;
	if (snd_BUG_ON(substream->dma_buffer.dev.type ==
		       SNDRV_DMA_TYPE_UNKNOWN))
		return -EINVAL;
	runtime = substream->runtime;
	card = substream->pcm->card;

	if (runtime->dma_buffer_p) {
		 
		if (runtime->dma_buffer_p->bytes >= size) {
			runtime->dma_bytes = size;
			return 0;	 
		}
		snd_pcm_lib_free_pages(substream);
	}
	if (substream->dma_buffer.area != NULL &&
	    substream->dma_buffer.bytes >= size) {
		dmab = &substream->dma_buffer;  
	} else {
		 
		if (substream->dma_buffer.area && !substream->dma_max)
			return -ENOMEM;
		dmab = kzalloc(sizeof(*dmab), GFP_KERNEL);
		if (! dmab)
			return -ENOMEM;
		dmab->dev = substream->dma_buffer.dev;
		if (do_alloc_pages(card,
				   substream->dma_buffer.dev.type,
				   substream->dma_buffer.dev.dev,
				   substream->stream,
				   size, dmab) < 0) {
			kfree(dmab);
			pr_debug("ALSA pcmC%dD%d%c,%d:%s: cannot preallocate for size %zu\n",
				 substream->pcm->card->number, substream->pcm->device,
				 substream->stream ? 'c' : 'p', substream->number,
				 substream->pcm->name, size);
			return -ENOMEM;
		}
	}
	snd_pcm_set_runtime_buffer(substream, dmab);
	runtime->dma_bytes = size;
	return 1;			 
}
EXPORT_SYMBOL(snd_pcm_lib_malloc_pages);

 
int snd_pcm_lib_free_pages(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime;

	if (PCM_RUNTIME_CHECK(substream))
		return -EINVAL;
	runtime = substream->runtime;
	if (runtime->dma_area == NULL)
		return 0;
	if (runtime->dma_buffer_p != &substream->dma_buffer) {
		struct snd_card *card = substream->pcm->card;

		 
		do_free_pages(card, runtime->dma_buffer_p);
		kfree(runtime->dma_buffer_p);
	}
	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}
EXPORT_SYMBOL(snd_pcm_lib_free_pages);

int _snd_pcm_lib_alloc_vmalloc_buffer(struct snd_pcm_substream *substream,
				      size_t size, gfp_t gfp_flags)
{
	struct snd_pcm_runtime *runtime;

	if (PCM_RUNTIME_CHECK(substream))
		return -EINVAL;
	runtime = substream->runtime;
	if (runtime->dma_area) {
		if (runtime->dma_bytes >= size)
			return 0;  
		vfree(runtime->dma_area);
	}
	runtime->dma_area = __vmalloc(size, gfp_flags);
	if (!runtime->dma_area)
		return -ENOMEM;
	runtime->dma_bytes = size;
	return 1;
}
EXPORT_SYMBOL(_snd_pcm_lib_alloc_vmalloc_buffer);

 
int snd_pcm_lib_free_vmalloc_buffer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime;

	if (PCM_RUNTIME_CHECK(substream))
		return -EINVAL;
	runtime = substream->runtime;
	vfree(runtime->dma_area);
	runtime->dma_area = NULL;
	return 0;
}
EXPORT_SYMBOL(snd_pcm_lib_free_vmalloc_buffer);

 
struct page *snd_pcm_lib_get_vmalloc_page(struct snd_pcm_substream *substream,
					  unsigned long offset)
{
	return vmalloc_to_page(substream->runtime->dma_area + offset);
}
EXPORT_SYMBOL(snd_pcm_lib_get_vmalloc_page);
