
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/sched.h>


#define MAX_DEVICES (10)
#define LOG_BUFFER (10)
#define DEVICE_NAME ("select_char")


struct logger_device {
    struct cdev cdev;
    struct device * pdevice;
    wait_queue_head_t wq;
    char name[20];
    char buf[LOG_BUFFER];
    size_t offset;
} logger[MAX_DEVICES];

static dev_t dev_number;
struct class * pclass;




struct holder {
    struct logger_device * plogger;
    size_t offset;
};
    


static int sc_open(struct inode * inode, struct file * file)
{
    int minor, ret = 0;
    struct holder * pholder;
    minor = MINOR(inode->i_rdev);
    if (minor >= MAX_DEVICES) {
        printk(KERN_ERR"invalid minor number %d\n", minor);
        ret = -ENODEV;
        goto rt;
    }
    
    pholder =(struct holder *)kmalloc(sizeof(struct holder), GFP_KERNEL);
    memset(pholder, 0, sizeof(struct holder));
    if (pholder == NULL) {
        ret = -ENOMEM;
        goto rt;
    }

    pholder->plogger = & logger[minor];
    file->private_data = pholder;

rt:
    return ret;
}

static int sc_release(struct inode * inode, struct file * file)
{
    struct holder * pholder = (struct holder *)file->private_data;
    if (pholder) {
        kfree(pholder);
    }
    return 0;
}



static ssize_t sc_read(struct file * file, char __user *buf,
               size_t count, loff_t *pos) {
    size_t len, ret = 0;
    struct holder * pholder;
    pholder = (struct holder *) file->private_data;
    if (!pholder) {
        printk(KERN_ERR"can not get private data\n");
        ret = -ENOMEM;
        goto error;
    }

    if (pholder->plogger->offset == 0 || pholder->offset == pholder->plogger->offset) {
        if(file->f_flags & O_NONBLOCK) {
            return -EAGAIN;
        } else {
            return 0;
        }
        
    }

    len = min(pholder->plogger->offset - pholder->offset , count - pholder->offset);
    ret = copy_to_user(buf, &pholder->plogger->buf[pholder->offset], len);
    printk(KERN_INFO"  read len  %d,   can't copy %d    offset %d \n", len, ret, pholder->offset);
    if (ret) {
        goto error;
    }
    pholder->offset += len;
    return len;

error:
    return ret;

}


static ssize_t sc_write(struct file * file, const char __user * buf, size_t count, loff_t *ppos) {
    size_t len, ret =0;
    struct holder * pholder;
    pholder = (struct holder *) file->private_data;
    if (!pholder) {
        printk(KERN_ERR"can not get private data\n");
        ret = -ENOMEM;
        goto error;
    }

    if (pholder->plogger->offset >= LOG_BUFFER) {
        printk(KERN_ERR" log buffet is full\n");
        ret = -EFAULT;
        goto error;
    }

    len = min(count, LOG_BUFFER - pholder->plogger->offset);
    ret = copy_from_user(&pholder->plogger->buf[pholder->plogger->offset], buf, len);
    if (ret) {
        goto error;
    }
    printk(KERN_INFO" copy len %d  from %d\n", len, pholder->plogger->offset);
    printk(KERN_INFO"%s\n", pholder->plogger->buf);
    pholder->plogger->offset += len;

    wake_up_interruptible(&pholder->plogger->wq); 
    return len;

error:
    return ret;
}



static unsigned int sc_poll(struct file * file, poll_table * wait)
{
    struct holder * pholder;
    unsigned int ret = 0;
    pholder = (struct holder *) file->private_data;

     
    if (pholder->plogger->offset == 0 || pholder->offset == pholder->plogger->offset) {
        poll_wait(file, &pholder->plogger->wq, wait);
        printk(KERN_INFO"wake up \n");
    } else {
        ret |= POLLIN | POLLRDNORM;
    }

    return ret;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = sc_open,
    .release = sc_release,
    .read = sc_read,
    .write = sc_write,
    .poll = sc_poll,
};

static int __init sc_init(void)
{
    int r,i;
    r = alloc_chrdev_region(&dev_number, 0, MAX_DEVICES, DEVICE_NAME);
    if (IS_ERR_VALUE(r)) {
        printk(KERN_ERR" can't alloct dev_t  %d\n", r);
        goto error;
    }

    pclass = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(pclass)) {
        printk(KERN_ERR"can't create class\n");
        r = -ENOMEM;
        goto destroy_char_region;
    }

    for (i = 0; i < MAX_DEVICES; i++) {
        sprintf(logger[i].name, "%s%d", DEVICE_NAME, i);
        logger[i].pdevice = device_create(pclass, NULL, (dev_number + i), NULL, logger[i].name);
        if (IS_ERR(logger[i].pdevice)) {
            r = -ENOMEM;
            goto destroy_all;
        }

        cdev_init(&logger[i].cdev, &fops);
        r = cdev_add(&logger[i].cdev, (dev_number + i), 1);
        if (IS_ERR_VALUE(r)) {
            printk(KERN_ERR"can't add device to tree\n");
            goto destroy_all;
        }

        logger[i].offset = 0;
        init_waitqueue_head(&logger[i].wq);
    }



    return 0;

destroy_all:
    while (i-- >= 0) {
        cdev_del(&logger[i].cdev);
        device_destroy(pclass, MKDEV(MAJOR(dev_number) ,i));
    }
    class_destroy(pclass);
destroy_char_region:
    unregister_chrdev_region(MAJOR(dev_number), MAX_DEVICES); 
error:
    return r;
}


static void __exit sc_exit(void)
{
    int i;
    i = MAX_DEVICES;
    while (--i >= 0) {
        cdev_del(&logger[i].cdev);
        device_destroy(pclass, MKDEV(MAJOR(dev_number) ,i));
    }
    class_destroy(pclass);
    unregister_chrdev_region(MKDEV(MAJOR(dev_number), 0), MAX_DEVICES); 
}


module_init(sc_init);
module_exit(sc_exit);
MODULE_LICENSE("GPL v2");

