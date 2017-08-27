#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/smp.h>

static const unsigned long SMEP_MASK = 0x100000;
static unsigned long smep_active;
static void print_smep_bit(void *p){
    smep_active = native_read_cr4() & SMEP_MASK;
}

static ssize_t smep_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    printk(KERN_INFO "smep: read called\n");

    int i=0;
    for_each_online_cpu(i){
        int err = smp_call_function_single(i, print_smep_bit, NULL, 1);
        if (err != 0){
            printk(KERN_ERR "smp_call_function_single failed on cpu %d\n", i);
        } else if (smep_active){
            printk(KERN_ERR "SMEP is active on cpu %d\n", i);
        } else {
            printk(KERN_ERR "SMEP has been disabled on cpu %d\n", i);
        }
    }

    return 0;
}

static ssize_t smep_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    printk(KERN_ERR "smep: write operation not permitted\n");
    return -EINVAL;
}

static int smep_open(struct inode *inode, struct file *file){
    printk(KERN_INFO "smep: open called\n");
    return 0;
}

static struct cdev *smep_cdev;
static struct class *smep_class;
static dev_t dev;
static const struct file_operations fops =
{
    .owner = THIS_MODULE,
    .read = smep_read,
    .write = smep_write,
    .open = smep_open
};

static int __init smep_init(void)
{
    int err = 0;
    printk(KERN_INFO "smep: init\n");

    // Get a device major number reservation
    if((err=alloc_chrdev_region(&dev, 0, 1, "smep"))<0){
        printk(KERN_ERR "alloc_chrdev_region failed\n");
        return -1;
    }

    // Create a cdev and attach handlers
    smep_cdev = cdev_alloc();
    if (IS_ERR_OR_NULL(smep_cdev)) {
        printk(KERN_ERR "cdev_alloc failed\n");
        goto err;
    }

    kobject_set_name(&(smep_cdev->kobj), "smep");
    smep_cdev->ops = &fops;
    
    // Add the cdev
    if((err=cdev_add(smep_cdev, dev, 1)) < 0) {
        printk(KERN_ERR "cdev_add failed\n");
        goto unregister_chrdev;
    };
    printk(KERN_INFO "got numbers %d:%d\n", MAJOR(smep_cdev->dev), MINOR(smep_cdev->dev));

    // Create the device in userspace
    smep_class = class_create(THIS_MODULE, "smep\n");
    if (IS_ERR_OR_NULL(smep_class)) {
        printk(KERN_ERR "class_create failed\n");
        goto cdev_del;
    }
    struct device *smep_device = device_create(smep_class, NULL, dev, NULL, "smep");
    if (IS_ERR(smep_device)) {
        printk(KERN_ERR "device_create failed\n");
        err = PTR_ERR(smep_device);
        goto class_destroy; 
    }

    return 0;

class_destroy:
    class_destroy(smep_class);
cdev_del:
    cdev_del(smep_cdev);
unregister_chrdev:
    unregister_chrdev_region(dev, 1);
    return err;
err:
    return -1;
}

static void __exit smep_exit(void)
{
    printk(KERN_INFO "freeing major number and deleting device\n");
    device_destroy(smep_class, dev);
    class_destroy(smep_class);
    cdev_del(smep_cdev);
    unregister_chrdev_region(dev, 1);
}

module_init(smep_init);
module_exit(smep_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Santosh Ananthakrishnan");
MODULE_DESCRIPTION("Print SMEP state");
