/**
 * @file caximem.c
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>

#include <linux/string.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

MODULE_AUTHOR("wlanxww");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.0");
MODULE_DESCRIPTION("caximem - loadable module - provide char dev to read and write mem");

#include "caximem.h"

unsigned int minor_number = MINOR_NUMBER;
char *driver_name = MODULE_NAME;
const char *send_signal_name = SEND_IRQ_STR;
const char *recv_signal_name = RECV_IRQ_STR;
const char *send_buffer_name = SEND_REG_STR;
const char *recv_buffer_name = RECV_REG_STR;
module_param(minor_number, int, S_IRUGO);
module_param(driver_name, charp, S_IRUGO);

static int caximem_probe(struct platform_device *pdev) {
    int rc = 0;
    struct caximem_device *caximem_dev;         // caximem_device pointer
    struct device_node *np = pdev->dev.of_node; // The device tree node structure
    struct resource *send_irq, *recv_irq;       // send and recv irq resource
    struct resource *send_reg, *recv_reg;       // send and recv register resource
    const char *of_name;                        // The name in device tree node
    int id;                                     // The number of deive in device tree node

    // Allocate device structure
    caximem_dev = kmalloc(sizeof(*caximem_dev), GFP_KERNEL);
    if (caximem_dev == NULL) {
        caximem_err("Failed to allocate the CAXI MEM device.\n");
        return -ENOMEM;
    }
    caximem_dev->pdev = pdev;

    // Get interrupt
    send_irq = platform_get_resource(pdev, IORESOURCE_IRQ, SEND_IRQ_NO);
    if (send_irq == NULL) {
        caximem_err("Failed to attach irq resource.\n");
        rc = -EINVAL;
        goto destroy_mem_dev;
    }
    if (strcmp(send_irq->name, send_signal_name)) {
        caximem_err("Error send irq name.\n");
        rc = -EINVAL;
        goto destroy_mem_dev;
    }
    caximem_dev->send_signal = send_irq->start;

    recv_irq = platform_get_resource(pdev, IORESOURCE_IRQ, RECV_IRQ_NO);
    if (recv_irq == NULL) {
        caximem_err("Failed to attach irq resource.\n");
        rc = -EINVAL;
        goto destroy_mem_dev;
    }
    if (strcmp(recv_irq->name, recv_signal_name)) {
        caximem_err("Error recv irq name.\n");
        rc = -EINVAL;
        goto destroy_mem_dev;
    }
    caximem_dev->recv_signal = recv_irq->start;

    // Get io memory info
    send_reg = platform_get_resource(pdev, IORESOURCE_MEM, SEND_REG_NO);
    if (send_reg == NULL) {
        caximem_err("Failed to attach reg resource.\n");
        rc = -EINVAL;
        goto destroy_mem_dev;
    }
    if (strcmp(send_reg->name, send_buffer_name)) {
        caximem_err("Error send buffer name.\n");
        rc = -EINVAL;
        goto destroy_mem_dev;
    }
    caximem_dev->send_offset = send_reg->start;
    caximem_dev->send_max_size = send_reg->end - send_reg->start + 1;
    caximem_info("%s %08x %08x\n", send_reg->name, caximem_dev->send_offset, caximem_dev->send_max_size);


    recv_reg = platform_get_resource(pdev, IORESOURCE_MEM, RECV_REG_NO);
    if (recv_reg == NULL) {
        caximem_err("Failed to attach reg resource.\n");
        rc = -EINVAL;
        goto destroy_mem_dev;
    }
    if (strcmp(recv_reg->name, recv_buffer_name)) {
        caximem_err("Error recv buffer name.\n");
        rc = -EINVAL;
        goto destroy_mem_dev;
    }
    caximem_dev->recv_offset = recv_reg->start;
    caximem_dev->recv_max_size = recv_reg->end - recv_reg->start + 1;
    caximem_info("%s %08x %08x\n", recv_reg->name, caximem_dev->recv_offset, caximem_dev->recv_max_size);

    // Assign deivece name
    of_name = np->name;
    rc = of_property_read_s32(np, "id", &id);
    if (rc < 0) {
        caximem_err("No id parameter\n");
        goto destroy_mem_dev;
    }
    caximem_dev->dev_name = of_name;
    caximem_dev->dev_id = id;

    // Init character device
    rc = caximem_chrdev_init(caximem_dev);
    if (rc < 0) {
        goto destroy_mem_dev;
    }

    dev_set_drvdata(&pdev->dev, caximem_dev);

    // Success
    caximem_info("driver probed.\n");
    return 0;

destroy_mem_dev:
    caximem_chrdev_exit(caximem_dev);
    kfree(caximem_dev);

    return rc;
}

static int caximem_remove(struct platform_device *pdev) {
    struct caximem_device *caximem_dev;

    caximem_dev = dev_get_drvdata(&pdev->dev);
    caximem_chrdev_exit(caximem_dev);
    kfree(caximem_dev);
    dev_set_drvdata(&pdev->dev, NULL);

    return 0;
}

static struct of_device_id caximem_of_match[] = {
    {
        .compatible = "vendor,caximem",
    },
    {/* end of list */},
};
MODULE_DEVICE_TABLE(of, caximem_of_match);

static struct platform_driver caximem_driver = {
    .driver = {
        .name = MODULE_NAME,
        .owner = THIS_MODULE,
        .of_match_table = caximem_of_match,
    },
    .probe = caximem_probe,
    .remove = caximem_remove,
};

static int __init caximem_init(void) {

    return platform_driver_register(&caximem_driver);
}

static void __exit caximem_exit(void) {
    platform_driver_unregister(&caximem_driver);
}

module_init(caximem_init);
module_exit(caximem_exit);
