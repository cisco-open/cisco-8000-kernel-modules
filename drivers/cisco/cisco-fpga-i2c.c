// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco FPGA I2C driver.
 *
 * Copyright (c) 2018, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/io.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>

#include "cisco/i2c-arbitrate.h"
#include "cisco/mfd.h"
#include "cisco/util.h"

#define DRIVER_NAME     "cisco-fpga-i2c"
#define DRIVER_VERSION  "1.0"

#define CISCO_FPGA_I2C_INFO0    0x0000
#define CISCO_FPGA_I2C_TXBUF    0x0020
#define CISCO_FPGA_I2C_RXBUF    0x0024
#define CISCO_FPGA_I2C_CSR      0x0028
#define CISCO_FPGA_I2C_ISTAT    0x0030
#define CISCO_FPGA_I2C_EXT0     0x0050
#define CISCO_FPGA_I2C_EXT1     0x0054
#define CISCO_FPGA_I2C_DEV_CTRL 0x0058

#define CISCO_FPG_I2C_MAX_REG_v4 0x0058
#define CISCO_FPG_I2C_MAX_REG_v5 0x0064

#define CISCO_FPGA_HDR_GET_VER(x) (x & 0x1f)

#define HW_SUPPORTS_DEV_SEL(hw)	((hw)->ver > 4)

static inline int
_i2c_writel(struct cisco_fpga_i2c *hw, uint32_t val, uint addr)
{
	return regmap_write(hw->regmap, addr, val);
}

static inline int
_i2c_readl(struct cisco_fpga_i2c *hw, uint addr, u32 *val)
{
	return regmap_read(hw->regmap, addr, val);
}

static u32
cisco_fpga_i2c_func(struct i2c_adapter *adap)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adap);

	return hw->func;
}

static int
cisco_fpga_i2c_reset(struct i2c_adapter *adap, struct cisco_fpga_i2c *hw)
{
	struct device *dev = &adap->dev;
	int e;

	e = _i2c_writel(hw, BIT(25), CISCO_FPGA_I2C_CSR);
	if (e) {
		dev_err(dev, "i2c_reset csr write error %d", e);
		return e;
	}
	udelay(100);
	e = _i2c_writel(hw, 0, CISCO_FPGA_I2C_CSR);
	if (e) {
		dev_err(dev, "i2c_reset csr to 0 write error %d", e);
		return e;
	}
	e = _i2c_writel(hw, 0, CISCO_FPGA_I2C_EXT0);
	if (e)
		dev_err(dev, "i2c_reset ext0 write error %d", e);
	return e;
}

static int
cisco_fpga_i2c_wait_done(struct i2c_adapter *adap, struct cisco_fpga_i2c *hw)
{
	unsigned long timeout = jiffies + adap->timeout;
	u32 val;
	int e;

	do {
		schedule_timeout_uninterruptible(usecs_to_jiffies(100));
		e = _i2c_readl(hw, CISCO_FPGA_I2C_CSR, &val);
		if (e) {
			return e;
		} else if (val & BIT(14)) {
			continue;
		} else if (val & GENMASK(31, 30)) {
			return -EFAULT;
		} else if (val & BIT(29)) {
			i2c_recover_bus(adap);
			return -EBUSY;
		} else {
			return 0;
		}
	} while (time_before_eq(jiffies, timeout));

	return -ETIMEDOUT;
}

static u16
_msglen(const struct i2c_msg *msg, int num)
{
	if (num > 1) {
		if (msg[1].flags & I2C_M_RECV_LEN)
			return msg[1].len + I2C_SMBUS_BLOCK_MAX;
		return msg[1].len;
	}
	return msg[0].len;
}

static int
cisco_fpga_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msg, int num)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adap);
	u32 val;
	int i, err;
	u16 len = _msglen(msg, num);
	u8 *buf = num > 1 ? msg[1].buf : msg[0].buf;
	u8 presz = num > 1 ? msg[0].len : 0;
	u8 cmdsz = len;
	bool read = num > 1 || msg[0].flags & I2C_M_RD;
	bool use_ext_reg = len >= I2C_SMBUS_BLOCK_MAX;
	bool pseudo_ten_bit_sup = hw->func & I2C_FUNC_10BIT_ADDR;
	bool ten_bit_msg = msg[0].flags & I2C_FUNC_10BIT_ADDR;
	int e;

	if (!pseudo_ten_bit_sup && ten_bit_msg) {
		dev_err(&adap->dev, "10 bit addr not supported");
		return -EINVAL;
	}

	if (len > 511) {
		dev_err(&adap->dev, "length %d is larger than 511", len);
		return -EINVAL;
	}

	if (presz > 31) {
		dev_err(&adap->dev, "presz %d is larger than 31", presz);
		return -EINVAL;
	}

	/* clear interrupts */
	val = BIT(31) | BIT(30) | BIT(29) | BIT(27);
	e = _i2c_writel(hw, val, CISCO_FPGA_I2C_ISTAT);
	if (e)
		return e;

	if (use_ext_reg) {
		/* bit 31 is enable for the EXT regs */
		val = BIT(31) | len;
		e = _i2c_writel(hw, val, CISCO_FPGA_I2C_EXT0);
	} else {
		val = 0;
		e = _i2c_writel(hw, val, CISCO_FPGA_I2C_EXT0);
	}
	if (e)
		return e;

	if (!(msg[0].flags & I2C_M_RD))
		for (i = 0; i < msg[0].len; i++) {
			val = BIT(13) | msg[0].buf[i];
			if (use_ext_reg) {
				e = _i2c_writel(hw, i << 16, CISCO_FPGA_I2C_EXT1);
				if (e)
					return e;
			} else {
				val |= i << 8;
			}
			e = _i2c_writel(hw, val, CISCO_FPGA_I2C_TXBUF);
			if (e)
				return e;
		}

	if (HW_SUPPORTS_DEV_SEL(hw)) {
		if (pseudo_ten_bit_sup)
			val = (msg[0].addr >> 7) & 0x7;
		else
			val = adap - hw->adap;
		e = _i2c_writel(hw, val, CISCO_FPGA_I2C_DEV_CTRL);
		if (e)
			return e;
	}

	e = _i2c_readl(hw, CISCO_FPGA_I2C_CSR, &val);
	if (e)
		return e;
	val &= BIT(31) | BIT(30) | BIT(29) | BIT(27);
	val |= use_ext_reg ? 0 : cmdsz;
	val |= (msg[0].addr & 0x7f) << 5;
	val |= read ? BIT(12) : 0;
	val |= BIT(13);
	val |= presz && read ? BIT(16) : 0;
	val |= presz << 20;
	e = _i2c_writel(hw, val, CISCO_FPGA_I2C_CSR);
	if (e)
		return e;

	err = cisco_fpga_i2c_wait_done(adap, hw);
	if (err) {
		num = err;
	} else if (read) {
		for (i = 0; i < len; i++) {
			if (use_ext_reg)
				e = _i2c_writel(hw, i, CISCO_FPGA_I2C_EXT1);
			else
				e = _i2c_writel(hw, i<<8, CISCO_FPGA_I2C_RXBUF);
			if (e)
				return e;
			udelay(100);
			e = _i2c_readl(hw, CISCO_FPGA_I2C_RXBUF, &val);
			if (e)
				return e;
			buf[i] = val & 0xff;
		}
	}

	if (use_ext_reg) {
		e = _i2c_writel(hw, 0, CISCO_FPGA_I2C_EXT0);
		if (e)
			return e;
	}

	if (num > 1 && (msg[1].flags & I2C_M_RECV_LEN)
		    && (msg[1].len + buf[0]) <= len) {
		msg[1].len += buf[0];
	}

	return num;
}

static int
cisco_fpga_i2c_recover_bus(struct i2c_adapter *adap)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adap);
	unsigned long timeout = jiffies + adap->timeout;
	u32 val;
	int e;

	e = _i2c_readl(hw, CISCO_FPGA_I2C_CSR, &val);
	if (e)
		return e;

	val |= BIT(26);
	e = _i2c_writel(hw, val, CISCO_FPGA_I2C_CSR);
	if (e)
		return e;

	do {
		e = _i2c_readl(hw, CISCO_FPGA_I2C_CSR, &val);
		if (e)
			return e;
		if (val & BIT(14))
			schedule_timeout_uninterruptible(usecs_to_jiffies(160));
		else
			return 0;
	} while (time_before_eq(jiffies, timeout));

	return -EBUSY;
}

static const struct i2c_algorithm cisco_fpga_i2c_algo = {
	.master_xfer    = cisco_fpga_i2c_xfer,
	.functionality  = cisco_fpga_i2c_func,
};

static struct i2c_bus_recovery_info cisco_fpga_i2c_recovery = {
	.recover_bus    = cisco_fpga_i2c_recover_bus,
};

static const struct i2c_adapter_quirks _i2c_quirks = {
	.flags = I2C_AQ_COMB_WRITE_THEN_READ,
	.max_num_msgs = 2,
	.max_write_len = 511,
	.max_read_len = 511,
	.max_comb_1st_msg_len = 31,
	.max_comb_2nd_msg_len = 511,
};

static int
cisco_fpga_i2c_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cisco_fpga_i2c *hw;
	int e;
	static const struct regmap_config r_config = {
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.fast_io = false,
		.max_register = CISCO_FPG_I2C_MAX_REG_v4 - 1,
	};
	const struct i2c_adapter i2c_adapter_template = {
		.owner = THIS_MODULE,
		.algo = &cisco_fpga_i2c_algo,
		.retries = 3,
		.timeout = msecs_to_jiffies(350),
		.bus_recovery_info = &cisco_fpga_i2c_recovery,
		.quirks = &_i2c_quirks,
	};
	struct gpio_desc *reset_gpio;

	reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(reset_gpio)) {
		// return EPROBE_DEFER to retry probe later if GPIO is not ready.
		// Note: Make sure to return DEFER before subsequent init actions
		return PTR_ERR(reset_gpio);
	}

	e = cisco_i2c_init(pdev, &r_config, &i2c_adapter_template, "I2C");
	if (e) {
		dev_err(dev, "cisco_i2c_init failed; status %d\n", e);
		return e;
	}
	hw = platform_get_drvdata(pdev);

	if (HW_SUPPORTS_DEV_SEL(hw) && hw->num_adapters == 1) {
		hw->func |= I2C_FUNC_10BIT_ADDR;
		cisco_regmap_set_max_register(dev, CISCO_FPG_I2C_MAX_REG_v5 - 1);
	}

	/* unreset/reset if a reset GPIO is specified. */
	if (reset_gpio) {
		gpiod_set_value(reset_gpio, 1);
		udelay(1);
		gpiod_set_value(reset_gpio, 0);
		udelay(1);
	}

	return cisco_i2c_register(pdev, cisco_fpga_i2c_reset);
}

static const struct platform_device_id cisco_fpga_i2c_id_table[] = {
	{ .name = "i2c-smb-rp",		.driver_data = 1 },
	{ .name = "i2c-smb-lc",		.driver_data = 0 },
	{ .name = "i2c-smb-fc0",	.driver_data = 1 },
	{ .name = "i2c-smb-fc1",	.driver_data = 1 },
	{ .name = "i2c-smb-fc2",	.driver_data = 1 },
	{ .name = "i2c-smb-fc3",	.driver_data = 1 },
	{ .name = "i2c-smb-fc4",	.driver_data = 1 },
	{ .name = "i2c-smb-fc5",	.driver_data = 1 },
	{ .name = "i2c-smb-fc6",	.driver_data = 1 },
	{ .name = "i2c-smb-fc7",	.driver_data = 1 },
	{ .name = "i2c-smb-ft",		.driver_data = 1 },
	{ .name = "i2c-smb",		.driver_data = 1 },
	{ .name = "i2c-smb-pim1",	.driver_data = 1 },
	{ .name = "i2c-smb-pim2",	.driver_data = 1 },
	{ .name = "i2c-smb-pim3",	.driver_data = 1 },
	{ .name = "i2c-smb-pim4",	.driver_data = 1 },
	{ .name = "i2c-smb-pim5",	.driver_data = 1 },
	{ .name = "i2c-smb-pim6",	.driver_data = 1 },
	{ .name = "i2c-smb-pim7",	.driver_data = 1 },
	{ .name = "i2c-smb-pim8",	.driver_data = 1 },
	{ },
};
MODULE_DEVICE_TABLE(platform, cisco_fpga_i2c_id_table);

static struct platform_driver cisco_fpga_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= cisco_fpga_i2c_probe,
	.id_table	= cisco_fpga_i2c_id_table,
};

module_platform_driver(cisco_fpga_i2c_driver);

MODULE_AUTHOR("Cisco Systems, Inc. <ospo-kmod@cisco.com>");
MODULE_DESCRIPTION("Cisco FPGA I2C Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS(PLATFORM_MODULE_PREFIX DRIVER_NAME);
MODULE_SOFTDEP("pre: cisco-fpga-xil cisco-fpga-msd");
MODULE_VERSION(DRIVER_VERSION);
