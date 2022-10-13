/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cisco sysfs utilities
 *
 * Copyright (c) 2020, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#if !defined(_CISCO_SYSFS_H)
#define _CISCO_SYSFS_H

#include <linux/platform_device.h>

#define SYSFS_MAX_DATA  (2)

struct sysfs_ext_attribute;
typedef ssize_t (sysfs_ext_attribute_fmt_fn_t)(
		    const struct sysfs_ext_attribute *,
		    char *buf, ssize_t buflen,
		    const u32 *data, size_t data_dim);
typedef ssize_t (sysfs_ext_attribute_parse_fn_t)(
		    const struct sysfs_ext_attribute *,
		    const char *buf, ssize_t buflen,
		    u32 *data, size_t data_dim);

struct sysfs_ext_attribute_bit_store_table {
	const char *match;
	size_t match_len;
	u32 mask;
	u32 value;
};
#define STORE_TABLE_ENTRY(str, v)		\
	{					\
		.match = str,			\
		.match_len = sizeof(str) - 1,	\
		.mask = v,			\
		.value = v,			\
	}

#define CISCO_SYSFS_ATTR_F_HEX    0x0001
#define CISCO_SYSFS_ATTR_F_64     0x0002
#define CISCO_SYSFS_ATTR_F_MASKED 0x0004

#define CISCO_SYSFS_U32_MASK      0xffffffff

#define CISCO_SYSFS_REG_NOT_PRESENT 0xffffffff

struct sysfs_ext_attribute {
	struct device_attribute attr;
	sysfs_ext_attribute_parse_fn_t *parse_fn;
	sysfs_ext_attribute_fmt_fn_t *fmt_fn;
	u32 flags;
	const struct sysfs_ext_attribute_bit_store_table *store_table;
	size_t store_table_dim;
	u32 reg[SYSFS_MAX_DATA];
	u32 mask[SYSFS_MAX_DATA];
};
extern ssize_t
cisco_fpga_sysfs_show(struct device *dev, struct device_attribute *dattr,
		      char *buf);
extern ssize_t
cisco_fpga_sysfs_store(struct device *dev, struct device_attribute *dattr,
		       const char *buf, size_t buflen);
extern ssize_t
cisco_fpga_sysfs_store_table(struct device *dev, struct device_attribute *dattr,
			     const char *buf, size_t buflen);

#define CISCO_ATTR_U32_RW_F(_name, _f, _reg, _m) \
	struct sysfs_ext_attribute cisco_attr_##_name = { \
		.attr = __ATTR(_name, 0644, cisco_fpga_sysfs_show, cisco_fpga_sysfs_store), \
		.reg = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_REG_NOT_PRESENT, \
			[0] = (_reg), \
		}, \
		.mask = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_U32_MASK, \
			[0] = (_m), \
		}, \
		.flags = (_f), \
	}
#define CISCO_ATTR_U32_RO_F(_name, _f, _reg, _m) \
	struct sysfs_ext_attribute cisco_attr_##_name = { \
		.attr = __ATTR(_name, 0444, cisco_fpga_sysfs_show, NULL), \
		.reg = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_REG_NOT_PRESENT, \
			[0] = (_reg), \
		}, \
		.mask = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_U32_MASK, \
			[0] = (_m), \
		}, \
		.flags = (_f), \
	}

/*
 * Generic r/w access of a single register.
 * Output format is hex.
 */
#define CISCO_ATTR_U32_RW_HEX(_name, _reg) \
	CISCO_ATTR_U32_RW_F(_name, CISCO_SYSFS_ATTR_F_HEX, _reg, \
			    CISCO_SYSFS_U32_MASK)
#define CISCO_ATTR_U32_RO_HEX(_name, _reg) \
	CISCO_ATTR_U32_RW_F(_name, CISCO_SYSFS_ATTR_F_HEX, _reg, \
			    CISCO_SYSFS_U32_MASK)

/*
 * Generic r/w access of a single register.
 * Output format is decimal.
 */
#define CISCO_ATTR_U32_RW(_name, _reg) \
	CISCO_ATTR_U32_RW_F(_name, 0, _reg, CISCO_SYSFS_U32_MASK)
#define CISCO_ATTR_U32_RO(_name, _reg) \
	CISCO_ATTR_U32_RO_F(_name, 0, _reg, CISCO_SYSFS_U32_MASK)

/*
 * Generic r/w access of a single register.
 * input/output is user supplied.
 */
#define CISCO_ATTR_RW_F(_name, _f, _reg, _m) \
	struct sysfs_ext_attribute cisco_attr_##_name = { \
		.attr = __ATTR(_name, 0644, cisco_fpga_sysfs_show, cisco_fpga_sysfs_store), \
		.reg = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_REG_NOT_PRESENT, \
			[0] = (_reg), \
		}, \
		.mask = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_U32_MASK, \
			[0] = (_m), \
		}, \
		.flags = (_f), \
		.fmt_fn = _ ## _name ## _fmt, \
		.parse_fn = _ ## _name ## _parse, \
	}
#define CISCO_ATTR_RO_F(_name, _f, _reg, _m) \
	struct sysfs_ext_attribute cisco_attr_##_name = { \
		.attr = __ATTR(_name, 0444, cisco_fpga_sysfs_show, NULL), \
		.reg = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_REG_NOT_PRESENT, \
			[0] = (_reg), \
		}, \
		.mask = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_U32_MASK, \
			[0] = (_m), \
		}, \
		.flags = (_f), \
		.fmt_fn = _ ## _name ## _fmt, \
	}

#define CISCO_ATTR_RO(_name, _reg) \
	CISCO_ATTR_RO_F(_name, 0, _reg, CISCO_SYSFS_U32_MASK)
#define CISCO_ATTR_RW(_name, _reg) \
	CISCO_ATTR_RW_F(_name, 0, _reg, CISCO_SYSFS_U32_MASK)

/*
 * Generic r/w access of two registers.
 * input/output is user supplied.
 */
#define CISCO_ATTR_RW2_F(_name, _f, _reg0, _m0, _reg1, _m1) \
	struct sysfs_ext_attribute cisco_attr_##_name = { \
		.attr = __ATTR(_name, 0644, cisco_fpga_sysfs_show, cisco_fpga_sysfs_store), \
		.reg = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_REG_NOT_PRESENT, \
			[0] = (_reg0), \
			[1] = (_reg1), \
		}, \
		.mask = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_U32_MASK, \
			[0] = (_m0), \
			[1] = (_m1), \
		}, \
		.flags = (_f), \
		.fmt_fn = _ ## _name ## _fmt, \
		.parse_fn = _ ## _name ## _parse, \
	}
#define CISCO_ATTR_RO2_F(_name, _f, _reg0, _m0, _reg1, _m1) \
	struct sysfs_ext_attribute cisco_attr_##_name = { \
		.attr = __ATTR(_name, 0444, cisco_fpga_sysfs_show, NULL), \
		.reg = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_REG_NOT_PRESENT, \
			[0] = (_reg0), \
			[1] = (_reg1), \
		}, \
		.mask = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_U32_MASK, \
			[0] = (_m0), \
			[1] = (_m1), \
		}, \
		.flags = (_f), \
		.fmt_fn = _ ## _name ## _fmt, \
	}

#define CISCO_ATTR_RW2(_name, _reg0, _reg1) \
	CISCO_ATTR_RW2_F(_name, 0, \
			 _reg0, CISCO_SYSFS_U32_MASK, \
			 _reg1, CISCO_SYSFS_U32_MASK)
#define CISCO_ATTR_RO2(_name, _reg0, _reg1) \
	CISCO_ATTR_RO2_F(_name, 0, \
			 _reg0, CISCO_SYSFS_U32_MASK, \
			 _reg1, CISCO_SYSFS_U32_MASK)

/*
 * Generic r/w access of a single register.
 * input is table based.
 * output is user supplied.
 */
#define CISCO_ATTR_RW_TABLE(_name, _reg) \
	struct sysfs_ext_attribute cisco_attr_##_name = { \
		.attr = __ATTR(_name, 0644, cisco_fpga_sysfs_show, cisco_fpga_sysfs_store_table), \
		.reg = { \
			[1 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_REG_NOT_PRESENT, \
			[0] = _reg, \
		}, \
		.mask = { \
			[0 ... (SYSFS_MAX_DATA - 1)] = CISCO_SYSFS_U32_MASK, \
		}, \
		.flags = 0, \
		.fmt_fn = _ ## _name ## _fmt, \
		.store_table = _ ## _name ## _store_table, \
		.store_table_dim = ARRAY_SIZE(_ ## _name ## _store_table), \
	}

#endif /* !defined(_CISCO_SYSFS_H) */
