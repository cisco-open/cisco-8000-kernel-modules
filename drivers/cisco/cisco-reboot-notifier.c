// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco utilities for reboot notifier
 *
 * Copyright (c) 2020, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/regmap.h>
#include <linux/property.h>

#include <cisco/sysfs.h>
#include <cisco/util.h>

struct notifier_sysfs_attr {
	struct device_attribute sysfs;
	u32 reg;
	u32 mask;
	u32 value;
};

struct cisco_reboot_notifier {
	struct notifier_block reboot_notifier;
	struct device *dev;
	u32 notifier_mode;

	struct notifier_sysfs_attr restart;
	struct notifier_sysfs_attr halt;
	struct notifier_sysfs_attr power_off;

	struct attribute *sys_attrs[4];
	struct attribute_group group[1];
	const struct attribute_group *groups[2];
};

static const struct reboot_info def_reboot_info = {
	.enable = 0,
	.priority = 0,

	.restart.reg   = CISCO_SYSFS_REG_NOT_PRESENT,
	.restart.mask = CISCO_SYSFS_U32_MASK,
	.restart.value = 0,

	.halt.reg   = CISCO_SYSFS_REG_NOT_PRESENT,
	.halt.mask = CISCO_SYSFS_U32_MASK,
	.halt.value = 0,

	.poweroff.reg   = CISCO_SYSFS_REG_NOT_PRESENT,
	.poweroff.mask = CISCO_SYSFS_U32_MASK,
	.poweroff.value = 0,
};

static int
_regmap_update(struct device *dev,
	       u32 reg, u32 mask, u32 value)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	int err;
	u32 before;

	if (!r)
		return -ENXIO;

	if (reg == CISCO_SYSFS_REG_NOT_PRESENT)
		return -EINVAL;

	err = regmap_read(r, reg, &before);
	if (err >= 0) {
		if (mask) {
			err = regmap_update_bits(r, reg, mask, 0);
			if (err >= 0)
				err = regmap_update_bits(r, reg, mask, value);
		} else {
			err = regmap_write(r, reg, value);
		}
	}
	if (err < 0)
		dev_err(dev, "%s: write value %#x (mask %#x) to register %#x failed; status %d\n",
			__func__, value, mask, reg, err);

	return err;
}

static int
_reboot_notifier(struct notifier_block *nb,
		 unsigned long mode,
		 void *cmd)
{
	struct cisco_reboot_notifier *priv =
		container_of(nb, struct cisco_reboot_notifier, reboot_notifier);
	const struct notifier_sysfs_attr *action = NULL;
	const char *desc = NULL;

	switch (mode) {
	case SYS_RESTART:   // alias: SYS_DOWN
		action = &priv->restart;
		desc = "user power cycle";
		break;
	case SYS_HALT:
		action = &priv->halt;
		desc = "user halt";
		break;
	case SYS_POWER_OFF:
		action = &priv->power_off;
		desc = "user power off";
		break;
	}
	if (action && (priv->notifier_mode & BIT(mode))) {
		dev_err(priv->dev, "%s\n", desc);
		(void) _regmap_update(priv->dev, action->reg,
				      action->mask, action->value);
	}

	return NOTIFY_DONE;
}

ssize_t
_sysfs_show(struct device *dev,
	    struct device_attribute *dattr,
	    char *buf)
{
	struct notifier_sysfs_attr *attr =
		container_of(dattr, struct notifier_sysfs_attr, sysfs);
	struct regmap *r = dev_get_regmap(dev, NULL);
	size_t buflen = PAGE_SIZE;
	ssize_t err = 0;
	u32 v;

	if (!r) {
		err = -ENXIO;
	} else if (attr->reg == CISCO_SYSFS_REG_NOT_PRESENT) {
		err = -EINVAL;
	} else {
		err = regmap_read(r, attr->reg, &v);
		if (err >= 0)
			err = snprintf(buf, buflen,
				       "r=%#x; m=%#x; v=%#x (%#x); cur=%#x (%#x)\n",
				       attr->reg, attr->mask, attr->value,
				       attr->mask & attr->value,
				       v, attr->mask & v);
	}
	return err;
}

ssize_t
_sysfs_store(struct device *dev,
	     struct device_attribute *dattr,
	     const char *buf,
	     size_t buflen)
{
	struct notifier_sysfs_attr *attr =
		container_of(dattr, struct notifier_sysfs_attr, sysfs);
	struct regmap *r = dev_get_regmap(dev, NULL);
	ssize_t err = -EINVAL;
	int reg, mask, val;
	int consumed;

	if (!r) {
		err = -ENXIO;
	} else if ((sscanf(buf, "r=%i; m=%i; v=%i %n",
			   &reg, &mask, &val, &consumed) == 3)
		   && (consumed == buflen)) {
		attr->reg = reg;
		attr->mask = mask;
		attr->value = val;
		err = consumed;
	}
	return err;
}

static void
_init_action(struct device *dev,
	     struct cisco_reboot_notifier *priv,
	     unsigned long mode,
	     struct notifier_sysfs_attr *attr,
	     const struct reboot_reg_info *r_reg_info,
	     const char *acpi_label,
	     const char *sysfs_label)
{
	int err;
	u32 data[3];

	err = device_property_read_u32_array(dev, acpi_label, data, ARRAY_SIZE(data));
	if (err < 0) {
		data[0] = r_reg_info->reg;
		data[1] = r_reg_info->mask;
		data[2] = r_reg_info->value;
	}

	if (data[0] != CISCO_SYSFS_REG_NOT_PRESENT)
		priv->notifier_mode |= BIT(mode);

	sysfs_attr_init(attr);
	attr->sysfs.attr.name = sysfs_label;
	attr->sysfs.attr.mode = 0644;
	attr->sysfs.show = _sysfs_show;
	attr->sysfs.store = _sysfs_store;
	attr->reg = data[0];
	attr->mask = data[1];
	attr->value = data[2];
}

int
cisco_register_reboot_notifier(struct platform_device    *pdev,
			       const struct reboot_info  *r_info)
{
	struct device *dev = &pdev->dev;
	struct regmap *r = dev_get_regmap(dev, NULL);
	struct cisco_reboot_notifier *priv;
	int err;
	u32 val;
	u32 data[3];

	if (!r)
		return -EINVAL;

	if (!r_info)
		r_info = &def_reboot_info;

	err = device_property_read_u32_array(dev, "reboot-notifier-probe", data, ARRAY_SIZE(data));
	if (err >= 0)
		(void) _regmap_update(dev, data[0], data[1], data[2]);

	err = device_property_read_u32(dev, "reboot-notifier-enable", &val);
	if (err < 0)
		val = r_info->enable;

	if (!val)
		return 0;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	_init_action(dev, priv, SYS_RESTART, &priv->restart,
		      &r_info->restart, "reboot-notifier-restart", "restart");
	_init_action(dev, priv, SYS_HALT, &priv->halt,
		     &r_info->halt, "reboot-notifier-halt", "halt");
	_init_action(dev, priv, SYS_POWER_OFF, &priv->power_off,
		     &r_info->poweroff,
		     "reboot-notifier-power-off", "power-off");

	err = device_property_read_u32(dev, "reboot-notifier-priority", &val);
	if (err < 0)
		priv->reboot_notifier.priority = r_info->priority;
	else
		priv->reboot_notifier.priority = val;

	priv->reboot_notifier.notifier_call = _reboot_notifier;
	priv->dev = dev;

	priv->sys_attrs[0] = &priv->restart.sysfs.attr;
	priv->sys_attrs[1] = &priv->halt.sysfs.attr;
	priv->sys_attrs[2] = &priv->power_off.sysfs.attr;
	priv->sys_attrs[3] = NULL;

	priv->group[0].name = "reboot_notifier";
	priv->group[0].attrs = priv->sys_attrs;

	priv->groups[0] = &priv->group[0];
	priv->groups[1] = NULL;

	err = devm_device_add_groups(dev, priv->groups);
	if (err < 0)
		dev_err(dev, "failed to create reboot-notifier sysfs groups; status %d\n", err);

	if (!priv->notifier_mode)
		return 0;

	return devm_register_reboot_notifier(dev, &priv->reboot_notifier);
}
EXPORT_SYMBOL(cisco_register_reboot_notifier);
