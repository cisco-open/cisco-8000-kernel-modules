// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco utilities
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/regmap.h>
#include <regmap/internal.h>

#include <cisco/util.h>

#define DRIVER_VERSION "1.0"

void
cisco_regmap_set_max_register(struct device *dev, unsigned int max_reg)
{
	struct regmap *r = dev_get_regmap(dev, NULL);

	if (r)
		r->max_register = max_reg;
}
EXPORT_SYMBOL(cisco_regmap_set_max_register);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Cisco Utilities");
MODULE_AUTHOR("Cisco Systems, Inc. <ospo-kmod@cisco.com>");
MODULE_VERSION(DRIVER_VERSION);
