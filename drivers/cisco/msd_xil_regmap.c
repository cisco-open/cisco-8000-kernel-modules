// SPDX-License-Identifier: GPL-2.0-only
/*
 * Common regmap routines for msd and xil drivers
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/ctype.h>

#include "cisco/reg_access.h"
#include "cisco/hdr.h"
#include "cisco/xil.h"
#include "cisco/mfd.h"
#include "cisco/util.h"

/*
 * xil and mfd have the same size register maps
 */

/*
 * It is important that no reads to the arbitration registers
 * be made, unless explicitly requested.  In particular, this
 * means they should not be displayed in the debugfs.
 *
 * In addition, things seem to go south when drp_addr/drp_data
 * is read.
 */
static bool
_precious_reg(struct device *dev, unsigned int reg)
{
	bool b = (reg >= offsetof(struct xil_regs_t, arbi));

	b |= ((reg >= offsetof(struct xil_regs_t, drp_addr)) &&
	      (reg <= offsetof(struct xil_regs_t, status0)));
	return b;
}

int
cisco_fpga_msd_xil_mfd_init(struct platform_device *pdev,
			    size_t priv_size,
			    uintptr_t *csr)
{
	static const struct regmap_config r_config = {
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.fast_io = false,
		.max_register = sizeof(struct xil_regs_t) - 1,
		.precious_reg = _precious_reg,
	};

	return cisco_fpga_mfd_init(pdev, priv_size, csr, &r_config);
}
EXPORT_SYMBOL(cisco_fpga_msd_xil_mfd_init);
