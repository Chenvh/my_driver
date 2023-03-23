#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <asm/io.h>
#include <linux/ide.h>
#include "reg_addr.h"

#define DEVICE_NAME	"led"
#define DEVICE_NUMS 1

// virtual addr pointer
#if 0
static void __iomem * SW_MUX_GPIO1_IO03;
static void __iomem * CCM_CCGR1;
static void __iomem * SW_PAD_GPIO1_IO03;
static void __iomem * GPIO1_GDIR;
static void __iomem * GPIO1_DR;
#endif

struct dev_info {
	dev_t dev;
	unsigned int major;
	unsigned int minor;
	struct class *in_class;
	struct device *in_device;
	struct cdev in_cdev;
	struct device_node *nd; /* 设备节点 */
	int led_gpio;			// led used gpio number
};

struct dev_info led_info;


int led_open (struct inode *inode, struct file *filep)
{
	filep->private_data = &led_info;
	printk (KERN_ALERT "enter %s\n", __func__);
	return 0;

}

int led_release (struct inode *inode, struct file *filep)
{
	printk (KERN_ALERT "enter %s\n", __func__);
	return 0;

}

ssize_t led_write (struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
	int ret = 0;
	unsigned char databuf[1];
	unsigned char ledstat;
	struct dev_info * dev = filp->private_data;
	ret = copy_from_user(databuf, buf, cnt);
	if (ret < 0)
	{
		printk (KERN_ALERT "kernel write failed!\n");
	}

	ledstat = databuf[0];

	if (ledstat == 1)
	{
		printk (KERN_ALERT "\nLED on");
		// led on
		gpio_set_value (dev->led_gpio, 0);
	} else if (ledstat == 0) {
		printk (KERN_ALERT "\nLED off");
		// led off
		gpio_set_value (dev->led_gpio, 1);
	} else {
	
	}
	return 0;
}

ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	printk (KERN_ALERT "enter %s\n", __func__);
	return 0;
}

//
struct file_operations led_fops = 
{
	.owner = THIS_MODULE,
	.open = led_open,
	.write = led_write,
	.read = led_read,
	.release = led_release,
};



int __init led_drv_init (void)
{
	int ret = 0;
	led_info.major = 0;
	led_info.minor = 10;

	// get dts data
	led_info.nd = of_find_node_by_path ("/ledgpio");
	if (led_info.nd == NULL)
	{
		printk (KERN_ALERT "led node can not found!\n");
		return -1;
	} else {
		printk (KERN_ALERT "led node has been found!\n");
	}

	// get compatible data from device tree
	led_info.led_gpio = of_get_named_gpio (led_info.nd, "led-gpio", 0);
	if (led_info.led_gpio < 0)
	{
		printk (KERN_ALERT "can't get led-gpio!\b");
		return -1;
	} else {
		printk (KERN_ALERT "led-gpio num = %d\n", led_info.led_gpio);
	}

	// get compatible data from device tree
	ret = gpio_direction_output (led_info.led_gpio, 1);

	if (ret < 0)
	{
		printk (KERN_ALERT "can't set goio!\n");
		return -1;
	}


	// LED0 init 
	// Address mapping 
	// enabale GPIO1 clock, CCM_CCGR1 bit[27:26]
	printk (KERN_ALERT "register led\n");
	if (led_info.major)
	{
		led_info.dev = MKDEV (led_info.major, led_info.minor);
		register_chrdev_region (led_info.dev, 1, DEVICE_NAME );
	} else {
		
		alloc_chrdev_region (&led_info.dev, 100, DEVICE_NUMS, DEVICE_NAME);
		led_info.major = MAJOR (led_info.dev);
		led_info.minor = MINOR (led_info.dev);
		printk (KERN_ALERT "major = %d minor = %d\n", led_info.major, led_info.minor);
	}

	//init cdev
	led_info.in_cdev.owner = THIS_MODULE;
	cdev_init (&led_info.in_cdev, &led_fops);

	//register cdev
	cdev_add (&led_info.in_cdev, led_info.dev, DEVICE_NUMS);

	// init class
	led_info.in_class = class_create (THIS_MODULE, DEVICE_NAME);

	// create divece
	led_info.in_device = device_create (led_info.in_class, NULL, led_info.dev, NULL, "led");

	if (IS_ERR (led_info.in_device))
	{
		printk (KERN_ALERT "create device\n");
		return PTR_ERR (led_info.in_device);
	}
	
	return 0;

}

void __exit led_drv_exit (void)
{
	/* cancel Addr mapping */
	printk (KERN_ALERT "Addr unmapping\n");

	// destroy device
	device_destroy (led_info.in_class, led_info.dev);

	// destroy class
	class_destroy (led_info.in_class);

	cdev_del (&led_info.in_cdev);
	unregister_chrdev_region (led_info.dev, 1);
	printk (KERN_ALERT "unregister dev major = %d minor = %d\n", led_info.major, led_info.minor);
	return ;
}

module_init (led_drv_init);
module_exit (led_drv_exit);
MODULE_LICENSE ("GPL");
