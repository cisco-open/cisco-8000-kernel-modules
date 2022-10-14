// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco BMC FPGA I2C driver.
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/regmap.h>

#include "cisco/fpga.h"
#include "cisco/mfd.h"

#define DRIVER_NAME	"cisco-fpga-bmc"
#define DRIVER_VERSION	"1.0"

static int m_mfd_debug;
module_param(m_mfd_debug, int, 0444);
MODULE_PARM_DESC(m_mfd_debug, "MFD debug level. 0=none");

/*
 * Passed to regmap callbacks in child
 */
struct bmc_regmap {
	struct i2c_client *i2c;
	u32 base;
};

/*
 * Parent structure
 */
struct bmc_mfd {
	struct cisco_fpga_mfd mfd;
	struct bmc_regmap r;
};

static int
_bmc_read(void *context, unsigned int reg, unsigned int *val)
{
	struct bmc_regmap *bmc = (typeof(bmc))context;
	struct i2c_client *i2c = bmc->i2c;
	u8 buf[4];
	struct i2c_msg msg = {
		.addr	= i2c->addr + 1,
		.flags	= 0,
		.len	= 4,
		.buf	= buf,
	};
	int ret, err = 0;

	reg += bmc->base;

	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	buf[2] = (reg >> 16) & 0xff;
	buf[3] = (reg >> 24) & 0xff;

	i2c_lock_bus(i2c->adapter, I2C_LOCK_ROOT_ADAPTER);
	ret = __i2c_transfer(i2c->adapter, &msg, 1);
	if (ret < 1) {
		err = -EIO;
	} else {
		msg.flags = I2C_M_RD;
		msg.addr = i2c->addr + 5;
		ret = __i2c_transfer(i2c->adapter, &msg, 1);
		if (ret < 1)
			err = -EIO;
		else
			*val = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
	}
	i2c_unlock_bus(i2c->adapter, I2C_LOCK_ROOT_ADAPTER);
	return err;
}

static int
_bmc_write(void *context, unsigned int reg, unsigned int val)
{
	struct bmc_regmap *bmc = (typeof(bmc))context;
	struct i2c_client *i2c = bmc->i2c;
	u8 buf[4];
	struct i2c_msg msg = {
		.addr	= i2c->addr + 1,
		.flags	= 0,
		.len	= 4,
		.buf	= buf,
	};
	int ret, err = 0;

	reg += bmc->base;
	buf[0] = reg & 0xff;
	buf[1] = (reg >> 8) & 0xff;
	buf[2] = (reg >> 16) & 0xff;
	buf[3] = (reg >> 24) & 0xff;

	i2c_lock_bus(i2c->adapter, I2C_LOCK_ROOT_ADAPTER);
	ret = __i2c_transfer(i2c->adapter, &msg, 1);
	if (ret < 1) {
		err = -EIO;
	} else {
		buf[0] = val & 0xff;
		buf[1] = (val >> 8) & 0xff;
		buf[2] = (val >> 16) & 0xff;
		buf[3] = (val >> 24) & 0xff;
		msg.addr = i2c->addr + 5;
		ret = __i2c_transfer(i2c->adapter, &msg, 1);
		if (ret < 1)
			err = -EIO;
	}
	i2c_unlock_bus(i2c->adapter, I2C_LOCK_ROOT_ADAPTER);
	return err;
}

static const struct regmap_config _bmc_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.reg_write = _bmc_write,
	.reg_read = _bmc_read,
	.fast_io = false,
};

static int
_bmc_regmap(struct platform_device *pdev, size_t priv_size, uintptr_t *base,
	    const struct regmap_config *r_configp)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct bmc_regmap *priv;
	struct bmc_mfd *mfd = dev_get_drvdata(dev->parent);
	struct regmap *r;
	struct regmap_config regmap_config =
	    r_configp ? *r_configp : _bmc_regmap_config;

	regmap_config.reg_write = _bmc_write;
	regmap_config.reg_read = _bmc_read;

	if (!mfd)
		return -ENXIO;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENXIO;

	priv = devm_kzalloc(dev, sizeof(*priv) + priv_size, GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	if (priv_size)
		platform_set_drvdata(pdev, &priv[1]);
	else
		platform_set_drvdata(pdev, NULL);

	priv->base = res->start;
	priv->i2c = mfd->r.i2c;

	// ACPI_COMPANION_SET(&hw->adap.dev, ACPI_COMPANION(dev));
	r = devm_regmap_init(dev, NULL, priv, &regmap_config);
	if (IS_ERR(r))
		return PTR_ERR(r);

	if (base)
		*base = priv->base;

	return 0;
}

static struct cell_metadata *
_bmc_probe_regmap(struct i2c_client *client, struct bmc_regmap *priv)
{
	struct device *dev = &client->dev;
	struct regmap *r;
	struct cell_metadata *meta;
	struct resource template = {
		.flags = IORESOURCE_MEM,
		.name = "cell",
	};

	r = devm_regmap_init(dev, NULL, priv, &_bmc_regmap_config);
	if (IS_ERR(r)) {
		meta = (typeof(meta))r;
	} else {
		meta = cisco_fpga_mfd_cells(dev, r, &template,
					    NULL, 0,
					    CISCO_MFD_CELLS_FILTER_REGMAP,
					    m_mfd_debug);
		regmap_exit(r);
	}
	return meta;
}

static int cisco_fpga_bmc_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct cell_metadata *meta;
	int err;
	struct bmc_mfd *priv;

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->r.i2c = client;
	priv->r.base = 0;
	i2c_set_clientdata(client, priv);
	cisco_fpga_mfd_parent_init(&client->dev, &priv->mfd, _bmc_regmap);

	meta = _bmc_probe_regmap(client, &priv->r);
	if (IS_ERR(meta))
		return PTR_ERR(meta);

	err = devm_mfd_add_devices(&client->dev, 0, meta->cells, meta->ncells,
				   NULL, 0, 0 /*hw->pdata.domain */);
	kfree(meta);
	return err;
}

static const struct i2c_device_id cisco_fpga_bmc_id[] = {
	{DRIVER_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cisco_fpga_bmc_id);

static struct i2c_driver cisco_fpga_bmc_driver = {
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= cisco_fpga_bmc_probe,
	.id_table	= cisco_fpga_bmc_id,
};

module_i2c_driver(cisco_fpga_bmc_driver);

MODULE_AUTHOR("Cisco Systems, Inc. <ospo-kmod@cisco.com>");
MODULE_DESCRIPTION("Cisco BMC FPGA Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRIVER_VERSION);
