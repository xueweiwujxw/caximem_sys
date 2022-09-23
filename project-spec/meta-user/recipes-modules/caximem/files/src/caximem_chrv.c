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

static ssize_t caximem_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {
    caximem_info("read divice\n");
    return 0;
}
static ssize_t caximem_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset) {
    caximem_info("write divice\n");
    return 0;
}
static int caximem_open(struct inode *inode, struct file *file) {
    caximem_info("open divice\n");
    return 0;
}
static int caximem_release(struct inode *inode, struct file *file) {
    caximem_info("release divice\n");
    return 0;
}
static int caximem_mmap(struct file *file, struct vm_area_struct *vma) {
    caximem_info("mmap divice\n");
    return 0;
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

    // Success
    caximem_info("Success initialize chardev %s_%d.\n", dev->dev_name, dev->dev_id);
    return 0;

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
    cdev_del(&dev->chrdev);
    device_destroy(dev->dev_class, dev->cdevno);
    class_destroy(dev->dev_class);
    unregister_chrdev_region(0, 1);
}