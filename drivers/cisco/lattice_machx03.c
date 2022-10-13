// SPDX-License-Identifier: GPL-2.0-only
/*
 * Lattice machx03 i2c driver.
 *
 * Copyright (c) 2021-2022 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/sysfs.h>
#include <linux/version.h>

#define DRIVER_NAME		"lattice_machx03"
#define DRIVER_VERSION		"1.0"

#define MINOR_VERSION_OFFSET	8

enum machxo3_regs_e {
	MACHXO3_REG_SCRATCH_0,   // 0
	MACHXO3_REG_SCRATCH_1,   // 1
	MACHXO3_REG_FW_VERSION,  // 2
	MAX_NUM_OF_MACHXO3_REGS
};

struct machx03_data {
	uint16_t cpld_version;
	struct i2c_client *client;
};

static ssize_t
cpld_version_show(struct device *dev,
		  struct device_attribute *attr,
		  char *buf)
{
	struct machx03_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	size_t len = PAGE_SIZE;
	uint16_t version;
	uint8_t major, minor;

	i2c_smbus_write_word_data(client, 0, MACHXO3_REG_FW_VERSION);

	version = i2c_smbus_read_word_data(client, 0);

	major = version & 0xFF;
	minor = (version >> MINOR_VERSION_OFFSET) & 0xFF;

	return scnprintf(buf, len, "%d.%d\n", major, minor);
}
static DEVICE_ATTR_RO(cpld_version);

static struct attribute *_machx03_data_sys_attrs[] = {
	&dev_attr_cpld_version.attr,
	NULL,
};
static const struct attribute_group _machx03_data_attr_group = {
	.name = NULL,
	.attrs = _machx03_data_sys_attrs,
};
static const struct attribute_group *_machx03_attr_groups[] = {
	&_machx03_data_attr_group,
	NULL,
};

static int
lattice_machx03_probe_new(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct machx03_data *data;

	data = devm_kzalloc(dev, sizeof(struct machx03_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->client = client;

	dev_set_drvdata(dev, data);

	return devm_device_add_groups(dev, _machx03_attr_groups);
}

#if KERNEL_VERSION(4, 9, 189) >= LINUX_VERSION_CODE
static int
lattice_machx03_probe(struct i2c_client *client,
		      const struct i2c_device_id *id)
{
	return lattice_machx03_probe_new(client);
}
#endif /* KERNEL_VERSION(4, 9, 189) >= LINUX_VERSION_CODE */

static int
lattice_machx03_detect(struct i2c_client *client,
		       struct i2c_board_info *info)
{
	return 0;
}

static const struct i2c_device_id lattice_machx03_id[] = {
	{ "lattice_machx03", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lattice_machx03_id);

static struct i2c_driver lattice_machx03_driver = {
	.driver = {
		.name   = DRIVER_NAME,
	},
#if KERNEL_VERSION(4, 9, 189) < LINUX_VERSION_CODE
	.probe_new = lattice_machx03_probe_new,
#else /* KERNEL_VERSION(4, 9, 189) >= LINUX_VERSION_CODE */
	.probe = lattice_machx03_probe,
#endif /* KERNEL_VERSION(4, 9, 189) >= LINUX_VERSION_CODE */
	.id_table = lattice_machx03_id,
	.detect = lattice_machx03_detect,
};

module_i2c_driver(lattice_machx03_driver);

MODULE_AUTHOR("Cisco Systems, Inc. <ospo-kmod@cisco.com>");
MODULE_DESCRIPTION("Cisco lattice_machx03 driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS(PLATFORM_MODULE_PREFIX DRIVER_NAME);
MODULE_VERSION(DRIVER_VERSION);
