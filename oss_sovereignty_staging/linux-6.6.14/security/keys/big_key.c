
 

#define pr_fmt(fmt) "big_key: "fmt
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/file.h>
#include <linux/shmem_fs.h>
#include <linux/err.h>
#include <linux/random.h>
#include <keys/user-type.h>
#include <keys/big_key-type.h>
#include <crypto/chacha20poly1305.h>

 
struct big_key_payload {
	u8 *data;
	struct path path;
	size_t length;
};
#define to_big_key_payload(payload)			\
	(struct big_key_payload *)((payload).data)

 
#define BIG_KEY_FILE_THRESHOLD (sizeof(struct inode) + sizeof(struct dentry))

 
struct key_type key_type_big_key = {
	.name			= "big_key",
	.preparse		= big_key_preparse,
	.free_preparse		= big_key_free_preparse,
	.instantiate		= generic_key_instantiate,
	.revoke			= big_key_revoke,
	.destroy		= big_key_destroy,
	.describe		= big_key_describe,
	.read			= big_key_read,
	.update			= big_key_update,
};

 
int big_key_preparse(struct key_preparsed_payload *prep)
{
	struct big_key_payload *payload = to_big_key_payload(prep->payload);
	struct file *file;
	u8 *buf, *enckey;
	ssize_t written;
	size_t datalen = prep->datalen;
	size_t enclen = datalen + CHACHA20POLY1305_AUTHTAG_SIZE;
	int ret;

	BUILD_BUG_ON(sizeof(*payload) != sizeof(prep->payload.data));

	if (datalen <= 0 || datalen > 1024 * 1024 || !prep->data)
		return -EINVAL;

	 
	prep->quotalen = 16;

	payload->length = datalen;

	if (datalen > BIG_KEY_FILE_THRESHOLD) {
		 
		loff_t pos = 0;

		buf = kvmalloc(enclen, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		 
		enckey = kmalloc(CHACHA20POLY1305_KEY_SIZE, GFP_KERNEL);
		if (!enckey) {
			ret = -ENOMEM;
			goto error;
		}
		ret = get_random_bytes_wait(enckey, CHACHA20POLY1305_KEY_SIZE);
		if (unlikely(ret))
			goto err_enckey;

		 
		chacha20poly1305_encrypt(buf, prep->data, datalen, NULL, 0,
					 0, enckey);

		 
		file = shmem_kernel_file_setup("", enclen, 0);
		if (IS_ERR(file)) {
			ret = PTR_ERR(file);
			goto err_enckey;
		}

		written = kernel_write(file, buf, enclen, &pos);
		if (written != enclen) {
			ret = written;
			if (written >= 0)
				ret = -EIO;
			goto err_fput;
		}

		 
		payload->data = enckey;
		payload->path = file->f_path;
		path_get(&payload->path);
		fput(file);
		kvfree_sensitive(buf, enclen);
	} else {
		 
		void *data = kmalloc(datalen, GFP_KERNEL);

		if (!data)
			return -ENOMEM;

		payload->data = data;
		memcpy(data, prep->data, prep->datalen);
	}
	return 0;

err_fput:
	fput(file);
err_enckey:
	kfree_sensitive(enckey);
error:
	kvfree_sensitive(buf, enclen);
	return ret;
}

 
void big_key_free_preparse(struct key_preparsed_payload *prep)
{
	struct big_key_payload *payload = to_big_key_payload(prep->payload);

	if (prep->datalen > BIG_KEY_FILE_THRESHOLD)
		path_put(&payload->path);
	kfree_sensitive(payload->data);
}

 
void big_key_revoke(struct key *key)
{
	struct big_key_payload *payload = to_big_key_payload(key->payload);

	 
	key_payload_reserve(key, 0);
	if (key_is_positive(key) && payload->length > BIG_KEY_FILE_THRESHOLD)
		vfs_truncate(&payload->path, 0);
}

 
void big_key_destroy(struct key *key)
{
	struct big_key_payload *payload = to_big_key_payload(key->payload);

	if (payload->length > BIG_KEY_FILE_THRESHOLD) {
		path_put(&payload->path);
		payload->path.mnt = NULL;
		payload->path.dentry = NULL;
	}
	kfree_sensitive(payload->data);
	payload->data = NULL;
}

 
int big_key_update(struct key *key, struct key_preparsed_payload *prep)
{
	int ret;

	ret = key_payload_reserve(key, prep->datalen);
	if (ret < 0)
		return ret;

	if (key_is_positive(key))
		big_key_destroy(key);

	return generic_key_instantiate(key, prep);
}

 
void big_key_describe(const struct key *key, struct seq_file *m)
{
	struct big_key_payload *payload = to_big_key_payload(key->payload);

	seq_puts(m, key->description);

	if (key_is_positive(key))
		seq_printf(m, ": %zu [%s]",
			   payload->length,
			   payload->length > BIG_KEY_FILE_THRESHOLD ? "file" : "buff");
}

 
long big_key_read(const struct key *key, char *buffer, size_t buflen)
{
	struct big_key_payload *payload = to_big_key_payload(key->payload);
	size_t datalen = payload->length;
	long ret;

	if (!buffer || buflen < datalen)
		return datalen;

	if (datalen > BIG_KEY_FILE_THRESHOLD) {
		struct file *file;
		u8 *buf, *enckey = payload->data;
		size_t enclen = datalen + CHACHA20POLY1305_AUTHTAG_SIZE;
		loff_t pos = 0;

		buf = kvmalloc(enclen, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		file = dentry_open(&payload->path, O_RDONLY, current_cred());
		if (IS_ERR(file)) {
			ret = PTR_ERR(file);
			goto error;
		}

		 
		ret = kernel_read(file, buf, enclen, &pos);
		if (ret != enclen) {
			if (ret >= 0)
				ret = -EIO;
			goto err_fput;
		}

		ret = chacha20poly1305_decrypt(buf, buf, enclen, NULL, 0, 0,
					       enckey) ? 0 : -EBADMSG;
		if (unlikely(ret))
			goto err_fput;

		ret = datalen;

		 
		memcpy(buffer, buf, datalen);

err_fput:
		fput(file);
error:
		kvfree_sensitive(buf, enclen);
	} else {
		ret = datalen;
		memcpy(buffer, payload->data, datalen);
	}

	return ret;
}

 
static int __init big_key_init(void)
{
	return register_key_type(&key_type_big_key);
}

late_initcall(big_key_init);
