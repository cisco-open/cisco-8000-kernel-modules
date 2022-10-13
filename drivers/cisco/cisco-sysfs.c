// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco sysfs utilities
 *
 * Copyright (c) 2020, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/ctype.h>

#include "cisco/reg_access.h"
#include "cisco/sysfs.h"

static int
_regmap_read(struct regmap *r,
	     const u32 *regp, size_t reg_dim,
	     u32 *datap, size_t data_dim)
{
	const u32 *reg_limitp;
	int err = 0;
	u32 reg;

	if ((reg_dim != data_dim) || (reg_dim < 1))
		return -EINVAL;

	reg_limitp = regp + reg_dim;
	for (; !err && regp != reg_limitp;) {
		reg = *regp++;
		if (reg != CISCO_SYSFS_REG_NOT_PRESENT)
			err = regmap_read(r, reg, datap);
		++datap;
	}
	return err;
}

static int
_regmap_write(struct regmap *r,
	      const u32 *regp, size_t reg_dim,
	      const u32 *maskp, size_t mask_dim,
	      const u32 *datap, size_t data_dim)
{
	const u32 *reg_limitp;
	int err = 0;
	u32 reg;

	if ((reg_dim != data_dim) || (reg_dim < 1) ||
	    (reg_dim != mask_dim))
		return -EINVAL;

	reg_limitp = regp + reg_dim;
	for (; !err && regp != reg_limitp;) {
		reg = *regp++;
		if (reg != CISCO_SYSFS_REG_NOT_PRESENT)
			err = regmap_update_bits(r, reg, *maskp, *datap);
		++datap;
		++maskp;
	}
	return err;
}

/*
 * Generic utility function to output using a simple format string
 */
static ssize_t
_sysfs_fmt_raw(const struct sysfs_ext_attribute *attr,
	       char *buf, ssize_t buflen,
	       const u32 *data, size_t data_dim)
{
	char *bufp = buf;
	size_t index;
	ssize_t err = 0;
	ssize_t wrote;
	const char *fmt;
	u64 data64, mask;
	u32 data32;

	for (index = 0; index < data_dim; ++index) {
		if (attr->reg[index] == CISCO_SYSFS_REG_NOT_PRESENT)
			continue;
		if (attr->flags & CISCO_SYSFS_ATTR_F_64) {
			BUG_ON((index + 1) >= data_dim);

			data64 = (u64)data[index] << 32;
			data64 |= data[++index];

			if (attr->flags & CISCO_SYSFS_ATTR_F_MASKED) {
				mask = (u64)attr->mask[index - 1] << 32;
				mask |= attr->mask[index];
				data64 &= mask;
			}

			if (attr->flags & CISCO_SYSFS_ATTR_F_HEX)
				fmt = "%#llx\n";
			else
				fmt = "%llu\n";

			wrote = snprintf(bufp, buflen, fmt, (unsigned long long)data64);
		} else {
			data32 = data[index];
			if (attr->flags & CISCO_SYSFS_ATTR_F_HEX)
				fmt = "%#x\n";
			else
				fmt = "%u\n";

			if (attr->flags & CISCO_SYSFS_ATTR_F_MASKED)
				data32 &= attr->mask[index];

			wrote = snprintf(bufp, buflen, fmt, data32);
		}
		if (wrote >= 0) {
			bufp += wrote;
			buflen -= wrote;
		} else {
			err = wrote;
			break;
		}
	}
	if (!err)
		err = bufp - buf;
	return err;
}

/*
 * Generic utility function to scan a number
 */
static ssize_t
_sysfs_parse_raw(const struct sysfs_ext_attribute *attr,
		 const char *buf, ssize_t buflen,
		 u32 *data, size_t data_dim)
{
	int consumed;
	int val;
	ssize_t err = 0;

	if ((attr->reg[0] == CISCO_SYSFS_REG_NOT_PRESENT) || !data_dim || !data) {
		err = -EINVAL;
	} else if (sscanf(buf, "%i %n", &val, &consumed) == 1) {
		data[0] = val;
		err = consumed;
	} else {
		err = -EINVAL;
	}
	return err;
}

/*
 * Generic utility function to scan an input buffer
 * and match it to one or more named values in a
 * table.  The data parsed is stored in data[0], and
 * the associated field mask is stored in data[1]
 */
static ssize_t
_sysfs_parse_table(const struct sysfs_ext_attribute *attr,
		   const char *buf, ssize_t buflen,
		   u32 *data, size_t data_dim)
{
	size_t table_dim = attr->store_table_dim;
	const struct sysfs_ext_attribute_bit_store_table *tablep = attr->store_table;
	const struct sysfs_ext_attribute_bit_store_table *limitp;
	u32 mask = 0;
	u32 value = 0;
	ssize_t consumed = 0;

	if (!tablep || !table_dim || data_dim < 2)
		return -EINVAL;

	limitp = tablep + table_dim;
	for (; buflen;) {
		const struct sysfs_ext_attribute_bit_store_table *table = tablep;

		while ((buflen > 0) && isspace(*buf)) {
			--buflen;
			++consumed;
			++buf;
		}
		if (!buflen)
			break;

		for (; table < limitp; ++table) {
			if ((buflen < table->match_len) ||
				strncmp(buf, table->match, table->match_len)) {
				continue;
			}
			if ((buflen == table->match_len) ||
			    isspace(buf[table->match_len])) {
				mask |= table->mask;
				value = (value & ~table->mask) | table->value;
				break;
			}
		}
		if (table >= limitp) {
			consumed = -EINVAL;
			break;
		}
		buflen -= table->match_len;
		consumed += table->match_len;
		buf += table->match_len;
	}

	data[0] = value;
	data[1] = mask;
	return consumed;
}

ssize_t
cisco_fpga_sysfs_show(struct device *dev,
		      struct device_attribute *dattr,
		      char *buf)
{
	struct sysfs_ext_attribute *attr = (typeof(attr))dattr;
	struct regmap *r = dev_get_regmap(dev, NULL);
	u32 data[ARRAY_SIZE(attr->reg)] = { 0 };
	size_t buflen = PAGE_SIZE;
	ssize_t err = 0;

	/*
	 * Support a child that requires parent's regmap (e.g. watchdog_device)
	 */
	if (!r && dev->parent)
		r = dev_get_regmap(dev->parent, NULL);

	if (!r) {
		err = -ENXIO;
	} else {
		err = _regmap_read(r, attr->reg, ARRAY_SIZE(attr->reg),
				      data, ARRAY_SIZE(data));
		if (!err) {
			typeof(attr->fmt_fn) fmt = attr->fmt_fn;

			if (!fmt)
				fmt = _sysfs_fmt_raw;
			err = fmt(attr, buf, buflen, data, ARRAY_SIZE(data));
		}
	}
	return err;
}
EXPORT_SYMBOL(cisco_fpga_sysfs_show);

ssize_t
cisco_fpga_sysfs_store(struct device *dev,
		       struct device_attribute *dattr,
		       const char *buf,
		       size_t buflen)
{
	struct sysfs_ext_attribute *attr = (typeof(attr))dattr;
	struct regmap *r = dev_get_regmap(dev, NULL);
	u32 data[ARRAY_SIZE(attr->reg)] = { 0 };
	ssize_t err = -EINVAL;
	ssize_t consumed;

	/*
	 * Support a child that requires parent's regmap (e.g. watchdog_device)
	 */
	if (!r && dev->parent)
		r = dev_get_regmap(dev->parent, NULL);

	if (!r) {
		err = -ENXIO;
	} else {
		typeof(attr->parse_fn) parse = attr->parse_fn;

		if (!parse)
			parse = _sysfs_parse_raw;

		consumed = parse(attr, buf, buflen, data, ARRAY_SIZE(data));
		if (consumed < 0) {
			err = consumed;
		} else if (consumed == buflen) {
			err = _regmap_write(r,
					    attr->reg, ARRAY_SIZE(attr->reg),
					    attr->mask, ARRAY_SIZE(attr->mask),
					    data, ARRAY_SIZE(data));
			if (!err)
				err = consumed;
		}
	}
	return err;
}
EXPORT_SYMBOL(cisco_fpga_sysfs_store);

ssize_t
cisco_fpga_sysfs_store_table(struct device *dev,
			     struct device_attribute *dattr,
			     const char *buf,
			     size_t buflen)
{
	struct sysfs_ext_attribute *attr = (typeof(attr))dattr;
	struct regmap *r = dev_get_regmap(dev, NULL);
	u32 reg = attr->reg[0];
	u32 data[ARRAY_SIZE(attr->reg)] = { 0 };
	ssize_t err = 0;
	ssize_t consumed;

	/*
	 * Support a child that requires parent's regmap (e.g. watchdog_device)
	 */
	if (!r && dev->parent)
		r = dev_get_regmap(dev->parent, NULL);

	if (!r) {
		err = -ENXIO;
	} else if (reg == CISCO_SYSFS_REG_NOT_PRESENT) {
		err = -EINVAL;
	} else {
		typeof(attr->parse_fn) parse = attr->parse_fn;

		if (!parse)
			parse = _sysfs_parse_table;

		consumed = parse(attr, buf, buflen, data, ARRAY_SIZE(data));
		if (consumed < 0) {
			err = consumed;
		} else if (consumed != buflen) {
			err = -EINVAL;
		} else {
			err = regmap_write(r, reg, data[0]);
			if (!err)
				err = consumed;
		}
	}
	return err;
}
EXPORT_SYMBOL(cisco_fpga_sysfs_store_table);
