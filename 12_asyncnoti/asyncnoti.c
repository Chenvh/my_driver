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
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/* gpio 中断驱动 */

#define IMX6UIRQ_CNT    1   /* 设备号个数 */
#define IMX6UIRQ_NAME   "asyncnoti"
#define KEY0VALUE       0x01    /* key0 的案件值*/
#define INVAKEY         0xFF    /* 无效的按键值 */
#define KEY_NUM          1       /* 按键数量 */


/* 中断 IO 描述结构体 */
struct irq_keydesc {
    int gpio;               /* gpio */
    int irqnum;             /* 中断号 */
    unsigned char value;    /* 按键对应的键值 */
    char name[10];          /* 中断名 */
    irqreturn_t (*handler)(int, void *); /* 中断服务函数 */

};


/* im6uirq 设备结构体 */
struct imx6uirq_dev {
    dev_t devid;
    int major;
    int minor;
    int led_gpio;           // led gpio
    struct cdev scdev;
    struct class *sclass;
    struct device *sdevice;
    struct device_node *nd; //设备节点

    atomic_t keyvalue;      /* 有效的按键值 */
    atomic_t releasekey;    /* 标记是否完成一次完成的按键 */

    struct timer_list timer;    /* 定时器 */
    struct irq_keydesc irqkeydesc[KEY_NUM]; /* 按键描述数组 */
    unsigned char curkeynum;    /* 当前的按键号 */

    wait_queue_head_t r_wait;       // 等待队列头

    struct fasync_struct *async_queue;      // 异步相关结构体
};

struct imx6uirq_dev imx6uirq;

/**
 * @描述：中断服务函数，开启定时器，延时10ms，定时器用于按键消抖
 * @参数 - irq：中断号
 * @参数 - dev_id：设备结构
 * @返回值：中断执行想结果
*/
static irqreturn_t key0_handler (int irq, void *dev_id)
{
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)dev_id;

    dev->curkeynum = 0;
    dev->timer.data = (volatile long )dev_id;

    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
    return IRQ_RETVAL (IRQ_HANDLED);
}

/**
 * @描述：定时器服务函数，用于按键消抖，定时器到了易购，在此处读取按键值
 *       如果按键还是处于按下状态就表示按键有效
 * @参数 - arg：设备结构变量
 * @返回值：无
*/
void timer_function (unsigned long arg)
{
    unsigned char value;
    unsigned char num;
    struct irq_keydesc *keydesc;
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)arg;

    num = dev->curkeynum;
    keydesc = &dev->irqkeydesc[num];

    value = gpio_get_value (keydesc->gpio);     /* 读取IO 值 */
    if (value == 0) {
        atomic_set (&dev->keyvalue, keydesc->value);
    } else {
        atomic_set (&dev->keyvalue, 0x80 | keydesc->value);
        atomic_set (&dev->releasekey, 1);
    }

    // 异步通知
    if (atomic_read (&dev->releasekey)) {
        if (dev->async_queue) {
            kill_fasync (&dev->async_queue, SIGIO, POLL_IN);
        }
    }

#if 0
    // 唤醒进程
    if (atomic_read (&dev->releasekey)) {       // 完成一次按键过程
        wake_up_interruptible (&dev->r_wait);
    }
#endif     
}

/**
 * @描述            : fasync 函数，用于处理异步通知
 * @参数 - fd       : 文件描述符
 * @参数 - filp     : 要打开的设备文件
 * @参数 - on       : 模式
 * @返回            : 负数表示函数执行失败
*/
static int imx6uirq_fasync (int fd, struct file *filp, int on)
{
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *) filp->private_data;
    return fasync_helper (fd, filp, on, &dev->async_queue);
}


/**
 * @描述    ： 初始化LED灯IO，open 函数打开的驱动的时候，初始化LED灯所使用的引脚
 * @参数    ： 无
 * @返回值   ： 无
*/
static int led_init (void)
{
    int ret = 0;


    imx6uirq.nd = of_find_node_by_path ("/myled");
    if (imx6uirq.nd == NULL) {
        return -EINVAL;
    }

    // 从设备树中得到使用的gpio的编号
    imx6uirq.led_gpio = of_get_named_gpio (imx6uirq.nd, "my-gpio", 0);
    if (imx6uirq.led_gpio < 0) {
		printk (KERN_ALERT "can't get my-gpio!\n");
		return -1;
	} else {
		printk (KERN_ALERT "my-gpio num = %d\n", imx6uirq.led_gpio);    
    }

    // 设置gpio
    gpio_request (imx6uirq.led_gpio, "led");            // 请求IO
    ret = gpio_direction_output (imx6uirq.led_gpio, 1); // 设置为输出
    if (ret < 0) {
        printk (KERN_ALERT "cant't set gpio\n");
    }

    return 0;

}

/**
 * @描述：按键 IO 初始化
 * @参数：无
 * @返回值：无
*/
static int keyio_init(void)
{
    unsigned char i = 0;
    int ret = 0;

    imx6uirq.nd = of_find_node_by_path("/key");
    if (imx6uirq.nd == NULL) {
        printk (KERN_ALERT "key node not find!\r\n");
        return -EINVAL;
    }

    /* 提取GPIO */
    for (i = 0; i < KEY_NUM; i++)
    {
        imx6uirq.irqkeydesc[i].gpio = of_get_named_gpio (imx6uirq.nd, "key-gpio", i);
        if (imx6uirq.irqkeydesc[i].gpio < 0) {
            printk (KERN_ALERT "can't get key %d\r\n", i);
        }
    }

    /* 初始化 key 所使用的IO， 并且设置成中断模式 */
    for (i = 0; i < KEY_NUM; i++)
    {
        memset (imx6uirq.irqkeydesc[i].name, 0, sizeof(imx6uirq.irqkeydesc[i].name));
        sprintf (imx6uirq.irqkeydesc[i].name, "KEY%d", i);
        gpio_request (imx6uirq.irqkeydesc[i].gpio, imx6uirq.irqkeydesc[i].name);
        gpio_direction_input (imx6uirq.irqkeydesc[i].gpio);
        imx6uirq.irqkeydesc[i].irqnum = irq_of_parse_and_map(imx6uirq.nd, i);
    }

    /* 申请中断 */
    imx6uirq.irqkeydesc[0].handler = key0_handler;
    imx6uirq.irqkeydesc[0].value = KEY0VALUE;

    for (i = 0; i < KEY_NUM; i++) {
        ret = request_irq (imx6uirq.irqkeydesc[i].irqnum, imx6uirq.irqkeydesc[i].handler, 
                            IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, imx6uirq.irqkeydesc[i].name, &imx6uirq);
        
        if (ret < 0) {
            printk (KERN_ALERT "irq %d request failed!\r\n", imx6uirq.irqkeydesc[i].irqnum);
            return -EFAULT;
        }
    }

    /* 创建定时器 */
    init_timer (&imx6uirq.timer);
    imx6uirq.timer.function = timer_function;

    // 初始化等待队列头
    init_waitqueue_head (&imx6uirq.r_wait);

    return 0;
}

/**
 * 描述：打开设备
 * 参数-inode： 传递给驱动的inode
 * 参数-filp：设备文件，
 * @返回值：0 成功， 其他失败
*/
static int imx6uirq_open (struct inode *inode, struct file *filp)
{
    filp->private_data = &imx6uirq;
    return 0;
}

 /*
  * @description     : 从设备读取数据 
  * @param - filp    : 要打开的设备文件(文件描述符)
  * @param - buf     : 返回给用户空间的数据缓冲区
  * @param - cnt     : 要读取的数据长度
  * @param - offt    : 相对于文件首地址的偏移
  * @return          : 读取的字节数，如果为负值，表示读取失败
  */
 static ssize_t imx6uirq_read (struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
 {
    int ret = 0; 
    static int sta = 1;
    unsigned char keyvalue = 0;
    unsigned char releasekey = 0;
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)filp->private_data;


    if (filp->f_flags & O_NONBLOCK) {       // 非阻塞访问
        if (atomic_read (&dev->releasekey) == 0) {     // 没有按键按下
            return -EAGAIN;
        }
    } else {        // 阻塞访问
        ret = wait_event_interruptible (dev->r_wait, atomic_read (&dev->releasekey));
        if (ret) {
            goto wait_error;   
        }        
    }


#if 0
    DECLARE_WAITQUEUE (wait, current);      // 定义一个等待队列
    if (atomic_read (&dev->releasekey) == 0) { //没有按键按下
        add_wait_queue (&dev->r_wait, &wait);   // 添加到等待队列头
        __set_current_state (TASK_INTERRUPTIBLE);   // 设置任务状态
        schedule ();
        if (signal_pending (current)) { // 判断是否为信号引起的唤醒
            ret = -ERESTARTSYS;
            goto wait_error;
        }

        __set_current_state (TASK_RUNNING);     // 设置为运行态
        remove_wait_queue (&dev->r_wait, &wait);
    }
#endif
    keyvalue = atomic_read (&dev->keyvalue);
    releasekey = atomic_read (&dev->releasekey);

    if (releasekey) {   // 有按键按下
        if (keyvalue & 0x80) {
            keyvalue &= ~0x80;
            ret = copy_to_user (buf, &keyvalue, sizeof(keyvalue));
            sta = !sta;
            gpio_set_value (dev->led_gpio, sta);    // 实现LED反转            
        } else {
            goto data_error;
        }
        atomic_set (&dev->releasekey, 0);   //按下按键清零
    } else {
        goto data_error;
    }
    return 0;

wait_error:
    return ret;

data_error:
    return -EINVAL;

 }

/**
 * @描述            ： release函数，应用程序调用close 关闭驱动文件时候会执行
 * @参数 - inode    ： inode 节点
 * @参数 - filp     ： 要打开的设备文件
 * 返回             ： 负数表示执行失败 
*/
static int imx6uirq_release (struct inode *inode, struct file *filp)
{
    // 使用异步
    return imx6uirq_fasync (-1, filp, 0);
}



 /**
  * @描述           : poll 函数，用于处理非阻塞访问
  * @参数 - filp    : 要打开的设备文件
  * @参数 - wait    : 要等待列表 (poll_table)
  * @返回           : 设备或者资源状态        
 */

unsigned int imx6uirq_poll (struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *) filp->private_data;

    poll_wait (filp, &dev->r_wait, wait);

    if (atomic_read (&dev->releasekey)) {  // 按键按下
        mask = POLLIN | POLLRDNORM;         // 返回POLLIN

    }

    return mask;
}

 /* 设备操作函数 */
 static struct file_operations imx6uirq_fops = {
    .owner = THIS_MODULE,
    .open = imx6uirq_open,
    .read = imx6uirq_read,
    .poll = imx6uirq_poll,
    .fasync = imx6uirq_fasync,
    .release = imx6uirq_release,
 };

/*
 * @description	: 驱动入口函数
 * @param 		: 无
 * @return 		: 无
 */
static int __init imx6uirq_init (void)
{
    // 注册字符设备驱动
    if (imx6uirq.major) {   // 如果定义了设备号, 静态注册
        imx6uirq.devid = MKDEV(imx6uirq.major, 0);
        register_chrdev_region (imx6uirq.devid, IMX6UIRQ_CNT, IMX6UIRQ_NAME);
    } else {            // 如果没有定义设备号，动态注册
        alloc_chrdev_region (&imx6uirq.devid, 0, IMX6UIRQ_CNT, IMX6UIRQ_NAME);
        // 获取主次设备号
        imx6uirq.major = MAJOR (imx6uirq.devid);
        imx6uirq.minor = MINOR (imx6uirq.devid);
        printk (KERN_ALERT "major = %d, minor = %d\n", imx6uirq.major, imx6uirq.minor);        
    }

    // 注册 字符设备
    cdev_init (&imx6uirq.scdev, &imx6uirq_fops);
    cdev_add (&imx6uirq.scdev, imx6uirq.devid, IMX6UIRQ_CNT);

    // 创建class
    imx6uirq.sclass = class_create (THIS_MODULE, IMX6UIRQ_NAME);
    if (IS_ERR (imx6uirq.sclass)) {
        return PTR_ERR(imx6uirq.sclass);
    }

    // 创建设备device
    imx6uirq.sdevice = device_create (imx6uirq.sclass, NULL, imx6uirq.devid, NULL, IMX6UIRQ_NAME);
    if (IS_ERR (imx6uirq.sdevice)) {
		printk (KERN_ALERT "can't create device\n");
		return PTR_ERR (imx6uirq.sdevice);        
    }

    // 初始化按键
    atomic_set (&imx6uirq.keyvalue, INVAKEY);
    atomic_set (&imx6uirq.releasekey, 0);
    keyio_init();   // keyio 初始化
    led_init();     // led 初始化
    return 0;
}

/*
 * @description	: 驱动出口函数
 * @param 		: 无
 * @return 		: 无
 */
static void __exit imx6uirq_exit (void)
{
    unsigned int i = 0;

    // 删除定时器
    del_timer_sync (&imx6uirq.timer);

    // 释放中断
    for (i = 0; i < KEY_NUM; i++) {
        free_irq (imx6uirq.irqkeydesc[i].irqnum, &imx6uirq);
        gpio_free (imx6uirq.irqkeydesc[i].gpio);
    }

    //删除设备
    device_destroy (imx6uirq.sclass, imx6uirq.devid);

    // 删除类
    class_destroy (imx6uirq.sclass);

    // 删除cdev
    cdev_del (&imx6uirq.scdev);
    
    //注销字符设备
    unregister_chrdev_region (imx6uirq.devid, IMX6UIRQ_CNT);
}

// 模块
module_init (imx6uirq_init);
module_exit (imx6uirq_exit);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("chen");

