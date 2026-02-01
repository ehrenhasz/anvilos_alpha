
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/capability.h>
#include <linux/device.h>
#include <linux/kernel_read_file.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/initrd.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/highmem.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/async.h>
#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/reboot.h>
#include <linux/security.h>
#include <linux/zstd.h>
#include <linux/xz.h>

#include <generated/utsrelease.h>

#include "../base.h"
#include "firmware.h"
#include "fallback.h"

MODULE_AUTHOR("Manuel Estrada Sainz");
MODULE_DESCRIPTION("Multi purpose firmware loading support");
MODULE_LICENSE("GPL");

struct firmware_cache {
	 
	spinlock_t lock;
	struct list_head head;
	int state;

#ifdef CONFIG_FW_CACHE
	 
	spinlock_t name_lock;
	struct list_head fw_names;

	struct delayed_work work;

	struct notifier_block   pm_notify;
#endif
};

struct fw_cache_entry {
	struct list_head list;
	const char *name;
};

struct fw_name_devm {
	unsigned long magic;
	const char *name;
};

static inline struct fw_priv *to_fw_priv(struct kref *ref)
{
	return container_of(ref, struct fw_priv, ref);
}

#define	FW_LOADER_NO_CACHE	0
#define	FW_LOADER_START_CACHE	1

 
DEFINE_MUTEX(fw_lock);

struct firmware_cache fw_cache;

void fw_state_init(struct fw_priv *fw_priv)
{
	struct fw_state *fw_st = &fw_priv->fw_st;

	init_completion(&fw_st->completion);
	fw_st->status = FW_STATUS_UNKNOWN;
}

static inline int fw_state_wait(struct fw_priv *fw_priv)
{
	return __fw_state_wait_common(fw_priv, MAX_SCHEDULE_TIMEOUT);
}

static void fw_cache_piggyback_on_request(struct fw_priv *fw_priv);

static struct fw_priv *__allocate_fw_priv(const char *fw_name,
					  struct firmware_cache *fwc,
					  void *dbuf,
					  size_t size,
					  size_t offset,
					  u32 opt_flags)
{
	struct fw_priv *fw_priv;

	 
	if ((opt_flags & FW_OPT_PARTIAL) && !dbuf)
		return NULL;

	 
	if (offset != 0 && !(opt_flags & FW_OPT_PARTIAL))
		return NULL;

	fw_priv = kzalloc(sizeof(*fw_priv), GFP_ATOMIC);
	if (!fw_priv)
		return NULL;

	fw_priv->fw_name = kstrdup_const(fw_name, GFP_ATOMIC);
	if (!fw_priv->fw_name) {
		kfree(fw_priv);
		return NULL;
	}

	kref_init(&fw_priv->ref);
	fw_priv->fwc = fwc;
	fw_priv->data = dbuf;
	fw_priv->allocated_size = size;
	fw_priv->offset = offset;
	fw_priv->opt_flags = opt_flags;
	fw_state_init(fw_priv);
#ifdef CONFIG_FW_LOADER_USER_HELPER
	INIT_LIST_HEAD(&fw_priv->pending_list);
#endif

	pr_debug("%s: fw-%s fw_priv=%p\n", __func__, fw_name, fw_priv);

	return fw_priv;
}

static struct fw_priv *__lookup_fw_priv(const char *fw_name)
{
	struct fw_priv *tmp;
	struct firmware_cache *fwc = &fw_cache;

	list_for_each_entry(tmp, &fwc->head, list)
		if (!strcmp(tmp->fw_name, fw_name))
			return tmp;
	return NULL;
}

 
int alloc_lookup_fw_priv(const char *fw_name, struct firmware_cache *fwc,
			 struct fw_priv **fw_priv, void *dbuf, size_t size,
			 size_t offset, u32 opt_flags)
{
	struct fw_priv *tmp;

	spin_lock(&fwc->lock);
	 
	if (!(opt_flags & (FW_OPT_NOCACHE | FW_OPT_PARTIAL))) {
		tmp = __lookup_fw_priv(fw_name);
		if (tmp) {
			kref_get(&tmp->ref);
			spin_unlock(&fwc->lock);
			*fw_priv = tmp;
			pr_debug("batched request - sharing the same struct fw_priv and lookup for multiple requests\n");
			return 1;
		}
	}

	tmp = __allocate_fw_priv(fw_name, fwc, dbuf, size, offset, opt_flags);
	if (tmp) {
		INIT_LIST_HEAD(&tmp->list);
		if (!(opt_flags & FW_OPT_NOCACHE))
			list_add(&tmp->list, &fwc->head);
	}
	spin_unlock(&fwc->lock);

	*fw_priv = tmp;

	return tmp ? 0 : -ENOMEM;
}

static void __free_fw_priv(struct kref *ref)
	__releases(&fwc->lock)
{
	struct fw_priv *fw_priv = to_fw_priv(ref);
	struct firmware_cache *fwc = fw_priv->fwc;

	pr_debug("%s: fw-%s fw_priv=%p data=%p size=%u\n",
		 __func__, fw_priv->fw_name, fw_priv, fw_priv->data,
		 (unsigned int)fw_priv->size);

	list_del(&fw_priv->list);
	spin_unlock(&fwc->lock);

	if (fw_is_paged_buf(fw_priv))
		fw_free_paged_buf(fw_priv);
	else if (!fw_priv->allocated_size)
		vfree(fw_priv->data);

	kfree_const(fw_priv->fw_name);
	kfree(fw_priv);
}

void free_fw_priv(struct fw_priv *fw_priv)
{
	struct firmware_cache *fwc = fw_priv->fwc;
	spin_lock(&fwc->lock);
	if (!kref_put(&fw_priv->ref, __free_fw_priv))
		spin_unlock(&fwc->lock);
}

#ifdef CONFIG_FW_LOADER_PAGED_BUF
bool fw_is_paged_buf(struct fw_priv *fw_priv)
{
	return fw_priv->is_paged_buf;
}

void fw_free_paged_buf(struct fw_priv *fw_priv)
{
	int i;

	if (!fw_priv->pages)
		return;

	vunmap(fw_priv->data);

	for (i = 0; i < fw_priv->nr_pages; i++)
		__free_page(fw_priv->pages[i]);
	kvfree(fw_priv->pages);
	fw_priv->pages = NULL;
	fw_priv->page_array_size = 0;
	fw_priv->nr_pages = 0;
	fw_priv->data = NULL;
	fw_priv->size = 0;
}

int fw_grow_paged_buf(struct fw_priv *fw_priv, int pages_needed)
{
	 
	if (fw_priv->page_array_size < pages_needed) {
		int new_array_size = max(pages_needed,
					 fw_priv->page_array_size * 2);
		struct page **new_pages;

		new_pages = kvmalloc_array(new_array_size, sizeof(void *),
					   GFP_KERNEL);
		if (!new_pages)
			return -ENOMEM;
		memcpy(new_pages, fw_priv->pages,
		       fw_priv->page_array_size * sizeof(void *));
		memset(&new_pages[fw_priv->page_array_size], 0, sizeof(void *) *
		       (new_array_size - fw_priv->page_array_size));
		kvfree(fw_priv->pages);
		fw_priv->pages = new_pages;
		fw_priv->page_array_size = new_array_size;
	}

	while (fw_priv->nr_pages < pages_needed) {
		fw_priv->pages[fw_priv->nr_pages] =
			alloc_page(GFP_KERNEL | __GFP_HIGHMEM);

		if (!fw_priv->pages[fw_priv->nr_pages])
			return -ENOMEM;
		fw_priv->nr_pages++;
	}

	return 0;
}

int fw_map_paged_buf(struct fw_priv *fw_priv)
{
	 
	if (!fw_priv->pages)
		return 0;

	vunmap(fw_priv->data);
	fw_priv->data = vmap(fw_priv->pages, fw_priv->nr_pages, 0,
			     PAGE_KERNEL_RO);
	if (!fw_priv->data)
		return -ENOMEM;

	return 0;
}
#endif

 
#ifdef CONFIG_FW_LOADER_COMPRESS_ZSTD
static int fw_decompress_zstd(struct device *dev, struct fw_priv *fw_priv,
			      size_t in_size, const void *in_buffer)
{
	size_t len, out_size, workspace_size;
	void *workspace, *out_buf;
	zstd_dctx *ctx;
	int err;

	if (fw_priv->allocated_size) {
		out_size = fw_priv->allocated_size;
		out_buf = fw_priv->data;
	} else {
		zstd_frame_header params;

		if (zstd_get_frame_header(&params, in_buffer, in_size) ||
		    params.frameContentSize == ZSTD_CONTENTSIZE_UNKNOWN) {
			dev_dbg(dev, "%s: invalid zstd header\n", __func__);
			return -EINVAL;
		}
		out_size = params.frameContentSize;
		out_buf = vzalloc(out_size);
		if (!out_buf)
			return -ENOMEM;
	}

	workspace_size = zstd_dctx_workspace_bound();
	workspace = kvzalloc(workspace_size, GFP_KERNEL);
	if (!workspace) {
		err = -ENOMEM;
		goto error;
	}

	ctx = zstd_init_dctx(workspace, workspace_size);
	if (!ctx) {
		dev_dbg(dev, "%s: failed to initialize context\n", __func__);
		err = -EINVAL;
		goto error;
	}

	len = zstd_decompress_dctx(ctx, out_buf, out_size, in_buffer, in_size);
	if (zstd_is_error(len)) {
		dev_dbg(dev, "%s: failed to decompress: %d\n", __func__,
			zstd_get_error_code(len));
		err = -EINVAL;
		goto error;
	}

	if (!fw_priv->allocated_size)
		fw_priv->data = out_buf;
	fw_priv->size = len;
	err = 0;

 error:
	kvfree(workspace);
	if (err && !fw_priv->allocated_size)
		vfree(out_buf);
	return err;
}
#endif  

 
#ifdef CONFIG_FW_LOADER_COMPRESS_XZ
 
static int fw_decompress_xz_error(struct device *dev, enum xz_ret xz_ret)
{
	if (xz_ret != XZ_STREAM_END) {
		dev_warn(dev, "xz decompression failed (xz_ret=%d)\n", xz_ret);
		return xz_ret == XZ_MEM_ERROR ? -ENOMEM : -EINVAL;
	}
	return 0;
}

 
static int fw_decompress_xz_single(struct device *dev, struct fw_priv *fw_priv,
				   size_t in_size, const void *in_buffer)
{
	struct xz_dec *xz_dec;
	struct xz_buf xz_buf;
	enum xz_ret xz_ret;

	xz_dec = xz_dec_init(XZ_SINGLE, (u32)-1);
	if (!xz_dec)
		return -ENOMEM;

	xz_buf.in_size = in_size;
	xz_buf.in = in_buffer;
	xz_buf.in_pos = 0;
	xz_buf.out_size = fw_priv->allocated_size;
	xz_buf.out = fw_priv->data;
	xz_buf.out_pos = 0;

	xz_ret = xz_dec_run(xz_dec, &xz_buf);
	xz_dec_end(xz_dec);

	fw_priv->size = xz_buf.out_pos;
	return fw_decompress_xz_error(dev, xz_ret);
}

 
static int fw_decompress_xz_pages(struct device *dev, struct fw_priv *fw_priv,
				  size_t in_size, const void *in_buffer)
{
	struct xz_dec *xz_dec;
	struct xz_buf xz_buf;
	enum xz_ret xz_ret;
	struct page *page;
	int err = 0;

	xz_dec = xz_dec_init(XZ_DYNALLOC, (u32)-1);
	if (!xz_dec)
		return -ENOMEM;

	xz_buf.in_size = in_size;
	xz_buf.in = in_buffer;
	xz_buf.in_pos = 0;

	fw_priv->is_paged_buf = true;
	fw_priv->size = 0;
	do {
		if (fw_grow_paged_buf(fw_priv, fw_priv->nr_pages + 1)) {
			err = -ENOMEM;
			goto out;
		}

		 
		page = fw_priv->pages[fw_priv->nr_pages - 1];
		xz_buf.out = kmap_local_page(page);
		xz_buf.out_pos = 0;
		xz_buf.out_size = PAGE_SIZE;
		xz_ret = xz_dec_run(xz_dec, &xz_buf);
		kunmap_local(xz_buf.out);
		fw_priv->size += xz_buf.out_pos;
		 
		if (xz_buf.out_pos != PAGE_SIZE)
			break;
	} while (xz_ret == XZ_OK);

	err = fw_decompress_xz_error(dev, xz_ret);
	if (!err)
		err = fw_map_paged_buf(fw_priv);

 out:
	xz_dec_end(xz_dec);
	return err;
}

static int fw_decompress_xz(struct device *dev, struct fw_priv *fw_priv,
			    size_t in_size, const void *in_buffer)
{
	 
	if (fw_priv->data)
		return fw_decompress_xz_single(dev, fw_priv, in_size, in_buffer);
	else
		return fw_decompress_xz_pages(dev, fw_priv, in_size, in_buffer);
}
#endif  

 
static char fw_path_para[256];
static const char * const fw_path[] = {
	fw_path_para,
	"/lib/firmware/updates/" UTS_RELEASE,
	"/lib/firmware/updates",
	"/lib/firmware/" UTS_RELEASE,
	"/lib/firmware"
};

 
module_param_string(path, fw_path_para, sizeof(fw_path_para), 0644);
MODULE_PARM_DESC(path, "customized firmware image search path with a higher priority than default path");

static int
fw_get_filesystem_firmware(struct device *device, struct fw_priv *fw_priv,
			   const char *suffix,
			   int (*decompress)(struct device *dev,
					     struct fw_priv *fw_priv,
					     size_t in_size,
					     const void *in_buffer))
{
	size_t size;
	int i, len, maxlen = 0;
	int rc = -ENOENT;
	char *path, *nt = NULL;
	size_t msize = INT_MAX;
	void *buffer = NULL;

	 
	if (!decompress && fw_priv->data) {
		buffer = fw_priv->data;
		msize = fw_priv->allocated_size;
	}

	path = __getname();
	if (!path)
		return -ENOMEM;

	wait_for_initramfs();
	for (i = 0; i < ARRAY_SIZE(fw_path); i++) {
		size_t file_size = 0;
		size_t *file_size_ptr = NULL;

		 
		if (!fw_path[i][0])
			continue;

		 
		maxlen = strlen(fw_path[i]);
		if (i == 0) {
			nt = strchr(fw_path[i], '\n');
			if (nt)
				maxlen = nt - fw_path[i];
		}

		len = snprintf(path, PATH_MAX, "%.*s/%s%s",
			       maxlen, fw_path[i],
			       fw_priv->fw_name, suffix);
		if (len >= PATH_MAX) {
			rc = -ENAMETOOLONG;
			break;
		}

		fw_priv->size = 0;

		 
		if ((fw_priv->opt_flags & FW_OPT_PARTIAL) && buffer)
			file_size_ptr = &file_size;

		 
		rc = kernel_read_file_from_path_initns(path, fw_priv->offset,
						       &buffer, msize,
						       file_size_ptr,
						       READING_FIRMWARE);
		if (rc < 0) {
			if (rc != -ENOENT)
				dev_warn(device, "loading %s failed with error %d\n",
					 path, rc);
			else
				dev_dbg(device, "loading %s failed for no such file or directory.\n",
					 path);
			continue;
		}
		size = rc;
		rc = 0;

		dev_dbg(device, "Loading firmware from %s\n", path);
		if (decompress) {
			dev_dbg(device, "f/w decompressing %s\n",
				fw_priv->fw_name);
			rc = decompress(device, fw_priv, size, buffer);
			 
			vfree(buffer);
			buffer = NULL;
			if (rc) {
				fw_free_paged_buf(fw_priv);
				continue;
			}
		} else {
			dev_dbg(device, "direct-loading %s\n",
				fw_priv->fw_name);
			if (!fw_priv->data)
				fw_priv->data = buffer;
			fw_priv->size = size;
		}
		fw_state_done(fw_priv);
		break;
	}
	__putname(path);

	return rc;
}

 
static void firmware_free_data(const struct firmware *fw)
{
	 
	if (!fw->priv) {
		vfree(fw->data);
		return;
	}
	free_fw_priv(fw->priv);
}

 
static void fw_set_page_data(struct fw_priv *fw_priv, struct firmware *fw)
{
	fw->priv = fw_priv;
	fw->size = fw_priv->size;
	fw->data = fw_priv->data;

	pr_debug("%s: fw-%s fw_priv=%p data=%p size=%u\n",
		 __func__, fw_priv->fw_name, fw_priv, fw_priv->data,
		 (unsigned int)fw_priv->size);
}

#ifdef CONFIG_FW_CACHE
static void fw_name_devm_release(struct device *dev, void *res)
{
	struct fw_name_devm *fwn = res;

	if (fwn->magic == (unsigned long)&fw_cache)
		pr_debug("%s: fw_name-%s devm-%p released\n",
				__func__, fwn->name, res);
	kfree_const(fwn->name);
}

static int fw_devm_match(struct device *dev, void *res,
		void *match_data)
{
	struct fw_name_devm *fwn = res;

	return (fwn->magic == (unsigned long)&fw_cache) &&
		!strcmp(fwn->name, match_data);
}

static struct fw_name_devm *fw_find_devm_name(struct device *dev,
		const char *name)
{
	struct fw_name_devm *fwn;

	fwn = devres_find(dev, fw_name_devm_release,
			  fw_devm_match, (void *)name);
	return fwn;
}

static bool fw_cache_is_setup(struct device *dev, const char *name)
{
	struct fw_name_devm *fwn;

	fwn = fw_find_devm_name(dev, name);
	if (fwn)
		return true;

	return false;
}

 
static int fw_add_devm_name(struct device *dev, const char *name)
{
	struct fw_name_devm *fwn;

	if (fw_cache_is_setup(dev, name))
		return 0;

	fwn = devres_alloc(fw_name_devm_release, sizeof(struct fw_name_devm),
			   GFP_KERNEL);
	if (!fwn)
		return -ENOMEM;
	fwn->name = kstrdup_const(name, GFP_KERNEL);
	if (!fwn->name) {
		devres_free(fwn);
		return -ENOMEM;
	}

	fwn->magic = (unsigned long)&fw_cache;
	devres_add(dev, fwn);

	return 0;
}
#else
static bool fw_cache_is_setup(struct device *dev, const char *name)
{
	return false;
}

static int fw_add_devm_name(struct device *dev, const char *name)
{
	return 0;
}
#endif

int assign_fw(struct firmware *fw, struct device *device)
{
	struct fw_priv *fw_priv = fw->priv;
	int ret;

	mutex_lock(&fw_lock);
	if (!fw_priv->size || fw_state_is_aborted(fw_priv)) {
		mutex_unlock(&fw_lock);
		return -ENOENT;
	}

	 
	 
	if (device && (fw_priv->opt_flags & FW_OPT_UEVENT) &&
	    !(fw_priv->opt_flags & FW_OPT_NOCACHE)) {
		ret = fw_add_devm_name(device, fw_priv->fw_name);
		if (ret) {
			mutex_unlock(&fw_lock);
			return ret;
		}
	}

	 
	if (!(fw_priv->opt_flags & FW_OPT_NOCACHE) &&
	    fw_priv->fwc->state == FW_LOADER_START_CACHE)
		fw_cache_piggyback_on_request(fw_priv);

	 
	fw_set_page_data(fw_priv, fw);
	mutex_unlock(&fw_lock);
	return 0;
}

 
static int
_request_firmware_prepare(struct firmware **firmware_p, const char *name,
			  struct device *device, void *dbuf, size_t size,
			  size_t offset, u32 opt_flags)
{
	struct firmware *firmware;
	struct fw_priv *fw_priv;
	int ret;

	*firmware_p = firmware = kzalloc(sizeof(*firmware), GFP_KERNEL);
	if (!firmware) {
		dev_err(device, "%s: kmalloc(struct firmware) failed\n",
			__func__);
		return -ENOMEM;
	}

	if (firmware_request_builtin_buf(firmware, name, dbuf, size)) {
		dev_dbg(device, "using built-in %s\n", name);
		return 0;  
	}

	ret = alloc_lookup_fw_priv(name, &fw_cache, &fw_priv, dbuf, size,
				   offset, opt_flags);

	 
	firmware->priv = fw_priv;

	if (ret > 0) {
		ret = fw_state_wait(fw_priv);
		if (!ret) {
			fw_set_page_data(fw_priv, firmware);
			return 0;  
		}
	}

	if (ret < 0)
		return ret;
	return 1;  
}

 
static void fw_abort_batch_reqs(struct firmware *fw)
{
	struct fw_priv *fw_priv;

	 
	if (!fw || !fw->priv)
		return;

	fw_priv = fw->priv;
	mutex_lock(&fw_lock);
	if (!fw_state_is_aborted(fw_priv))
		fw_state_aborted(fw_priv);
	mutex_unlock(&fw_lock);
}

#if defined(CONFIG_FW_LOADER_DEBUG)
#include <crypto/hash.h>
#include <crypto/sha2.h>

static void fw_log_firmware_info(const struct firmware *fw, const char *name, struct device *device)
{
	struct shash_desc *shash;
	struct crypto_shash *alg;
	u8 *sha256buf;
	char *outbuf;

	alg = crypto_alloc_shash("sha256", 0, 0);
	if (IS_ERR(alg))
		return;

	sha256buf = kmalloc(SHA256_DIGEST_SIZE, GFP_KERNEL);
	outbuf = kmalloc(SHA256_BLOCK_SIZE + 1, GFP_KERNEL);
	shash = kmalloc(sizeof(*shash) + crypto_shash_descsize(alg), GFP_KERNEL);
	if (!sha256buf || !outbuf || !shash)
		goto out_free;

	shash->tfm = alg;

	if (crypto_shash_digest(shash, fw->data, fw->size, sha256buf) < 0)
		goto out_shash;

	for (int i = 0; i < SHA256_DIGEST_SIZE; i++)
		sprintf(&outbuf[i * 2], "%02x", sha256buf[i]);
	outbuf[SHA256_BLOCK_SIZE] = 0;
	dev_dbg(device, "Loaded FW: %s, sha256: %s\n", name, outbuf);

out_shash:
	crypto_free_shash(alg);
out_free:
	kfree(shash);
	kfree(outbuf);
	kfree(sha256buf);
}
#else
static void fw_log_firmware_info(const struct firmware *fw, const char *name,
				 struct device *device)
{}
#endif

 
static int
_request_firmware(const struct firmware **firmware_p, const char *name,
		  struct device *device, void *buf, size_t size,
		  size_t offset, u32 opt_flags)
{
	struct firmware *fw = NULL;
	struct cred *kern_cred = NULL;
	const struct cred *old_cred;
	bool nondirect = false;
	int ret;

	if (!firmware_p)
		return -EINVAL;

	if (!name || name[0] == '\0') {
		ret = -EINVAL;
		goto out;
	}

	ret = _request_firmware_prepare(&fw, name, device, buf, size,
					offset, opt_flags);
	if (ret <= 0)  
		goto out;

	 
	kern_cred = prepare_kernel_cred(&init_task);
	if (!kern_cred) {
		ret = -ENOMEM;
		goto out;
	}
	old_cred = override_creds(kern_cred);

	ret = fw_get_filesystem_firmware(device, fw->priv, "", NULL);

	 
	if (!(opt_flags & FW_OPT_PARTIAL))
		nondirect = true;

#ifdef CONFIG_FW_LOADER_COMPRESS_ZSTD
	if (ret == -ENOENT && nondirect)
		ret = fw_get_filesystem_firmware(device, fw->priv, ".zst",
						 fw_decompress_zstd);
#endif
#ifdef CONFIG_FW_LOADER_COMPRESS_XZ
	if (ret == -ENOENT && nondirect)
		ret = fw_get_filesystem_firmware(device, fw->priv, ".xz",
						 fw_decompress_xz);
#endif
	if (ret == -ENOENT && nondirect)
		ret = firmware_fallback_platform(fw->priv);

	if (ret) {
		if (!(opt_flags & FW_OPT_NO_WARN))
			dev_warn(device,
				 "Direct firmware load for %s failed with error %d\n",
				 name, ret);
		if (nondirect)
			ret = firmware_fallback_sysfs(fw, name, device,
						      opt_flags, ret);
	} else
		ret = assign_fw(fw, device);

	revert_creds(old_cred);
	put_cred(kern_cred);

out:
	if (ret < 0) {
		fw_abort_batch_reqs(fw);
		release_firmware(fw);
		fw = NULL;
	} else {
		fw_log_firmware_info(fw, name, device);
	}

	*firmware_p = fw;
	return ret;
}

 
int
request_firmware(const struct firmware **firmware_p, const char *name,
		 struct device *device)
{
	int ret;

	 
	__module_get(THIS_MODULE);
	ret = _request_firmware(firmware_p, name, device, NULL, 0, 0,
				FW_OPT_UEVENT);
	module_put(THIS_MODULE);
	return ret;
}
EXPORT_SYMBOL(request_firmware);

 
int firmware_request_nowarn(const struct firmware **firmware, const char *name,
			    struct device *device)
{
	int ret;

	 
	__module_get(THIS_MODULE);
	ret = _request_firmware(firmware, name, device, NULL, 0, 0,
				FW_OPT_UEVENT | FW_OPT_NO_WARN);
	module_put(THIS_MODULE);
	return ret;
}
EXPORT_SYMBOL_GPL(firmware_request_nowarn);

 
int request_firmware_direct(const struct firmware **firmware_p,
			    const char *name, struct device *device)
{
	int ret;

	__module_get(THIS_MODULE);
	ret = _request_firmware(firmware_p, name, device, NULL, 0, 0,
				FW_OPT_UEVENT | FW_OPT_NO_WARN |
				FW_OPT_NOFALLBACK_SYSFS);
	module_put(THIS_MODULE);
	return ret;
}
EXPORT_SYMBOL_GPL(request_firmware_direct);

 
int firmware_request_platform(const struct firmware **firmware,
			      const char *name, struct device *device)
{
	int ret;

	 
	__module_get(THIS_MODULE);
	ret = _request_firmware(firmware, name, device, NULL, 0, 0,
				FW_OPT_UEVENT | FW_OPT_FALLBACK_PLATFORM);
	module_put(THIS_MODULE);
	return ret;
}
EXPORT_SYMBOL_GPL(firmware_request_platform);

 
int firmware_request_cache(struct device *device, const char *name)
{
	int ret;

	mutex_lock(&fw_lock);
	ret = fw_add_devm_name(device, name);
	mutex_unlock(&fw_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(firmware_request_cache);

 
int
request_firmware_into_buf(const struct firmware **firmware_p, const char *name,
			  struct device *device, void *buf, size_t size)
{
	int ret;

	if (fw_cache_is_setup(device, name))
		return -EOPNOTSUPP;

	__module_get(THIS_MODULE);
	ret = _request_firmware(firmware_p, name, device, buf, size, 0,
				FW_OPT_UEVENT | FW_OPT_NOCACHE);
	module_put(THIS_MODULE);
	return ret;
}
EXPORT_SYMBOL(request_firmware_into_buf);

 
int
request_partial_firmware_into_buf(const struct firmware **firmware_p,
				  const char *name, struct device *device,
				  void *buf, size_t size, size_t offset)
{
	int ret;

	if (fw_cache_is_setup(device, name))
		return -EOPNOTSUPP;

	__module_get(THIS_MODULE);
	ret = _request_firmware(firmware_p, name, device, buf, size, offset,
				FW_OPT_UEVENT | FW_OPT_NOCACHE |
				FW_OPT_PARTIAL);
	module_put(THIS_MODULE);
	return ret;
}
EXPORT_SYMBOL(request_partial_firmware_into_buf);

 
void release_firmware(const struct firmware *fw)
{
	if (fw) {
		if (!firmware_is_builtin(fw))
			firmware_free_data(fw);
		kfree(fw);
	}
}
EXPORT_SYMBOL(release_firmware);

 
struct firmware_work {
	struct work_struct work;
	struct module *module;
	const char *name;
	struct device *device;
	void *context;
	void (*cont)(const struct firmware *fw, void *context);
	u32 opt_flags;
};

static void request_firmware_work_func(struct work_struct *work)
{
	struct firmware_work *fw_work;
	const struct firmware *fw;

	fw_work = container_of(work, struct firmware_work, work);

	_request_firmware(&fw, fw_work->name, fw_work->device, NULL, 0, 0,
			  fw_work->opt_flags);
	fw_work->cont(fw, fw_work->context);
	put_device(fw_work->device);  

	module_put(fw_work->module);
	kfree_const(fw_work->name);
	kfree(fw_work);
}

 
int
request_firmware_nowait(
	struct module *module, bool uevent,
	const char *name, struct device *device, gfp_t gfp, void *context,
	void (*cont)(const struct firmware *fw, void *context))
{
	struct firmware_work *fw_work;

	fw_work = kzalloc(sizeof(struct firmware_work), gfp);
	if (!fw_work)
		return -ENOMEM;

	fw_work->module = module;
	fw_work->name = kstrdup_const(name, gfp);
	if (!fw_work->name) {
		kfree(fw_work);
		return -ENOMEM;
	}
	fw_work->device = device;
	fw_work->context = context;
	fw_work->cont = cont;
	fw_work->opt_flags = FW_OPT_NOWAIT |
		(uevent ? FW_OPT_UEVENT : FW_OPT_USERHELPER);

	if (!uevent && fw_cache_is_setup(device, name)) {
		kfree_const(fw_work->name);
		kfree(fw_work);
		return -EOPNOTSUPP;
	}

	if (!try_module_get(module)) {
		kfree_const(fw_work->name);
		kfree(fw_work);
		return -EFAULT;
	}

	get_device(fw_work->device);
	INIT_WORK(&fw_work->work, request_firmware_work_func);
	schedule_work(&fw_work->work);
	return 0;
}
EXPORT_SYMBOL(request_firmware_nowait);

#ifdef CONFIG_FW_CACHE
static ASYNC_DOMAIN_EXCLUSIVE(fw_cache_domain);

 
static int cache_firmware(const char *fw_name)
{
	int ret;
	const struct firmware *fw;

	pr_debug("%s: %s\n", __func__, fw_name);

	ret = request_firmware(&fw, fw_name, NULL);
	if (!ret)
		kfree(fw);

	pr_debug("%s: %s ret=%d\n", __func__, fw_name, ret);

	return ret;
}

static struct fw_priv *lookup_fw_priv(const char *fw_name)
{
	struct fw_priv *tmp;
	struct firmware_cache *fwc = &fw_cache;

	spin_lock(&fwc->lock);
	tmp = __lookup_fw_priv(fw_name);
	spin_unlock(&fwc->lock);

	return tmp;
}

 
static int uncache_firmware(const char *fw_name)
{
	struct fw_priv *fw_priv;
	struct firmware fw;

	pr_debug("%s: %s\n", __func__, fw_name);

	if (firmware_request_builtin(&fw, fw_name))
		return 0;

	fw_priv = lookup_fw_priv(fw_name);
	if (fw_priv) {
		free_fw_priv(fw_priv);
		return 0;
	}

	return -EINVAL;
}

static struct fw_cache_entry *alloc_fw_cache_entry(const char *name)
{
	struct fw_cache_entry *fce;

	fce = kzalloc(sizeof(*fce), GFP_ATOMIC);
	if (!fce)
		goto exit;

	fce->name = kstrdup_const(name, GFP_ATOMIC);
	if (!fce->name) {
		kfree(fce);
		fce = NULL;
		goto exit;
	}
exit:
	return fce;
}

static int __fw_entry_found(const char *name)
{
	struct firmware_cache *fwc = &fw_cache;
	struct fw_cache_entry *fce;

	list_for_each_entry(fce, &fwc->fw_names, list) {
		if (!strcmp(fce->name, name))
			return 1;
	}
	return 0;
}

static void fw_cache_piggyback_on_request(struct fw_priv *fw_priv)
{
	const char *name = fw_priv->fw_name;
	struct firmware_cache *fwc = fw_priv->fwc;
	struct fw_cache_entry *fce;

	spin_lock(&fwc->name_lock);
	if (__fw_entry_found(name))
		goto found;

	fce = alloc_fw_cache_entry(name);
	if (fce) {
		list_add(&fce->list, &fwc->fw_names);
		kref_get(&fw_priv->ref);
		pr_debug("%s: fw: %s\n", __func__, name);
	}
found:
	spin_unlock(&fwc->name_lock);
}

static void free_fw_cache_entry(struct fw_cache_entry *fce)
{
	kfree_const(fce->name);
	kfree(fce);
}

static void __async_dev_cache_fw_image(void *fw_entry,
				       async_cookie_t cookie)
{
	struct fw_cache_entry *fce = fw_entry;
	struct firmware_cache *fwc = &fw_cache;
	int ret;

	ret = cache_firmware(fce->name);
	if (ret) {
		spin_lock(&fwc->name_lock);
		list_del(&fce->list);
		spin_unlock(&fwc->name_lock);

		free_fw_cache_entry(fce);
	}
}

 
static void dev_create_fw_entry(struct device *dev, void *res,
				void *data)
{
	struct fw_name_devm *fwn = res;
	const char *fw_name = fwn->name;
	struct list_head *head = data;
	struct fw_cache_entry *fce;

	fce = alloc_fw_cache_entry(fw_name);
	if (fce)
		list_add(&fce->list, head);
}

static int devm_name_match(struct device *dev, void *res,
			   void *match_data)
{
	struct fw_name_devm *fwn = res;
	return (fwn->magic == (unsigned long)match_data);
}

static void dev_cache_fw_image(struct device *dev, void *data)
{
	LIST_HEAD(todo);
	struct fw_cache_entry *fce;
	struct fw_cache_entry *fce_next;
	struct firmware_cache *fwc = &fw_cache;

	devres_for_each_res(dev, fw_name_devm_release,
			    devm_name_match, &fw_cache,
			    dev_create_fw_entry, &todo);

	list_for_each_entry_safe(fce, fce_next, &todo, list) {
		list_del(&fce->list);

		spin_lock(&fwc->name_lock);
		 
		if (!__fw_entry_found(fce->name)) {
			list_add(&fce->list, &fwc->fw_names);
		} else {
			free_fw_cache_entry(fce);
			fce = NULL;
		}
		spin_unlock(&fwc->name_lock);

		if (fce)
			async_schedule_domain(__async_dev_cache_fw_image,
					      (void *)fce,
					      &fw_cache_domain);
	}
}

static void __device_uncache_fw_images(void)
{
	struct firmware_cache *fwc = &fw_cache;
	struct fw_cache_entry *fce;

	spin_lock(&fwc->name_lock);
	while (!list_empty(&fwc->fw_names)) {
		fce = list_entry(fwc->fw_names.next,
				struct fw_cache_entry, list);
		list_del(&fce->list);
		spin_unlock(&fwc->name_lock);

		uncache_firmware(fce->name);
		free_fw_cache_entry(fce);

		spin_lock(&fwc->name_lock);
	}
	spin_unlock(&fwc->name_lock);
}

 
static void device_cache_fw_images(void)
{
	struct firmware_cache *fwc = &fw_cache;
	DEFINE_WAIT(wait);

	pr_debug("%s\n", __func__);

	 
	cancel_delayed_work_sync(&fwc->work);

	fw_fallback_set_cache_timeout();

	mutex_lock(&fw_lock);
	fwc->state = FW_LOADER_START_CACHE;
	dpm_for_each_dev(NULL, dev_cache_fw_image);
	mutex_unlock(&fw_lock);

	 
	async_synchronize_full_domain(&fw_cache_domain);

	fw_fallback_set_default_timeout();
}

 
static void device_uncache_fw_images(void)
{
	pr_debug("%s\n", __func__);
	__device_uncache_fw_images();
}

static void device_uncache_fw_images_work(struct work_struct *work)
{
	device_uncache_fw_images();
}

 
static void device_uncache_fw_images_delay(unsigned long delay)
{
	queue_delayed_work(system_power_efficient_wq, &fw_cache.work,
			   msecs_to_jiffies(delay));
}

static int fw_pm_notify(struct notifier_block *notify_block,
			unsigned long mode, void *unused)
{
	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
	case PM_RESTORE_PREPARE:
		 
		kill_pending_fw_fallback_reqs(true);
		device_cache_fw_images();
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		 
		mutex_lock(&fw_lock);
		fw_cache.state = FW_LOADER_NO_CACHE;
		mutex_unlock(&fw_lock);

		device_uncache_fw_images_delay(10 * MSEC_PER_SEC);
		break;
	}

	return 0;
}

 
static int fw_suspend(void)
{
	fw_cache.state = FW_LOADER_NO_CACHE;
	return 0;
}

static struct syscore_ops fw_syscore_ops = {
	.suspend = fw_suspend,
};

static int __init register_fw_pm_ops(void)
{
	int ret;

	spin_lock_init(&fw_cache.name_lock);
	INIT_LIST_HEAD(&fw_cache.fw_names);

	INIT_DELAYED_WORK(&fw_cache.work,
			  device_uncache_fw_images_work);

	fw_cache.pm_notify.notifier_call = fw_pm_notify;
	ret = register_pm_notifier(&fw_cache.pm_notify);
	if (ret)
		return ret;

	register_syscore_ops(&fw_syscore_ops);

	return ret;
}

static inline void unregister_fw_pm_ops(void)
{
	unregister_syscore_ops(&fw_syscore_ops);
	unregister_pm_notifier(&fw_cache.pm_notify);
}
#else
static void fw_cache_piggyback_on_request(struct fw_priv *fw_priv)
{
}
static inline int register_fw_pm_ops(void)
{
	return 0;
}
static inline void unregister_fw_pm_ops(void)
{
}
#endif

static void __init fw_cache_init(void)
{
	spin_lock_init(&fw_cache.lock);
	INIT_LIST_HEAD(&fw_cache.head);
	fw_cache.state = FW_LOADER_NO_CACHE;
}

static int fw_shutdown_notify(struct notifier_block *unused1,
			      unsigned long unused2, void *unused3)
{
	 
	kill_pending_fw_fallback_reqs(false);

	return NOTIFY_DONE;
}

static struct notifier_block fw_shutdown_nb = {
	.notifier_call = fw_shutdown_notify,
};

static int __init firmware_class_init(void)
{
	int ret;

	 
	fw_cache_init();

	ret = register_fw_pm_ops();
	if (ret)
		return ret;

	ret = register_reboot_notifier(&fw_shutdown_nb);
	if (ret)
		goto out;

	return register_sysfs_loader();

out:
	unregister_fw_pm_ops();
	return ret;
}

static void __exit firmware_class_exit(void)
{
	unregister_fw_pm_ops();
	unregister_reboot_notifier(&fw_shutdown_nb);
	unregister_sysfs_loader();
}

fs_initcall(firmware_class_init);
module_exit(firmware_class_exit);
