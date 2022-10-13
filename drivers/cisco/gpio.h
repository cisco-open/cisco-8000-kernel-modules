/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * gpio ip block register definitions
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef _CISCO_GPIO_H
#define _CISCO_GPIO_H

#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/irq.h>
#include <linux/gpio/driver.h>

#include "cisco/hdr.h"

struct gpio_io_t {
	uint32_t cfg_stat;	/*        0x0 - 0x4        */
	uint32_t set;		/*        0x4 - 0x8        */
	uint32_t clr;		/*        0x8 - 0xc        */
	uint32_t intr_data;	/*        0xc - 0x10       */
	uint32_t mem[4];	/*       0x10 - 0x20       */
};

struct gpio_regs_v5_t {
	struct regblk_hdr_t hdr;	/*        0x0 - 0x14       */
	char pad__0[0xc];	/*       0x14 - 0x20       */
	uint32_t cfg0;		/*       0x20 - 0x24       */
	uint32_t cfg1;		/*       0x24 - 0x28       */
	char pad__1[0x18];	/*       0x28 - 0x40       */
	struct gpio_io_t io[1022]; /*      0x40 - 0x8000     */
};

extern u8 m_reboot_type;

enum reboot_type_e {
	UNSET = 0,
	COLD_REBOOT = 1,
	FAST_REBOOT = 2,
	WARM_REBOOT = 3,
	MAX_REBOOT_TYPE = 4
};

#define REBOOT_TYPE_MASK GENMASK(1, 0)

#define GPIO_CFG0             gpio, cfg0,         raw, 31,  0, gpio_regs_v5_t
#define GPIO_CFG0_REMAPEN     gpio, cfg0,     remapEn,  1,  1, gpio_regs_v5_t
#define GPIO_CFG0_REMAPRDWREN gpio, cfg0, remapRdWrEn,  0,  0, gpio_regs_v5_t

#define GPIO_CFG1             gpio, cfg1,     raw, 31,  0, gpio_regs_v5_t
#define GPIO_CFG1_DLYDUR      gpio, cfg1,  dlyDur, 31, 16, gpio_regs_v5_t
#define GPIO_CFG1_FLTDUR      gpio, cfg1,  fltDur, 15,  0, gpio_regs_v5_t

#define GPIO_IO_CFG_STAT           gpio, io_cfg_stat,       raw, 31,  0, gpio_regs_v5_t
#define GPIO_IO_CFG_STAT_FUNCEN    gpio, io_cfg_stat,    funcEn, 31, 31, gpio_regs_v5_t
#define GPIO_IO_CFG_STAT_INTTYPE   gpio, io_cfg_stat,   intType, 30, 28, gpio_regs_v5_t
enum gpio_io_cfg_stat_intType_e {
	gpio_io_cfg_stat_intType__disabled = 0,
	gpio_io_cfg_stat_intType__level_active_high = 1,
	gpio_io_cfg_stat_intType__level_active_low = 2,
	gpio_io_cfg_stat_intType__positive_edge = 3,
	gpio_io_cfg_stat_intType__negative_edge = 4,
	gpio_io_cfg_stat_intType__any_edge = 5,
};
#define GPIO_IO_CFG_STAT_FITSEL    gpio, io_cfg_stat,    fitSel, 27, 26, gpio_regs_v5_t
enum gpio_io_cfg_stat_fitSel_e {
	gpio_io_cfg_stat_fitSel__disable = 0,
	gpio_io_cfg_stat_fitSel__invert  = 1,
	gpio_io_cfg_stat_fitSel__stuck_1 = 2,
	gpio_io_cfg_stat_fitSel__stuck_0 = 3,
};
#define GPIO_IO_CFG_STAT_TRIGGER   gpio, io_cfg_stat,   trigger, 25, 25, gpio_regs_v5_t
enum gpio_io_cfg_stat_trigger_e {
	gpio_io_cfg_stat_trigger__clear = 0,
	gpio_io_cfg_stat_trigger__trigger = 1,
};
#define GPIO_IO_CFG_STAT_DIR       gpio, io_cfg_stat,       dir, 24, 24, gpio_regs_v5_t
enum gpio_io_cfg_stat_dir_e {
	gpio_io_cfg_stat_dir__input = 0,
	gpio_io_cfg_stat_dir__output = 1,
};
#define GPIO_IO_CFG_STAT_INTMSI    gpio, io_cfg_stat,    intMSI, 23, 20, gpio_regs_v5_t
#define GPIO_IO_CFG_STAT_INFLTR    gpio, io_cfg_stat,   intFltr, 19, 12, gpio_regs_v5_t
#define GPIO_IO_CFG_STAT_DISOUTPUT gpio, io_cfg_stat, disOutput,  6,  6, gpio_regs_v5_t
enum gpio_io_cfg_stat_disoutput_e {
	gpio_io_cfg_stat_disOutput__enable = 0,
	gpio_io_cfg_stat_disOutput__tristate = 1,
};
#define GPIO_IO_CFG_STAT_INTENB    gpio, io_cfg_stat,    intEnb,  5,  5, gpio_regs_v5_t
enum gpio_io_cfg_stat_intenb_e {
	gpio_io_cfg_stat_intEnb__disable = 0,
	gpio_io_cfg_stat_intEnb__enable = 1,
};
#define GPIO_IO_CFG_STAT_OUTSTATE  gpio, io_cfg_stat,  outState,  4,  4, gpio_regs_v5_t
enum gpio_io_cfg_stat_outState_e {
	gpio_io_cfg_stat_outState__low = 0,
	gpio_io_cfg_stat_outState__high = 1,
};
#define GPIO_IO_CFG_STAT_INTSTATE  gpio, io_cfg_stat,  intState,  1,  1, gpio_regs_v5_t
#define GPIO_IO_CFG_STAT_INSTATE   gpio, io_cfg_stat,   inState,  0,  0, gpio_regs_v5_t
enum gpio_io_cfg_stat_inState_e {
	gpio_io_cfg_stat_inState__low = 0,
	gpio_io_cfg_stat_inState__high = 1,
};

#define GPIO_IO_SET           gpio, io_set,       raw, 31,  0, gpio_regs_v5_t
#define GPIO_IO_SET_DISOUTPUT gpio, io_set, disOutput,  6,  6, gpio_regs_v5_t
#define GPIO_IO_SET_INTENB    gpio, io_set,    intEnb,  5,  5, gpio_regs_v5_t
#define GPIO_IO_SET_OUTSTATE  gpio, io_set,  outState,  4,  4, gpio_regs_v5_t
#define GPIO_IO_SET_INTSTATE  gpio, io_set,  intState,  1,  1, gpio_regs_v5_t

#define GPIO_IO_CLR           gpio, io_clr,       raw, 31,  0, gpio_regs_v5_t
#define GPIO_IO_CLR_DISOUTPUT gpio, io_clr, disOutput,  6,  6, gpio_regs_v5_t
#define GPIO_IO_CLR_INTENB    gpio, io_clr,    intEnb,  5,  5, gpio_regs_v5_t
#define GPIO_IO_CLR_OUTSTATE  gpio, io_clr,  outState,  4,  4, gpio_regs_v5_t
#define GPIO_IO_CLR_INTSTATE  gpio, io_clr,  intState,  1,  1, gpio_regs_v5_t

#define GPIO_IO_INTR_DATA      gpio, io_intr_data,      raw, 31,  0, gpio_regs_v5_t
#define GPIO_IO_INTR_DATA_DATA gpio, io_intr_data,     data, 23,  0, gpio_regs_v5_t

#define GPIO_IO_MEM            gpio, io_mem,      raw, 31,  0, gpio_regs_v5_t
#define GPIO_IO_MEM_IS_GROUP   gpio, io_mem, is_group, 31, 31, gpio_regs_v5_t

#define GPIO_IO_MEM_GROUP_ID        gpio, io_mem,         group_id, 27, 16, gpio_regs_v5_t
#define GPIO_IO_MEM_GROUP_PIN_COUNT gpio, io_mem,  group_pin_count, 15,  8, gpio_regs_v5_t
#define GPIO_IO_MEM_GROUP_INSTANCE  gpio, io_mem,   group_instance,  7,  0, gpio_regs_v5_t

#define GPIO_IO_MEM_PIN_ID       gpio, io_mem,        pin_id, 30,  8, gpio_regs_v5_t
enum gpio_io_mem_pin_id_e {
	gpio_io_mem_pin_id__no_group = 0,
	gpio_io_mem_pin_id__unsupported = 0x7fffff,
};
#define GPIO_IO_MEM_PIN_INSTANCE gpio, io_mem, pint_instance,  7,  0, gpio_regs_v5_t

#define GPIO_MAX_GPIOS 1022

struct gpio_adapter_t {
	struct gpio_chip chip;
	struct irq_chip irqchip;
	struct device *dev;
	struct regmap *map;
	struct gpio_regs_v5_t __iomem *csr;
	int irq;
	u16 ngpio;
	uint16_t off[];
};

struct seq_file;
struct gpio_chip;
struct attribute_group;
struct platform_device;
extern int cisco_fpga_gpio_init(struct platform_device *pdev);
extern void cisco_fpga_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip);
extern const struct attribute_group *_gpio_attr_groups[];
extern struct gpio_io_t __iomem *cisco_fpga_gpio_io(struct gpio_chip *chip, unsigned int offset);
extern int gpio_ioread32(struct gpio_adapter_t *priv, void __iomem *iop, unsigned int *val);
extern int gpio_iowrite32(struct gpio_adapter_t *priv, unsigned int val, void __iomem *iop);

extern int m_debug;
#define DEBUG_RECORD_STATUS     0x0001
#define DEBUG_VERBOSE_INFO      0x0010
#define DEBUG_VERBOSE_WARN      0x0020
#define DEBUG_VERBOSE_ERR       0x0040

#define dbg_dev_info(dev, fmt...)			\
	do {						\
		if (m_debug & DEBUG_VERBOSE_INFO)	\
			dev_info(dev, ##fmt);		\
	} while (0)

#define dbg_dev_warn(dev, fmt...)			\
	do {						\
		if (m_debug & DEBUG_VERBOSE_WARN)	\
			dev_warn(dev, ##fmt);		\
	} while (0)

#define dbg_dev_err(dev, fmt...)			\
	do {						\
		if (m_debug & DEBUG_VERBOSE_ERR)	\
			dev_err(dev, ##fmt);		\
	} while (0)

#endif /* ndef _CISCO_GPIO_H */
