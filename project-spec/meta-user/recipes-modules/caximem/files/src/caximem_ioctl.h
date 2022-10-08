/**
 * @file caximem_ioctl.h
 * @author wlanxww (xueweiwujxw@outlook.com)
 * @brief
 * @version 0.1
 * @date 2022-10-05
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef CAXIMEM_IOCTL_H_
#define CAXIMEM_IOCTL_H_

#include <asm/ioctl.h>

#define CAXIMEM_IOCTL_MAGIC 'W'

#define CAXIMEM_CANCEL _IO(CAXIMEM_IOCTL_MAGIC, 0)

#endif