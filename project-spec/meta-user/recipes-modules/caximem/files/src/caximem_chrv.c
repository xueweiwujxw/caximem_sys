/**
 * @file caximem_chrv.c
 * @author wlanxww (xueweiwujxw@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-09-22
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>

#include "caximem.h"

static const char *dev_fmt = "%s_%d";

static irqreturn_t send_irq_handler(int irq, void *dev) {
    struct caximem_device *cdev;

    cdev = (struct caximem_device *)dev;
    caximem_debug("caximem send irq triggered. %d, %d\n", irq, cdev->send_signal);
    return IRQ_HANDLED;
}

static irqreturn_t recv_irq_handler(int irq, void *dev) {
    struct caximem_device *cdev;

    cdev = (struct caximem_device *)dev;
    caximem_debug("caximem send irq triggered. %d, %d\n", irq, cdev->recv_signal);
    return IRQ_HANDLED;
}

/**
 * File Operations
 */

/**
 * @brief read data from character device
 *
 * @param file The file structure pointer
 * @param buffer The memory address of user space
 * @param length The number of bytes to read
 * @param offset The offset of the read position relative to the beginning of the file
 * @return ssize_t Returns the number of bytes read, or error code less than 0 for errors
 */
static ssize_t caximem_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {
    caximem_debug("read device\n");
    unsigned long p;
    p = *offset;
    struct caximem_device *caximem_dev;
    caximem_dev = (struct caximem_device *)file->private_data;
    if (p > caximem_dev->recv_max_size) {
        caximem_err("Invalid offset.\n");
        return length == 0 ? 0 : -ENXIO;
    }
    if (length > caximem_dev->recv_max_size - p) {
        length = caximem_dev->recv_max_size - p;
    }
    if (copy_to_user(buffer, (char *)caximem_dev->recv_buffer + p, length)) {
        caximem_err("Read buffer failed.\n");
        return -EFAULT;
    } else {
        offset += length;
        caximem_debug("read %d bytes from %ld.\n", length, p);
        return length;
    }
}

/**
 * @brief read data to character device
 *
 * @param file  The file structure pointer
 * @param buffer  The memory address of user space
 * @param length  The number of bytes to write
 * @param offset  The offset of the write position relative to the beginning of the file
 * @return ssize_t  Returns the number of bytes written, or error code less than 0 for errors
 */
static ssize_t caximem_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset) {
    caximem_debug("write device\n");
    unsigned long p;
    p = *offset;
    struct caximem_device *caximem_dev;
    caximem_dev = (struct caximem_device *)file->private_data;
    if (p > caximem_dev->send_max_size) {
        caximem_err("Invalid offset.\n");
        return length == 0 ? 0 : -ENXIO;
    }
    if (length > caximem_dev->send_max_size - p) {
        length = caximem_dev->send_max_size - p;
    }
    if (copy_from_user((char *)caximem_dev->send_buffer + p, buffer, length)) {
        caximem_err("Write buffer failed.\n");
        return -EFAULT;
    } else {
        offset += length;
        caximem_debug("write %d bytes from %ld.\n", length, p);
        return length;
    }
}

/**
 * @brief open the character device
 *
 * @param inode The inode structure pointer
 * @param file The file structure pointer
 * @return int Returns 0, or error code less than 0 for errors
 */
static int caximem_open(struct inode *inode, struct file *file) {
    struct caximem_device *caximem_dev;
    int rc = 0;
    caximem_dev = container_of(inode->i_cdev, struct caximem_device, chrdev);
    if (caximem_dev->magic != CAXIMEM_MAGIC) {
        caximem_err("caximem_dev 0x%p inode 0x%lx magic mismatch 0x%x.\n", caximem_dev, inode->i_ino, caximem_dev->magic);
        return -EINVAL;
    }
    if (!capable(CAP_SYS_ADMIN)) {
        caximem_err("No permission.\n");
        return -EACCES;
    } else if (!(file->f_flags & O_EXCL)) {
        caximem_err("No O_EXCL flags.\n");
        return -EINVAL;
    }
    file->private_data = caximem_dev;
    caximem_dev->send_buffer = ioremap(caximem_dev->send_offset, caximem_dev->send_max_size);
    if (caximem_dev->send_buffer == NULL) {
        caximem_err("send buffer ioremap error");
        rc = -ENOMEM;
        goto release_private_data;
    }
    caximem_dev->recv_buffer = ioremap(caximem_dev->recv_offset, caximem_dev->recv_max_size);
    if (caximem_dev->recv_buffer == NULL) {
        caximem_err("recv buffer ioremap error");
        rc = -ENOMEM;
        goto unmap_send_buffer;
    }
    caximem_debug("open device\n");
    return 0;
unmap_send_buffer:
    iounmap(caximem_dev->send_buffer);
release_private_data:
    file->private_data = NULL;
    return rc;
}

/**
 * @brief release the character device
 *
 * @param inode The inode structure pointer
 * @param file The file structure pointer
 * @return int Returns 0, or error code less than 0 for errors
 */
static int caximem_release(struct inode *inode, struct file *file) {
    struct caximem_device *caximem_dev;
    caximem_dev = container_of(inode->i_cdev, struct caximem_device, chrdev);
    if (caximem_dev->magic != CAXIMEM_MAGIC) {
        caximem_err("caximem_dev 0x%p inode 0x%lx magic mismatch 0x%x.\n", caximem_dev, inode->i_ino, caximem_dev->magic);
        return -EINVAL;
    }
    iounmap(caximem_dev->recv_buffer);
    iounmap(caximem_dev->send_buffer);
    file->private_data = NULL;
    caximem_debug("release device\n");
    return 0;
}

/**
 * @brief mmap (not supported)
 *
 * @return int Return -EPREM directly.
 */
static int caximem_mmap(struct file *file, struct vm_area_struct *vma) {
    return -EPERM;
}

static const struct file_operations caximem_fops = {
    .owner = THIS_MODULE,
    .open = caximem_open,
    .read = caximem_read,
    .write = caximem_write,
    .mmap = caximem_mmap,
    .release = caximem_release};

// Initialize caximem character device
int caximem_chrdev_init(struct caximem_device *dev) {
    int rc;

    // Set macic number;
    dev->magic = CAXIMEM_MAGIC;

    // Allocate a major and minor number region
    rc = alloc_chrdev_region(&dev->cdevno, 0, 1, dev->dev_name);
    if (rc < 0) {
        caximem_err("failed to allocate character device region.\n");
        goto ret;
    }

    // Create device class
    dev->dev_class = class_create(THIS_MODULE, dev->dev_name);
    if (IS_ERR(dev->dev_class)) {
        caximem_err("failed to create device class.\n");
        rc = PTR_ERR(dev->dev_class);
        goto free_chrdev_region;
    }

    // Create device
    dev->sys_device = device_create(dev->dev_class, NULL, dev->cdevno, NULL, dev_fmt, dev->dev_name, dev->dev_id);
    if (IS_ERR(dev->sys_device)) {
        caximem_err("failed to create device.\n");
        rc = PTR_ERR(dev->sys_device);
        goto class_cleanup;
    }

    // Register this character device in kernel
    cdev_init(&dev->chrdev, &caximem_fops);
    rc = cdev_add(&dev->chrdev, dev->cdevno, 1);
    if (rc < 0) {
        caximem_err("failed to add a character device.\n");
        goto device_cleanup;
    }
    caximem_debug("major: %d, minor: %d", MAJOR(dev->cdevno), MINOR(dev->cdevno));

    // Register interrupt
    rc = request_irq(dev->send_signal, send_irq_handler, IRQF_TRIGGER_RISING, MODULE_NAME, dev);
    if (rc < 0) {
        caximem_err("failed to request send interrupt.\n");
        goto chrdev_cleanup;
    }
    rc = request_irq(dev->recv_signal, recv_irq_handler, IRQF_TRIGGER_RISING, MODULE_NAME, dev);
    if (rc < 0) {
        caximem_err("failed to request send interrupt.\n");
        goto send_irq_cleanup;
    }

    // Success
    caximem_info("Success initialize chardev %s_%d.\n", dev->dev_name, dev->dev_id);
    return 0;

send_irq_cleanup:
    free_irq(dev->send_signal, dev);
chrdev_cleanup:
    cdev_del(&dev->chrdev);
device_cleanup:
    device_destroy(dev->dev_class, dev->cdevno);
class_cleanup:
    class_destroy(dev->dev_class);
free_chrdev_region:
    unregister_chrdev_region(0, 1);
ret:
    return rc;
}

// Clean up caximem character device struct
void caximem_chrdev_exit(struct caximem_device *dev) {
    free_irq(dev->recv_signal, dev);
    free_irq(dev->send_signal, dev);
    cdev_del(&dev->chrdev);
    device_destroy(dev->dev_class, dev->cdevno);
    class_destroy(dev->dev_class);
    unregister_chrdev_region(0, 1);
}