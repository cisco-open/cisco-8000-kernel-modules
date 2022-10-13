// SPDX-License-Identifier: GPL-2.0-only
/*
 * msd_xil_sysfs.c
 *
 * Common sysfs entries for msd and xil
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/ctype.h>
#include <linux/sysfs.h>

#include "cisco/reg_access.h"
#include "cisco/hdr.h"
#include "cisco/xil.h"
#include "cisco/mfd.h"
#include "cisco/sysfs.h"

/*
 * Note that MSD and XIL have the same register offsets, and same register
 * layouts for the fields handled in this file
 */

#define F(f) offsetof(struct xil_regs_t, f)
#define SCRATCH_F(f) (F(scratchram) + offsetof(struct xil_msd_scratchram_t, f))

struct scratchram_attribute {
	struct device_attribute	attr;
	off_t			reg_offset;
	size_t			len;
	const char * const	*map;
};

static inline void
_byp(const char **bufpp)
{
	const char *bufp = *bufpp;

	while (isspace(*bufp))
		++bufp;
	*bufpp = bufp;
}

static ssize_t
_scratch_show_bit(struct device *dev,
		  struct device_attribute *attr,
		  char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	struct scratchram_attribute *xattr = (typeof(xattr))attr;
	ssize_t err;
	size_t len = PAGE_SIZE;

	if (r) {
		size_t index;

		for (err = 0, index = 0; len && (index < xattr->len); ++index) {
			const char *p = xattr->map[index];

			if (p) {
				ssize_t bytes = scnprintf(buf, len, "%s\n", p);

				if (bytes > len) {
					*buf = 0;
					break;
				}
				len -= bytes;
				buf += bytes;
				err += bytes;
			}
		}
	} else {
		err = -ENXIO;
	}

	return err;
}

static ssize_t
_scratch_store_bit(struct device *dev,
		   struct device_attribute *attr,
		   const char *buf,
		   size_t buflen)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	struct scratchram_attribute *xattr = (typeof(xattr))attr;
	int e, i;
	const char *bufp = buf;

	if (!r)
		return -ENXIO;
	e = -EINVAL;
	_byp(&bufp);
	if (xattr->map) {
		for (i = 0; i < xattr->len; ++i) {
			size_t len;

			if (!xattr->map[i])
				continue;

			len = strlen(xattr->map[i]);
			if (buflen < len)
				continue;

			if (strncmp(bufp, xattr->map[i], len))
				continue;

			if ((buflen > len) && !isspace(bufp[len]))
				continue;

			bufp += len;
			_byp(&bufp);
			if (!*bufp) {
				u32 v;

				e = regmap_read(r, xattr->reg_offset, &v);
				if (!e && v) {
					dev_warn(dev, "write %#lx (%s) to register %#lx (current value %#x) refused\n",
							BIT(i), xattr->map[i], xattr->reg_offset, v);
					e = -EAGAIN;
				} else if (!e) {
					e = regmap_write(r, xattr->reg_offset, BIT(i));
					if (!e)
						e = bufp - buf;
				}
			}
			break;
		}
	}

	return e;
}

static ssize_t
_scratch_show_u32(struct device *dev,
		  struct device_attribute *attr,
		  char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	struct scratchram_attribute *xattr = (typeof(xattr))attr;
	ssize_t err;
	size_t len = PAGE_SIZE;

	if (r) {
		u32 v;

		err = regmap_read(r, xattr->reg_offset, &v);
		if (!err) {
			if (xattr->map && (v < xattr->len) && xattr->map[v])
				err = scnprintf(buf, len, "%s\n", xattr->map[v]);
			else
				err = scnprintf(buf, len, "%u\n", v);
		}
	} else {
		err = -ENXIO;
	}

	return err;
}

static ssize_t
_scratch_store_u32(struct device *dev,
		   struct device_attribute *attr,
		   const char *buf,
		   size_t buflen)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	struct scratchram_attribute *xattr = (typeof(xattr))attr;
	int e, i;
	const char *bufp = buf;

	if (!r)
		return -ENXIO;

	e = -EINVAL;
	_byp(&bufp);
	if (xattr->map) {
		for (i = 0; i < xattr->len; ++i) {
			size_t len;

			if (!xattr->map[i])
				continue;

			len = strlen(xattr->map[i]);
			if (buflen < len)
				continue;

			if (strncmp(bufp, xattr->map[i], len))
				continue;

			if ((buflen > len) && !isspace(bufp[len]))
				continue;

			bufp += len;
			_byp(&bufp);

			if (!*bufp) {
				e = regmap_write(r, xattr->reg_offset, i);
				if (!e)
					e = bufp - buf;
			}
			break;
		}
	} else {
		unsigned int value;
		int consumed;

		if ((sscanf(buf, "%i %n", &value, &consumed) == 1)
						&& (consumed == buflen)) {
			e = regmap_write(r, xattr->reg_offset, value);
			if (!e)
				e = consumed;
		}
	}
	return e;
}

static ssize_t
_scratch_show_hw_ver(struct device *dev,
		     struct device_attribute *attr,
		     char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	struct scratchram_attribute *xattr = (typeof(xattr))attr;
	ssize_t err;
	size_t len = PAGE_SIZE;

	if (r) {
		u32 v;

		err = regmap_read(r, xattr->reg_offset, &v);
		if (!err)
			err = scnprintf(buf, len, "%u.%u\n",
						(v >> 16), (v & 0xffff));
	} else {
		err = -ENXIO;
	}

	return err;
}

static ssize_t
_scratch_store_hw_ver(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf,
		      size_t buflen)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	struct scratchram_attribute *xattr = (typeof(xattr))attr;
	int e;
	unsigned int major, minor;
	int consumed;

	if (!r)
		return -ENXIO;

	e = -EINVAL;
	if ((sscanf(buf, "%u.%u %n", &major, &minor, &consumed) == 2)
			&& (consumed == buflen)) {
		e = 0;
	} else if ((sscanf(buf, "%u %n", &major, &consumed) == 1)
			&& (consumed == buflen)) {
		minor = 0;
		e = 0;
	}
	if (!e) {
		if ((major < 0x10000) && (minor < 0x10000)) {
			e = regmap_write(r, xattr->reg_offset, (major << 16) | minor);
			if (!e)
				e = consumed;
		} else {
			e = -ERANGE;
		}
	}

	return e;
}

static ssize_t
_scratch_show_str(struct device *dev,
		  struct device_attribute *attr,
		  char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	struct scratchram_attribute *xattr = (typeof(xattr))attr;
	ssize_t err = 0;

	if (r) {
		ssize_t len = xattr->len;
		off_t off = xattr->reg_offset;
		char *bufp = buf;

		BUG_ON(len >= (PAGE_SIZE - 1));
		do {
			u32 v;

			err = regmap_read(r, off, &v);
			if (err)
				break;

			memcpy(bufp, &v, sizeof(v));
			off += sizeof(v);
			bufp += sizeof(v);
			len -= sizeof(v);
		} while (len > 0);
		if (!err) {
			*bufp = 0;
			err = strlen(buf);
			buf[err++] = '\n';
			buf[err] = 0;
		}
	} else
		err = -ENXIO;

	return err;
}

static ssize_t
_scratch_store_str(struct device *dev,
		   struct device_attribute *attr,
		   const char *buf,
		   size_t buflen)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	struct scratchram_attribute *xattr = (typeof(xattr))attr;
	ssize_t err = 0;
	char *bufp;
	char *nl;
	size_t len, new_buflen;
	off_t off = xattr->reg_offset;
	int consumed = 0;

	if (!r)
		return -ENXIO;
	bufp = kzalloc(xattr->len + sizeof(len), GFP_KERNEL);
	if (!bufp)
		return -ENOMEM;

	new_buflen = buflen;
	if (new_buflen > xattr->len)
		new_buflen = xattr->len;

	strncpy(bufp, buf, new_buflen);
	nl = strchr(bufp, '\n');
	if (nl) {
		memset(nl, 0, &bufp[new_buflen] - nl);
		new_buflen = nl - bufp;
		consumed = 1;
	}
	consumed += new_buflen;
	if (consumed == buflen) {
		len = 0;
		nl = bufp;
		while ((len < xattr->len) && !err) {
			u32 v;

			memcpy(&v, nl, sizeof(v));
			err = regmap_write(r, off, v);
			off += sizeof(v);
			nl += sizeof(v);
			len += sizeof(v);
		}
		err = buflen;
	} else {
		err = -EINVAL;
	}

	kfree(bufp);

	return err;
}

static const char * const _control[] = {
	[3] = "power-off",
	[5] = "power-on",
	[6] = "cold-reset",
	[9] = "warm-reset",
	[10] = "power-cycle",
};

static struct scratchram_attribute control = {
	.attr = {
		.attr.name = "control",
		.attr.mode = 0600,
		.show = _scratch_show_bit,
		.store = _scratch_store_bit,
	},
	.reg_offset = F(cfg7),
	.map = _control,
	.len = ARRAY_SIZE(_control),
};

static const char * const _boot_mode[] = {
	[0] = "default",
	[1] = "SSD",
	[2] = "USB",
	[3] = "IPXE",
};

static struct scratchram_attribute bios_boot_mode = {
	.attr = {
		.attr.name = "boot_mode",
		.attr.mode = 0644,
		.show = _scratch_show_u32,
		.store = _scratch_store_u32,
	},
	.reg_offset = SCRATCH_F(bios_boot_mode),
	.map = _boot_mode,
	.len = ARRAY_SIZE(_boot_mode),
};

static struct scratchram_attribute bios_running_version = {
	.attr = {
		.attr.name = "running_version",
		.attr.mode = 0644,
		.show = _scratch_show_u32,
		.store = _scratch_store_u32,
	},
	.reg_offset = SCRATCH_F(bios_running_version),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute bios_flash_select = {
	.attr = {
		.attr.name = "flash_select",
		.attr.mode = 0644,
		.show = _scratch_show_u32,
		.store = _scratch_store_u32,
	},
	.reg_offset = SCRATCH_F(bios_flash_select),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute uboot_running_version = {
	.attr = {
		.attr.name = "running_version",
		.attr.mode = 0644,
		.show = _scratch_show_u32,
		.store = _scratch_store_u32,
	},
	.reg_offset = SCRATCH_F(uboot_running_version),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute uboot_mac_addr = {
	.attr = {
		.attr.name = "mac_addr",
		.attr.mode = 0644,
		.show = _scratch_show_str,
		.store = _scratch_store_str,
	},
	.reg_offset = SCRATCH_F(uboot_mac_addr),
	.map = 0,
	.len = 12,
};

static struct scratchram_attribute chassis_info_valid = {
	.attr = {
		.attr.name = "info_valid",
		.attr.mode = 0644,
		.show = _scratch_show_u32,
		.store = _scratch_store_u32,
	},
	.reg_offset = SCRATCH_F(chassis_info_valid),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute chassis_pd_type = {
	.attr = {
		.attr.name = "pd_type",
		.attr.mode = 0644,
		.show = _scratch_show_u32,
		.store = _scratch_store_u32,
	},
	.reg_offset = SCRATCH_F(chassis_pd_type),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute chassis_hw_version = {
	.attr = {
		.attr.name = "hw_version",
		.attr.mode = 0644,
		.show = _scratch_show_hw_ver,
		.store = _scratch_store_hw_ver,
	},
	.reg_offset = SCRATCH_F(chassis_hw_version),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute chassis_rack_id = {
	.attr = {
		.attr.name = "rack_id",
		.attr.mode = 0644,
		.show = _scratch_show_u32,
		.store = _scratch_store_u32,
	},
	.reg_offset = SCRATCH_F(chassis_rack_id),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute chassis_pid = {
	.attr = {
		.attr.name = "pid",
		.attr.mode = 0644,
		.show = _scratch_show_str,
		.store = _scratch_store_str,
	},
	.reg_offset = SCRATCH_F(chassis_pid),
	.map = 0,
	.len = 20,
};

static struct scratchram_attribute chassis_serial_number = {
	.attr = {
		.attr.name = "serial_number",
		.attr.mode = 0644,
		.show = _scratch_show_str,
		.store = _scratch_store_str,
	},
	.reg_offset = SCRATCH_F(chassis_sn),
	.map = 0,
	.len = 12,
};

static struct scratchram_attribute idprom_info_valid = {
	.attr = {
		.attr.name = "info_valid",
		.attr.mode = 0644,
		.show = _scratch_show_u32,
		.store = _scratch_store_u32,
	},
	.reg_offset = SCRATCH_F(idprom_info_valid),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute idprom_pd_type = {
	.attr = {
		.attr.name = "pd_type",
		.attr.mode = 0644,
		.show = _scratch_show_u32,
		.store = _scratch_store_u32,
	},
	.reg_offset = SCRATCH_F(idprom_pd_type),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute idprom_hw_version = {
	.attr = {
		.attr.name = "hw_version",
		.attr.mode = 0644,
		.show = _scratch_show_hw_ver,
		.store = _scratch_store_hw_ver,
	},
	.reg_offset = SCRATCH_F(idprom_hw_version),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute idprom_tan_version = {
	.attr = {
		.attr.name = "tan_version",
		.attr.mode = 0644,
		.show = _scratch_show_u32,
		.store = _scratch_store_u32,
	},
	.reg_offset = SCRATCH_F(idprom_tan_version),
	.map = 0,
	.len = 0,
};

static struct scratchram_attribute idprom_pid = {
	.attr = {
		.attr.name = "pid",
		.attr.mode = 0644,
		.show = _scratch_show_str,
		.store = _scratch_store_str,
	},
	.reg_offset = SCRATCH_F(idprom_pid),
	.map = 0,
	.len = 20,
};

static struct scratchram_attribute idprom_serial_number = {
	.attr = {
		.attr.name = "serial_number",
		.attr.mode = 0644,
		.show = _scratch_show_str,
		.store = _scratch_store_str,
	},
	.reg_offset = SCRATCH_F(idprom_sn),
	.map = 0,
	.len = 12,
};

static ssize_t
platform_type_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	char *bufp = buf;
	size_t len = PAGE_SIZE;
	static const char *_platform_type[REG_LIMIT(XIL_STATUS0_PLATFORM_ID)] = {
		[xil_status0_platform_id__FIXED] = "fixed",
		[xil_status0_platform_id__DISTRIBUTED] = "distributed",
		[xil_status0_platform_id__CENTRAL] = "centralized",
	};
	ssize_t err;
	u32 data;

	if (r) {
		err = regmap_read(r, F(status0), &data);
		if (!err) {
			uint32_t v = REG_GET(XIL_STATUS0_PLATFORM_ID, data);
			const char *platform_type = _platform_type[v];

			if (platform_type)
				err = scnprintf(bufp, len, "%s\n", platform_type);
			else
				err = scnprintf(bufp, len, "%u: unknown\n", v);
		}
	} else
		err = -ENXIO;

	return err;
}
static DEVICE_ATTR_RO(platform_type);

static ssize_t
card_type_show(struct device *dev,
	       struct device_attribute *attr,
	       char *buf)
{
	struct regmap *r = dev_get_regmap(dev, NULL);
	char *bufp = buf;
	size_t len = PAGE_SIZE;
	static const char *_fpga_id_distributed[REG_LIMIT(XIL_STATUS0_FPGA_ID)] = {
		[xil_status0_fpga_id__DISTRIBUTED_RP_PEMBREY] = "RP",
		[xil_status0_fpga_id__DISTRIBUTED_RP_ZENITH] = "RP:Zenith",
		[xil_status0_fpga_id__DISTRIBUTED_EXETER_GAUNTLET] = "LC:Exeter:Gauntlet",
		[xil_status0_fpga_id__DISTRIBUTED_EXETER_CORSAIR] = "LC:Exeter:Corsair",
		[xil_status0_fpga_id__DISTRIBUTED_EXETER_DAUNTLESS] = "LC:Exeter:Dauntless",
		[xil_status0_fpga_id__DISTRIBUTED_KENLEY_GAUNTLET] = "LC:Kenley:Gauntlet",
		[xil_status0_fpga_id__DISTRIBUTED_KENLEY_CORSAIR] = "LC:Kenley:Corsair",
		[xil_status0_fpga_id__DISTRIBUTED_KIRKWALL_VANGUARD] = "LC:Kirkwall:Vanguard",
		[xil_status0_fpga_id__DISTRIBUTED_KIRKWALL_LANCER] = "LC:Kirkwall:Lancer",
		[xil_status0_fpga_id__DISTRIBUTED_REDCLIFF_DAUNTLESS] = "LC:Redcliff:Dauntless",
		[xil_status0_fpga_id__DISTRIBUTED_WARMWELL] = "FT:Warmwell",
		[xil_status0_fpga_id__DISTRIBUTED_FABRIC] = "FC",
		[xil_status0_fpga_id__DISTRIBUTED_FABRIC_FOWLMERE] = "FC:Fowlmere",
	};
	static const char *_fpga_id_fixed[REG_LIMIT(XIL_STATUS0_FPGA_ID)] = {
		[xil_status0_fpga_id__FIXED_BMC_FPGA] = "RP:Fixed [BMC]",
		[xil_status0_fpga_id__FIXED_X86_FPGA] = "RP:Fixed [X86]",
		[xil_status0_fpga_id__FIXED_IOFPGA_SHERMAN] = "RP:Fixed [Sherman]",
		[xil_status0_fpga_id__FIXED_IOFPGA_KANGAROO] = "RP:Fixed [Kangaroo]",
		[xil_status0_fpga_id__FIXED_IOFPGA_PERSHING_BASE] = "RP:Fixed [Pershing:Base]",
		[xil_status0_fpga_id__FIXED_IOFPGA_PERSHING_MEZZ] = "RP:Fixed [Pershing:Mezzanine]",
		[xil_status0_fpga_id__FIXED_IOFPGA_CHURCHILL] = "RP:Fixed [Churchill]",
		[xil_status0_fpga_id__FIXED_IOFPGA_VALENTINE] = "RP:Fixed [Valentine]",
		[xil_status0_fpga_id__FIXED_IOFPGA_MATILDA_32] = "RP:Fixed [Matilda_32]",
		[xil_status0_fpga_id__FIXED_IOFPGA_MATILDA_64] = "RP:Fixed [Matilda_64]",
		[xil_status0_fpga_id__FIXED_IOFPGA_CROCODILE] = "RP:Fixed [Crocodile]",
		[xil_status0_fpga_id__FIXED_IOFPGA_ELMDON] = "RP:Fixed [Elmdon]",
	};
	static const char *_fpga_id_central[REG_LIMIT(XIL_STATUS0_FPGA_ID)] = {
		[xil_status0_fpga_id__CENTRAL_ALTUS] = "ALTUS",
		[xil_status0_fpga_id__CENTRAL_KOBLER] = "KOBLER",
		[xil_status0_fpga_id__CENTRAL_BFISH] = "BFISH",
		[xil_status0_fpga_id__CENTRAL_CYCLONUS] = "CYCLONUS",
	};
	u32 data;
	ssize_t err;

	if (r) {
		err = regmap_read(r, F(status0), &data);
		if (!err) {
			u32 id = REG_GET(XIL_STATUS0_FPGA_ID, data);
			u32 platform = REG_GET(XIL_STATUS0_PLATFORM_ID, data);
			const char *card = NULL;
			const char *unknown;

			if (platform == REG_CONST(XIL_STATUS0_PLATFORM_ID, FIXED)) {
				card = _fpga_id_fixed[id];
				unknown = "RP:Fixed [%u:unknown]\n";
			} else if (platform == REG_CONST(XIL_STATUS0_PLATFORM_ID, DISTRIBUTED)) {
				card = _fpga_id_distributed[id];
				unknown = "[distributed:%u:unknown]\n";
			} else if (platform == REG_CONST(XIL_STATUS0_PLATFORM_ID, CENTRAL)) {
				card = _fpga_id_central[id];
				unknown = "[centralized:%u:unknown]\n";
			} else
				unknown = "[unknown:%u:unknown]\n";
			if (card)
				err = scnprintf(bufp, len, "%s\n", card);
			else
				err = scnprintf(bufp, len, unknown, id);
		}
	} else
		err = -ENXIO;

	return err;
}
static DEVICE_ATTR_RO(card_type);

static CISCO_ATTR_U32_RW_HEX(cfg0, F(cfg0));
static CISCO_ATTR_U32_RW_HEX(cfg1, F(cfg1));
static CISCO_ATTR_U32_RW_HEX(cfg2, F(cfg2));
static CISCO_ATTR_U32_RW_HEX(cfg3, F(cfg3));
static CISCO_ATTR_U32_RW_HEX(cfg4, F(cfg4));
static CISCO_ATTR_U32_RW_HEX(cfg5, F(cfg5));
static CISCO_ATTR_U32_RW_HEX(cfg6, F(cfg6));
static CISCO_ATTR_U32_RW_HEX(cfg7, F(cfg7));

static CISCO_ATTR_U32_RO_HEX(status0, F(status0));
static CISCO_ATTR_U32_RO_HEX(status1, F(status1));
static CISCO_ATTR_U32_RO_HEX(status2, F(status2));
static CISCO_ATTR_U32_RO_HEX(status3, F(status3));
static CISCO_ATTR_U32_RO_HEX(status4, F(status4));
static CISCO_ATTR_U32_RO_HEX(status5, F(status5));
static CISCO_ATTR_U32_RO_HEX(status6, F(status6));
static CISCO_ATTR_U32_RO_HEX(status7, F(status7));

static struct attribute *_msd_xil_sys_attrs[] = {
	&dev_attr_platform_type.attr,
	&dev_attr_card_type.attr,
	&control.attr.attr,
	&cisco_attr_cfg0.attr.attr,
	&cisco_attr_cfg1.attr.attr,
	&cisco_attr_cfg2.attr.attr,
	&cisco_attr_cfg3.attr.attr,
	&cisco_attr_cfg4.attr.attr,
	&cisco_attr_cfg5.attr.attr,
	&cisco_attr_cfg6.attr.attr,
	&cisco_attr_cfg7.attr.attr,
	&cisco_attr_status0.attr.attr,
	&cisco_attr_status1.attr.attr,
	&cisco_attr_status2.attr.attr,
	&cisco_attr_status3.attr.attr,
	&cisco_attr_status4.attr.attr,
	&cisco_attr_status5.attr.attr,
	&cisco_attr_status6.attr.attr,
	&cisco_attr_status7.attr.attr,
	NULL,
};
static struct attribute *_msd_xil_scratch_bios_sys_attrs[] = {
	&bios_boot_mode.attr.attr,
	&bios_running_version.attr.attr,
	&bios_flash_select.attr.attr,
	NULL,
};
static struct attribute *_msd_xil_scratch_uboot_sys_attrs[] = {
	&uboot_running_version.attr.attr,
	&uboot_mac_addr.attr.attr,
	NULL,
};
static struct attribute *_msd_xil_scratch_chassis_sys_attrs[] = {
	&chassis_info_valid.attr.attr,
	&chassis_pd_type.attr.attr,
	&chassis_hw_version.attr.attr,
	&chassis_pid.attr.attr,
	&chassis_serial_number.attr.attr,
	&chassis_rack_id.attr.attr,
	NULL,
};
static struct attribute *_msd_xil_scratch_idprom_sys_attrs[] = {
	&idprom_info_valid.attr.attr,
	&idprom_pd_type.attr.attr,
	&idprom_hw_version.attr.attr,
	&idprom_tan_version.attr.attr,
	&idprom_pid.attr.attr,
	&idprom_serial_number.attr.attr,
	NULL,
};
const struct attribute_group cisco_fpga_msd_xil_attr_group = {
	.name = NULL,
	.attrs = _msd_xil_sys_attrs,
};
EXPORT_SYMBOL(cisco_fpga_msd_xil_attr_group);

const struct attribute_group cisco_fpga_msd_xil_scratch_chassis_attr_group = {
	.name = "chassis",
	.attrs = _msd_xil_scratch_chassis_sys_attrs,
};
EXPORT_SYMBOL(cisco_fpga_msd_xil_scratch_chassis_attr_group);

const struct attribute_group cisco_fpga_msd_xil_scratch_idprom_attr_group = {
	.name = "idprom",
	.attrs = _msd_xil_scratch_idprom_sys_attrs,
};
EXPORT_SYMBOL(cisco_fpga_msd_xil_scratch_idprom_attr_group);

const struct attribute_group cisco_fpga_msd_xil_scratch_bios_attr_group = {
	.name = "bios",
	.attrs = _msd_xil_scratch_bios_sys_attrs,
};
EXPORT_SYMBOL(cisco_fpga_msd_xil_scratch_bios_attr_group);

const struct attribute_group cisco_fpga_msd_xil_scratch_uboot_attr_group = {
	.name = "uboot",
	.attrs = _msd_xil_scratch_uboot_sys_attrs,
};
EXPORT_SYMBOL(cisco_fpga_msd_xil_scratch_uboot_attr_group);
