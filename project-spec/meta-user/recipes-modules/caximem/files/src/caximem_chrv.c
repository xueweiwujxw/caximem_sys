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
#include <linux/string.h>

#include "caximem.h"
#include "caximem_ioctl.h"

static const char *dev_fmt = "%s_%d";

void caximem_ctrl_set(void *phyaddr, caximem_ctrl_t *kaddr) {
    memcpy(phyaddr, (void *)kaddr, sizeof(caximem_ctrl_t));
}

void caximem_ctrl_get(void *phyaddr, caximem_ctrl_t *kaddr) {
    memcpy((void *)kaddr, phyaddr, sizeof(caximem_ctrl_t));
}

static irqreturn_t send_irq_handler(int irq, void *dev) {
    struct caximem_device *cdev;

    cdev = (struct caximem_device *)dev;
    // if (waitqueue_active(&cdev->send_wq_head)) {
    atomic_dec(&cdev->send_wait);
    wake_up(&cdev->send_wq_head);
    // }
    caximem_debug("caximem send irq triggered. %d, %d\n", irq, cdev->send_signal);
    return IRQ_HANDLED;
}

static irqreturn_t recv_irq_handler(int irq, void *dev) {
    struct caximem_device *cdev;

    cdev = (struct caximem_device *)dev;
    if (waitqueue_active(&cdev->recv_wq_head)) {
        atomic_dec(&cdev->recv_wait);
        wake_up(&cdev->recv_wq_head);
    } else {
        cdev->recv_info.size = 0;
        cdev->recv_info.enable = true;
        caximem_ctrl_set(cdev->recv_info_reg, &cdev->recv_info);
    }
    caximem_debug("caximem recv irq triggered. %d, %d\n", irq, cdev->recv_signal);
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
    unsigned long p;
    struct caximem_device *caximem_dev;
    int rc;
    int atomic_store;
    p = *offset;
    caximem_dev = (struct caximem_device *)file->private_data;
    down(&caximem_dev->recv_sem);
    if (p > caximem_dev->recv_max_size) {
        caximem_err("Invalid offset.\n");
        rc = length == 0 ? 0 : -ENXIO;
        goto up_sem;
    }
    atomic_store = atomic_read(&caximem_dev->recv_wait);
    atomic_inc(&caximem_dev->recv_wait);
    wait_event(caximem_dev->recv_wq_head, atomic_read(&caximem_dev->recv_wait) == atomic_store);
    caximem_ctrl_get(caximem_dev->recv_info_reg, &caximem_dev->recv_info);
    length = caximem_dev->recv_info.size > length ? length : caximem_dev->recv_info.size;
    if (copy_to_user(buffer, (char *)caximem_dev->recv_buffer + sizeof(caximem_ctrl_t), length)) {
        caximem_err("Read buffer failed.\n");
        rc = -EFAULT;
    } else {
        caximem_debug("read %d bytes from %ld.\n", length, p);
        rc = length;
    }
    caximem_dev->recv_info.size = 0;
    caximem_dev->recv_info.enable = true;
    caximem_ctrl_set(caximem_dev->recv_info_reg, &caximem_dev->recv_info);
up_sem:
    up(&caximem_dev->recv_sem);
    return rc;
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
    unsigned long p;
    struct caximem_device *caximem_dev;
    int rc;
    int atomic_store;
    p = *offset;
    caximem_dev = (struct caximem_device *)file->private_data;
    down(&caximem_dev->send_sem);
    if (p > caximem_dev->send_max_size) {
        caximem_err("Invalid offset.\n");
        rc = length == 0 ? 0 : -ENXIO;
        goto up_sem;
    }
    if (length > caximem_dev->send_max_size - sizeof(caximem_ctrl_t)) {
        length = caximem_dev->send_max_size - sizeof(caximem_ctrl_t);
    }
    if (copy_from_user((char *)caximem_dev->send_buffer + sizeof(caximem_ctrl_t), buffer, length)) {
        caximem_err("Write buffer failed.\n");
        rc = -EFAULT;
    } else {
        atomic_store = atomic_read(&caximem_dev->send_wait);
        atomic_inc(&caximem_dev->send_wait);
        caximem_dev->send_info.size = length;
        caximem_dev->send_info.enable = true;
        caximem_ctrl_set(caximem_dev->send_info_reg, &caximem_dev->send_info);
        wait_event(caximem_dev->send_wq_head, atomic_read(&caximem_dev->send_wait) == atomic_store);
        caximem_debug("write %d bytes from %ld.\n", length, p);
        rc = length;
    }
up_sem:
    up(&caximem_dev->send_sem);
    return rc;
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
    if (down_trylock(&caximem_dev->file_sem)) {
        caximem_err("Current device is busy.\n");
        return -EBUSY;
    }
    atomic_set(&caximem_dev->send_wait, 0);
    atomic_set(&caximem_dev->recv_wait, 0);
    file->private_data = caximem_dev;
    caximem_debug("open device\n");
    return 0;
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
    atomic_set(&caximem_dev->send_wait, 0);
    atomic_set(&caximem_dev->recv_wait, 0);
    caximem_dev->send_info.size = 0;
    caximem_dev->send_info.enable = false;
    caximem_dev->recv_info.size = 0;
    caximem_dev->recv_info.enable = true;
    caximem_ctrl_set(caximem_dev->send_info_reg, &caximem_dev->send_info);
    caximem_ctrl_set(caximem_dev->recv_info_reg, &caximem_dev->recv_info);
    file->private_data = NULL;
    up(&caximem_dev->file_sem);
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

/**
 * @brief caximem device io control
 *
 * @param file The file structure pointer
 * @param cmd IO control command
 * @param arg IO control command extends args
 * @return long Returns 0, or error code less than 0 for errors
 */
static long caximem_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct caximem_device *caximem_dev;
    long rc;
    rc = 0;
    caximem_dev = (struct caximem_device *)file->private_data;

    switch (cmd) {
    case CAXIMEM_CANCEL:
        while (waitqueue_active(&caximem_dev->recv_wq_head)) {
            atomic_dec(&caximem_dev->recv_wait);
            wake_up(&caximem_dev->recv_wq_head);
        }
        while (waitqueue_active(&caximem_dev->send_wq_head)) {
            caximem_dev->send_info.size = 0;
            caximem_dev->send_info.enable = false;
            caximem_ctrl_set(caximem_dev->send_info_reg, &caximem_dev->send_info);
            atomic_dec(&caximem_dev->send_wait);
            wake_up(&caximem_dev->send_wq_head);
        }
        caximem_debug("get caximem cancel request\n");
        break;
    default:
        rc = -EPERM;
        break;
    }
    return rc;
}

static const struct file_operations caximem_fops = {
    .owner = THIS_MODULE,
    .open = caximem_open,
    .read = caximem_read,
    .write = caximem_write,
    .mmap = caximem_mmap,
    .unlocked_ioctl = caximem_ioctl,
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

    // Init send buffer
    dev->send_buffer = ioremap(dev->send_offset, dev->send_max_size);
    if (dev->send_buffer == NULL) {
        caximem_err("send buffer ioremap error");
        rc = -ENOMEM;
        goto send_irq_cleanup;
    }
    dev->send_info_reg = (caximem_ctrl_t *)dev->send_buffer;
    dev->recv_buffer = ioremap(dev->recv_offset, dev->recv_max_size);
    if (dev->recv_buffer == NULL) {
        caximem_err("recv buffer ioremap error");
        rc = -ENOMEM;
        goto unmap_send_buffer;
    }
    dev->recv_info_reg = (caximem_ctrl_t *)dev->recv_buffer;
    dev->send_info.size = 0;
    dev->send_info.enable = false;
    dev->recv_info.size = 0;
    dev->recv_info.enable = true;
    caximem_ctrl_set(dev->send_info_reg, &dev->send_info);
    caximem_ctrl_set(dev->recv_info_reg, &dev->recv_info);

    // Init semaphore
    sema_init(&dev->file_sem, 1);
    sema_init(&dev->send_sem, 1);
    sema_init(&dev->recv_sem, 1);

    // Init wait queue
    init_waitqueue_head(&dev->send_wq_head);
    init_waitqueue_head(&dev->recv_wq_head);

    // Success
    caximem_info("Success initialize chardev %s_%d.\n", dev->dev_name, dev->dev_id);
    return 0;

unmap_send_buffer:
    iounmap(dev->send_buffer);
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
    iounmap(dev->recv_buffer);
    iounmap(dev->send_buffer);
    free_irq(dev->recv_signal, dev);
    free_irq(dev->send_signal, dev);
    cdev_del(&dev->chrdev);
    device_destroy(dev->dev_class, dev->cdevno);
    class_destroy(dev->dev_class);
    unregister_chrdev_region(0, 1);
}