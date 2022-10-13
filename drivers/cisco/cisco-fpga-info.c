// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco FPGA info_rom driver.
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/version.h>
#include <linux/regmap.h>
#include <linux/property.h>

#include "cisco/reg_access.h"
#include "cisco/info.h"
#include "cisco/mfd.h"
#include "cisco/sysfs.h"

#define DRIVER_NAME "cisco-fpga-info"
#define DRIVER_VERSION  "1.0"

#define F(field) offsetof(struct info_regs_v6_t, field)

/*
 * sysfs file fpga_family
 */
static ssize_t
_fpga_family_fmt(const struct sysfs_ext_attribute *attr,
		 char *buf, ssize_t len,
		 const u32 *data, size_t data_dim)
{
	return scnprintf(buf, len, "%u\n",
			 REG_GET(INFO_DEVICE_FAMILY, data[0]));
}
static CISCO_ATTR_RO(fpga_family, F(device));

/*
 * sysfs file fpga_vendor
 */
static ssize_t
_fpga_vendor_fmt(const struct sysfs_ext_attribute *attr,
		 char *buf, ssize_t len,
		 const u32 *data, size_t data_dim)
{
	return scnprintf(buf, len, "%u\n",
			 REG_GET(INFO_DEVICE_VENDOR, data[0]));
}
static CISCO_ATTR_RO(fpga_vendor, F(device));

/*
 * sysfs file fpga_id
 */
static ssize_t
_fpga_id_fmt(const struct sysfs_ext_attribute *attr,
	     char *buf, ssize_t len,
	     const u32 *data, size_t data_dim)
{
	return scnprintf(buf, len, "%#x\n", data[0]);
}
static CISCO_ATTR_RO(fpga_id, F(fpga_id));

/*
 * sysfs file config_info
 */
static ssize_t
_config_info_fmt(const struct sysfs_ext_attribute *attr,
		 char *buf, ssize_t len,
		 const u32 *data, size_t data_dim)
{
	static const char * const _cfg_info[REG_LIMIT(INFO_CFG_INFO_CFG_INFO)] = {
	    [0] = "0: reserved",
	    [1] = "1: reserved",
	    [2] = "Golden image",
	    [3] = "Upgrade image",
	};
	u32 index = REG_GET(INFO_CFG_INFO_CFG_INFO, data[0]);
	const char *s = _cfg_info[index];

	if (s)
		return scnprintf(buf, len, "%s\n", s);
	return scnprintf(buf, len, "%u: illegal\n", index);
}
static CISCO_ATTR_RO(config_info, F(cfg_info));

/*
 * sysfs file version
 */
static ssize_t
_version_fmt(const struct sysfs_ext_attribute *attr,
	     char *buf, ssize_t len,
	     const u32 *data, size_t data_dim)
{
	u32 version;
	u32 build;

	BUG_ON(data_dim < 2);
	version = data[0];
	build = data[1];

	return scnprintf(buf, len, "%u.%u.%u-%u\n",
			 REG_GET(INFO_VERSION_REVMAJ, version),
			 REG_GET(INFO_VERSION_REVMIN, version),
			 REG_GET(INFO_VERSION_REVDBG, version),
			 REG_GET(INFO_BUILD_BLDNO, build));
}
static CISCO_ATTR_RO2(version, F(version), F(build));

/*
 * sysfs file comment
 */
static ssize_t
comment_show(struct device *dev,
	     struct device_attribute *attr,
	     char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	size_t len = PAGE_SIZE;
	ssize_t err = 0;
	size_t i;
	union {
		char comment[24];
		u32 data[6];
	} u;

	if (r) {
		for (i = 0; !err && (i < ARRAY_SIZE(u.data)); i++)
			err = regmap_read(r, F(comment_str[i]), &u.data[i]);
		if (!err)
			err = scnprintf(buf, len, "%.*s\n",
					(int) sizeof(u),
					u.comment);
	} else {
		err = -ENXIO;
	}
	return err;
}
static DEVICE_ATTR_RO(comment);

/*
 * sysfs file name
 */
static ssize_t
name_show(struct device *dev,
	  struct device_attribute *attr,
	  char *buf)
{
	size_t len = PAGE_SIZE;
	ssize_t err = 0;
	const char *value;

	err = device_property_read_string(dev, "fpd-name", &value);
	if (err >= 0)
		err = scnprintf(buf, len, "%s\n", value);
	return err;
}
static DEVICE_ATTR_RO(name);

/*
 * sysfs file description
 */
static ssize_t
description_show(struct device *dev,
		 struct device_attribute *attr,
		 char *buf)
{
	size_t len = PAGE_SIZE;
	ssize_t err = 0;
	const char *value;

	err = device_property_read_string(dev, "fpd-description", &value);
	if (err >= 0)
		err = scnprintf(buf, len, "%s\n", value);
	return err;
}
static DEVICE_ATTR_RO(description);

static struct attribute *_info_sys_attrs[] = {
	&cisco_attr_fpga_family.attr.attr,
	&cisco_attr_fpga_vendor.attr.attr,
	&cisco_attr_fpga_id.attr.attr,
	&cisco_attr_config_info.attr.attr,
	&cisco_attr_version.attr.attr,
	&dev_attr_comment.attr,
	NULL,
};
static const struct attribute_group _info_attr_group = {
	.attrs = _info_sys_attrs,
};

static umode_t
_info_sys_acpi_is_visible(struct kobject *kobj,
			  struct attribute *attr,
			  int blah)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	char *var = kasprintf(GFP_KERNEL, "fpd-%s", attr->name);
	bool b = device_property_present(dev, var);

	kfree(var);
	return b ? attr->mode : 0;
}

static struct attribute *_info_sys_acpi_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_description.attr,
	NULL,
};
static const struct attribute_group _info_sys_acpi_attr_group = {
	.attrs = _info_sys_acpi_attrs,
	.is_visible = _info_sys_acpi_is_visible,
};

static const struct attribute_group *_info_attr_groups[] = {
	&_info_attr_group,
	&_info_sys_acpi_attr_group,
	&cisco_fpga_reghdr_attr_group,
	NULL,
};

static int
cisco_fpga_info_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int err;
	static const struct regmap_config r_config = {
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.fast_io = false,
		.max_register = sizeof(struct info_regs_v6_t) - 1,
	};

	err = cisco_fpga_mfd_init(pdev, 0, NULL, &r_config);
	if (!err) {
		err = sysfs_create_groups(&dev->kobj, _info_attr_groups);
		if (err < 0)
			dev_err(dev, "sysfs_create_groups failed; status %d\n", err);
	}
	return err;
}

static int
cisco_fpga_info_remove(struct platform_device *pdev)
{
	sysfs_remove_groups(&pdev->dev.kobj, _info_attr_groups);
	return 0;
}

static const struct platform_device_id cisco_fpga_info_id_table[] = {
	{ .name = "info-lc",		.driver_data = 0 },
	{ .name = "info-fc0",		.driver_data = 0 },
	{ .name = "info-fc1",		.driver_data = 0 },
	{ .name = "info-fc2",		.driver_data = 0 },
	{ .name = "info-fc3",		.driver_data = 0 },
	{ .name = "info-fc4",		.driver_data = 0 },
	{ .name = "info-fc5",		.driver_data = 0 },
	{ .name = "info-fc6",		.driver_data = 0 },
	{ .name = "info-fc7",		.driver_data = 0 },
	{ .name = "info-ft",		.driver_data = 1 },
	{ .name = "info2-ft",		.driver_data = 1 },
	{ .name = "info-rp",		.driver_data = 1 },
	{ .name = "info-peer",		.driver_data = 0 },
	{ .name = "info",		.driver_data = 1 },
	{ .name = "info-pim1",		.driver_data = 1 },
	{ .name = "info-pim2",		.driver_data = 1 },
	{ .name = "info-pim3",		.driver_data = 1 },
	{ .name = "info-pim4",		.driver_data = 1 },
	{ .name = "info-pim5",		.driver_data = 1 },
	{ .name = "info-pim6",		.driver_data = 1 },
	{ .name = "info-pim7",		.driver_data = 1 },
	{ .name = "info-pim8",		.driver_data = 1 },
	{ },
};
MODULE_DEVICE_TABLE(platform, cisco_fpga_info_id_table);

static struct platform_driver cisco_fpga_info_driver = {
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= cisco_fpga_info_probe,
	.remove		= cisco_fpga_info_remove,
	.id_table	= cisco_fpga_info_id_table,
};

module_platform_driver(cisco_fpga_info_driver);

MODULE_AUTHOR("Cisco Systems, Inc. <ospo-kmod@cisco.com>");
MODULE_DESCRIPTION("Cisco FPGA info_rom driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS(PLATFORM_MODULE_PREFIX DRIVER_NAME);
MODULE_VERSION(DRIVER_VERSION);
