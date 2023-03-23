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
#include "reg_addr.h"

#define DEVICE_NAME	"led"
#define DEVICE_NUMS 1

// virtual addr pointer
static void __iomem * SW_MUX_GPIO1_IO03;
static void __iomem * CCM_CCGR1;
static void __iomem * SW_PAD_GPIO1_IO03;
static void __iomem * GPIO1_GDIR;
static void __iomem * GPIO1_DR;

struct dev_info {
	dev_t dev;
	unsigned int major;
	unsigned int minor;
	struct class *in_class;
	struct device *in_device;
	struct cdev in_cdev;
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
	u32 val = 0;
	unsigned char databuf[1];
	unsigned char ledstat;

	ret = copy_from_user(databuf, buf, cnt);
	if (ret < 0)
	{
		printk (KERN_ALERT "kernel write failed!\n");
	}

	ledstat = databuf[0];

	if (ledstat == 1)
	{
		printk (KERN_ALERT "\nLED on");
		// reset bit 3
		val = readl (GPIO1_DR);
		val &= ~(1 << 3);
		writel (val, GPIO1_DR);
	} else if (ledstat == 0) {
		printk (KERN_ALERT "\nLED off");
		// set bit 3
		val = readl (GPIO1_DR);
		val |= (1 << 3);
		writel (val, GPIO1_DR);
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
	#if 0
	dev = MKDEV (major, minor);
	register_chrdev_region (dev, 1, "leds" );
	#endif
	u32 val = 0;
	led_info.major = 0;
	led_info.minor = 10;
	// LED0 init 
	// Address mapping 
	printk (KERN_ALERT "Addr mapping\n");
	SW_MUX_GPIO1_IO03 = ioremap (SW_MUX_GPIO1_IO03_BASE, 4);
	SW_PAD_GPIO1_IO03 = ioremap (SW_PAD_GPIO1_IO03_BASE, 4);
	CCM_CCGR1 = ioremap (CCM_CCGR1_BASE, 4);
	GPIO1_GDIR = ioremap (GPIO1_GDIR_BASE, 4);
	GPIO1_DR = ioremap (GPIO1_DR_BASE, 4);

	// enabale GPIO1 clock, CCM_CCGR1 bit[27:26]
	val = readl (CCM_CCGR1);
	val &= ~(3 << 26);			// reset bit 26 27
	val |= (3 << 26);			// set bit
	writel (val, CCM_CCGR1);	// write to reg

	// set SW_MUX_GPIO1_IO03, config MUX_MODE 0101 GPIO mode
	writel (5, SW_MUX_GPIO1_IO03);

	/* config SW_PAD_GPIO1_IO03 
	* bit 16:0 HYS关闭
	* bit [15:14]: 00 默认下拉
	* bit [13]: 0 kepper功能
	* bit [12]: 1 pull/keeper使能
	* bit [11]: 0 关闭开路输出
	* bit [7:6]: 10 速度100Mhz
	* bit [5:3]: 110 R0/6驱动能力
	* bit [0]: 0 低转换率
	* 0001 0000 1011 0000 ===> 0x10B0
	*/
	writel (0x10B0, SW_PAD_GPIO1_IO03);

	/* config GPIO */
	val = readl (GPIO1_GDIR);
	val &= ~(1 << 3);		// reset bit 3
	val |= (1 << 3);		// set bit 3
	writel (val, GPIO1_GDIR);

	/* set default level: HIGH ===> led off  */
	val = readl (GPIO1_DR);
	val |= (1 << 3);
	writel (val, GPIO1_DR);
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
	iounmap (CCM_CCGR1);
	iounmap (SW_MUX_GPIO1_IO03);
	iounmap (SW_PAD_GPIO1_IO03);
	iounmap (GPIO1_GDIR);
	iounmap (GPIO1_DR);


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
