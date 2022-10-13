/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * linux/mdio-ioctl.h: definitions for user space access to MDIO devices
 * Copyright 2022 Cisco Systems, Inc.
 */
#ifndef __LINUX_MDIO_IOCTL_H__
#define __LINUX_MDIO_IOCTL_H__

#if defined(__KERNEL__)
#  include <linux/types.h>
#else /* ndef __KERNEL__ */
#  include <sys/types.h>
#  include <stdint.h>
#  ifndef __user
#    define __user
#  endif /* ndef __user */
#endif /* ndef __KERNEL__ */

#include <linux/ioctl.h>

/**
 * struct mdio_access_req - register read/write over mdio bus
 * @addr: device address on bus
 * @reg: register to access on device
 * @value: For successful read requests, the register value read
 *	For write requests, the register value to write
 */
struct mdio_access_req {
	uint32_t addr;
	uint32_t reg;
	uint32_t value;
};

/**
 * struct mdio_access_blk_req - register block write over mdio bus
 * This writes a stream of data to the given register address of the
 * specified device.  This is typically used to download firmware.
 *
 * @addr: device address on bus
 * @reg: register to access on device
 * @bytes: The number of bytes to write from the given user buffer
 * @user_buffer: The data to write
 */
struct mdio_access_blk_req {
	uint32_t addr;
	uint32_t reg;
	uint32_t bytes;
	void __user *user_buffer;
};

#define MDIO_ACCESS_REGRD     _IOR('m', 1, struct mdio_access_req)
#define MDIO_ACCESS_REGWR     _IOW('m', 2, struct mdio_access_req)
#define MDIO_ACCESS_REGWR_BLK _IOW('m', 3, struct mdio_access_blk_req)

#if defined(__KERNEL__)

struct mii_bus;
struct file;

int devm_mdiodev_register(struct mii_bus *bus,
			  long (*ioctl)(struct mii_bus *bus, uint cmd, ulong arg));
#endif /* defined(__KERNEL__) */

#endif /* __LINUX_MDIO_IOCTL_H__ */
