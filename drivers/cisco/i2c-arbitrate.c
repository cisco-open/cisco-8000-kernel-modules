// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco I2C arbitration routines
 *
 * Copyright (c) 2022 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/minmax.h>
#include <linux/regmap.h>

#include "cisco/hdr.h"
#include "cisco/i2c-arbitrate.h"
#include "cisco/mfd.h"
#include "cisco/msd.h"
#include "cisco/reg_access.h"
#include "cisco/util.h"

/* Note msd and xil have same layout */
#define F(index) offsetof(struct msd_regs_v5_t, arbi[index])

#define MAX_DEV_SEL      16

static int
read_arb(const char *fn, struct cisco_fpga_i2c *hw)
{
	struct regmap *r = dev_get_regmap(hw->arb.info, NULL);
	u32 v;
	int e = -ENODEV;

	if (r) {
		e = regmap_read(r, F(hw->arb.index), &v);
		if (!e)
			e = v;
	}
	if (e < 0) {
		dev_err_ratelimited(
			hw->arb.info, "%s: read arbitration failed; status %d\n",
			fn, e);
		hw->arb.read_arb_err++;
	}

	return e;
}

static int
write_arb(const char *fn, struct cisco_fpga_i2c *hw)
{
	struct regmap *r = dev_get_regmap(hw->arb.info, NULL);
	int e = -ENODEV;

	if (r)
		e = regmap_write(r, F(hw->arb.index), 0);
	if (e < 0) {
		dev_err_ratelimited(
			hw->arb.info, "%s: clear arbitration failed; status %d\n",
			fn, e);
		hw->arb.write_arb_err++;
	}

	return e;
}

/**
 * obtain_arbitration - Obtain exclusive access for multi-master
 * @adapter: Target I2C bus segment
 */
static void
obtain_arbitration(struct i2c_adapter *adapter)
{
	struct device *dev = &adapter->dev;
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adapter);
	unsigned long timeout, expires, start;
	int e;
	u32 peer, arb;

	e = regmap_read(hw->regmap, hw->arb.peer, &peer);
	if (e) {
		dev_err_ratelimited(
			dev, "%s: read arbitration peer request failed; status %d\n",
			__func__, e);
		hw->arb.read_peer_err++;
		peer = 0;
	}

	arb = read_arb(__func__, hw);

	/* on error, pretend ownership taken */
	if (arb <= 0 && !peer) {
		hw->arb.undisputed++;
		return;
	}
	hw->arb.disputed++;

	/* tell peer we also want to become master */
	e = regmap_write(hw->regmap, hw->arb.local, 1);
	if (e) {
		dev_err_ratelimited(
			dev, "%s: write arbitration local request failed; status %d\n",
			__func__, e);
		hw->arb.write_local_err++;
		return;
	}

	if (!arb) {
		/* clear arbitration */
		arb = write_arb(__func__, hw);
		if (arb < 0)
			return;
	}

	start = jiffies;
	expires = start + hw->arb.timeout_jiffies;
	timeout = hw->arb.peer_grant_jiffies;
	do {
		schedule_timeout_uninterruptible(timeout);
		timeout = hw->arb.peer_retry_jiffies;

		arb = read_arb(__func__, hw);
	} while (arb && time_before_eq(jiffies, expires));

	if (arb) {
		hw->arb.expires++;
		dev_err_ratelimited(dev, "%s: arbitration expired\n",
				    __func__);
	} else {
		u64 msecs;

		msecs = jiffies_delta_to_msecs(jiffies - start);
		if (msecs) {
			hw->arb.total_wait_msecs += msecs;
			hw->arb.max_wait_msecs = max(hw->arb.max_wait_msecs,
						     msecs);
			hw->arb.min_wait_msecs = min(hw->arb.min_wait_msecs,
						     msecs);
		}
	}
}

/**
 * release_arbitration - Release exclusive access for multi-master
 * @adapter: Target I2C bus segment
 */
static void
release_arbitration(struct i2c_adapter *adapter)
{
	struct device *dev = &adapter->dev;
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adapter);
	int e;

	/* We are no longer requesting arbitration */
	e = regmap_write(hw->regmap, hw->arb.local, 0);

	/* Release arbitration */
	(void) write_arb(__func__, hw);

	if (e) {
		dev_err_ratelimited(dev,
			"%s: clear local request failed; status %d\n",
			__func__, e);
		hw->arb.write_local_err++;
	}
}

static void
arb_lock_bus(struct i2c_adapter *adapter,
	     unsigned int flags)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adapter);

	rt_mutex_lock_nested(hw->bus_lock, i2c_adapter_depth(adapter));
	obtain_arbitration(adapter);
}

static int
arb_trylock_bus(struct i2c_adapter *adapter,
		unsigned int flags)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adapter);

	if (rt_mutex_trylock(hw->bus_lock)) {
		obtain_arbitration(adapter);
		return true;
	}
	return false;
}

static void
arb_unlock_bus(struct i2c_adapter *adapter,
		unsigned int flags)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adapter);

	release_arbitration(adapter);
	rt_mutex_unlock(hw->bus_lock);
}

static const struct i2c_lock_operations arb_lock_ops = {
	.lock_bus =    arb_lock_bus,
	.trylock_bus = arb_trylock_bus,
	.unlock_bus =  arb_unlock_bus,
};

static void
noarb_lock_bus(struct i2c_adapter *adapter,
	       unsigned int flags)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adapter);

	rt_mutex_lock_nested(hw->bus_lock, i2c_adapter_depth(adapter));
}

static int
noarb_trylock_bus(struct i2c_adapter *adapter,
		  unsigned int flags)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adapter);

	return rt_mutex_trylock(hw->bus_lock);
}

static void
noarb_unlock_bus(struct i2c_adapter *adapter,
		 unsigned int flags)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adapter);

	rt_mutex_unlock(hw->bus_lock);
}

static const struct i2c_lock_operations noarb_lock_ops = {
	.lock_bus =    noarb_lock_bus,
	.trylock_bus = noarb_trylock_bus,
	.unlock_bus =  noarb_unlock_bus,
};

static int
i2c_arbitrate(struct device *dev, struct i2c_adapter *adapter)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adapter);
	u32 e, v;

	adapter->lock_ops = NULL;

	e = device_property_read_u32(dev, "multi-master", &v);
	if (e || !v)
		return 0;

	e = device_property_read_u32(dev, "arbitration-request", &v);
	if (e || !v) {
		hw->arb.local = offsetof(struct regblk_hdr_t, sw0);
		hw->arb.peer = offsetof(struct regblk_hdr_t, sw1);
	} else {
		hw->arb.local = offsetof(struct regblk_hdr_t, sw1);
		hw->arb.peer = offsetof(struct regblk_hdr_t, sw0);
	}

	e = device_property_read_u32(dev, "arbitration-index", &v);
	if (e) {
		dev_err(dev, "%s: missing arbitration-index for multi-master; status %d\n",
			__func__, e);
		return e;
	}
	hw->arb.index = v;

	e = device_property_read_u32(dev, "arbitration-timeout-msecs", &v);
	hw->arb.timeout_msecs = e ? 1000 : v;

	e = device_property_read_u32(dev, "arbitration-peer-grant-msecs", &v);
	hw->arb.peer_grant_msecs = e ? 100 : v;

	e = device_property_read_u32(dev, "arbitration-peer-retry-msecs", &v);
	hw->arb.peer_retry_msecs = e ? 10 : v;

	hw->arb.timeout_jiffies = msecs_to_jiffies(hw->arb.timeout_msecs);
	hw->arb.peer_grant_jiffies = msecs_to_jiffies(hw->arb.peer_grant_msecs);
	hw->arb.peer_retry_jiffies = msecs_to_jiffies(hw->arb.peer_retry_msecs);

	hw->arb.min_wait_msecs = hw->arb.timeout_msecs;

	{
#if defined(CONFIG_ACPI)
	const char *block = NULL;
	acpi_status status;
	acpi_handle h;

	e = device_property_read_string(dev, "arbitration-ip-block", &block);
	if (e < 0 || !block) {
		dev_err(dev, "%s: missing arbitration-ip-block; status %d\n",
			__func__, e);
		return -ENODEV;
	}
	status = acpi_get_handle(ACPI_HANDLE(dev), (acpi_string)block, &h);
	if (ACPI_FAILURE(status)) {
		dev_err(dev, "%s: failed to get acpi handle for arbitration block %s\n",
			__func__, block);
		return -ENODEV;
	}
	hw->arb.info = cisco_acpi_find_device_by_handle(h);
	if (!hw->arb.info) {
		dev_err_ratelimited(dev, "%s: cannot find arbitration block device %s\n",
				    __func__, block);
		return -EPROBE_DEFER;
	}
#else /* !defined(CONFIG_ACPI) */
	struct platform_device *platform;
	struct device_node *p;
	phandle block;

	e = device_property_read_u32(dev, "arbitration-ip-block", &block);
	if (e < 0) {
		dev_err(dev, "%s: missing arbitration-ip-block; status %d\n",
			__func__, e);
		return -ENODEV;
	}
	p = of_find_node_by_phandle(block);
	if (!p) {
		dev_err(dev, "%s: cannot find arbitration block node device %x\n",
			__func__, block);
		return -EPROBE_DEFER;
	}
	platform = of_find_device_by_node(p);
	of_node_put(p);
	if (!platform) {
		dev_err(dev, "%s: cannot find arbitration block device\n",
			__func__);
		return -EPROBE_DEFER;
	}
	hw->arb.info = &platform->dev;
#endif /* !defined(CONFIG_ACPI) */
	}

	/* Ensure the arbitration block has a regmap */
	if (!dev_get_regmap(hw->arb.info, NULL)) {
		dev_err(hw->arb.info, "%s: waiting for regmap\n",
			__func__);
		put_device(hw->arb.info);
		return -EPROBE_DEFER;
	}

	adapter->lock_ops = &arb_lock_ops;

	e = devm_add_action_or_reset(dev, (void (*)(void *))put_device,
				     hw->arb.info);
	if (e) {
		dev_err(dev,
			"arbitration block (%s) action registration failed; status %d\n",
			dev_name(hw->arb.info), e);
		return e;
	}
	dev_err(dev, "multi-master arbitration enabled\n");
	return 0;
}

int
cisco_i2c_init(struct platform_device *pdev,
	       const struct regmap_config *cfg,
	       const struct i2c_adapter *template,
	       const char *block_name)
{
	struct device *dev = &pdev->dev;
	struct cisco_fpga_i2c *hw;
	struct resource *res;
	struct i2c_adapter *adap;
	uintptr_t csr;
	int e;
	const char *nicknames[MAX_DEV_SEL];
	const char **nickname = nicknames;
	size_t adapters = 1;
	size_t hw_size = sizeof(*hw);
	u32 data;

	e = device_property_read_string_array(dev, "nicknames", nicknames, ARRAY_SIZE(nicknames));
	if (e <= 0) {
		e = device_property_read_string(dev, "nickname", &nicknames[0]);
		if (e < 0)
			nicknames[0] = NULL;
		e = 1;
	}
	adapters = e;
	hw_size += adapters * sizeof(hw->adap[0]);

	e = cisco_fpga_mfd_init(pdev, hw_size, &csr, cfg);
	if (e) {
		dev_err(dev, "cisco_fpga_mfd_init failed; status %d\n", e);
		return e;
	}

	hw = platform_get_drvdata(pdev);
	hw->num_adapters = adapters;
	hw->csr = (typeof(hw->csr))csr;
	hw->regmap = dev_get_regmap(dev, NULL);
	hw->func = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL_ALL;
	hw->bus_lock = &hw->adap[0].bus_lock;

	e = regmap_read(hw->regmap, offsetof(struct regblk_hdr_t, info0), &data);
	if (e) {
		dev_err(dev, "failed to read ip block version; status %d\n", e);
		return e;
	}
	hw->ver = REG_GET(HDR_INFO0_MAJORVER, data);

	dev_info(dev, "Cisco %s adapter version %u\n",
		 block_name, hw->ver);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	for (adap = hw->adap; adapters; ++adap, --adapters, ++nickname) {
		*adap = *template;

		adap->dev.of_node = dev->of_node;
		adap->dev.parent = dev;
		ACPI_COMPANION_SET(&adap->dev, ACPI_COMPANION(dev));

		i2c_set_adapdata(adap, hw);

		if (res) {
			snprintf(adap->name, sizeof(adap->name),
				 "%s%sCisco %s adapter at %llx",
				 *nickname ? *nickname : "",
				 *nickname ? ": " : "",
				 block_name,
				 (unsigned long long)res->start);
		} else {
			snprintf(adap->name, sizeof(adap->name),
				 "%s%sCisco %s adapter at %p",
				 *nickname ? *nickname : "",
				 *nickname ? ": " : "",
				 block_name, hw->csr);
		}

		if (adap == hw->adap) {
			e = i2c_arbitrate(dev, adap);
			if (e)
				return e;
			if (!adap->lock_ops && hw->num_adapters > 1)
				adap->lock_ops = &noarb_lock_ops;
		} else {
			adap->lock_ops = hw->adap[0].lock_ops;
		}
	}
	return 0;
}
EXPORT_SYMBOL(cisco_i2c_init);

static void
_i2c_unregister(void *data)
{
	struct i2c_adapter *adap = (typeof(adap))data;

	i2c_del_adapter(adap);
}

static const struct attribute_group *_arb_groups[] = {
	&cisco_fpga_reghdr_attr_group,
	&i2c_arbitrate_attr_group,
	NULL,
};

static const struct attribute_group *_noarb_groups[] = {
	&cisco_fpga_reghdr_attr_group,
	NULL,
};

int
cisco_i2c_register(struct platform_device *pdev,
		   int (*reset)(struct i2c_adapter *adap,
				struct cisco_fpga_i2c *hw))
{
	struct device *dev = &pdev->dev;
	struct cisco_fpga_i2c *hw;
	struct i2c_adapter *adap;
	size_t adapters;
	int e;

	hw = platform_get_drvdata(pdev);
	adapters = hw->num_adapters;

	for (adap = hw->adap; adapters; ++adap, --adapters) {
		e = i2c_add_adapter(adap);
		if (e) {
			dev_err(dev, "i2c_add_adapter(%s) failed; status %d\n",
				adap->name, e);
			return e;
		}
		e = devm_add_action_or_reset(dev, _i2c_unregister, adap);
		if (e) {
			dev_err(dev, "i2c_add_adapter(%s) action registration failed; status %d\n",
				adap->name, e);
			return e;
		}
		if (reset) {
			i2c_lock_bus(adap, I2C_LOCK_ROOT_ADAPTER);
			e = reset(adap, hw);
			i2c_unlock_bus(adap, I2C_LOCK_ROOT_ADAPTER);
			if (e) {
				dev_err(dev, "i2c reset devsel %zu failed; status %d\n",
					adap - hw->adap, e);
				return e;
			}
		}
	}

	if (hw->adap[0].lock_ops == &arb_lock_ops)
		e = devm_device_add_groups(dev, _arb_groups);
	else
		e = devm_device_add_groups(dev, _noarb_groups);
	if (e)
		dev_err(dev, "devm_device_add_groups failed; status %d\n", e);
	return e;
}
EXPORT_SYMBOL(cisco_i2c_register);
