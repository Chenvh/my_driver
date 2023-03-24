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

#define DEVICE_NAME "exled"
#define DEVICE_NUMS 1
#define LEDON 	1
#define LEDOFF 	0

struct dev_info {
	dev_t dev;
	unsigned int major;
	unsigned int minor;
	struct class *in_class;
	struct device *in_device;
	struct cdev in_cdev;
	struct device_node *nd;		//device node
	int dev_gpio;					// the gpio number of device
};

struct dev_info exled;

/*
 * @discription: open device
 * @param - node
 * @param - filep
 * return : none
*/
static int exled_open (struct inode * inode, struct file *filep)
{
	filep->private_data = &exled;
	printk (KERN_ALERT "enter %s\n", __func__);
	return 0;

}

/*
 * @discription: read data from device 
 * @param - filep : the file device file, file discription
 * @param - buf : returned to the user space buffer
 * @param - cnt : The read data length
 * @param - off_set: Relative to the document first address offset
 * @return : Number of bytes to read. If negative, said read failure
*/
static ssize_t exled_read (struct file *filep, char __user *buf, size_t cnt, loff_t *off_set)
{
	return 0;
}

/*
 * @discription: send data to device
 * @param - filep : the file device file, file discription
 * @param - buf : returned to the user space buffer
 * @param - cnt : The read data length
 * @param - off_set: Relative to the document first address offset
 * @return : Write the number of bytes, if negative, said write fail
*/
ssize_t exled_write (struct file *filep, const char __user *buf, size_t cnt, loff_t *off_set)
{
	int ret = 0;
	unsigned char databuf[1];
	unsigned char exledstat;
	struct dev_info * dev = filep->private_data;
	
	ret = copy_from_user (databuf, buf, cnt);
	if (ret < 0)
	{
		printk (KERN_ALERT "kernel write faild!\n");
		return -EFAULT;
	}
	
	exledstat = databuf[0];

	if (exledstat == LEDON)
	{
		printk (KERN_ALERT "extern led on!\n");
		gpio_set_value (dev->dev_gpio, 0);
	} else if (exledstat == LEDOFF) {
		printk (KERN_ALERT "extern led off!\n");
		gpio_set_value (dev->dev_gpio, 1);
	} else {
		printk (KERN_ALERT "unkwon inst\n");
		return -2;
	}

	return 0;
}

/*
 * @discription: close/release device
 * @param - filep: file discription
 * @return: success 0 ,faild other
*/
static int exled_release (struct inode *inode, struct file *filep)
{

	return 0;

}
/*
* @discription: Equipment operation
*/
struct file_operations exled_fops = 
{
	.owner = THIS_MODULE,
	.open = exled_open,
	.write = exled_write,
	.read = exled_read,
	.release = exled_release,
};
/*
 * @discription: device import fucntion
 * @param: void
 * return: success return 0, faild return errno
*/
static int __init exled_init (void)
{
	int ret = 0;
	/* set gpio for extern led */
	// get device node :exled
	exled.nd = of_find_node_by_path ("/exled");
	if (exled.nd == NULL)
	{
		printk (KERN_ALERT "extern led node can not found!\n");
		return -1;
	} else {
		printk (KERN_ALERT "extern led node has been found!\n");
	}

	// get compatible from device tree, get gpio number for extern led 
	exled.dev_gpio = of_get_named_gpio (exled.nd, "exled-gpio", 0);
	if (exled.dev_gpio < 0)
	{
		printk (KERN_ALERT "can't get exled-gpio!\n");
		return -1;
	} else {
		printk (KERN_ALERT "exled-gpio num = %d\n", exled.dev_gpio);
	}

	// set gpio1_1 for output and output high level, shutdown led
	ret = gpio_direction_output (exled.dev_gpio, 1);
	if (ret < 0)
	{
		printk (KERN_ALERT "can't set gpio!\b");
		return -1;
	}

	/* register character device driver */
	printk (KERN_ALERT "register extern led\n");
	if (exled.major)
	{
		exled.dev = MKDEV (exled.major, exled.minor);
		register_chrdev_region (exled.dev, DEVICE_NUMS, DEVICE_NAME);
	} else {
		alloc_chrdev_region (&exled.dev, 100, DEVICE_NUMS, DEVICE_NAME);
		exled.major = MAJOR (exled.dev);
		exled.minor = MINOR (exled.dev);
		printk (KERN_ALERT "major = %d, minor = %d\n", exled.major, exled.minor);
	}

	/* init cedv */
	exled.in_cdev.owner = THIS_MODULE;
	cdev_init (&exled.in_cdev, &exled_fops);

	// register cdev
	cdev_add (&exled.in_cdev, exled.dev, DEVICE_NUMS);

	// create class
	exled.in_class = class_create (THIS_MODULE, DEVICE_NAME);

	// create device
	exled.in_device = device_create (exled.in_class, NULL, exled.dev, NULL, DEVICE_NAME);
	if (IS_ERR (exled.in_device))
	{
		printk (KERN_ALERT "can't create device\n");
		return PTR_ERR (exled.in_device);
	}

	return 0;
}

/*
 * @discription: device export function
 * @param: none
 * @return: none
*/
void __exit exled_exit (void)
{
	printk (KERN_ALERT "device export\n");
	/* destroy device */
	device_destroy (exled.in_class, exled.dev);

	/* destroy class  */
	class_destroy (exled.in_class);

	/* delete cdev */
	cdev_del (&exled.in_cdev);

	/* unregister character device driver */
	unregister_chrdev_region (exled.dev, DEVICE_NUMS);

}

module_init (exled_init);
module_exit (exled_exit);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("chenwenhong");













