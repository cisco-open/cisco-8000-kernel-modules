// SPDX-License-Identifier: GPL-2.0-only
/*
 * Register header block data
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include "linux/regmap.h"

#include "cisco/reg_access.h"
#include "cisco/hdr.h"
#include "cisco/sysfs.h"

static const struct reg_field_layout_t regblk_hdr_t_info0_field_layout[] = {
	REG_FIELD_LAYOUT(HDR_INFO0_OFFSET, 0),
	REG_FIELD_LAYOUT(HDR_INFO0_ID, 0),
	REG_FIELD_LAYOUT(HDR_INFO0_MAJORVER, 0),
	REG_FIELD_LAYOUT_TERMINATOR
};

static const struct reg_field_layout_t regblk_hdr_t_info1_field_layout[] = {
	REG_FIELD_LAYOUT(HDR_INFO1_CFGREGSNUM, 0),
	REG_FIELD_LAYOUT(HDR_INFO1_ARRAYSZ, 0),
	REG_FIELD_LAYOUT(HDR_INFO1_INSTNUM, 0),
	REG_FIELD_LAYOUT(HDR_INFO1_FPGANUM, 0),
	REG_FIELD_LAYOUT(HDR_INFO1_MINORVER, 0),
	REG_FIELD_LAYOUT_TERMINATOR
};

static const struct reg_field_layout_t regblk_hdr_t_sw0_field_layout[] = {
	REG_FIELD_LAYOUT(HDR_SW0_STAT, 0),
	REG_FIELD_LAYOUT_TERMINATOR
};

static const struct reg_field_layout_t regblk_hdr_t_sw1_field_layout[] = {
	REG_FIELD_LAYOUT(HDR_SW1_STAT, 0),
	REG_FIELD_LAYOUT_TERMINATOR
};

static const struct reg_field_layout_t regblk_hdr_t_magicNo_field_layout[] = {
	REG_FIELD_LAYOUT(HDR_MAGICNO_MAGICNO, 0),
	REG_FIELD_LAYOUT_TERMINATOR
};

static const struct reg_layout_t _regblk_hdr_t_layout[] = {
	REG_LAYOUT(HDR_INFO0),
	REG_LAYOUT(HDR_INFO1),
	REG_LAYOUT(HDR_SW0_STAT),
	REG_LAYOUT(HDR_SW1_STAT),
	REG_LAYOUT(HDR_MAGICNO),
	REG_LAYOUT_TERMINATOR
};

const struct reg_layout_t *regblk_hdr_t_layout = _regblk_hdr_t_layout;

#define R(field) offsetof(struct regblk_hdr_t, field)

/*
 * sysfs file block_id
 */
static ssize_t
_block_id_fmt(const struct sysfs_ext_attribute *attr,
	      char *buf, ssize_t buflen,
	      const u32 *data, size_t data_dim)
{
	u32 info0 = data[0];

	return scnprintf(buf, buflen, "%u\n", REG_GET(HDR_INFO0_ID, info0));
}
CISCO_ATTR_RO(block_id, R(info0));

/*
 * sysfs file version
 */
static ssize_t
_version_fmt(const struct sysfs_ext_attribute *attr,
	     char *buf, ssize_t buflen,
	     const u32 *data, size_t data_dim)
{
	u32 info0, info1;

	BUG_ON(data_dim < 2);

	info0 = data[0];
	info1 = data[1];
	return scnprintf(buf, buflen, "%u.%u\n",
			 REG_GET(HDR_INFO0_MAJORVER, info0),
			 REG_GET(HDR_INFO1_MINORVER, info1));
}
CISCO_ATTR_RO2(version, R(info0), R(info1));

static int
_regmap_read_u64(struct regmap *r, u32 reg, u64 *dst)
{
	u32 sw0, sw1;
	int err = regmap_read(r, reg, &sw0);

	if (!err) {
		err = regmap_read(r, reg + 4, &sw1);
		if (!err)
			*dst = ((u64)sw0 << 32ull) | sw1;
	}
	return err;
}

static int
_regmap_write_u64(struct regmap *r, u32 reg, u64 src)
{
	u32 sw0 = (src >> 32ull);
	u32 sw1 = src; /* only low bits */
	int err = regmap_write(r, reg, sw0);

	if (!err)
		err = regmap_write(r, reg + 4, sw1);
	return err;
}

static ssize_t
scratch_show(struct device *dev,
	     struct device_attribute *attr,
	     char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	u64 sw;
	ssize_t err;
	size_t len = PAGE_SIZE;

	/*
	 * Support a child that requires parent's regmap (e.g. watchdog_device)
	 */
	if (!r && dev->parent)
		r = dev_get_regmap(dev->parent, NULL);
	if (r) {
		err = _regmap_read_u64(r, R(sw0), &sw);
		if (!err)
			err = scnprintf(buf, len, "%#llx\n",
					(unsigned long long) sw);
	} else {
		err = -ENXIO;
	}
	return err;
}

static ssize_t
scratch_store(struct device *dev,
	      struct device_attribute *attr,
	      const char *buf,
	      size_t buflen)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	ssize_t err = 0;
	int consumed = -1;

	/*
	 * Support a child that requires parent's regmap (e.g. watchdog_device)
	 */
	if (!r && dev->parent)
		r = dev_get_regmap(dev->parent, NULL);
	if (r) {
		long long and_val, or_val, xor_val, mask;
		int bit;

		if ((sscanf(buf, "set-bit %i %n", &bit, &consumed) == 1)
		    && (bit >= 0) && (bit < 64)) {
			xor_val = 0;
			and_val = ~xor_val;
			or_val = (1ull << bit);
		} else if ((sscanf(buf, "clear-bit %i %n", &bit, &consumed) == 1)
			   && (bit >= 0) && (bit < 64)) {
			or_val = xor_val = 0;
			and_val = ~xor_val & ~(1ull << bit);
		} else if (sscanf(buf, "%lli mask %lli %n", &or_val, &mask, &consumed) == 2) {
			xor_val = 0;
			and_val = ~mask;
		} else if (sscanf(buf, "and %lli %n", &and_val, &consumed) == 1) {
			or_val = xor_val = 0;
		} else if (sscanf(buf, "andn %lli %n", &and_val, &consumed) == 1) {
			or_val = xor_val = 0;
			and_val = ~and_val;
		} else if (sscanf(buf, "or %lli %n", &or_val, &consumed) == 1) {
			xor_val = 0;
			and_val = ~xor_val;
		} else if (sscanf(buf, "xor %lli %n", &xor_val, &consumed) == 1) {
			or_val = 0;
			and_val = ~or_val;
		} else if (sscanf(buf, "%lli %n", &or_val, &consumed) == 1) {
			and_val = xor_val = 0;
		}
		if (consumed != buflen) {
			err = -EINVAL;
		} else {
			u64 data;

			err = _regmap_read_u64(r, R(sw0), &data);
			if (!err) {
				data = ((data & and_val) ^ xor_val) | or_val;
				err = _regmap_write_u64(r, R(sw0), data);
				if (!err)
					err = consumed;
			}
		}
	} else {
		err = -ENXIO;
	}
	return err;
}
static DEVICE_ATTR_RW(scratch);

static struct attribute *_hdr_sys_attrs[] = {
	&cisco_attr_block_id.attr.attr,
	&cisco_attr_version.attr.attr,
	&dev_attr_scratch.attr,
	NULL,
};
const struct attribute_group cisco_fpga_reghdr_attr_group = {
	.name = "info",
	.attrs = _hdr_sys_attrs,
};
EXPORT_SYMBOL(cisco_fpga_reghdr_attr_group);
