// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco I2C arbitration sysfs routines
 *
 * Copyright (c) 2022 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */

#include <linux/device.h>

#include "cisco/hdr.h"
#include "cisco/i2c-arbitrate.h"

struct arbitrate_attribute {
	struct device_attribute attr;
	off_t offset;
};
#define GET_ATTR_U32(hw, attr) ((u32 *)(attr->offset + (u8 *)&hw->arb))
#define GET_ATTR_U64(hw, attr) ((u64 *)(attr->offset + (u8 *)&hw->arb))

#define ATTR_U32(_name) \
	static struct arbitrate_attribute cisco_attr_##_name = { \
		.attr = __ATTR(_name, 0444, _show_u32, NULL), \
		.offset = offsetof(struct cisco_i2c_arbitrate, _name), \
	}
#define ATTR_U32_RW(_name) \
	static struct arbitrate_attribute cisco_attr_##_name = { \
		.attr = __ATTR(_name, 0644, _show_u32, _store_u32), \
		.offset = offsetof(struct cisco_i2c_arbitrate, _name), \
	}
#define ATTR_U64(_name) \
	static struct arbitrate_attribute cisco_attr_##_name = { \
		.attr = __ATTR(_name, 0444, _show_u64, NULL), \
		.offset = offsetof(struct cisco_i2c_arbitrate, _name), \
	}

static ssize_t
_show_u32(struct device *dev,
	  struct device_attribute *attr,
	  char *buf)
{
	struct cisco_fpga_i2c *hw = dev_get_drvdata(dev);
	struct arbitrate_attribute *xattr = container_of(attr, typeof(*xattr), attr);

	return snprintf(buf, PAGE_SIZE, "%u\n", *GET_ATTR_U32(hw, xattr));
}

static ssize_t
_show_u64(struct device *dev,
	  struct device_attribute *attr,
	  char *buf)
{
	struct cisco_fpga_i2c *hw = dev_get_drvdata(dev);
	struct arbitrate_attribute *xattr = container_of(attr, typeof(*xattr), attr);

	return snprintf(buf, PAGE_SIZE, "%llu\n",
			(unsigned long long)*GET_ATTR_U64(hw, xattr));
}

static ssize_t
_store_u32(struct device *dev,
	   struct device_attribute *attr,
	   const char *buf,
	   size_t buflen)
{
	struct cisco_fpga_i2c *hw = dev_get_drvdata(dev);
	struct arbitrate_attribute *xattr = container_of(attr, typeof(*xattr), attr);
	ssize_t err = -ENXIO;
	int val;
	int consumed;

	if (sscanf(buf, "%i %n", &val, &consumed) == 1 && consumed == buflen) {
		*GET_ATTR_U32(hw, xattr) = val;
		err = consumed;

		hw->arb.timeout_jiffies = msecs_to_jiffies(hw->arb.timeout_msecs);
		hw->arb.peer_grant_jiffies = msecs_to_jiffies(hw->arb.peer_grant_msecs);
		hw->arb.peer_retry_jiffies = msecs_to_jiffies(hw->arb.peer_retry_msecs);
	}
	return err;
}

ATTR_U32(peer);
ATTR_U32(local);
ATTR_U32(index);

ATTR_U32_RW(timeout_msecs);
ATTR_U32_RW(peer_grant_msecs);
ATTR_U32_RW(peer_retry_msecs);

ATTR_U64(timeout_jiffies);
ATTR_U64(peer_grant_jiffies);
ATTR_U64(peer_retry_jiffies);

ATTR_U64(disputed);
ATTR_U64(undisputed);

ATTR_U64(read_local_err);
ATTR_U64(write_local_err);

ATTR_U64(read_peer_err);
ATTR_U64(write_peer_err);

ATTR_U64(read_arb_err);
ATTR_U64(write_arb_err);

ATTR_U64(expires);
ATTR_U64(total_wait_msecs);
ATTR_U64(max_wait_msecs);
ATTR_U64(min_wait_msecs);

static ssize_t
info_show(struct device *dev,
	  struct device_attribute *attr,
	  char *buf)
{
	struct cisco_fpga_i2c *hw = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			hw->arb.info ? dev_name(hw->arb.info) : "");
}
static DEVICE_ATTR_RO(info);

static struct attribute *i2c_arbitrate_attrs[] = {
	&cisco_attr_peer.attr.attr,
	&cisco_attr_local.attr.attr,
	&cisco_attr_index.attr.attr,

	&cisco_attr_timeout_msecs.attr.attr,
	&cisco_attr_peer_grant_msecs.attr.attr,
	&cisco_attr_peer_retry_msecs.attr.attr,

	&cisco_attr_timeout_jiffies.attr.attr,
	&cisco_attr_peer_grant_jiffies.attr.attr,
	&cisco_attr_peer_retry_jiffies.attr.attr,

	&cisco_attr_disputed.attr.attr,
	&cisco_attr_undisputed.attr.attr,

	&cisco_attr_read_local_err.attr.attr,
	&cisco_attr_write_local_err.attr.attr,

	&cisco_attr_read_peer_err.attr.attr,
	&cisco_attr_write_peer_err.attr.attr,

	&cisco_attr_read_arb_err.attr.attr,
	&cisco_attr_write_arb_err.attr.attr,

	&cisco_attr_expires.attr.attr,
	&cisco_attr_total_wait_msecs.attr.attr,
	&cisco_attr_max_wait_msecs.attr.attr,
	&cisco_attr_min_wait_msecs.attr.attr,

	&dev_attr_info.attr,

	NULL,
};
struct attribute_group i2c_arbitrate_attr_group = {
	.name = "arbitration",
	.attrs = i2c_arbitrate_attrs,
};
EXPORT_SYMBOL(i2c_arbitrate_attr_group);
