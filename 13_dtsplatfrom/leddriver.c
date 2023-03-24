#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/***************************************
 * 设备数下的 platform 驱动
***************************************/

#define LEDDEV_CNT      1               // 设备号长度
#define LEDDEV_NAME     "dtsplatfrom"   // 设备名字
#define LEDOFF          1
#define LEDON           0

/* leddev 设备结构体 */
struct leddev_dev {
    dev_t devid;                /* 设备号 */
    struct cdev scdev;          /* cdev */
    struct device *sdevice;     /* 设备 */
    struct device_node *nd;   /* LED 设备节点*/
    struct class *sclass;        /* 类 */
    int major;                  /* 主设备号 */
    int minor;                  /* 次设备号 */
    int led0_gpio;              /* led 灯的 GPIO 标号 */
};

struct leddev_dev leddev;       /* led 设备 */

/**
 * @描述            ： 打开设备
 * 参数 - inode     ： 传递给驱动的inode
 * 参数 - filp      ： 设备文件
 * 返回             ： 0 成功； 其他失败
*/
static int led_open (struct inode *inode, struct file *filp)
{
    filp->private_data = &leddev;       /* 设置私有数据 */
    return 0;
}


/**
 * @描述        ： LED 打开/关闭
 * @参数 - sta  ： LEDON(0), LEDOFF(1)
 * @返回        ： 无
*/
void led0_switch (u8 sta)
{
    if (sta == LEDON) {
        gpio_set_value (leddev.led0_gpio, 0);
    } else if (sta == 1) {
        gpio_set_value (leddev.led0_gpio, 1);
    }
}

/*
 * @描述		    : 向设备写数据 
 * @参数 - filp 	: 设备文件，表示打开的文件描述符
 * @参数 - buf 	    : 要写给设备写入的数据
 * @参数 - cnt 	    : 要写入的数据长度
 * @参数 - offt 	: 相对于文件首地址的偏移
 * @返回 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t led_write (struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue;
    unsigned char databuf[2];
    unsigned char ledstat;

    retvalue = copy_from_user (databuf, buf, cnt);

    if (retvalue < 0) {
        printk (KERN_ALERT "kernel write failed!\r\n");
        return -EFAULT;
    }

    ledstat = databuf[0];
    led0_switch(ledstat);

    return 0;
}

/* 设备操作函数 */
static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .write = led_write,
};

/**
 * @描述            ： platform 驱动的probe 函数， 当驱动与设备匹配后此函数就会执行
 * @参数 - dev      ： platform 设备
 * @返回            ： 0成功； 其他负值 失败
*/
static int led_probe (struct platform_device *dev)
{
    int ret = 0;
    printk (KERN_ALERT "led driver and device was matched!\r\n");

    /* 设置设备号 */
    if (leddev.major) {
        leddev.devid = MKDEV (leddev.major, 0);
        register_chrdev_region (leddev.devid, LEDDEV_CNT, LEDDEV_NAME);
    } else {
        alloc_chrdev_region (&leddev.devid, 0, LEDDEV_CNT, LEDDEV_NAME);
        leddev.major = MAJOR (leddev.devid);
        leddev.minor = MINOR (leddev.devid);
    }

    // 注册cdev
    cdev_init (&leddev.scdev, &led_fops);
    cdev_add (&leddev.scdev, leddev.devid, LEDDEV_CNT);

    // 创建类
    leddev.sclass = class_create (THIS_MODULE, LEDDEV_NAME);
    if (IS_ERR (leddev.sclass)) {
        return PTR_ERR (leddev.sclass);
    }

    // 注册device
    leddev.sdevice = device_create (leddev.sclass, NULL, leddev.devid, NULL, LEDDEV_NAME);
    if (IS_ERR (leddev.sdevice)) {
        return PTR_ERR (leddev.sdevice);
    }

    leddev.nd = of_find_node_by_path ("/myled");
    if (leddev.nd == NULL) {
        return -EINVAL;
    }

    /* 初始化 IO*/
    // 从设备树中得到使用的gpio的编号
    leddev.led0_gpio = of_get_named_gpio (leddev.nd, "my-gpio", 0);
    if (leddev.led0_gpio < 0) {
		printk (KERN_ALERT "can't get my-gpio!\n");
		return -1;
	} else {
		printk (KERN_ALERT "my-gpio num = %d\n", leddev.led0_gpio);    
    }

    // 设置gpio
    gpio_request (leddev.led0_gpio, "led");            // 请求IO
    ret = gpio_direction_output (leddev.led0_gpio, 1); // 设置为输出
    if (ret < 0) {
        printk (KERN_ALERT "cant't set gpio\n");
    }

    return 0;
    
}

/**
 * @描述        ： platform驱动的remove函数，移除platform驱动的时候此函数会执行
 * @参数 - dev  ： platform 设备
 * @返回        ： 0 成功； 其他负值失败
*/
static int led_remove (struct platform_device *dev)
{

    gpio_set_value (leddev.led0_gpio, 0);  // 卸载驱动的时候，关闭led
    gpio_free (leddev.led0_gpio);           // 释放IO

    device_destroy (leddev.sclass, leddev.devid);
    class_destroy (leddev.sclass);
    cdev_del (&leddev.scdev);
    unregister_chrdev_region (leddev.devid, LEDDEV_CNT);
    
    printk (KERN_ALERT "platform led removed!\r\n");

    return 0;
}

/* 匹配列表 */
static const struct of_device_id led_of_match[] = {
    {.compatible = "my_led"},
    {/**/}
};

/* platform 驱动结构体 */
static struct platform_driver led_driver = {
    .driver = {
        .name = "imx6ul-led",              // 驱动名字，用于和设备匹配
        .of_match_table = led_of_match,     // 设备数匹配表
    },
    .probe = led_probe,
    .remove = led_remove,
};

/**
 * @描述    ： 驱动模块加载函数
 * @参数    ： 无
 * @返回    ： 无
*/
static int __init leddriver_init (void)
{
    return platform_driver_register (&led_driver);
}

/**
 * @描述    ： 驱动模块卸载函数
 * @参数    ： 无
 * @返回    ： 无
*/
static void __exit leddriver_exit (void)
{
    platform_driver_unregister (&led_driver);
}

/* 模块 */
module_init (leddriver_init);
module_exit (leddriver_exit);
MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("chen");
