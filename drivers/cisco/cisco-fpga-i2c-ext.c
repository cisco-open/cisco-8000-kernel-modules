// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco FPGA I2C EXT driver.
 *
 * Copyright (c) 2018-2022 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/slab.h>
#include <linux/regmap.h>

#include "cisco/mfd.h"
#include "cisco/reg_access.h"
#include "cisco/i2c-arbitrate.h"
#include "cisco/i2c-ext.h"

#define DRIVER_NAME                 "cisco-fpga-i2c-ext"
#define DRIVER_VERSION              "1.0"
#define DRIVER_I2C_DEBUG_LEVEL      0
#define DRIVER_I2C_HW_BUF_SIZE      256
#define DRIVER_I2C_HW_BUF_SIZE_v5   512

#define F(hw, addr) (((u8 *)addr) - ((u8 *)hw->csr))

#define DEBUG_RECORD_STATUS     0x0001

long m_debug;
module_param(m_debug, long, 0644);
MODULE_PARM_DESC(m_debug, "Debug level. 0=none");

long m_error_trace;
enum {
	M_ERROR_TRACE_FAULT,
	M_ERROR_TRACE_BUSY,
	M_ERROR_TRACE_TIMEOUT,
	M_ERROR_TRACE_OTHER,
};
module_param(m_error_trace, long, 0644);
MODULE_PARM_DESC(m_error_trace, "Generate register trace on error. bit: 0=fault; 1=busy; 2=timeout; 3=other");

#define DEFAULT_SPEED   i2c_ext_cfg_spdCnt__100Kbps

static inline int
_i2c_writel(struct cisco_fpga_i2c *hw,
	    uint32_t v, void __iomem *addr)
{
	return regmap_write(hw->regmap, F(hw, addr), v);
}

static inline int
_i2c_readl(struct cisco_fpga_i2c *hw, void __iomem *addr, u32 *val)
{
	return regmap_read(hw->regmap, F(hw, addr), val);
}

static u32
cisco_fpga_i2c_func(struct i2c_adapter *adap)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adap);

	return hw->func;
}

static int
_clear_intr_status(struct cisco_fpga_i2c *hw)
{
	struct i2c_ext_regs_v5_t __iomem *csr = hw->csr;
	u32 v = REG_SET(I2C_EXT_INTSTS_ERROR, 1) |
		REG_SET(I2C_EXT_INTSTS_TIMEOUT, 1) |
		REG_SET(I2C_EXT_INTSTS_DONE, 1);

	return _i2c_writel(hw, v, &csr->intSts);
}

static inline int
_i2c_reset(struct i2c_adapter *adap, struct cisco_fpga_i2c *hw)
{
	struct i2c_ext_regs_v5_t __iomem *csr = hw->csr;
	int e;

	e = _i2c_writel(hw, REG_SET(I2C_EXT_CFG_RST, 1), &csr->cfg);
	if (!e) {
		/* ltc4151 wants 33 ms, but fpgalib (user mode) was only delaying 20 us */
		mdelay(33); // udelay(20);
		e = _i2c_writel(hw, REG_SET(I2C_EXT_CFG_RST, 0), &csr->cfg);
		udelay(10);
	}
	return e;
}

static int
cisco_fpga_i2c_recover_bus(struct i2c_adapter *adap)
{
	dev_warn(&adap->dev, "bus recovery\n");
	return _i2c_reset(adap, i2c_get_adapdata(adap));
}

static int
_wait_done(struct i2c_adapter *adap, struct cisco_fpga_i2c *hw, u32 cfg_len)
{
	struct i2c_ext_regs_v5_t __iomem *csr = hw->csr;
	u32 val;
	unsigned long timeout;
	int e;
	int usleep_min = cfg_len * 10;
	int usleep_max = (cfg_len + 50) * 10;

	/*
	 * 100KHz clock is about 10us per bit.
	 * Seems like we should be waiting at least for 21 bits?
	 */
	if (cfg_len)
		usleep_range(usleep_min, usleep_max);
	e = _i2c_readl(hw, &csr->cfg, &val);
	if (!e) {
		if (REG_GET(I2C_EXT_CFG_STARTACCESS, val)) {
			timeout = jiffies + adap->timeout;
			do {
				usleep_range(80, 160);
				e = regmap_read(hw->regmap, F(hw, &csr->cfg), &val);
			} while (!e &&
				 REG_GET(I2C_EXT_CFG_STARTACCESS, val) &&
				 !time_after(jiffies, timeout));
		}
		if (!e && REG_GET(I2C_EXT_CFG_STARTACCESS, val))
			e = -EBUSY;
	}
	return e;
}

static int
_check_err(struct cisco_fpga_i2c *hw)
{
	struct i2c_ext_regs_v5_t __iomem *csr = hw->csr;
	u32 val;
	int e = _i2c_readl(hw, &csr->intSts, &val);

	if (!e) {
		if (REG_GET(I2C_EXT_INTSTS_TIMEOUT, val))
			return -EAGAIN;
		if (REG_GET(I2C_EXT_INTSTS_ERROR, val))
			return -EFAULT;
		if (!REG_GET(I2C_EXT_INTSTS_DONE, val))
			return -EBUSY;
	}
	return e;
}

static int
_retryable_cfg(struct i2c_adapter *adap, struct cisco_fpga_i2c *hw, u32 cfg, u32 cfg_len)
{
	struct i2c_ext_regs_v5_t __iomem *csr = hw->csr;
	u32 retry = 0;
	int e;

	do {
		e = _i2c_writel(hw, cfg, &csr->cfg);
		if (!e) {
			e = _wait_done(adap, hw, cfg_len);
			if (!e)
				e = _check_err(hw);
		}
		if (e) {
			(void)_i2c_reset(adap, hw);
			_clear_intr_status(hw);
		}
	} while (e && (++retry <= adap->retries));
	return e;
}

static int
_write_cfg2_retryable_cfg(struct i2c_adapter *adap,
			  struct cisco_fpga_i2c *hw,
			  u32 cfg, u32 cfg2, u32 cfg_len)
{
	struct i2c_ext_regs_v5_t __iomem *csr = hw->csr;
	int e;

	e = _i2c_writel(hw, cfg2, &csr->cfg2);
	if (!e)
		e = _retryable_cfg(adap, hw, cfg, cfg_len);
	return e;
}

static int
_i2c_xfer(struct i2c_adapter *adap, struct cisco_fpga_i2c *hw, struct i2c_msg *msg)
{
	struct i2c_ext_regs_v5_t __iomem *csr = hw->csr;
	int e = 0;
	int index = 0;
	struct device *dev = adap->dev.parent;
	u32 cfg_acc = REG_SETe(I2C_EXT_CFG_ACCESSTYPE, cur_read);
	bool read = (msg->flags & I2C_M_RD);
	u8 *bufp = msg->buf;
	u16 len = msg->len;
	u16 dev_addr = msg->addr;
	u32 data = 0;
	u32 data0 = 0;
	u32 dev_sel;
	u32 *bufd = (u32 *) bufp;
	u16 start_len;
	u16 offset = 0;

	if (read) {
		if (msg->flags & I2C_M_RECV_LEN)
			len += I2C_SMBUS_BLOCK_MAX;
	} else {
		cfg_acc = REG_SETe(I2C_EXT_CFG_ACCESSTYPE, cur_write);
	}
	start_len = len;

	if (hw->func & I2C_FUNC_10BIT_ADDR)
		dev_sel = dev_addr >> 7;
	else
		dev_sel = adap - hw->adap;

	e = _wait_done(adap, hw, 0);
	if (e) {
		dev_err(dev, "%s:%d %s %d error %d adapter is busy?\n",
			__func__, __LINE__, adap->name, dev_sel, e);
		goto error_exit;
	}
	while (len && !e) {
		u16 cfg_len = (len > DRIVER_I2C_HW_BUF_SIZE) ? DRIVER_I2C_HW_BUF_SIZE : len;
		u32 cfg = REG_SET(I2C_EXT_CFG_TEST, 0)
			| REG_SET(I2C_EXT_CFG_DEVADDR, dev_addr)
			| REG_SET(I2C_EXT_CFG_REGADDR, offset >> 8)
			| REG_SET(I2C_EXT_CFG_SPDCNT, DEFAULT_SPEED)
			| REG_SET(I2C_EXT_CFG_DEVSEL, dev_sel)
			| REG_SETe(I2C_EXT_CFG_MODE, i2c)
			| cfg_acc
			| REG_SET(I2C_EXT_CFG_STARTACCESS, 1)
			| REG_SET(I2C_EXT_CFG_RST, 0)
			;
		u32 cfg2 = REG_SET(I2C_EXT_CFG2_RDATASIZE, cfg_len);
		void __iomem *addr = &(csr->rdata[index]);
		int i;

		index = 0;
		cfg_len = (len > hw->bufsize) ? hw->bufsize : len;

		if (!read) {
			cfg2 = REG_SET(I2C_EXT_CFG2_WDATASIZE, cfg_len);

			while (len && cfg_len && !e) {
				u16 cp_len = (cfg_len > 3) ? 4 : len;
				u8 *datac = (u8 *) &data;

				data = 0;
				for (i = 0; i < cp_len; i++)
					*datac++ = *bufp++;
				if (!index)
					data0 = data;
				e = _i2c_writel(hw, data, &(csr->wdata[index]));
				len -= cp_len;
				cfg_len -= cp_len;
				index++;
			}
		}

		e = _write_cfg2_retryable_cfg(adap, hw, cfg, cfg2, cfg_len);
		while (read && len && cfg_len && !e) {
			addr = &hw->rdata_ptr[index];
			e = _i2c_readl(hw, addr, &data);
			if (!e) {
				u16 cp_len = (cfg_len > 3) ? 4 : len;
				u8 *datac = (u8 *) &data;

				for (i = 0; i < cp_len; i++)
					*bufp++ = *datac++;
				if (!index)
					data0 = data;
				len -= cp_len;
				cfg_len -= cp_len;
				index++;
			}
		}
	}
error_exit:
	if (e || DRIVER_I2C_DEBUG_LEVEL)
		dev_info(dev, "%s:%d i2c %s error: addr 0x%03x start_len %d len %d err %d data 0x%08x data0 0x%08x bufd 0x%08x\n",
			 __func__, __LINE__, read ? "read " : "write", dev_addr, start_len, len, e, data, data0, *bufd);
	if (!e && read && msg->flags & I2C_M_RECV_LEN
		       && (msg->len + msg->buf[0]) <= start_len)
		msg->len += msg->buf[0];
	return e;
}

static int
cisco_fpga_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg *msg, int num)
{
	struct cisco_fpga_i2c *hw = i2c_get_adapdata(adap);
	int i, err;
	struct device *dev = adap->dev.parent;

	/* clear interrupts */
	err = _clear_intr_status(hw);

	for (i = 0; i < num; ++i) {
		err = _i2c_xfer(adap, hw, &msg[i]);
		if (err || DRIVER_I2C_DEBUG_LEVEL) {
			dev_info(dev, "%s: msg %d addr 0x%x flags 0x%08x len %d err %d\n",
				     __func__, i, msg[i].addr, msg[i].flags, msg[i].len, err);
			if (err)
				break;
		}
	}

	if (err)
		(void)_i2c_reset(adap, hw);

	return err ? err : num;
}

static const struct i2c_algorithm cisco_fpga_i2c_algo = {
	.master_xfer    = cisco_fpga_i2c_xfer,
	.functionality  = cisco_fpga_i2c_func,
};

static struct i2c_bus_recovery_info cisco_fpga_i2c_recovery = {
	.recover_bus    = cisco_fpga_i2c_recover_bus,
};

static int
cisco_fpga_i2c_ext_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct cisco_fpga_i2c *hw;
	int e;
	struct i2c_ext_regs_v5_t __iomem *rcsr;
	static const struct regmap_config r_config = {
	    .reg_bits = 32,
	    .val_bits = 32,
	    .reg_stride = 4,
	    .fast_io = false,
	    .max_register = sizeof(struct i2c_ext_regs_v5_t) - 1,
	};
	const struct i2c_adapter i2c_adapter_template = {
	    .owner = THIS_MODULE,
	    .algo = &cisco_fpga_i2c_algo,
	    .retries = 3,
	    .timeout = msecs_to_jiffies(300),
	    .bus_recovery_info = &cisco_fpga_i2c_recovery,
	};

	e = cisco_i2c_init(pdev, &r_config,
			   &i2c_adapter_template, "I2C-EXT");
	if (e) {
		dev_err(dev, "cisco_i2c_init failed; status %d\n", e);
		return e;
	}
	hw = platform_get_drvdata(pdev);

	if (hw->num_adapters == 1)
		hw->func |= I2C_FUNC_10BIT_ADDR;

	rcsr = hw->csr;
	if (hw->ver <= 4) {
		hw->bufsize = DRIVER_I2C_HW_BUF_SIZE;
		hw->rdata_ptr = (u32 *) &rcsr->rdata;
	} else {
		hw->bufsize = DRIVER_I2C_HW_BUF_SIZE_v5;
		hw->rdata_ptr = (u32 *) &rcsr->rdata_v5;
	}

	return cisco_i2c_register(pdev, _i2c_reset);
}

static const struct platform_device_id cisco_fpga_i2c_ext_id_table[] = {
	{ .name = "i2c-ext-rp",		.driver_data = 1 },
	{ .name = "i2c-ext-ft",		.driver_data = 1 },
	{ .name = "i2c-ext-fc0",	.driver_data = 1 },
	{ .name = "i2c-ext-fc1",	.driver_data = 1 },
	{ .name = "i2c-ext-fc2",	.driver_data = 1 },
	{ .name = "i2c-ext-fc3",	.driver_data = 1 },
	{ .name = "i2c-ext-fc4",	.driver_data = 1 },
	{ .name = "i2c-ext-fc5",	.driver_data = 1 },
	{ .name = "i2c-ext-fc6",	.driver_data = 1 },
	{ .name = "i2c-ext-fc7",	.driver_data = 1 },
	{ .name = "i2c-ext-lc",		.driver_data = 0 },
	{ .name = "i2c-ext",		.driver_data = 1 },
	{ .name = "i2c-ext-pim1",	.driver_data = 1 },
	{ .name = "i2c-ext-pim2",	.driver_data = 1 },
	{ .name = "i2c-ext-pim3",	.driver_data = 1 },
	{ .name = "i2c-ext-pim4",	.driver_data = 1 },
	{ .name = "i2c-ext-pim5",	.driver_data = 1 },
	{ .name = "i2c-ext-pim6",	.driver_data = 1 },
	{ .name = "i2c-ext-pim7",	.driver_data = 1 },
	{ .name = "i2c-ext-pim8",	.driver_data = 1 },
	{ },
};
MODULE_DEVICE_TABLE(platform, cisco_fpga_i2c_ext_id_table);

static struct platform_driver cisco_fpga_i2c_driver = {
	.driver = {
		.name   = DRIVER_NAME,
	},
	.probe      = cisco_fpga_i2c_ext_probe,
	.id_table   = cisco_fpga_i2c_ext_id_table,
};
module_platform_driver(cisco_fpga_i2c_driver);

MODULE_AUTHOR("Cisco Systems, Inc. <ospo-kmod@cisco.com>");
MODULE_VERSION(DRIVER_VERSION);
MODULE_DESCRIPTION("Cisco FPGA I2C-EXT Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS(PLATFORM_MODULE_PREFIX DRIVER_NAME);
MODULE_SOFTDEP("pre: cisco-fpga-xil cisco-fpga-msd");
