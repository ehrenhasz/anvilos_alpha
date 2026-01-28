#ifndef GPIOLIB_CDEV_H
#define GPIOLIB_CDEV_H
#include <linux/types.h>
struct gpio_device;
int gpiolib_cdev_register(struct gpio_device *gdev, dev_t devt);
void gpiolib_cdev_unregister(struct gpio_device *gdev);
#endif  
