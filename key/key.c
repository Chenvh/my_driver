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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#define DEVICE_CNT     1       /* 设备号个数 */
#define DEVICE_NAME    "key"   /* 名字 */


/* 定义按键值 */
#define KEY0VALUE   0xF0    /* 按键值 */
#define INVAKEY     0x00    /* 无效的按键值 */

/* key 设备结构体 */
struct key_dev {
    dev_t dev;
    unsigned int major;
    unsigned int minor;
    struct class *s_class;
    struct device *s_device;
    struct cdev s_cdev;
    struct device_node *nd;     //设备节点
    int dev_gpio;               //设备使用的gpio的编号
    atomic_t keyvalue;          // 按键值
};

struct key_dev key_dev;

/*
 * @description  :  初始化按键IO，open 函数打开驱动的时候，
                    初始化按键所使用的GPIO 引脚
 * @param        ： 无
 * @return       ： 无
 */
static int keyio_init (void)
{
    key_dev.nd = of_find_node_by_path("/key");
    if (key_dev.nd == NULL) {
        printk (KERN_ALERT "key node can't found!\n");
        return -EINVAL;
    } else {
        printk (KERN_ALERT "key node has been found!\n");
    }

    // 从设备数中获得 GPIO 属性，得到 key 使用的 GPIO 编号
    key_dev.dev_gpio = of_get_named_gpio (key_dev.nd, "key-gpio", 0);
    if (key_dev.dev_gpio < 0) {
        printk (KERN_ALERT "can't get my-key!\n");
        return -EINVAL;
    } else {
        printk (KERN_ALERT "my-ky num = %d\n", key_dev.dev_gpio);
    }

    // 初始化 key 使用的 IO 
    // 设置 GPIO 为 输入模式
    gpio_request (key_dev.dev_gpio, "key0");    /* 请求IO */
    gpio_direction_input (key_dev.dev_gpio);   /* 设置为输入 */
    
    return 0;
    
}

/*
 * @description :
*/
static int key_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    filp->private_data = &key_dev;

    ret = keyio_init();

    if (ret < 0) {
        return ret;
    }
    
    return 0;
}

/*

*/
static ssize_t key_read (struct file *filp, char __user *buf,
                        size_t cnt, loff_t *offt)
{
    int ret = 0;
    int value;
    struct key_dev *dev = filp->private_data;

    if (gpio_get_value (dev->dev_gpio) == 0) { // wait key press
        while (!gpio_get_value (dev->dev_gpio)) {// wait key release
            atomic_set (&dev->keyvalue, KEY0VALUE);
        }
    } else {
        atomic_set (&dev->keyvalue, INVAKEY);
    }

    value = atomic_read (&dev->keyvalue);       //
    ret = copy_to_user(buf, &value, sizeof(value));

    return ret;
}

/**
 * 
*/
static ssize_t key_write (struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/*
*/
static int key_release (struct inode *inode, struct file *filp)
{
    return 0;
}


/*

*/
static struct file_operations key_ops = {
	.owner = THIS_MODULE,
	.open = key_open,
	.read = key_read,
	.write = key_write,
	.release = 	key_release,
};

/*
* 
*/
static int __init mykey_init(void)
{
    /* */
    atomic_set (&key_dev.keyvalue, INVAKEY);

    printk (KERN_ALERT "register character device key\n");
    if (key_dev.major) 
    {
        key_dev.dev = MKDEV (key_dev.major, key_dev.minor);
        register_chrdev_region (key_dev.dev, DEVICE_CNT, DEVICE_NAME);
    } else {
        alloc_chrdev_region (&key_dev.dev, 0, DEVICE_CNT, DEVICE_NAME);

        // get 
        key_dev.major = MAJOR (key_dev.dev);
        key_dev.minor = MINOR (key_dev.dev);
        printk (KERN_ALERT "major = %d, minor = %d\n", key_dev.major, key_dev.minor);        
    }

    // init cdev
    key_dev.s_cdev.owner = THIS_MODULE;
    cdev_init (&key_dev.s_cdev, &key_ops);

    // register cdev
    cdev_add (&key_dev.s_cdev, key_dev.dev, DEVICE_CNT);

    // create class
    key_dev.s_class = class_create (THIS_MODULE, DEVICE_NAME);

    // create device
    key_dev.s_device = device_create (key_dev.s_class, NULL, key_dev.dev, NULL, DEVICE_NAME);
    if (IS_ERR (key_dev.s_device))
    {
        printk (KERN_ALERT "can't create device\n");
        return PTR_ERR (key_dev.s_device);
    }


    return 0;

}

/*
*/
void __exit mykey_exit (void)
{
    // 
    printk (KERN_ALERT "device export\n");

    //
    device_destroy (key_dev.s_class, key_dev.dev);

    //
    class_destroy (key_dev.s_class);

    //
    cdev_del (&key_dev.s_cdev);

    // 
    unregister_chrdev_region (key_dev.dev, DEVICE_CNT);
}

// module
module_init (mykey_init);
module_exit (mykey_exit);

MODULE_LICENSE ("GPL");
MODULE_AUTHOR  ("chen");