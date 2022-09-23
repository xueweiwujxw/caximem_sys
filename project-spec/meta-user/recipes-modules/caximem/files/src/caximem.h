/**
 * @file caximem.h
 * @author wlanxww (xueweiwujxw@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-09-22
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef CAXIMEM_H_
#define CAXIMEM_H_

// kernel
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>

#define MODULE_NAME "caximem"
#define MINOR_NUMBER 0

#define SEND_IRQ_NO 0
#define SEND_IRQ_STR "send_signal"
#define RECV_IRQ_NO 1
#define RECV_IRQ_STR "recv_signal"

struct caximem_device
{
    unsigned int magic;           // Magic number
    int send_signal;              // Signal used to notify send finish
    int recv_signal;              // Signal used to notify recive finish
    int minor;                    // The major number of the device
    dev_t cdevno;                 // The device number of the device
    struct device *sys_device;    // Device structure for the device
    struct class *dev_class;      // Device class for the device
    struct cdev chrdev;           // Character device structure for the device
    struct platform_device *pdev; // Platform device structure for the device
    const char *dev_name;         // The name of the device
    int dev_id;                   // The id of the device
};

int caximem_chrdev_init(struct caximem_device *dev);
void caximem_chrdev_exit(struct caximem_device *dev);

#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define caximem_err(fmt, ...) \
    printk(KERN_ERR MODULE_NAME ": %s: %s: %d: " fmt, __FILENAME__, __func__, __LINE__, ##__VA_ARGS__)

#define caximem_info(fmt, ...) \
    printk(KERN_INFO MODULE_NAME ": %s: %s: %d: " fmt, __FILENAME__, __func__, __LINE__, ##__VA_ARGS__)

#define caximem_warn(fmt, ...) \
    printk(KERN_WARNING MODULE_NAME ": %s: %s: %d: " fmt, __FILENAME__, __func__, __LINE__, ##__VA_ARGS__)

#define caximem_debug(fmt, ...) \
    printk(KERN_DEBUG MODULE_NAME ": %s: %s: %d: " fmt, __FILENAME__, __func__, __LINE__, ##__VA_ARGS__)

#endif