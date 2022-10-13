// SPDX-License-Identifier: GPL-2.0-only
/*
 * Utility function for selecting FPGA ACPI companion.
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */

#include <linux/mfd/core.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/acpi.h>

#include "cisco/fpga.h"
#include "cisco/reg_access.h"
#include "cisco/xil.h"
#include "cisco/util.h"

/*
 * If XIL IP block ever moves on the line card, then we need to switch
 * to dynamically probing the fpga to find the XIL block.
 */
#define LC_XIL(f)	(0x1000 + offsetof(struct xil_regs_t, f))

struct walk_context {
	struct device *dev;
	int e;
	u32 fpga_id;
};

static int
_set_companion(struct acpi_device *child, void *data)
{
#ifdef CONFIG_ACPI
	struct walk_context *wc = (typeof(wc))data;
	acpi_status status;
	unsigned long long adr;

	status = acpi_evaluate_integer(child->handle, "_ADR", NULL, &adr);
	if (ACPI_SUCCESS(status) && (adr == wc->fpga_id)) {
		ACPI_COMPANION_SET(wc->dev, child);
		wc->e = 0;
		return 1;
	}
	return 0;
#else /* ndef CONFIG_ACPI */
	return -ENOTSUPP;
#endif /* ndef CONFIG_ACPI */
}

int
cisco_fpga_select_new_acpi_companion(struct device *dev, struct regmap *r)
{
	struct acpi_device *parent;
	u32 e, v;
	struct walk_context wc = {
	    .dev = dev,
	    .e = 0,
	    .fpga_id = 0,
	};

	if (!dev || !r) {
		dev_err(dev, "%s: parameter error\n", __func__);
		return -EINVAL;
	}
	parent = ACPI_COMPANION(dev);
	if (!parent) {
		dev_err(dev, "%s: missing acpi parent node\n", __func__);
		return -ENODEV;
	}
	e = regmap_read(r, LC_XIL(status0), &v);
	if (!e) {
		wc.fpga_id = REG_GET(XIL_STATUS0_FPGA_ID, v);
		dev_err(dev, "%s: searching for child status0 %#x; fpga_id %#x\n",
			__func__, v, wc.fpga_id);

		wc.e = -ENODEV;
		e = acpi_dev_for_each_child(parent, _set_companion, &wc);
		if (e == 1)
			e = 0;
		if (!e && wc.e)
			e = wc.e;
		if (e)
			dev_err(dev, "%s: failed to match child node %#x\n",
				__func__, wc.fpga_id);
	} else {
		dev_err(dev, "%s: regmap read offset %#zx failed; status %d\n",
			__func__, LC_XIL(status0), e);
	}
	return e;
}
EXPORT_SYMBOL(cisco_fpga_select_new_acpi_companion);

static int
_acpi_find_match(struct device *dev, const void *data)
{
	return ACPI_HANDLE(dev) == (acpi_handle)data;
}

struct device *
cisco_acpi_find_device_by_handle(acpi_handle h)
{
	return bus_find_device(&platform_bus_type, NULL, h, _acpi_find_match);
}
EXPORT_SYMBOL(cisco_acpi_find_device_by_handle);

#if KERNEL_VERSION(5, 19, 0) > LINUX_VERSION_CODE
int
acpi_dev_for_each_child(struct acpi_device *parent,
			int (*fn)(struct acpi_device *dev, void *v),
			void *v)
{
#ifdef CONFIG_ACPI
	struct acpi_device *child;
	int e = 0;

	list_for_each_entry(child, &parent->children, node) {
		e = fn(child, v);
		if (e)
			break;
	}
	return e;
#else /* ndef CONFIG_ACPI */
	return -ENOTSUPP;
#endif /* ndef CONFIG_ACPI */
}
EXPORT_SYMBOL(acpi_dev_for_each_child);
#endif /* KERNEL_VERSION(5, 19, 0) > LINUX_VERSION_CODE */
