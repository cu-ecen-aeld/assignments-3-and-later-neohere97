/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations

#include "aesd-circular-buffer.h"
#include "aesdchar.h"
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Chinmay Shalawadi"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;

    PDEBUG("open");

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    ssize_t retval = 0;
    // size_t *entry_offset;
    // struct aesd_buffer_entry *entry;    
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    // entry = aesd_circular_buffer_find_entry_offset_for_fpos

    // if (copy_to_user(buf, p, count))
    // {
    //     retval = -EFAULT;
    // }
    return count;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    char *ker_buff, *potentially_free_memory;
    struct aesd_buffer_entry entry;
    struct aesd_dev *buffer = (struct aesd_dev *)filp->private_data;
    int newlineflag = 0;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    ker_buff = kmalloc(count * sizeof(char), GFP_KERNEL);

    if (copy_from_user(ker_buff, buf, count))
    {
        retval = -EFAULT;
        goto err;
    }

    int i;
    for (i = 0; i < count; i++)
    {
        if (ker_buff[i] == '\n')
        {
            newlineflag = 1;
            break;
        }
    }

    if (!buffer->global_buffer_size)
    {
        buffer->global_copy_buffer = kmalloc(count * sizeof(char), GFP_KERNEL);
        printk(KERN_ALERT "memory allocated for global copy buffer \n");
    }
    else
    {
        buffer->global_copy_buffer = krealloc(buffer->global_copy_buffer, (count + buffer->global_buffer_size) * sizeof(char *), GFP_KERNEL);
        printk(KERN_ALERT "memory reallocated for global copy buffer \n");
    }

    memcpy((buffer->global_copy_buffer) + (buffer->global_buffer_size), ker_buff, count * sizeof(char));

    if (newlineflag)
    {
        entry.buffptr = buffer->global_copy_buffer;
        entry.size = buffer->global_buffer_size;
        mutex_lock(&(buffer->lock));
        potentially_free_memory = aesd_circular_buffer_add_entry(&(buffer->data_buffer), &entry);
        mutex_unlock(&(buffer->lock));

        if (potentially_free_memory != NULL)
        {
            kfree(potentially_free_memory);
            printk("Freed rolled over memory \n");
        }

        buffer->global_buffer_size = 0;
    }
    else
        buffer->global_buffer_size += count;

    kfree(ker_buff);

    return count;

err:
    kfree(ker_buff);
    return retval;
}
struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    mutex_init(&aesd_device.lock);
    /**
     * TODO: initialize the AESD specific portion of the device
     */

    printk(KERN_ALERT "In the init function !!!!!!!!!!!!\n");

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
