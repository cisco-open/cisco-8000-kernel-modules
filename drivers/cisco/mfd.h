/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * mfd.h
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifndef CISCO_MFD_H_
#define CISCO_MFD_H_

#include <linux/kernel.h>     /* __iomem */
#include <linux/device.h>
#include <linux/acpi.h>

struct resource;
struct mfd_cell;
struct mfd_cell_acpi_match;
struct regmap;
struct regmap_config;

/*
 * This structure is passed in the parent of MFD cells.
 * Parent driver specific data may follow.
 */
struct cisco_fpga_mfd {
	u32 *magic;
	int (*init_regmap)(struct platform_device *pdev, size_t priv_size,
			   uintptr_t *base,
			   const struct regmap_config *r_configp);
};

struct child_metadata {
	unsigned long long	adr;
	int			id;
	const char		*name;
	const char		*name_suffix;
	u8			ignore_cell;
	u8			block_id;
	u8			have_block_id;
};
struct cell_metadata {
	struct device		*dev;
	int			default_id;
	u32			ncells;
	u32			max_cells;
	u32			debug;
	char			*name_bufp;
	struct mfd_cell		*cells;
	struct resource		*res;
	struct resource		*intr;
	const struct resource	*resource_template;
	struct mfd_cell_acpi_match *match;
	void			*pdata;
	size_t			pdata_size;
	void			(*dev_msg)(const struct device *dev,
					   const char *fmt, ...);
	u16			block_offset[256];
	u32			nchildren;
	u32			max_irqs;
	struct child_metadata	child[];
};

#define CISCO_MFD_CELLS_FILTER_PCI           0x1
#define CISCO_MFD_CELLS_FILTER_REGMAP        0x2
#define CISCO_MFD_CELLS_FILTER_PASSIVE_SLOT  0x4

extern struct cell_metadata *
cisco_fpga_mfd_cells(struct device *dev, struct regmap *r,
		     const struct resource *resource_template,
		     void *pdata, size_t pdata_size,
		     u32 filter, u32 debug);

extern int
cisco_fpga_mfd_init(struct platform_device *pdev, size_t priv_size,
		    uintptr_t *base,
		    const struct regmap_config *r_configp);

extern void
cisco_fpga_mfd_parent_init(struct device *dev,
			   struct cisco_fpga_mfd *init,
			   int (*init_regmap)(struct platform_device *pdev,
					      size_t priv_size, uintptr_t *base,
					      const struct regmap_config *r_configp));

#endif /* ndef CISCO_MFD_H_ */
