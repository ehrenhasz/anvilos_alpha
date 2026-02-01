 

#ifndef _FB_INTERNAL_H
#define _FB_INTERNAL_H

#include <linux/device.h>
#include <linux/fb.h>
#include <linux/mutex.h>

 
#if defined(CONFIG_FB_DEVICE)
int fb_register_chrdev(void);
void fb_unregister_chrdev(void);
#else
static inline int fb_register_chrdev(void)
{
	return 0;
}
static inline void fb_unregister_chrdev(void)
{ }
#endif

 
extern struct class *fb_class;
extern struct mutex registration_lock;
extern struct fb_info *registered_fb[FB_MAX];
extern int num_registered_fb;
struct fb_info *get_fb_info(unsigned int idx);
void put_fb_info(struct fb_info *fb_info);

 
#if defined(CONFIG_FB_DEVICE)
int fb_init_procfs(void);
void fb_cleanup_procfs(void);
#else
static inline int fb_init_procfs(void)
{
	return 0;
}
static inline void fb_cleanup_procfs(void)
{ }
#endif

 
#if defined(CONFIG_FB_DEVICE)
int fb_device_create(struct fb_info *fb_info);
void fb_device_destroy(struct fb_info *fb_info);
#else
static inline int fb_device_create(struct fb_info *fb_info)
{
	 
	get_device(fb_info->device);

	return 0;
}
static inline void fb_device_destroy(struct fb_info *fb_info)
{
	 
	put_device(fb_info->device);
}
#endif

#endif
