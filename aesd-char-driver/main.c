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
    size_t entry_offset;
    int buff_count = 0;
    struct aesd_dev *dev_strcut = (struct aesd_dev *)filp->private_data;
    struct aesd_buffer_entry *entry;
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    mutex_lock(&(dev_strcut->lock));
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev_strcut->data_buffer, *f_pos, &entry_offset);
    mutex_unlock(&(dev_strcut->lock));

    if (entry == NULL)
    {
        printk(KERN_ALERT "NULLLLLL \n");
        *f_pos = 0;
        goto end;
    }

    if ((entry->size - entry_offset) < count)
    {
        printk("entry offset -> %ld, size-> %ld", entry_offset, entry->size);
        *f_pos += (entry->size - entry_offset);
        buff_count = entry->size - entry_offset;
    }
    else
    {
        *f_pos += count;
        buff_count = count;
    }

    if (copy_to_user(buf, entry->buffptr + entry_offset, buff_count))
    {
        retval = -EFAULT;
        goto end;
    }

    printk("buff_count %d \n", buff_count);
    retval = buff_count;
end:
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;

    // Buffer Pointers for logic
    char *ker_buff, *potentially_free_memory;
    struct aesd_buffer_entry entry;

    // Device struct from private data
    struct aesd_dev *buffer = (struct aesd_dev *)filp->private_data;

    // Variables for write logic
    int newlineflag = 0, i;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    // Allocating memory for copy buffer
    ker_buff = kmalloc(count * sizeof(char), GFP_KERNEL);

    if (!ker_buff)
        goto err;

    if (copy_from_user(ker_buff, buf, count))
    {
        retval = -EFAULT;
        goto err;
    }

    // Find new line
    for (i = 0; i < count; i++)
    {
        if (ker_buff[i] == '\n')
        {
            newlineflag = 1;
            break;
        }
    }

    // if there's no global buffer allocation
    if (!buffer->global_buffer_size)
    {
        buffer->global_copy_buffer = kmalloc(count * sizeof(char), GFP_KERNEL);
        if (!buffer->global_copy_buffer)
        {
            goto err;
        }
    }
    else
    {
        // Extend the global buffer when there's new line
        buffer->global_copy_buffer = krealloc(buffer->global_copy_buffer, (count + buffer->global_buffer_size) * sizeof(char *), GFP_KERNEL);
        if (!buffer->global_copy_buffer)
        {
            goto err;
        }
        printk(KERN_ALERT "memory reallocated for global copy buffer \n");
    }

    // copy contents from copy buffer to global buffer
    memcpy((buffer->global_copy_buffer) + (buffer->global_buffer_size), ker_buff, count * sizeof(char));
    buffer->global_buffer_size += count;

    if (newlineflag)
    {
        // Fill out the new entry
        entry.buffptr = buffer->global_copy_buffer;
        entry.size = buffer->global_buffer_size;

        // Lock the resource before addding entry
        mutex_lock(&(buffer->lock));
        potentially_free_memory = aesd_circular_buffer_add_entry(&(buffer->data_buffer), &entry);
        

        // When pointer is returned from circular buffer, free it
        if (potentially_free_memory != NULL)
        {
            kfree(potentially_free_memory);
            printk("Freed rolled over memory \n");
        }

        // reset global buffer size
        buffer->global_buffer_size = 0;
    }
    retval = count;

err:
    kfree(ker_buff);
    mutex_unlock(&(buffer->lock));
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

    // Initialize Mutex
    mutex_init(&aesd_device.lock);
    /**
     * TODO: initialize the AESD specific portion of the device
     */

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    // Variables to loop over the circular buffer
    uint8_t index;
    struct aesd_buffer_entry *entry;

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    // Freeing Circular Buffer Memory
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.data_buffer, index)
    {
        kfree(entry->buffptr);
    }

    // Destroy Mutex
    mutex_destroy(&aesd_device.lock);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
