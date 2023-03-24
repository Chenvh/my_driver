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

#define DEVICE_NAME "myled"
#define DEVICE_NUMS 1
#define LEDON 	0
#define LEDOFF 	1

struct gpio_info {
    dev_t dev;
    unsigned int major;
    unsigned int minor;
    struct class *s_class;
    struct device *s_device;
    struct cdev s_cdev;
    struct device_node *nd;     //设备节点
    int dev_gpio;               //设备使用的gpio的编号
};

struct gpio_info myled;



// 打开设备
static int myled_open (struct inode * inode, struct file *filep)
{
    filep->private_data = &myled;
	printk (KERN_ALERT "enter %s\n", __func__);
	return 0;    
}

// 读取设备
static ssize_t myled_read (struct file *filep, char __user *buf, size_t cnt, loff_t *off_set)
{
	return 0;
}

// 向设备传输数据
ssize_t myled_write (struct file *filep, const char __user *buf, size_t cnt, loff_t *off_set)
{
    int ret = 0;
    unsigned char databuf[1];
    unsigned char myledstat;
    struct gpio_info * dev = filep->private_data;

    ret = copy_from_user (databuf, buf, cnt);
	if (ret < 0)
	{
		printk (KERN_ALERT "kernel write faild!\n");
		return -EFAULT;
	}    

    myledstat = databuf[0];

	if (myledstat == LEDON)
	{
		printk (KERN_ALERT "extern led on!\n");
		gpio_set_value (dev->dev_gpio, 0);
	} else if (myledstat == LEDOFF) {
		printk (KERN_ALERT "extern led off!\n");
		gpio_set_value (dev->dev_gpio, 1);
	} else {
		printk (KERN_ALERT "unkwon inst\n");
		return -2;
	}

	return 0;

}

// 关闭设备
static int myled_release (struct inode *inode, struct file *filep)
{
	return 0;

}


struct file_operations myled_fops = 
{
	.owner = THIS_MODULE,
	.open = myled_open,
	.write = myled_write,
	.read = myled_read,
	.release = myled_release,
};

// 驱动入口函数
static int __init myled_init(void)
{
    int ret = 0;

    // 设置led 使用的 GPIO1_3
    // 获得设备树节点：my_led
    myled.nd = of_find_node_by_path ("/myled");
    if ( myled.nd == NULL) 
    {
		printk (KERN_ALERT "led node can not found!\n");
		return -1;
    } else {
        printk (KERN_ALERT "led node has been found!\n");
    }

    //从设备树中获得gpio属性，得到led 使用的gpio编号
    myled.dev_gpio = of_get_named_gpio (myled.nd, "my-gpio", 0);
    if (myled.dev_gpio < 0)
	{
		printk (KERN_ALERT "can't get my-gpio!\n");
		return -1;
	} else {
		printk (KERN_ALERT "my-gpio num = %d\n", myled.dev_gpio);
	}

    // 设置 GPIO1_IO03 为输出，并且输出高电平，默认关闭 LED 灯
    ret = gpio_direction_output (myled.dev_gpio, 1);
    if (ret < 0)
	{
		printk (KERN_ALERT "can't set gpio!\b");
		return -1;
	}

    /* 注册字符设备驱动  */
    printk (KERN_ALERT "register my led\n");
    if (myled.major)  // 如果定义了设备编号
    {
        myled.dev = MKDEV (myled.major, myled.minor);
        register_chrdev_region (myled.dev, DEVICE_NUMS, DEVICE_NAME);
    } else {    // 如果没有定义设备编号
        alloc_chrdev_region (&myled.dev, 0, DEVICE_NUMS, DEVICE_NAME);
        // 获得设备号
        myled.major = MAJOR (myled.dev);
        myled.minor = MINOR (myled.dev);
        printk (KERN_ALERT "major = %d, minor = %d\n", myled.major, myled.minor);
    }

    // 初始化cdev
    myled.s_cdev.owner = THIS_MODULE;
    cdev_init (&myled.s_cdev, &myled_fops);

    // 注册cdev
    cdev_add (&myled.s_cdev, myled.dev, DEVICE_NUMS);

    // 创建一个类
    myled.s_class = class_create (THIS_MODULE, DEVICE_NAME);

    // 创建设备
    myled.s_device = device_create (myled.s_class, NULL, myled.dev, NULL, DEVICE_NAME);
	if (IS_ERR (myled.s_device))
	{
		printk (KERN_ALERT "can't create device\n");
		return PTR_ERR (myled.s_device);
	}        

    return 0;

}

/* 驱动出口函数 */
void __exit myled_exit (void)
{
    printk (KERN_ALERT "device export\n");
    // 最后申请的最先释放
    // 释放设备
    device_destroy (myled.s_class, myled.dev);

    // 释放class
    class_destroy (myled.s_class);

    // 删除cdev
    cdev_del (&myled.s_cdev);

    // 注销字符设备驱动
    unregister_chrdev_region (myled.dev, DEVICE_NUMS);
}

// 模块
module_init (myled_init);
module_exit (myled_exit);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("CHEN");














