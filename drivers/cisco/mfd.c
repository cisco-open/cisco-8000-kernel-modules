// SPDX-License-Identifier: GPL-2.0-only
/*
 * Cisco FPGA driver.
 *
 * Copyright (c) 2018, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/mfd/core.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/msi.h>
#include <linux/acpi.h>
#include <linux/regmap.h>
#include <linux/version.h>

#include <cisco/mfd.h>
#include <cisco/fpga.h>
#include <cisco/hdr.h>

#define IGNORE_UNKNOWN_CHILDREN 0

static u32 cisco_fpga_mfd_magic;

static struct cell_metadata *
_init_metadata(struct device *dev,
					u32 ncells,
					const struct resource *resource_template,
					void *pdata, size_t pdata_size,
					u32 debug)
{
	struct fwnode_handle *parent_fwnode;
	struct fwnode_handle *child_fwnode;
	struct child_metadata *meta_child;
	size_t size_per_block, total_size, fwnode_nchildren;
	char *limit;
	struct cell_metadata *meta;
	void (*dev_msg)(const struct device *dev, const char *fmt, ...) = _dev_info;
	u32 v;
	const char *default_suffix = NULL;

	if (debug & 2) {
#if defined(dev_err)
		/* dev_err is a macro referring to _dev_err */
		dev_msg = _dev_err;
#else /* !defined(dev_err) */
		/* dev_err is a function prototype */
		dev_msg = dev_err;
#endif /* !defined(dev_err) */
	}

	fwnode_nchildren = 0;
	fwnode_nchildren = device_get_child_node_count(dev);

	++ncells;
	size_per_block = sizeof(*meta->cells)
					+ sizeof(*meta->match)
					+ (2 * sizeof(*meta->res))
					+ PLATFORM_NAME_SIZE;
	total_size = (ncells * size_per_block)
					+ (fwnode_nchildren * sizeof(struct child_metadata))
					+ sizeof(*meta);
	meta = kzalloc(total_size, GFP_KERNEL);
	if (!meta)
		return ERR_PTR(-ENOMEM);

	if (ncells >= ARRAY_SIZE(meta->block_offset)) {
		if (debug)
			dev_err(dev, "bad num_blocks %u\n", ncells);
		kfree(meta);
		return ERR_PTR(-EINVAL);
	}

	/* MFD cells follow meta structure */
	meta->cells = (typeof(meta->cells))&meta->child[fwnode_nchildren];

	/* ACPI matches start right after cells */
	meta->match = (typeof(meta->match))&meta->cells[ncells];

	/* Followed by resources */
	meta->res = (typeof(meta->res))&meta->match[ncells];

	/* Followed by name buffer */
	meta->name_bufp = (typeof(meta->name_bufp))&meta->res[ncells * 2];

	limit = (char *)meta + total_size;
	BUG_ON(limit != (char *)(&meta->name_bufp[ncells * PLATFORM_NAME_SIZE]));

	meta->dev = dev;
	meta->default_id = PLATFORM_DEVID_AUTO;
	meta->ncells = 0;
	meta->max_cells = ncells;
	meta->nchildren = 0;
	meta->debug = debug;
	meta->intr = NULL;

	meta->resource_template = resource_template;

	meta->pdata = pdata;
	meta->pdata_size = pdata_size;

	meta->dev_msg = dev_msg;

#ifdef CONFIG_ACPI
	if (debug) {
		acpi_handle handle = ACPI_HANDLE(dev);

		if (handle) {
			struct acpi_buffer acpi_buffer = { ACPI_ALLOCATE_BUFFER, NULL };
			acpi_status status = acpi_get_name(handle, ACPI_FULL_PATHNAME, &acpi_buffer);

			if (ACPI_SUCCESS(status)) {
				dev_msg(dev, "%s", (char *)acpi_buffer.pointer);
				acpi_os_free(acpi_buffer.pointer);
			}
		}
	}
#endif /* def CONFIG_ACPI */

	parent_fwnode = dev_fwnode(dev);
	if (!parent_fwnode) {
		dev_msg(dev, "no fwnode\n");
		return meta;
	}

	/*
	 * Defaults from parent
	 */
	if (!fwnode_property_read_u32(parent_fwnode, "devid", &v))
		meta->default_id = v;
	else if (!fwnode_property_read_u32(parent_fwnode, "devid-none", &v) && v)
		meta->default_id = PLATFORM_DEVID_NONE;

	if (fwnode_property_read_string(parent_fwnode, "device-name-suffix", &default_suffix))
		default_suffix = NULL;

	meta_child = meta->child;
	fwnode_for_each_child_node(parent_fwnode, child_fwnode) {
		unsigned long long adr;
#ifdef CONFIG_ACPI
		acpi_status status;

		status = acpi_evaluate_integer(ACPI_HANDLE_FWNODE(child_fwnode), "_ADR", NULL, &adr);
		if (ACPI_SUCCESS(status))
#else /* ndef CONFIG_ACPI */
		int e;

		e = fwnode_property_read_u32(child_fwnode, "reg", &v);
		adr = v;
		if (e >= 0)
#endif /* ndef CONFIG_ACPI */
		{
			const char *name = NULL;

			if (!fwnode_property_read_u32(child_fwnode, "devid", &v))
				meta_child->id = v;
			else if (!fwnode_property_read_u32(child_fwnode, "devid-none", &v) && v)
				meta_child->id = PLATFORM_DEVID_NONE;
			else if (!fwnode_property_read_u32(child_fwnode, "devid-auto", &v) && v)
				meta_child->id = PLATFORM_DEVID_AUTO;
			else
				meta_child->id = meta->default_id;
			if (!fwnode_property_read_string(child_fwnode, "device-name", &name) && name)
				meta_child->name = name;
			else
				meta_child->name_suffix = default_suffix;

			if (!fwnode_property_read_u32(child_fwnode, "ignore-cell", &v) && v)
				meta_child->ignore_cell = 1;

			if (!fwnode_property_read_u32(child_fwnode, "block-id", &v) && (v < 256)) {
				meta_child->block_id = v;
				meta_child->have_block_id = 1;
			} else {
				meta_child->block_id = 0;
				meta_child->have_block_id = 0;
			}
			meta_child->adr = adr;
			++meta_child;
			++meta->nchildren;
		}
	}
	BUG_ON(meta->nchildren > fwnode_nchildren);
	if (debug)
		dev_msg(dev, "%u fwnode children\n", meta->nchildren);

	return meta;
}

static const struct cisco_fpga_blk {
	int   id;
	char  *name;
	char  *compatible;
	u8    num_irqs;
	u8    irq_set;
	u32   filter;
} cisco_fpga_blks[] = {
	/* Note: cisco-fpga- prefix removed because max length is 19 in id_table */
	{   7, "mdio", "cisco-fpga-mdio",       0, 0x00, CISCO_MFD_CELLS_FILTER_PCI
														| CISCO_MFD_CELLS_FILTER_REGMAP},
	{   8, "i2c-pex", "cisco-fpga-i2c-pex", 0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{  11, "spi", "cisco-fpga-spi",         1, 0x01, CISCO_MFD_CELLS_FILTER_PCI },
	//
	// WARNING: wdt interrupt line should not be shared, which implies
	//          there should only be a single instance of wdt on each
	//          parent irq domain
	//
	{ 25,  "led-ng", "cisco-fpga-led-ng",   0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{  33, "wdt", "cisco-fpga-wdt",         1, 0x02, CISCO_MFD_CELLS_FILTER_PCI },
	{  33, "wdt", "cisco-fpga-wdt",         0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{  34, "uxbar", "cisco-fpga-uxbar",     1, 0x04, CISCO_MFD_CELLS_FILTER_REGMAP
														| CISCO_MFD_CELLS_FILTER_PASSIVE_SLOT },
	{  35, "info", "cisco-fpga-info",       0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP
														| CISCO_MFD_CELLS_FILTER_PASSIVE_SLOT },
	{  37, "gpio", "cisco-fpga-gpio",       1, 0x08, CISCO_MFD_CELLS_FILTER_PCI },
	{  37, "gpio", "cisco-fpga-gpio",       0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{  50, "poller", "cisco-fpga-poller",   0, 0x00, CISCO_MFD_CELLS_FILTER_PCI },
	{  57, "xil", "cisco-fpga-xil",         0, 0x00, CISCO_MFD_CELLS_FILTER_PCI
														| CISCO_MFD_CELLS_FILTER_REGMAP
														| CISCO_MFD_CELLS_FILTER_PASSIVE_SLOT },
	{  59, "spi", "cisco-fpga-spi",         1, 0x10, CISCO_MFD_CELLS_FILTER_PCI },  // qspi
	{  72, "i2c-smb", "cisco-fpga-i2c",     0, 0x00, CISCO_MFD_CELLS_FILTER_PCI
														| CISCO_MFD_CELLS_FILTER_REGMAP },
	{  80, "i2c-ext", "cisco-fpga-i2c-ext", 0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{  89, "pseq", "cisco-fpga-pseq",       0, 0x00, CISCO_MFD_CELLS_FILTER_PCI
														| CISCO_MFD_CELLS_FILTER_REGMAP
														| CISCO_MFD_CELLS_FILTER_PASSIVE_SLOT },
	//
	// WARNING: p2pm-m interrupt line should not be shared, which implies
	//          there should only be a single instance of p2pm-m on each
	//          parent irq domain
	//
	{  93, "p2pm-m", "cisco-fpga-p2pm-m",   1, 0x20, CISCO_MFD_CELLS_FILTER_PCI },
	{  94, "p2pm-s", "cisco-fpga-p2pm-s",   0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP
														| CISCO_MFD_CELLS_FILTER_PASSIVE_SLOT },
	{  96, "pwm", "cisco-bmc-pwm",          0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{  98, "bmc-led", "cisco-bmc-led",      0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{  99, "msd", "cisco-fpga-msd",         0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{ 104, "fs", "cisco-fpga-fs",           0, 0x00, CISCO_MFD_CELLS_FILTER_PCI
														| CISCO_MFD_CELLS_FILTER_PASSIVE_SLOT
														| CISCO_MFD_CELLS_FILTER_REGMAP },
	{ 105, "rptime", "cisco-fpga-rptime",   0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{ 111, "cspi", "cisco-fpga-cspi",       0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP
														| CISCO_MFD_CELLS_FILTER_PCI },
	{ 123, "misc-intrs", "cisco-fpga-misc-intrs", 0,    0, CISCO_MFD_CELLS_FILTER_REGMAP },
	{ 124, "bmc-uart", "cisco-bmc-uart",    0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{ 125, "lrstr", "cisco-fpga-lrstr",     0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP
														| CISCO_MFD_CELLS_FILTER_PASSIVE_SLOT },
	{ 132, "led", "cisco-fpga-led",         0, 0x00, CISCO_MFD_CELLS_FILTER_PCI },
	{ 133, "i2c-pex-tod", "cisco-fpga-i2c-pex-tod",  0, 0x00, CISCO_MFD_CELLS_FILTER_PCI },
	{ 138, "retimer-dl", "cisco-fpga-retimer-dl",    0, 0x00, CISCO_MFD_CELLS_FILTER_PCI
																| CISCO_MFD_CELLS_FILTER_REGMAP },
	{ 140, "bmc-p2pm-m-lite", "cisco-bmc-p2pm-m-lite", 0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{ 152, "pzctl",  "cisco-fpga-pzctl",    0, 0x00, CISCO_MFD_CELLS_FILTER_PCI
														| CISCO_MFD_CELLS_FILTER_PASSIVE_SLOT
														| CISCO_MFD_CELLS_FILTER_REGMAP },
	{ 166, "slpc-m", "cisco-fpga-slpc-m", 1, 0x40, CISCO_MFD_CELLS_FILTER_PCI |
													CISCO_MFD_CELLS_FILTER_REGMAP},
	{ 167, "slpc-s", "cisco-fpga-slpc-s", 0,    0, CISCO_MFD_CELLS_FILTER_REGMAP },
	{   0, 0, "",                                    0, 0x00, CISCO_MFD_CELLS_FILTER_PASSIVE_SLOT },
	{   0, "cisco-fpga-uio", "cisco-fpga-uio",       0, 0x00, CISCO_MFD_CELLS_FILTER_PCI },
	{   0, "cisco-bmc-uio", "cisco-bmc-uio",         0, 0x00, CISCO_MFD_CELLS_FILTER_REGMAP },
	{   0, "cisco-unknown-uio", "cisco-unknown-uio", 0, 0x00, 0 },
};

static const struct cisco_fpga_blk _irq_blk = { 38, "intr", 0, 0, 0 };

static const struct cisco_fpga_blk *
cisco_fpga_blk_match(int id, u32 filter, struct cisco_fpga_blk *scratch)
{
	const struct cisco_fpga_blk *blk = cisco_fpga_blks;

	while (blk->id) {
		if ((blk->id == id) && (blk->filter & filter))
			return blk;
		blk++;
	}
	while (blk->filter) {
		if (blk->filter & filter)
			break;
		blk++;
	}
	*scratch = *blk;
	scratch->id = id;

	return scratch;
}

static int
_fwnode_config(struct cell_metadata *meta,
				const struct cisco_fpga_blk *blk,
				unsigned long long absoff,
				unsigned long long nxtoff,
				int blk_id)
{
	struct child_metadata *child = meta->child;
	u32 i, irq_set;
	int irq;
	struct mfd_cell *cell;
	struct resource *res;

	if (meta->ncells >= meta->max_cells) {
		dev_err(meta->dev, "too many cells; reached limit of %u cells\n", meta->ncells);
		return 1; /* ignore */
	}
	res = meta->res;
	*res = *meta->resource_template;
	res->start = absoff;
	res->end = nxtoff - 1;

	if (blk->id == 38) { /* interrupt */
		BUG_ON(meta->intr != NULL); /* can only have one! */
		meta->intr = meta->res++;
		return 0;
	}

	cell = &meta->cells[meta->ncells];
	cell->num_resources = 1;
	cell->resources = meta->res;
	cell->acpi_match = meta->match;
	cell->platform_data = meta->pdata;
	cell->pdata_size = meta->pdata_size;
	cell->name = blk->name;

	/* cast away const... */
	*((unsigned long long *)&meta->match->adr) = absoff;

#if KERNEL_VERSION(5, 10, 0) < LINUX_VERSION_CODE
	/* cast away const... */
	*((u64 *)&cell->of_reg) = absoff;

	cell->of_compatible = blk->compatible;
	cell->use_of_reg = true;
#endif

	/*
	 * Note: meta->default_id was used earlier in initializing
	 *       the child->id field.  It is not to be used for
	 *       unknown children, since there could be duplicates.
	 */
	cell->id = PLATFORM_DEVID_AUTO;
	for (i = 0; i < meta->nchildren; ++i, ++child) {
		if (absoff == child->adr) {
			if (child->ignore_cell)
				return 1;

			if (child->have_block_id && (child->block_id != blk_id)) {
				dev_err(meta->dev, "Expected block_id %u @ offset %#llx; read block_id %u\n",
								child->block_id, (unsigned long long)child->adr, blk_id);
				return 1;
			}
			cell->id = child->id;

			if (child->name) {
				cell->name = child->name;
			} else if (child->name_suffix) {
				snprintf(meta->name_bufp, PLATFORM_NAME_SIZE, "%s%s",
				blk->name, child->name_suffix);
				meta->name_bufp[PLATFORM_NAME_SIZE - 1] = 0;
				cell->name = meta->name_bufp;
				meta->name_bufp += PLATFORM_NAME_SIZE;
			}
			//ACPI_COMPANION_SET(&pdev->dev, child);
			break;
		}
	}
	/*
	 * For unexpected cells, we choose to keep them with AUTO devid.
	 * We could also choose to simply return here, and treat them
	 * as if ignore_cell were configured.
	 */
	if (IGNORE_UNKNOWN_CHILDREN && i >= meta->nchildren)
		return 1;

	irq_set = blk->irq_set;
	for (irq = 0; irq < blk->num_irqs; irq++) {
		irq_hw_number_t hwirq;
		int bit = ffs(irq_set);

		if (!bit)
			break;

		hwirq = bit - 1;

		if (hwirq >= meta->max_irqs) {
			dev_err(meta->dev, "IRQ %u out of range [0, %u]", (int)hwirq, meta->max_irqs);
			break;
		}
		cell->num_resources++;
		++res;
		++meta->res;
		res->start = hwirq;
		res->end = res->start;
		res->flags = IORESOURCE_IRQ;
		irq_set &= ~BIT(hwirq);
	}
	// UIO always is AUTO
	if (!blk->id || (blk->name && strstr(blk->name, "-uio")))
		cell->id =  PLATFORM_DEVID_AUTO;

	meta->match++;
	meta->res++;
	meta->ncells++;

	return 0;
}

static int
_blkread(struct regmap *r, size_t reg, void *vdst, size_t len)
{
	u32 *dst = vdst;
	int e = 0;

	if (len & 3)
		return -EINVAL;

	while (len && !e) {
		e = regmap_read(r, reg, dst);
		reg += 4;
		++dst;
		len -= 4;
	}

	return e;
}

static int
_setup_max_irqs(struct device *dev, struct regmap *r,
				struct cell_metadata *meta, struct info_rom *info, u32 debug)
{
	u32 absoff = 0;
	u32 nxtoff = 0;
	int err;
	struct blkhdr hdr;

	for (int i = 0; i < info->num_blocks; i++) {
		absoff = nxtoff;
		nxtoff = absoff + (meta->block_offset[i] << 8);

		err = _blkread(r, absoff, &hdr, sizeof(hdr));
		if (err)
			return err;

		if (info->hdr.magic != CISCO_FPGA_MAGIC) {
			if (debug & 1)
				dev_err(dev, "setup_irq: bad magic %#x; expected %#x\n",
								info->hdr.magic, CISCO_FPGA_MAGIC);
			return -ENODEV;
		}
		if (hdr.id == 255) /* tail block */
			break;

		if (hdr.id == 38) { /* interrupt block */
			if (hdr.maj < 8)
				meta->max_irqs = CISCO_FPGA_MAX_IRQS_LT_V8;
			else
				meta->max_irqs = CISCO_FPGA_MAX_IRQS_V8;
			dev_info(dev, "max_irqs = %u (v%d.%d cell %u)",
						meta->max_irqs, hdr.maj, hdr.minorVer, hdr.id);
			return 0;
		}
	}
	dev_err(dev, "Missing INTR block");

	/* setting max irqs to 0 if interrupt ip-block is not available for FPGA */
	meta->max_irqs = 0;

	return 0;
}

struct cell_metadata *
cisco_fpga_mfd_cells(struct device *dev, struct regmap *r,
						const struct resource *resource_template,
						void *pdata, size_t pdata_size,
						u32 filter, u32 debug)
{
	struct info_rom info;
	int err;
	struct blkhdr hdr;
	struct cisco_fpga_blk scratch;
	const struct cisco_fpga_blk *blk;
	u32 absoff = 0, nxtoff;
	int i;
	u8 info_ver;
	struct cell_metadata *meta;

	err = _blkread(r, absoff, &info, sizeof(info));
	if (err)
		return ERR_PTR(err);

	if (info.hdr.magic != CISCO_FPGA_MAGIC) {
		if (debug & 1)
			dev_err(dev, "bad magic %#x; expected %#x\n",
						info.hdr.magic, CISCO_FPGA_MAGIC);
		return ERR_PTR(-ENODEV);
	}

	info_ver = info.hdr.maj;
	if (info_ver < 6) {
		/*
		 * Earlier versions did not have num_blocks and
		 * block offset table.
		 */
		info.num_blocks = ARRAY_SIZE(meta->block_offset) - 1;
	}

	meta = _init_metadata(dev, info.num_blocks, resource_template,
								pdata, pdata_size, debug);
	if (IS_ERR(meta))
		return meta;

	meta->dev_msg(dev, "%d.%d.%d-%d", info.rev_maj, info.rev_min,
								info.rev_dbg, info.build);

	if (info_ver >= 6) {
		err = _blkread(r, offsetof(struct info_rom, block_offset),
								meta->block_offset,
								sizeof(meta->block_offset));
		if (err) {
			kfree(meta);
			return ERR_PTR(err);
		}
	} else {
		meta->dev_msg(dev, "hdr %u.%u; probe method: scan@0x1000\n",
		info_ver, info.hdr.minorVer);
		for (i = 0; i < info.num_blocks; ++i) {
			/* presumption is that they are 0x1000 bytes apart */
			meta->block_offset[i] =  0x1000 >> 8;
		}
	}

	blk = cisco_fpga_blk_match(info.hdr.id, filter, &scratch);
	if (blk->id != 35) {
		dev_err(dev, "missing info_rom for id %u; filter %#x\n", info.hdr.id, filter);
		kfree(meta);
		return ERR_PTR(-ENXIO);
	}

	err = _setup_max_irqs(dev, r, meta, &info, debug);
	if (err) {
		kfree(meta);
		return ERR_PTR(err);
	}

	nxtoff = absoff + (meta->block_offset[0] << 8);
	(void)_fwnode_config(meta, blk, 0, nxtoff, blk->id);

	/*
	 * Build an MFD cell per block.
	 * Start at 1 because we have already processed the info_rom block above.
	 */
	for (i = 1; i < info.num_blocks; i++) {
		absoff = nxtoff;
		nxtoff = absoff + (meta->block_offset[i] << 8);

		err = _blkread(r, absoff, &hdr, sizeof(hdr));
		if (err) {
			kfree(meta);
			return ERR_PTR(err);
		}

		if (hdr.magic != CISCO_FPGA_MAGIC) {
			if (info_ver >= 6) {
				if (absoff != 0x20000) {
					dev_warn(dev, "bad block at 0x%08x magic:0x%08x",
									absoff, hdr.magic);
				} else {
					(void)_fwnode_config(meta, &_irq_blk, absoff, nxtoff, 38);
				}
			}
			continue;
		}

		if (hdr.id == 255) { /* tail block */
			if (debug & 4)
				meta->dev_msg(dev, "tail block @ %x", absoff);
			break;
		}

		if (hdr.id == 38) { /* interrupt block */
			(void)_fwnode_config(meta, &_irq_blk, absoff, nxtoff, hdr.id);
			continue;
		}

		/* regular block */
		blk = cisco_fpga_blk_match(hdr.id, filter, &scratch);
		if (!blk->name || _fwnode_config(meta, blk, absoff, nxtoff, hdr.id)) {
			/*
			 * Explicitly ignore this block. Generally used when we
			 * want to access a subset of cells on a remote card.
			 * The CPU on that card itself would be managing most
			 * of the cells.
			 */
			if (debug & 4)
				meta->dev_msg(dev, "skipping v%d.%d cell %u @ %x",
									hdr.maj, hdr.minorVer, hdr.id, absoff);
		} else {
			meta->dev_msg(dev, "%s v%d.%d cell %u @ %x",
			blk->name, hdr.maj, hdr.minorVer, hdr.id, absoff);
		}
	}

	/*
	 * Look for duplicate devices, and adjust id as necessary
	 */
	for (i = 0; i < meta->ncells; ++i) {
		int j;
		struct mfd_cell *cell = &meta->cells[i];
		int id = cell->id;

		if (cell->id == PLATFORM_DEVID_AUTO)
			continue;

		for (j = i + 1; j < meta->ncells; ++j) {
			struct mfd_cell *c = &meta->cells[j];

			if (strcmp(cell->name, c->name))
				continue;

			if (cell->id == c->id)
				c->id = id = PLATFORM_DEVID_AUTO;
		}
#ifdef NOTYET
		/*
		 * Seems we need to iterate drivers on platform bus,
		 * and then call driver_find_device?
		 */
		if (id != PLATFORM_DEVID_AUTO) {
			struct device *other_dev = driver_find_device(blah, blah, blah, match_fn);

			if (other_dev) {
				if (c->id == other_dev->id)
					id = PLATFORM_DEVID_AUTO;
				put_device(other_dev);
			}
		}
#endif /* NOTYET */
		if (cell->id != id) {
			dev_err(meta->dev, "%s.%u duplicates detected; using auto devid\n",
									cell->name, cell->id);
			cell->id = id;
		}
	}

	if (debug & 8) {
		struct mfd_cell *cell = meta->cells;

		for (i = 0; i < meta->ncells; ++i, ++cell)
			dev_err(dev, "cell %u: %s [%#llx: %#llx..%#llx]\n",
							i, cell->name,
							(unsigned long long)cell->resources[0].flags,
							(unsigned long long)cell->resources[0].start,
							(unsigned long long)cell->resources[0].end);
	}

	return meta;
}
EXPORT_SYMBOL(cisco_fpga_mfd_cells);

int
cisco_fpga_mfd_init(struct platform_device *pdev, size_t priv_size,
					uintptr_t *base, const struct regmap_config *r_configp)
{
	struct device *dev = &pdev->dev;
	struct device *parent = dev->parent;
	struct cisco_fpga_mfd *mfd = parent ? dev_get_drvdata(parent) : NULL;
	int e = -ENODEV;

	if (!parent)
		dev_err(dev, "device has no parent device\n");
	else if (!mfd)
		dev_err(dev, "parent %s has no private data\n",
						dev_name(dev->parent));
	else if (mfd->magic != &cisco_fpga_mfd_magic)
		dev_err(dev, "parent %s private data is corrupted\n",
						dev_name(dev->parent));
	else if (!mfd->init_regmap)
		dev_err(dev, "%s has no regmap initialization function\n",
						dev_name(dev->parent));
	else
		e = mfd->init_regmap(pdev, priv_size, base, r_configp);

	return e;
}
EXPORT_SYMBOL(cisco_fpga_mfd_init);

void
cisco_fpga_mfd_parent_init(struct device *dev, struct cisco_fpga_mfd *init,
							int (*init_regmap)(struct platform_device *pdev,
							size_t priv_size, uintptr_t *base,
							const struct regmap_config *r_configp))
{
	void *priv = dev_get_drvdata(dev);

	/* cisco_fpga_mfd must be the start of the private data block */
	BUG_ON(priv != (void *)init);

	init->magic = &cisco_fpga_mfd_magic;
	init->init_regmap = init_regmap;
}
EXPORT_SYMBOL(cisco_fpga_mfd_parent_init);
