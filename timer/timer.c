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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define DEVICE_CNT       1                   /* 设备个数 */
#define DEVICE_NAME      "timer"             /* 设备名字 */
#define CLOSE_CMD       (_IO(0xEF, 0x1))    /* 关闭定时器 */
#define OPEN_CMD        (_IO(0xEF, 0x2))    // 打开定时器
#define SETPERIOD_CMD   (_IO(0xEF, 0x3))    // 修改定时器
#define LEDON           1
#define LEDOFF          0

struct timer_dev {
    dev_t devid;
    unsigned int major;     // 主设备号
    unsigned int minor;     // 副设备号
    struct cdev scdev;      // cdev
    struct class *sclass;   // 类
    struct device *sdevice; //device
    struct device_node *nd; //设备节点    
    int led_gpio;           // led 
    int timerperiod;        // 定时周期，单位ms
    struct timer_list timer;// 定义一个定时器
    spinlock_t lock;        // 自旋锁
};

struct timer_dev tdev;


/**
 * @描述    ： 初始化LED灯IO，open 函数打开的驱动的时候，初始化LED灯所使用的引脚
 * @参数    ： 无
 * @返回值   ： 无
*/
static int led_init (void)
{
    int ret = 0;


    tdev.nd = of_find_node_by_path ("/myled");
    if (tdev.nd == NULL) {
        return -EINVAL;
    }

    // 从设备树中得到使用的gpio的编号
    tdev.led_gpio = of_get_named_gpio (tdev.nd, "my-gpio", 0);
    if (tdev.led_gpio < 0) {
		printk (KERN_ALERT "can't get my-gpio!\n");
		return -1;
	} else {
		printk (KERN_ALERT "my-gpio num = %d\n", tdev.led_gpio);    
    }

    // 设置gpio
    gpio_request (tdev.led_gpio, "led");            // 请求IO
    ret = gpio_direction_output (tdev.led_gpio, 1); // 设置为输出
    if (ret < 0) {
        printk (KERN_ALERT "cant't set gpio\n");
    }

    return 0;

}

/**
 * @描述            ：   打开设备
 * @参数 - inode    ：   传递给驱动的inode
 * @参数 - filp     :   设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体
 * @返回值          ：0 成功， 其他失败
*/
static int timer_open (struct inode *inode, struct file *filp)
{
    int ret = 0;
    filp->private_data = &tdev;

    tdev.timerperiod = 1000;       //默认周期为 1 ms
    ret = led_init();               // 初始化led io

    if (ret < 0) {
        printk (KERN_ALERT "初始化led io 失败\n");
        return ret;
    }

    return 0;
}

/**
 * @描述    ： ioctl 函数
 * @参数 - filp ：要打开的文件
 * @参数 - cmd   ：应用程序发送过来的指令
 * @参数 - arg    ： 参数
 * @返回值：    0成功，其他 失败
*/
static long timer_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)filp->private_data;
    int timerperiod;
    unsigned long flags;

    switch (cmd)
    {
    case CLOSE_CMD:     // 关闭定时器
        del_timer_sync (&dev->timer);
        break;
    case OPEN_CMD:      // 打开定时器
        spin_lock_irqsave (&dev->lock, flags);      // 获得自旋锁
        timerperiod = dev->timerperiod;
        spin_unlock_irqrestore (&dev->lock, flags); // 释放锁
        mod_timer (&dev->timer, jiffies + msecs_to_jiffies (timerperiod));
        break;
    case SETPERIOD_CMD: // 设置定时器周期
        spin_lock_irqsave (&dev->lock, flags);      // 获得锁
        dev->timerperiod = arg;
        spin_unlock_irqrestore (&dev->lock, flags);
        mod_timer (&dev->timer, jiffies + msecs_to_jiffies (arg));
        break;  
    default:
        break;
    }
    return 0;
}

/* 设备操作函数 */
static struct file_operations timer_fops = {
    .owner = THIS_MODULE,
    .open = timer_open,
    .unlocked_ioctl = timer_unlocked_ioctl,
};

/* 定时器回调函数 */
void timer_function (unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)arg;
    static int sta = 1;
    int timerperiod;
    unsigned long flags;
    
    sta = !sta;
    gpio_set_value (dev->led_gpio, sta);    // 实现LED反转

    /* 重启定时器 */
    spin_lock_irqsave (&dev->lock, flags);
    timerperiod = dev->timerperiod;
    spin_unlock_irqrestore (&dev->lock, flags);
    mod_timer (&dev->timer, jiffies + msecs_to_jiffies (dev->timerperiod));
}

/**
 * @描述    ： 驱动函数入口
 * @参数    ： 无
 * @返回值  ：  无
*/
static int __init timer_init (void)
{
    // 初始化自旋锁
    spin_lock_init (&tdev.lock);

    // 注册字符设备驱动
    if (tdev.major) {   // 如果定义了设备号, 静态注册
        tdev.devid = MKDEV(tdev.major, 0);
        register_chrdev_region (tdev.devid, DEVICE_CNT, DEVICE_NAME);
    } else {            // 如果没有定义设备号，动态注册
        alloc_chrdev_region (&tdev.devid, 0, DEVICE_CNT, DEVICE_NAME);
        // 获取主次设备号
        tdev.major = MAJOR (tdev.devid);
        tdev.minor = MINOR (tdev.devid);
        printk (KERN_ALERT "major = %d, minor = %d\n", tdev.major, tdev.minor);        
    }

    // 初始化 cdev
    tdev.scdev.owner = THIS_MODULE;
    cdev_init (&tdev.scdev, &timer_fops);

    // 添加一个cdev
    cdev_add (&tdev.scdev, tdev.devid, DEVICE_CNT);

    // 创建一个类
    tdev.sclass = class_create (THIS_MODULE, DEVICE_NAME);
    if (IS_ERR (tdev.sclass)) {
        return PTR_ERR (tdev.sclass);
    }

    // 创建设备
    tdev.sdevice = device_create (tdev.sclass, NULL, tdev.devid, NULL, DEVICE_NAME);
    if (IS_ERR (tdev.sdevice)) {
		printk (KERN_ALERT "can't create device\n");
		return PTR_ERR (tdev.sdevice);        
    }

    // 初始化timer，设置定时器处理函数， 还未设置周期，所以不会激活定时器
    init_timer (&tdev.timer);
    tdev.timer.function = timer_function;
    tdev.timer.data = (unsigned long)&tdev;

    return 0;
}


/**
 * @描述：驱动入口函数
 * @参数：无
 * 返回值：无
*/
static void __exit timer_exit (void)
{
    printk (KERN_ALERT "timer device export\n");

    // 卸载的时候关闭LED
    gpio_set_value (tdev.led_gpio, 1);
    
    // 先删除timer
    del_timer_sync (&tdev.timer);

    // 删除设备
    device_destroy (tdev.sclass, tdev.devid);

    // 删除类
    class_destroy (tdev.sclass);

    // 删除cdev
    cdev_del (&tdev.scdev);

    // 注销字符设备驱动
    unregister_chrdev_region (tdev.devid, DEVICE_CNT);

}

module_init (timer_init);
module_exit (timer_exit);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("chen");