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
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Trapti Balgi"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    /* Device information*/
    struct scull_dev *dev;
    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = dev;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */

    filp->private_data = NULL;

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    ssize_t bytes_to_copy = 0;
    struct aesd_dev *device = NULL;
    struct aesd_buffer_entry *entry = NULL;
    size_t entry_offset_byte = 0;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    /* Input validity check */
    if ((buf == NULL) || (filp == NULL) || (f_pos == NULL))
    {
        PDEBUG("aesd_read: Invalid inputs");
        retval = -EINVAL;
        goto read_exit;
    }

    device = filp->private_data;
    if (device == NULL)
    {
        PDEBUG("aesd_read: filp->private_data failed");
        retval = -EPERM;
        goto read_exit;
    }

    /* Lock the buffer_mutex */
    if (mutex_lock_interruptible(&device->buffer_mutex)) 
    {
        PDEBUG("aesd_read: Could not lock buffer_mutex");
        retval = -ERESTARTSYS;
        goto read_exit;
    }

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&device->buffer, *f_pos, &entry_offset_byte);
    if (entry == NULL)
    {
        PDEBUG("aesd_read: aesd_circular_buffer_find_entry_offset_for_fpos failed");
        /* Read 0 bytes */
        retval = 0;
    	goto read_unlock;
    }
    
    bytes_to_copy = entry->size - entry_offset_byte;
    /* Check for max limit to bytes to be copied */
    if (bytes_to_copy > count)
        bytes_to_copy = count;

    /* Copy bytes. Returns number of bytes not copied*/
    retval = copy_to_user(buf, entry->buffptr + entry_offset_byte, bytes_to_copy);
    if (retval != 0)
    {
        PDEBUG("aesd_write: copy_to_user failed");
        retval = -EFAULT;
        goto read_unlock;
    }

    retval = bytes_to_copy - retval;
    *f_pos += retval;

read_unlock:
    mutex_unlock(&device->buffer_mutex);

read_exit:
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    ssize_t bytes_not_copied = 0;
    struct aesd_dev *device = NULL;
    char *tmp_buffer = NULL;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    /* Input validity check */
    if ((buf == NULL) || (filp == NULL))
    {
        PDEBUG("aesd_write: Invalid inputs");
        retval = -EINVAL;
        goto write_exit;
    }

    device = filp->private_data;
    if (device == NULL)
    {
        PDEBUG("aesd_write: filp->private_data failed");
        retval = -EPERM;
        goto write_exit;
    }

    /* Lock the buffer_mutex */
    if (mutex_lock_interruptible(&device->buffer_mutex)) 
    {
        PDEBUG("aesd_write: Could not lock buffer_mutex");
        retval = -ERESTARTSYS;
        goto write_exit;
    }

    tmp_buffer = kmalloc(count, GFP_KERNEL);
    if (tmp_buffer == NULL)
    {
        PDEBUG("aesd_write: kmalloc failed for tmp_buffer");
        retval = -ENOMEM;
        goto write_unlock;
    }

    bytes_not_copied = copy_from_user(tmp_buffer, buf, count);
    if (bytes_not_copied != 0)
    {
        PDEBUG("aesd_write: copy_from_user failed");
        retval = -EFAULT;
        goto write_free_buffer;
    }

    size_t copied_size = count - bytes_not_copied;
    /* Check for newline */
    bool is_newline = (copied_size > 0 && tmp_buffer[copied_size - 1] == '\n');

    /* If newline, add to the circular buffer*/
    if (is_newline)
    {
        /* Resize the CB */
        device->entry.buffptr = kmalloc(device->entry.size + copied_size, GFP_KERNEL);
        if (device->entry.buffptr == NULL)
        {
            PDEBUG("aesd_write: kmalloc failed for entry buffer");
            retval = -ENOMEM;
            goto write_free_buffer;
        }

        /* Copy data to buffer */
        for (size_t i = 0; i < copied_size; i++) 
        {
            device->entry.buffptr[device->entry.size + i] = tmp_buffer[i];
        }
        device->entry.size += copied_size;

        /* Add to circular buffer and free the old buffer if needed */
        const char *old_buffer = aesd_circular_buffer_add_entry(&device->buffer, &device->entry);
        if (old_buffer != NULL)
        {
            kfree(old_buffer);
            old_buffer = NULL;
        }

        /* Reset for next write */
        device->entry.buffptr = NULL;
        device->entry.size = 0;
    }
    else
    {
        /* If doesn't end with a newline, accumulate for next write */
        device->entry.buffptr = krealloc(device->entry.buffptr, device->entry.size + copied_size, GFP_KERNEL);
        if (device->entry.buffptr == NULL)
        {
            PDEBUG("aesd_write: krealloc failed for entry buffer");
            retval = -ENOMEM;
            goto write_free_buffer;
        }

        /* Append data to the current entry */
        for (size_t i = 0; i < copied_size; i++) 
        {
            device->entry.buffptr[device->entry.size + i] = tmp_buffer[i];
        }
        device->entry.size += copied_size;
    }

    retval = copied_size; 

write_free_buffer:
    kfree(tmp_buffer);

write_unlock:
    mutex_unlock(&device->buffer_mutex);

write_exit:
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
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
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.buffer_mutex);
    aesd_circular_buffer_init(&aesd_device.buffer);

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

    uint8_t index = 0;
    struct aesd_buffer_entry *entryptr = NULL;

    AESD_CIRCULAR_BUFFER_FOREACH(entryptr, &aesd_device.buffer, index)
    {
        if(entry->buffptr != NULL)
		{
			kfree(entry->buffptr);
			entry->buffptr = NULL;
		}
    }

    mutex_destroy(&aesd_device.buffer_mutex);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
