// SPDX-License-Identifier: GPL-2.0-only
/*
 * XIL IP Block driver
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/ctype.h>

#define DRIVER_NAME     "cisco-fpga-xil"
#define DRIVER_VERSION  "1.0"

#define DRIVER_DATA_ACTIVE     0x1
#define DRIVER_DATA_OVERRIDE   0x2

#include "cisco/reg_access.h"
#include "cisco/hdr.h"
#include "cisco/xil.h"
#include "cisco/mfd.h"
#include "cisco/util.h"

#define XIL_NNPUS       6

struct xil_ext_attribute {
struct device_attribute attr;
	u16 reg;
	u8 npu;
	u8 attr_no;
	u8 bit;
};
enum xil_sysfs_attribute {
	XIL_DONE,
	XIL_INVALID_OPCODE_ERR,
	XIL_SPI_CRC_ERR,
	XIL_I2C_NACK_ERR,
	XIL_NATTRS,
};

struct xil_adapter_t {
	struct xil_regs_t __iomem *csr;
	struct regmap *regmap;
	u8 active;
	u8 nnpus;

	struct xil_ext_attribute npu_sysfs[XIL_NNPUS][XIL_NATTRS];
	struct attribute *npu_attrs[XIL_NNPUS][XIL_NATTRS + 1];
	struct attribute_group npu_group[XIL_NNPUS];
	const struct attribute_group *groups[XIL_NNPUS + 8];

} xil_adapter_t;

static const struct reboot_info r_info = {
	.enable = 1,
	.priority = 64,

	.restart.reg = 0x3c,
	.restart.mask = 0xfff,
	.restart.value = 0x400,

	.halt.reg = 0x3c,
	.halt.mask = 0xfff,
	.halt.value = 0x8,

	.poweroff.reg = 0x3c,
	.poweroff.mask = 0xfff,
	.poweroff.value = 0x8,
};

static struct platform_driver cisco_fpga_xil_driver;

#define F(f) offsetof(struct xil_regs_t, f)

static ssize_t
outshifts_enable_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	size_t len = PAGE_SIZE;
	ssize_t err;
	u32 data;

	if (r) {
		err = regmap_read(r, F(cfg1), &data);
		if (!err) {
			int enabled =
				(REG_GET(XIL_CFG1_GEN_CONF_OUTSHIFTS, data) ==
					REG_CONST(XIL_CFG1_GEN_CONF_OUTSHIFTS, enable));

			err = scnprintf(buf, len, "%u\n", enabled);
		}
	} else {
		err = -ENXIO;
	}

	return err;
}

static ssize_t
outshifts_enable_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t buflen)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	unsigned int v;
	int e, consumed;
	u32 cfg;

	if (!r)
		return -ENXIO;

	if ((sscanf(buf, "%i %n", &v, &consumed) > 0)
				&& (consumed == buflen)) {
		if (v == 1)
			cfg = xil_cfg1_outshifts__enable;
		else if (!v)
			cfg = xil_cfg1_outshifts__disable;
		else
			return -EINVAL;
		e = REG_UPDATE_BITS(r, XIL_CFG1_GEN_CONF_OUTSHIFTS, cfg);
		if (e)
			return e;
		return consumed;
	}

	return -EINVAL;
}
static DEVICE_ATTR_RW(outshifts_enable);

static const char * const _console_source[] = {
	[REG_CONST(XIL_CFG1_GEN_CONF_CONSOLE, jumper)] = "jumper",
	[REG_CONST(XIL_CFG1_GEN_CONF_CONSOLE, uxbar)] = "uxbar",
};

static ssize_t
console_source_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	size_t len = PAGE_SIZE;
	ssize_t err;
	u32 data;

	if (r) {
		err = regmap_read(r, F(cfg1), &data);
		if (!err) {
			int index = REG_GET(XIL_CFG1_GEN_CONF_CONSOLE, data);

			if (index < ARRAY_SIZE(_console_source) && _console_source[index])
				err = scnprintf(buf, len, "%s\n", _console_source[index]);
			else
				err = scnprintf(buf, len, "%u\n", index);
		}
	} else {
		err = -ENXIO;
	}

	return err;
}

static inline void
_byp(const char **bufpp)
{
	const char *bufp = *bufpp;

	while (isspace(*bufp))
		++bufp;
	*bufpp = bufp;
}

static ssize_t
console_source_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t buflen)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	int e, i;
	const char *bufp = buf;

	if (!r)
		return -ENXIO;

	e = -EINVAL;
	_byp(&bufp);
	for (i = 0; i < ARRAY_SIZE(_console_source); ++i) {
		size_t len;

		if (!_console_source[i])
			continue;

		len = strlen(_console_source[i]);
		if (buflen < len)
			continue;

		if (strncmp(bufp, _console_source[i], len))
			continue;

		bufp += len;
		_byp(&bufp);
		if (!*bufp) {
			e = REG_UPDATE_BITS(r, XIL_CFG1_GEN_CONF_CONSOLE, i);
			if (!e)
				e = bufp - buf;
		}
		break;
	}

	return e;
}
static DEVICE_ATTR_RW(console_source);

static ssize_t
board_type_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	char *bufp = buf;
	size_t len = PAGE_SIZE;
	static const char *_board_type[REG_LIMIT(XIL_STATUS1_BOARD_TYPE)] = {
	};
	ssize_t err;
	u32 data;

	if (r) {
		err = regmap_read(r, F(status1), &data);
		if (!err) {
			uint32_t v = REG_GET(XIL_STATUS1_BOARD_TYPE, data);
			uint32_t version = REG_GET(XIL_STATUS1_BOARD_VER, data);
			const char *board_type = _board_type[v];

			if (board_type)
				err = scnprintf(bufp, len, "%s, v%u\n", board_type, version);
			else
				err = scnprintf(bufp, len, "%u: unknown, v%u\n", v, version);
		}
	} else
		err = -ENXIO;

	return err;
}
static DEVICE_ATTR_RO(board_type);

static ssize_t
_npu_show(struct device *dev,
		  struct device_attribute *dattr,
		  char *buf)
{
	struct xil_ext_attribute *attr = (typeof(attr))dattr;
	struct regmap *r = dev_get_regmap(dev, NULL);
	char *bufp = buf;
	size_t len = PAGE_SIZE;
	ssize_t err;
	u32 data;

	if (r) {
		err = regmap_read(r, attr->reg, &data);
		if (!err) {
			u32 v = (data >> attr->bit) & 1;

			err = scnprintf(bufp, len, "%u\n", v);
		}
	} else
		err = -ENXIO;

	return err;
}

static struct attribute *_xil_sys_attrs[] = {
	&dev_attr_outshifts_enable.attr,
	&dev_attr_console_source.attr,
	&dev_attr_board_type.attr,
	NULL,
};
static const struct attribute_group _xil_attr_group = {
	.attrs = _xil_sys_attrs,
};

static bool
_xil_status0_is_fowlmere(u32 data)
{
	return (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, DISTRIBUTED_FABRIC_FOWLMERE)) &&
			(REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, DISTRIBUTED));
}

static bool
_xil_status0_is_filton(u32 data)
{
	return (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, DISTRIBUTED_FABRIC)) &&
			(REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, DISTRIBUTED));
}

static bool
_xil_status0_is_kenley(u32 data)
{
	return ((REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, DISTRIBUTED_KENLEY_GAUNTLET)) ||
			(REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, DISTRIBUTED_KENLEY_CORSAIR))) &&
			(REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, DISTRIBUTED));
}

static bool
_xil_status0_is_kirkwall(u32 data)
{
	return ((REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, DISTRIBUTED_KIRKWALL_VANGUARD)) ||
			(REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, DISTRIBUTED_KIRKWALL_LANCER))) &&
			(REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, DISTRIBUTED));
}

static bool
_xil_status0_is_pembrey(u32 data)
{
	return (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, DISTRIBUTED_RP_PEMBREY) &&
			REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, DISTRIBUTED));
}

static bool
_xil_status0_is_zenith(u32 data)
{
	return (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, DISTRIBUTED_RP_ZENITH) &&
			REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, DISTRIBUTED));
}

static bool
_xil_status0_is_sherman(u32 data)
{
	return (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, FIXED_IOFPGA_SHERMAN)) &&
			(REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, FIXED));
}

static bool
_xil_status0_is_churchill(u32 data)
{
	return (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, FIXED_IOFPGA_CHURCHILL)) &&
			(REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, FIXED));
}

static bool
_xil_status0_is_crocodile(u32 data)
{
	return (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, FIXED_IOFPGA_CROCODILE)) &&
			(REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, FIXED));
}

static bool
_xil_status0_is_matilda(u32 data)
{
	return (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, FIXED_IOFPGA_MATILDA_32) ||
			REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, FIXED_IOFPGA_MATILDA_64)) &&
			(REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, FIXED));
}

static bool
_xil_status0_is_pershing(u32 data)
{
	return (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, FIXED_IOFPGA_PERSHING_BASE) ||
			REG_GET(XIL_STATUS0_FPGA_ID, data) ==
				REG_CONST(XIL_STATUS0_FPGA_ID, FIXED_IOFPGA_PERSHING_MEZZ)) &&
			(REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
				REG_CONST(XIL_STATUS0_PLATFORM_ID, FIXED));
}

static bool
_xil_status0_is_kodiak(u32 data)
{
	bool central = (REG_GET(XIL_STATUS0_PLATFORM_ID, data) ==
					REG_CONST(XIL_STATUS0_PLATFORM_ID, CENTRAL));

	if (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
			REG_CONST(XIL_STATUS0_FPGA_ID, CENTRAL_CYCLONUS) &&
			central)
		return TRUE;
	if (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
			REG_CONST(XIL_STATUS0_FPGA_ID, CENTRAL_SILVERBOLT) &&
			central)
		return TRUE;
	if (REG_GET(XIL_STATUS0_FPGA_ID, data) ==
			REG_CONST(XIL_STATUS0_FPGA_ID, CENTRAL_PINPOINTER) &&
			central)
		return TRUE;

	return FALSE;
}

static int
_xil_sysfs_init(struct device *dev, struct xil_adapter_t *priv)
{
	static const char * const _npu_names[XIL_NNPUS] = {
		"NPU0",
		"NPU1",
		"NPU2",
		"NPU3",
		"NPU4",
		"NPU5",
	};
	static const char * const _npu_attrs[XIL_NATTRS] = {
		[XIL_DONE]               "init_done",
		[XIL_INVALID_OPCODE_ERR] "invalid_opcode_error",
		[XIL_SPI_CRC_ERR]        "spi_crc_error",
		[XIL_I2C_NACK_ERR]       "i2c_nack_error",
	};
	int ngroups = 0;
	int err;
	u32 max_nnpus = 0, nnpus = 0, npu;
	u32 data;
	u32 status_reg = F(status2);
	u8  bit_offset = 0;

	err = regmap_read(priv->regmap, F(status0), &data);
	if (err)
		dev_warn(dev, "failed to read status0; status %d\n", err);
	else if (_xil_status0_is_fowlmere(data))
		max_nnpus = 2;
	else if (_xil_status0_is_filton(data))
		max_nnpus = 6;
	else if (_xil_status0_is_kenley(data))
		max_nnpus = 4;
	else if (_xil_status0_is_kirkwall(data))
		max_nnpus = 3;
	else if (_xil_status0_is_sherman(data)) {
		max_nnpus = 1;
		status_reg = F(status1);
		bit_offset = 24;
	} else if (_xil_status0_is_churchill(data) ||
		_xil_status0_is_matilda(data) ||
		_xil_status0_is_crocodile(data) ||
		_xil_status0_is_pershing(data))
		/* Just until we get good documentation on the registers */
		max_nnpus = 0;
	else if (_xil_status0_is_pembrey(data) ||
		_xil_status0_is_zenith(data))
		max_nnpus = 0;
	else if (_xil_status0_is_kodiak(data))
		max_nnpus = 0;
	else
		dev_warn(dev, "status0 %#x is not supported\n", data);

	if (!err && max_nnpus) {
		if (!priv->active || device_property_read_u32(dev, "nnpus", &nnpus) < 0)
			nnpus = max_nnpus;
		else if (nnpus > max_nnpus)
			nnpus = max_nnpus;
		priv->nnpus = nnpus;
	}

	if (!err && nnpus) {
		for (npu = 0; npu < nnpus; ++npu) {
			struct xil_ext_attribute *sysfs = &priv->npu_sysfs[npu][0];
			struct attribute **attrs = &priv->npu_attrs[npu][0];
			int bit;

			priv->npu_group[npu].name = _npu_names[npu];
			priv->npu_group[npu].attrs = attrs;
			priv->groups[ngroups++] = &priv->npu_group[npu];

			for (bit = 0; bit < XIL_NATTRS; ++bit, ++sysfs) {
				sysfs->attr.attr.mode = 0444;
				sysfs->attr.attr.name = _npu_attrs[bit];
				sysfs->attr.show = _npu_show;
				sysfs->attr.store = NULL;
				sysfs->npu = npu;
				sysfs->attr_no = bit;
				sysfs->bit = (bit * nnpus) + npu + bit_offset;
				sysfs->reg = status_reg;
				*attrs++ = &sysfs->attr.attr;
			}
			*attrs++ = NULL;
		}
	}

	if (!err) {
		priv->groups[ngroups++] = &_xil_attr_group;
		priv->groups[ngroups++] = &cisco_fpga_msd_xil_attr_group;
		priv->groups[ngroups++] = &cisco_fpga_msd_xil_scratch_bios_attr_group;
		priv->groups[ngroups++] = &cisco_fpga_msd_xil_scratch_uboot_attr_group;
		priv->groups[ngroups++] = &cisco_fpga_msd_xil_scratch_chassis_attr_group;
		priv->groups[ngroups++] = &cisco_fpga_msd_xil_scratch_idprom_attr_group;
		priv->groups[ngroups++] = &cisco_fpga_reghdr_attr_group;
		priv->groups[ngroups] = NULL;
		err = devm_device_add_groups(dev, priv->groups);
	}
	return err;
}

static int
_xil_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct xil_adapter_t *priv;
	uintptr_t csr;
	int e;

	e = cisco_fpga_msd_xil_mfd_init(pdev, sizeof(*priv), &csr);
	if (e)
		return e;

	priv = platform_get_drvdata(pdev);
	priv->regmap = dev_get_regmap(dev, NULL);
	if (!priv->regmap)
		return -ENXIO;

	priv->csr = (typeof(priv->csr))csr;
	if (pdev->id_entry && (pdev->id_entry->driver_data & DRIVER_DATA_OVERRIDE))
		priv->active = pdev->id_entry->driver_data & DRIVER_DATA_ACTIVE;
	else if (pdev->mfd_cell && pdev->mfd_cell->platform_data
					&& pdev->mfd_cell->pdata_size == sizeof(u8))
		priv->active = *(u8 *)pdev->mfd_cell->platform_data;
	else if (pdev->id_entry)
		priv->active = pdev->id_entry->driver_data & DRIVER_DATA_ACTIVE;
	else
		priv->active = 1;

	if (priv->active) {
		e = regmap_update_bits(priv->regmap, F(cfg1), BIT(8), BIT(8));
		if (e)
			dev_warn(dev, "failed to enable outshifts; status %d\n", e);
	} else
		dev_info(dev, "passive\n");

	e = _xil_sysfs_init(dev, priv);
	if (e < 0)
		dev_err(dev, "_xil_sysfs_init failed; status %d\n", e);
	else if (priv->active) {
		/* Reset boot mode */
		e = regmap_write(priv->regmap, F(scratchram), 0);
		if (e) {
			dev_err(dev, "failed to reset boot mode; status %d\n", e);
			e = 0;
		}
		e = REG_UPDATE_BITSe(priv->regmap, XIL_CFG5_MASTER_SELECT, X86);
		if (e)
			dev_warn(dev, "failed to set X86 as i2c master; status %d\n", e);
		e = cisco_register_reboot_notifier(pdev, &r_info);
	}

	return e;
}

static int
_driver_match(struct device *dev,
#if KERNEL_VERSION(5, 3, 0) <= LINUX_VERSION_CODE
				const
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0) */
				void *data)
{
	const char *name = (typeof(name)) data;

	return strcmp(name, dev_name(dev)) ? 0 : 1;
}

int
cisco_fpga_xil_npu_status(struct device *npu_dev, const char *npu_name)
{
	const char *p = strstr(npu_name, "NPU");
	int npu, e = -EINVAL;
	struct device *xil_dev;
	struct xil_adapter_t *priv;
	u32 status0, status2;
	int done, opcode_err, spi_err, i2c_err;
	unsigned long ul;

	e = kstrtol(p + 3, 10, &ul);
	if (!p || e) {
		dev_info(npu_dev, "%s: invalid NPU name %s\n", __func__, npu_name);
		return e;
	}

	npu = ul;
	if (npu_name == p) {  /* line card or fixed */
		xil_dev = driver_find_device(&cisco_fpga_xil_driver.driver, NULL, "xil", _driver_match);
	} else if (!strncmp(npu_name, "FC", 2) && (p == npu_name + 4)) {
		char driver[] = "xil-fcx";

		driver[6] = npu_name[2];
		xil_dev = driver_find_device(&cisco_fpga_xil_driver.driver, NULL, driver, _driver_match);
	} else {
		dev_err(npu_dev, "%s: malformed NPU name %s\n", __func__, npu_name);
		return e;
	}
	if (!xil_dev) {
		dev_info(npu_dev, "%s: cannot find xil driver for %s\n", __func__, npu_name);
		return 0;
		// dev_info(npu_dev, "%s: deferred for lack of %s\n", __func__, npu_name);
		// return -EPROBE_DEFER;
	}
	priv = (typeof(priv))dev_get_drvdata(xil_dev);
	if (!priv) {
		dev_info(xil_dev, "%s: no private data for %s\n", __func__, npu_name);
		e = -EPROBE_DEFER;
	} else {
		e = regmap_read(priv->regmap, F(status0), &status0);
		if (e) {
			dev_warn(xil_dev, "%s: failed to read status0 for %s; status %d\n", __func__, npu_name, e);
			e = -EPROBE_DEFER;
		}
	}
	if (!e) {
		e = regmap_read(priv->regmap, F(status2), &status2);
		if (e) {
			dev_warn(xil_dev, "%s: failed to read status2 for %s; status %d\n", __func__, npu_name, e);
			e = -EPROBE_DEFER;
		}
	}
	if (!e && (npu >= priv->nnpus)) {
		if (priv->nnpus) {
			dev_err(xil_dev, "%s: NPU%u out of range (max %u)\n", __func__, npu, priv->nnpus);
			e = -EINVAL;
		}
	} else if (!e) {
		done = (status2 >> npu) & 1;
		opcode_err = (status2 >> (priv->nnpus + npu)) & 1;
		spi_err = (status2 >> ((priv->nnpus * 2) + npu)) & 1;
		i2c_err = (status2 >> ((priv->nnpus * 3) + npu)) & 1;
		dev_info(npu_dev, "%s: %s: %s: status2 %#x; done %u; opcode_err %u; spi_err %u; i2c_err %u\n",
				__func__, dev_name(xil_dev), npu_name, status2,
				done, opcode_err, spi_err, i2c_err);
		if (done && !opcode_err && !spi_err && !i2c_err) {
			e = 0;
		} else {
			dev_info(npu_dev, "%s: %s: %s: deferred\n",
				__func__, dev_name(xil_dev), npu_name);
			e = -EPROBE_DEFER;
		}
	}
	put_device(xil_dev);
	if (e == -EPROBE_DEFER) {
		dev_warn(npu_dev, "%s: %s: %s: ignoring deferral request\n",
			__func__, dev_name(xil_dev), npu_name);
		e = 0;
	}

	return e;
}
EXPORT_SYMBOL_GPL(cisco_fpga_xil_npu_status);

static const struct platform_device_id cisco_fpga_xil_id_table[] = {
	{ .name = "xil-lc", .driver_data = 0 },
	{ .name = "xil-fc0", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc1", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc2", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc3", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc4", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc5", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc6", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc7", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc0.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc1.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc2.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc3.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc4.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc5.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc6.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-fc7.p2pm", .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-rp", .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "xil",    .driver_data = DRIVER_DATA_ACTIVE },
	{ .name = "xil-pim1",    .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-pim2",    .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-pim3",    .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-pim4",    .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-pim5",    .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-pim6",    .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-pim7",    .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ .name = "xil-pim8",    .driver_data = DRIVER_DATA_ACTIVE | DRIVER_DATA_OVERRIDE },
	{ },
};
MODULE_DEVICE_TABLE(platform, cisco_fpga_xil_id_table);

static struct platform_driver cisco_fpga_xil_driver = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.probe    = _xil_probe,
	.id_table = cisco_fpga_xil_id_table,
};
module_platform_driver(cisco_fpga_xil_driver);

MODULE_AUTHOR("Cisco Systems, Inc. <ospo-kmod@cisco.com>");
MODULE_DESCRIPTION("Cisco 8000 XIL Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS(PLATFORM_MODULE_PREFIX DRIVER_NAME);
