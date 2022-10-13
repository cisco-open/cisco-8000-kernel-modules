/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * util.h
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef _CISCO_UTIL_H
#define _CISCO_UTIL_H

#include <linux/version.h>	/* KERNEL_VERSION */
#include <linux/acpi.h>		/* acpi_handle */

/* forward */
struct regmap;

struct reboot_reg_info {
	u32  reg;
	u32  mask;
	u32  value;
};

struct reboot_info {
	u32			enable;
	u32			priority;
	struct reboot_reg_info	restart;
	struct reboot_reg_info	halt;
	struct reboot_reg_info	poweroff;
};

struct device;
struct attribute_group;
struct platform_device;

extern const struct attribute_group cisco_fpga_msd_xil_attr_group;
extern const struct attribute_group cisco_fpga_msd_xil_scratch_bios_attr_group;
extern const struct attribute_group cisco_fpga_msd_xil_scratch_chassis_attr_group;
extern const struct attribute_group cisco_fpga_msd_xil_scratch_uboot_attr_group;
extern const struct attribute_group cisco_fpga_msd_xil_scratch_idprom_attr_group;

extern int cisco_fpga_msd_xil_mfd_init(struct platform_device *pdev,
				       size_t priv_size,
				       uintptr_t *csr);

extern int cisco_register_reboot_notifier(struct platform_device   *pdev,
					  const struct reboot_info *r_info);

extern int cisco_fpga_select_new_acpi_companion(struct device *dev, struct regmap *r);
extern struct device *cisco_acpi_find_device_by_handle(acpi_handle h);

extern void cisco_regmap_set_max_register(struct device *dev, unsigned int max_reg);

#if KERNEL_VERSION(5, 19, 0) > LINUX_VERSION_CODE
int acpi_dev_for_each_child(struct acpi_device *parent,
			    int (*fn)(struct acpi_device *dev, void *v),
			    void *v);
#endif /* KERNEL_VERSION(5, 19, 0) > LINUX_VERSION_CODE */

#endif /* ndef _CISCO_UTIL_H */
