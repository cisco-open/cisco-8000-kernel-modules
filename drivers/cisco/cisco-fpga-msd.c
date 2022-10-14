// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco FPGA ms_dev driver.
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/version.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>

#include "cisco/reg_access.h"
#include "cisco/reg_trace.h"
#include "cisco/msd.h"
#include "cisco/mfd.h"
#include "cisco/sysfs.h"
#include "cisco/util.h"

#define DRIVER_NAME "cisco-fpga-msd"
#define DRIVER_VERSION  "1.0"

struct cisco_fpga_msd {
	u8		major_ver;
	u8		active;
	struct msd_regs_v5_t *csr;
	struct regmap	*regmap;
};

static const struct reboot_info r_info = {
	.enable  = 1,
	.priority = 32,

	.restart.reg = 0x3c,
	.restart.mask = 0xfffff7ff,
	.restart.value = 0x400,

	.halt.reg = 0x3c,
	.halt.mask = 0xfffff7ff,
	.halt.value = 0x8,

	.poweroff.reg = 0x3c,
	.poweroff.mask = 0xfffff7ff,
	.poweroff.value = 0x8,
};

#define F(field) offsetof(struct msd_regs_v5_t, field)

static ssize_t
_fc_ready_fmt(const struct sysfs_ext_attribute *attr,
	      char *buf, ssize_t buflen,
	      const u32 *data, size_t data_dim)
{
	u32 ready = data[0];

	return scnprintf(buf, buflen, "%c%c%c%c%c%c%c%c\n",
			 (ready & BIT(0)) ? '1' : '0',
			 (ready & BIT(1)) ? '1' : '0',
			 (ready & BIT(2)) ? '1' : '0',
			 (ready & BIT(3)) ? '1' : '0',
			 (ready & BIT(4)) ? '1' : '0',
			 (ready & BIT(5)) ? '1' : '0',
			 (ready & BIT(6)) ? '1' : '0',
			 (ready & BIT(7)) ? '1' : '0');
}
static ssize_t
_fc_ready_parse(const struct sysfs_ext_attribute *attr,
		const char *buf, ssize_t buflen,
		u32 *data, size_t data_dim)
{
	u32 v = 0;
	int consumed;
	char indicator[8];
	size_t index;

	if ((sscanf(buf, " %c%c%c%c%c%c%c%c %n",
		   &indicator[0],
		   &indicator[1],
		   &indicator[2],
		   &indicator[3],
		   &indicator[4],
		   &indicator[5],
		   &indicator[6],
		   &indicator[7],
		   &consumed) != 8) ||
	      (consumed != buflen))
		return -EINVAL;
	for (index = 0; index < ARRAY_SIZE(indicator); ++index) {
		char c = indicator[index];

		if (c == '1')
			v |= BIT(index);
		else if (c != '0')
			return -EINVAL;
	}
	data[0] = v;
	return consumed;
}
static CISCO_ATTR_RW_F(fc_ready, CISCO_SYSFS_ATTR_F_MASKED, F(cfg5), 0xff);

static struct attribute *_pembrey_sys_attrs[] = {
	&cisco_attr_fc_ready.attr.attr,
	NULL,
};
static const struct attribute_group _pembrey_attr_group = {
	.name = NULL,
	.attrs = _pembrey_sys_attrs,
};

static const struct attribute_group *_msd_attr_groups_v4[] = {
	&cisco_fpga_msd_xil_attr_group,
	&cisco_fpga_reghdr_attr_group,
	&_pembrey_attr_group,
	NULL,
};

static const struct attribute_group *_msd_attr_groups_v5[] = {
	&cisco_fpga_msd_xil_attr_group,
	&cisco_fpga_msd_xil_scratch_bios_attr_group,
	&cisco_fpga_msd_xil_scratch_uboot_attr_group,
	&cisco_fpga_msd_xil_scratch_chassis_attr_group,
	&cisco_fpga_msd_xil_scratch_idprom_attr_group,
	&cisco_fpga_reghdr_attr_group,
	&_pembrey_attr_group,
	NULL,
};

static int cisco_fpga_msd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cisco_fpga_msd *priv;
	int err, r;
	uintptr_t base;
	u32 v;

	err = cisco_fpga_msd_xil_mfd_init(pdev, sizeof(*priv), &base);
	if (err)
		return err;

	priv = platform_get_drvdata(pdev);
	priv->csr = (typeof(priv->csr))base;
	priv->regmap = dev_get_regmap(dev, NULL);

	if (pdev->mfd_cell && pdev->mfd_cell->platform_data
			   && pdev->mfd_cell->pdata_size == sizeof(u8))
		priv->active = *(u8 *)pdev->mfd_cell->platform_data;
	else if (pdev->id_entry)
		priv->active = pdev->id_entry->driver_data;
	else
		priv->active = 1;

	err = device_property_read_u32(dev, "standby", &v);
	if (!err && v)
		priv->active = 0;

	err = regmap_read(priv->regmap, F(hdr.info0), &v);
	if (err) {
		dev_err(dev, "failed to read version; status %d\n", err);
		return err;
	}
	priv->major_ver = REG_GET(HDR_INFO0_MAJORVER, v);
	if (priv->major_ver >= 5)
		err = devm_device_add_groups(dev, _msd_attr_groups_v5);
	else
		err = devm_device_add_groups(dev, _msd_attr_groups_v4);

	if (err < 0) {
		dev_err(dev, "devm_device_add_groups failed; status %d\n", err);
		return err;
	}
	if (priv->active && (priv->major_ver >= 5)) {
		int e = regmap_write(priv->regmap, F(scratchram), 0);

		if (e) {
			dev_warn(dev, "failed to reset boot_mode; status %d\n", e);
		} else {
			r = regmap_read(priv->regmap, F(scratchram), &v);
			if (r || v)
				dev_err(dev, "failed to clear scratchram; offset %#lx; readback %u; status %d\n",
					(unsigned long)F(scratchram), v, r);
		}
		e = REG_UPDATE_BITSe(priv->regmap, MSD_CFG5_MASTER_SELECT, X86);
		if (e)
			dev_warn(dev, "failed to set X86 as i2c master; status %d\n", e);

		r = regmap_read(priv->regmap, F(scratchram), &v);
		if (r || v)
			dev_err(dev, "secondary failure to clear scratchram; offset %#lx; readback %u; status %d\n",
				(unsigned long)F(scratchram), v, r);

		err = cisco_register_reboot_notifier(pdev, &r_info);
	} else {
		dev_warn(dev, "bypass boot_mode init; active %u; major %u\n",
			 priv->active, priv->major_ver);
	}
	return 0;
}

static const struct platform_device_id cisco_fpga_msd_id_table[] = {
	{ .name = "msd-lc",   .driver_data = 0 },
	{ .name = "msd-ft",   .driver_data = 1 },
	{ .name = "msd-fc0",  .driver_data = 1 },
	{ .name = "msd-fc1",  .driver_data = 1 },
	{ .name = "msd-fc2",  .driver_data = 1 },
	{ .name = "msd-fc3",  .driver_data = 1 },
	{ .name = "msd-fc4",  .driver_data = 1 },
	{ .name = "msd-fc5",  .driver_data = 1 },
	{ .name = "msd-fc6",  .driver_data = 1 },
	{ .name = "msd-fc7",  .driver_data = 1 },
	{ .name = "msd-rp",   .driver_data = 1 },
	{ .name = "msd-bmc",  .driver_data = 1 },
	{ .name = "msd-peer", .driver_data = 0 },
	{ .name = "msd",      .driver_data = 1 },
	{ },
};
MODULE_DEVICE_TABLE(platform, cisco_fpga_msd_id_table);

static struct platform_driver cisco_fpga_msd_driver = {
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= cisco_fpga_msd_probe,
	.id_table	= cisco_fpga_msd_id_table,
};

module_platform_driver(cisco_fpga_msd_driver);

MODULE_AUTHOR("Cisco Systems, Inc. <ospo-kmod@cisco.com>");
MODULE_DESCRIPTION("Cisco FPGA ms_dev driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS(PLATFORM_MODULE_PREFIX DRIVER_NAME);
MODULE_VERSION(DRIVER_VERSION);
