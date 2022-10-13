// SPDX-License-Identifier: GPL-2.0-only
/*
 * Register access helpers
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/delay.h>

#undef DEBUG_REG_TRACE
#define DEBUG_REG_TRACE 1
#include <cisco/reg_access.h>

void
reg_write32(const struct device *dev, uint32_t v, void __iomem *addr)
{
	iowrite32(v, addr);
}
EXPORT_SYMBOL(reg_write32);

uint32_t
reg_read32(const struct device *dev, void __iomem *addr)
{
	return ioread32(addr);
}
EXPORT_SYMBOL(reg_read32);
