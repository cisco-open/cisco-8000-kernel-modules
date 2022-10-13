// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco FPGA Power Sequencing driver.
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/regmap.h>
#include <linux/property.h>
#include <linux/mfd/core.h>

#include "cisco/reg_access.h"
#include "cisco/mfd.h"
#include "cisco/pseq.h"
#include "cisco/sysfs.h"
#include "cisco/util.h"

#define DRIVER_NAME	"cisco-fpga-pseq"
#define DRIVER_VERSION	"1.0"

#define DRIVER_DATA_ACTIVE     0x1
#define DRIVER_DATA_OVERRIDE   0x2

struct cisco_fpga_pseq {
	struct regmap *regmap;
	struct pseq_regs_v4_t __iomem *csr;
	struct delayed_work work;
	u8 active;
	u32 num_rails[2];
	char *rail_names[64];
};

#define R(field) offsetof(struct pseq_regs_v4_t, field)

static const char *
_power_down_reason(u32 raw)
{
	static const char * const _reasons[REG_LIMIT(PSEQ_GEN_STAT_POWER_DOWN_REASON)] = {
		[0] = "Sequencer has not been powered down",
		[1] = "User powered down",
		[2] = "Overvoltage error",
		[3] = "Undervoltage error",
		[4] = "Failed FPGA power rail",
		[5] = "Error from other power sequencer",
		[6] = "User power cycled",
		[7] = "Unknown reason #7",
	};
	return _reasons[REG_GET(PSEQ_GEN_STAT_POWER_DOWN_REASON, raw)];
}

static const char *
_power_state(u32 raw)
{
	static const char *_reasons[REG_LIMIT(PSEQ_GEN_STAT_POWER_STATE)] = {
		[0] = "All rails powered off",
		[1] = "Rails are being sequenced on",
		[2] = "All rails powered on",
		[3] = "Rails are being sequenced off",
	};
	return _reasons[REG_GET(PSEQ_GEN_STAT_POWER_STATE, raw)];
}

/*
 * sysfs file interrupt
 */
static ssize_t
_interrupt_fmt(const struct sysfs_ext_attribute *attr,
	       char *buf, ssize_t buflen,
	       const u32 *data, size_t data_dim)
{
	u32 cfg0, cfg1;

	BUG_ON(data_dim < 2);
	cfg0 = data[0];
	cfg1 = data[1];

	return scnprintf(buf, buflen, "msi: %u; cookie: %u\n",
			     REG_GET(PSEQ_INTR_CFG1_MSI, cfg1),
			     REG_GET(PSEQ_INTR_CFG0_DATA, cfg0));
}
static ssize_t
_interrupt_parse(const struct sysfs_ext_attribute *attr,
		 const char *buf, ssize_t buflen,
		 u32 *data, size_t data_dim)
{
	int msi, cookie;
	int consumed;

	BUG_ON(data_dim < 2);
	if ((sscanf(buf, "msi: %i; cookie: %i %n", &msi, &cookie, &consumed) == 2)) {
		data[0] = REG_SET(PSEQ_INTR_CFG0_DATA, cookie);
		data[1] = REG_SET(PSEQ_INTR_CFG1_MSI, msi);
		return consumed;
	}
	return -EINVAL;
}
CISCO_ATTR_RW2(interrupt, R(intr_cfg0), R(intr_cfg1));

/*
 * sysfs file config
 */
static ssize_t
_config_fmt(const struct sysfs_ext_attribute *attr,
	    char *buf, ssize_t buflen,
	    const u32 *data, size_t data_dim)
{
	u32 cfg = data[0];

	return scnprintf(buf, buflen, "raw=%#x\n"
				      "ignore_other_err=%c\n"
				      "ignore_device_err=%c\n"
				      "ignore_ov=%c\n"
				      "ignore_uv=%c\n",
			 REG_GET(PSEQ_GEN_CFG, cfg),
			 REG_GET(PSEQ_GEN_CFG_IGNORE_OTHER_ERR, cfg) ? '1' : '0',
			 REG_GET(PSEQ_GEN_CFG_IGNORE_DEVICE_ERR, cfg) ? '1' : '0',
			 REG_GET(PSEQ_GEN_CFG_IGNORE_OV, cfg) ? '1' : '0',
			 REG_GET(PSEQ_GEN_CFG_IGNORE_UV, cfg) ? '1' : '0');
}

/*
 * The register spec defines these as bits, but doesn't state
 * the priority of the bits.  It may be better for us to treat
 * the command bits as a 3-bit field, with 3 explicitly allowed
 * settings, instead of as a bitmask here (which would allow
 * 8 settings).
 */
static const struct sysfs_ext_attribute_bit_store_table _config_store_table[] = {
	STORE_TABLE_ENTRY("on", REG_SETc(PSEQ_GEN_CFG_USER_POWER_ON, 1)),
	STORE_TABLE_ENTRY("off", REG_SETc(PSEQ_GEN_CFG_USER_POWER_OFF, 1)),
	STORE_TABLE_ENTRY("cycle", REG_SETc(PSEQ_GEN_CFG_USER_POWER_CYCLE, 1)),
	STORE_TABLE_ENTRY("ignore", REG_SETc(PSEQ_GEN_CFG_IGNORE_OTHER_ERR, 1)
			   |  REG_SETc(PSEQ_GEN_CFG_IGNORE_DEVICE_ERR, 1)
			   |  REG_SETc(PSEQ_GEN_CFG_IGNORE_OV, 1)
			   |  REG_SETc(PSEQ_GEN_CFG_IGNORE_UV, 1)),
	STORE_TABLE_ENTRY("other", REG_SETc(PSEQ_GEN_CFG_IGNORE_OTHER_ERR, 1)),
	STORE_TABLE_ENTRY("device", REG_SETc(PSEQ_GEN_CFG_IGNORE_DEVICE_ERR, 1)),
	STORE_TABLE_ENTRY("ov", REG_SETc(PSEQ_GEN_CFG_IGNORE_OV, 1)),
	STORE_TABLE_ENTRY("uv", REG_SETc(PSEQ_GEN_CFG_IGNORE_UV, 1)),
};
CISCO_ATTR_RW_TABLE(config, R(gen_cfg));

/*
 * sysfs file status
 */
static ssize_t
_status_fmt(const struct sysfs_ext_attribute *attr,
	    char *buf, ssize_t buflen,
	    const u32 *data, size_t data_dim)
{
	u32 gen_stat = data[0];

	return scnprintf(buf, buflen, "raw=%#x\n"
				      "power_down_reason=%s\n"
				      "power_state=%s\n"
				      "power_status_led=%#x\n",
			 REG_GET(PSEQ_GEN_STAT, gen_stat),
			 _power_down_reason(gen_stat),
			 _power_state(gen_stat),
			 REG_GET(PSEQ_GEN_STAT_POWER_STATUS_LED, gen_stat));
}
CISCO_ATTR_RO(status, R(gen_stat));

/*
 * sysfs file power_down_reason
 */
static ssize_t
_power_down_reason_fmt(const struct sysfs_ext_attribute *attr,
		       char *buf, ssize_t buflen,
		       const u32 *data, size_t data_dim)
{
	return scnprintf(buf, buflen, "%s\n", _power_down_reason(*data));
}
CISCO_ATTR_RO(power_down_reason, R(gen_stat));

/*
 * sysfs file power_state
 */
static ssize_t
_power_state_fmt(const struct sysfs_ext_attribute *attr,
		 char *buf, ssize_t buflen,
		 const u32 *data, size_t data_dim)
{
	return scnprintf(buf, buflen, "%s\n", _power_state(*data));
}
CISCO_ATTR_RO(power_state, R(gen_stat));

static ssize_t
_show_rails(struct cisco_fpga_pseq *priv, const u32 *datap, char *buf, size_t len)
{
	size_t wrote = 0;
	u32 *limitp = priv->num_rails;
	char **names = priv->rail_names;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(priv->num_rails); ++index) {
		u32 data = *datap++;
		u32 limit = *limitp++;
		int bit;

		for (bit = 0; (len > 0) && (bit < limit); ++bit, ++names) {
			if (data & (1 << bit)) {
				ssize_t e = 0;

				if (!*names)
					e = scnprintf(buf, len, "Rail @ bit %u\n", bit);
				else if (**names)
					e = scnprintf(buf, len, "%s\n", *names);
				wrote += e;
				buf += e;
				len -= e;
			}
		}
	}
	return wrote;
}

static int
_read_reg(struct cisco_fpga_pseq *priv, u32 reg, u32 *data, int invert)
{
	struct regmap *r = priv->regmap;
	int e;

	if (!r)
		return -ENXIO;

	e = regmap_read(r, reg, data);
	if (invert)
		*data = ~*data;

	++data;
	if (!e && priv->num_rails[1]) {
		e = regmap_read(r, reg + 4, data);
		if (invert)
			*data = ~*data;
	}
	return e;
}

static ssize_t
power_enabled_show(struct device *dev,
		   struct device_attribute *attr,
		   char *buf)
{
	struct cisco_fpga_pseq *priv = dev_get_drvdata(dev);
	size_t len = PAGE_SIZE;
	ssize_t err;
	u32 data[2];

	err = _read_reg(priv, R(power_en0), data, 0);
	if (!err)
		err = _show_rails(priv, data, buf, len);

	return err;
}
static DEVICE_ATTR_RO(power_enabled);

static ssize_t
power_disabled_show(struct device *dev,
		    struct device_attribute *attr,
		    char *buf)
{
	struct cisco_fpga_pseq *priv = dev_get_drvdata(dev);
	size_t len = PAGE_SIZE;
	ssize_t err;
	u32 data[2];

	err = _read_reg(priv, R(power_en0), data, 1);
	if (!err)
		err = _show_rails(priv, data, buf, len);

	return err;
}
static DEVICE_ATTR_RO(power_disabled);

static ssize_t
power_good_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct cisco_fpga_pseq *priv = dev_get_drvdata(dev);
	size_t len = PAGE_SIZE;
	ssize_t err;
	u32 data[2];

	err = _read_reg(priv, R(power_good0), data, 0);
	if (!err)
		err = _show_rails(priv, data, buf, len);

	return err;
}
static DEVICE_ATTR_RO(power_good);

static ssize_t
power_bad_show(struct device *dev,
	       struct device_attribute *attr,
	       char *buf)
{
	struct cisco_fpga_pseq *priv = dev_get_drvdata(dev);
	size_t len = PAGE_SIZE;
	ssize_t err;
	u32 data[2];

	err = _read_reg(priv, R(power_good0), data, 1);
	if (!err)
		err = _show_rails(priv, data, buf, len);

	return err;
}
static DEVICE_ATTR_RO(power_bad);

static ssize_t
power_over_voltage_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct cisco_fpga_pseq *priv = dev_get_drvdata(dev);
	size_t len = PAGE_SIZE;
	ssize_t err;
	u32 data[2];

	err = _read_reg(priv, R(power_ov0), data, 1);
	if (!err)
		err = _show_rails(priv, data, buf, len);

	return err;
}
static DEVICE_ATTR_RO(power_over_voltage);

static struct attribute *_pseq_sys_attrs[] = {
	&cisco_attr_interrupt.attr.attr,
	&cisco_attr_config.attr.attr,
	&cisco_attr_status.attr.attr,
	&cisco_attr_power_down_reason.attr.attr,
	&cisco_attr_power_state.attr.attr,
	&dev_attr_power_enabled.attr,
	&dev_attr_power_disabled.attr,
	&dev_attr_power_good.attr,
	&dev_attr_power_bad.attr,
	&dev_attr_power_over_voltage.attr,
	NULL,
};
static const struct attribute_group _pseq_attr_group = {
	.attrs = _pseq_sys_attrs,
};
static const struct attribute_group *_pseq_attr_groups[] = {
	&_pseq_attr_group,
	&cisco_fpga_reghdr_attr_group,
	NULL,
};

static void
_probe_status(struct work_struct *work)
{
	struct cisco_fpga_pseq *priv = container_of(work, typeof(*priv), work.work);
	struct device *dev = regmap_get_device(priv->regmap);
	int err, err2;
	u32 power_good, gen_stat;

	err = regmap_read(priv->regmap, R(power_good0), &power_good);
	err2 = regmap_read(priv->regmap, R(gen_stat), &gen_stat);
	if (err < 0)
		dev_err(dev, "failed to read power_good0; status %d\n", err);
	else if (err2 < 0)
		dev_err(dev, "failed to read gen_stat; status %d\n", err2);
	else
		dev_info(dev, "power good 0x%08x; power_state=%s",
			 power_good, _power_state(gen_stat));
}

static int
cisco_fpga_pseq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cisco_fpga_pseq *priv;
	u32 val;
	int err;
	uintptr_t base;
	static const struct regmap_config r_config = {
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.fast_io = false,
		.max_register = sizeof(struct pseq_regs_v4_t) - 1,
	};

	err = cisco_fpga_mfd_init(pdev, sizeof(*priv), &base, &r_config);
	if (err)
		return err;

	priv = platform_get_drvdata(pdev);
	priv->csr = (typeof(priv->csr))base;
	priv->regmap = dev_get_regmap(dev, NULL);

	if (pdev->id_entry && (pdev->id_entry->driver_data & DRIVER_DATA_OVERRIDE))
		priv->active = pdev->id_entry->driver_data & DRIVER_DATA_ACTIVE;
	else if (pdev->mfd_cell && pdev->mfd_cell->platform_data
		 && pdev->mfd_cell->pdata_size == sizeof(u8))
		priv->active = *(u8 *)pdev->mfd_cell->platform_data;
	else if (pdev->id_entry)
		priv->active = pdev->id_entry->driver_data & DRIVER_DATA_ACTIVE;
	else
		priv->active = 1;

	err = device_property_read_u32(dev, "standby", &val);
	if (!err && val)
		priv->active = 0;

	err = device_property_read_u32(dev, "num-rails", &val);
	if (err)
		val = 32;

	if (val > 64)
		val = 64;

	if (val > 32) {
		priv->num_rails[0] = 32;
		priv->num_rails[1] = val - 32;
	} else {
		priv->num_rails[0] = val;
		priv->num_rails[1] = 0;
	}
	err = device_property_read_string_array(dev, "rail-names", (const char **)&priv->rail_names, val);
	if (err < 0)
		dev_info(dev, "no rail-names property");

	err = devm_device_add_groups(dev, _pseq_attr_groups);
	if (err < 0)
		dev_err(dev, "devm_device_add_groups failed; status %d\n", err);

	if (priv->active) {
		err = cisco_register_reboot_notifier(pdev, NULL);
		if (err < 0)
			dev_err(dev, "cisco_register_reboot_notifier failed; status %d\n", err);
	}
	INIT_DELAYED_WORK(&priv->work, _probe_status);
#ifdef __x86_64__
	schedule_delayed_work(&priv->work, msecs_to_jiffies(600));
#else /* ndef __x86_64__ */
	mdelay(600);
	_probe_status(&priv->work.work);
#endif /* ndef __x86_64__ */
	return 0;
}

static int
cisco_fpga_pseq_remove(struct platform_device *pdev)
{
#ifdef __x86_64__
	struct cisco_fpga_pseq *priv = platform_get_drvdata(pdev);

	cancel_delayed_work(&priv->work);
#endif /* def __x86_64__ */
	return 0;
}

static const struct platform_device_id cisco_fpga_pseq_id_table[] = {
	{ .name = "pseq-lc",           .driver_data = 0 },
	{ .name = "pseq-zone1-lc",     .driver_data = 0 },
	{ .name = "pseq-zone2-lc",     .driver_data = 0 },
	{ .name = "pseq-zone3-lc",     .driver_data = 0 },
	{ .name = "pseq-zone3c-lc",    .driver_data = 0 },
	{ .name = "pseq-zone3cb-lc",   .driver_data = 0 },
	{ .name = "pseq-fc0-z2",       .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc1-z2",       .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc2-z2",       .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc3-z2",       .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc4-z2",       .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc5-z2",       .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc6-z2",       .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc7-z2",       .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc0-z2.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc1-z2.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc2-z2.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc3-z2.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc4-z2.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc5-z2.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc6-z2.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc7-z2.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc0-z1",       .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc1-z1",       .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc2-z1",       .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc3-z1",       .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc4-z1",       .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc5-z1",       .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc6-z1",       .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc7-z1",       .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc0-z1.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc1-z1.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc2-z1.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc3-z1.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc4-z1.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc5-z1.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc6-z1.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc7-z1.p2pm",  .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc0-z2p",      .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc1-z2p",      .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc2-z2p",      .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc3-z2p",      .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc4-z2p",      .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc5-z2p",      .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc6-z1b",      .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc7-z2p",      .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-fc0-z2p.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc1-z2p.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc2-z2p.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc3-z2p.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc4-z2p.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc5-z2p.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc6-z2p.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-fc7-z2p.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "pseq-zone1",        .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-zone2",        .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-zone3",        .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-zone3c",       .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-zone3cb",      .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq",              .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "pseq-rp.p2pm",      .driver_data = DRIVER_DATA_ACTIVE },
	{ },
};
MODULE_DEVICE_TABLE(platform, cisco_fpga_pseq_id_table);

static struct platform_driver cisco_fpga_pseq_driver = {
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= cisco_fpga_pseq_probe,
	.remove		= cisco_fpga_pseq_remove,
	.id_table	= cisco_fpga_pseq_id_table,
};

module_platform_driver(cisco_fpga_pseq_driver);

MODULE_AUTHOR("Cisco Systems, Inc. <ospo-kmod@cisco.com>");
MODULE_DESCRIPTION("Cisco FPGA Power Sequencing driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS(PLATFORM_MODULE_PREFIX DRIVER_NAME);
MODULE_VERSION(DRIVER_VERSION);
