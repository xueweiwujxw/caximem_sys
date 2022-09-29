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
#include <linux/semaphore.h>
#include <linux/wait.h>
#include <linux/atomic.h>

#define MODULE_NAME "caximem"
#define MINOR_NUMBER 0

#define SEND_IRQ_NO 0
#define SEND_IRQ_STR "send_signal"
#define RECV_IRQ_NO 1
#define RECV_IRQ_STR "recv_signal"

#define SEND_REG_NO SEND_IRQ_NO
#define SEND_REG_STR "send_buffer"
#define RECV_REG_NO RECV_IRQ_NO
#define RECV_REG_STR "recv_buffer"

#define CAXIMEM_MAGIC 0x1acffc1dul

struct caximem_ctrl
{
    bool enable;            // process enable
    unsigned int size : 24; // data size in ram
};

struct caximem_device
{
    unsigned int magic; // Magic number

    struct semaphore file_sem; // Semaphore that guarantees that the file is only opened by one process
    /**
     * send process
     */
    int send_signal;                // Signal used to notify send finish
    struct semaphore send_sem;      // Semaphore for sending datq
    unsigned long send_offset;      // Then beginning offset of dev memory for sending data
    unsigned long send_max_size;    // The maximum dev memory size for sending data
    void *send_buffer;              // The buffer for sending data
    struct caximem_ctrl *send_info; // The info for sending data
    wait_queue_head_t send_wq_head; // The wait queue header for sending
    atomic_t send_wait;             // The atomic counter for sending

    /**
     * recv process
     */
    int recv_signal;                // Signal used to notify recive finish
    struct semaphore recv_sem;      // Semaphore for writing data
    unsigned long recv_offset;      // The beginning offset of dev memory for recving data
    unsigned long recv_max_size;    // The maximum dev memory size for recving data
    void *recv_buffer;              // The buffer for recving data
    struct caximem_ctrl *recv_info; // The info for reving data
    wait_queue_head_t recv_wq_head; // The wait queue header for recving
    atomic_t recv_wait;             // The atomic counter for recving

    /**
     * character device
     */
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

#define caximem_info(fmt, ...) \
    printk(KERN_INFO MODULE_NAME ": %s: %s: " fmt, __FILENAME__, __func__, ##__VA_ARGS__)

#define caximem_err(fmt, ...) \
    printk(KERN_ERR MODULE_NAME ": %s: %s: %d: " fmt, __FILENAME__, __func__, __LINE__, ##__VA_ARGS__)

#define caximem_warn(fmt, ...) \
    printk(KERN_WARNING MODULE_NAME ": %s: %s: %d: " fmt, __FILENAME__, __func__, __LINE__, ##__VA_ARGS__)

#define caximem_debug(fmt, ...) \
    printk(KERN_DEBUG MODULE_NAME ": %s: %s: %d: " fmt, __FILENAME__, __func__, __LINE__, ##__VA_ARGS__)

#endif