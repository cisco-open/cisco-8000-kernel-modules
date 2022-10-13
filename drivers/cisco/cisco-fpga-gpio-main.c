// SPDX-License-Identifier: GPL-2.0-only
/*
 * GPIO IP Block driver
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/sort.h>
#include <linux/bsearch.h>
#include <linux/string.h>
#include <linux/regmap.h>

#if KERNEL_VERSION(5, 6, 13) <= LINUX_VERSION_CODE
#include <linux/interrupt.h>
#endif /* KERNEL_VERSION(5, 6, 13) <= LINUX_VERSION_CODE */

#if KERNEL_VERSION(4, 9, 189) <= LINUX_VERSION_CODE
#include <linux/pinctrl/pinconf-generic.h>
#endif /* KERNEL_VERSION(4, 9, 189) <= LINUX_VERSION_CODE */

#include "cisco/reg_access.h"
#include "cisco/hdr.h"
#include "cisco/gpio.h"
#include "cisco/mfd.h"

#define DRIVER_NAME	"cisco-fpga-gpio"
#define DRIVER_VERSION	"1.1"

#define F(f) offsetof(struct gpio_regs_v5_t, f)

int m_debug;
module_param(m_debug, int, 0444);
MODULE_PARM_DESC(m_debug, "Debug level. 0=none");

static int m_ignore_group_id = 1;
module_param(m_ignore_group_id, int, 0444);
MODULE_PARM_DESC(m_debug, "Pin match algorithm: 1=ignore group id");

u8 m_reboot_type = UNSET;
module_param(m_reboot_type, byte, 0444);
MODULE_PARM_DESC(m_reboot_type, "Reboot Type: 1=cold-reboot/2=fast-reboot/3=warm-reboot");

struct gpio_io_t __iomem *
cisco_fpga_gpio_io(struct gpio_chip *chip,
		   unsigned int offset)
{
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	unsigned int pin;

	if (offset >= chip->ngpio)
		return ERR_PTR(-EINVAL);

	pin = priv->off[offset];
	if (pin >= GPIO_MAX_GPIOS)
		return ERR_PTR(-EINVAL);

	return &priv->csr->io[pin];
}

int
gpio_ioread32(struct gpio_adapter_t *priv, void __iomem *iop, unsigned int *val)
{
	int rc;
	unsigned int reg = iop - (void __iomem *)priv->csr;

	rc = regmap_read(priv->map, reg, val);
	if (rc < 0)
		dev_err(priv->dev, "regmap_read(%#x) failed; status %d\n", reg, rc);
	return rc;
}

int
gpio_iowrite32(struct gpio_adapter_t *priv, unsigned int val, void __iomem *iop)
{
	int rc;
	unsigned int reg = iop - (void __iomem *)priv->csr;

	rc = regmap_write(priv->map, reg, val);
	if (rc < 0)
		dev_err(priv->dev, "regmap_write(%#x) failed; status %d\n", reg, rc);
	return rc;
}

static int
_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(chip, offset);
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	int rc;
	uint32_t v;

	if (IS_ERR(iop))
		return PTR_ERR(iop);

	rc = gpio_ioread32(priv, &iop->cfg_stat, &v);
	if (rc)
		return rc;

	if (REG_GET(GPIO_IO_CFG_STAT_DIR, v) == REG_CONST(GPIO_IO_CFG_STAT_DIR, input))
		return (REG_GET(GPIO_IO_CFG_STAT_INSTATE, v) == REG_CONST(GPIO_IO_CFG_STAT_INSTATE, low)) ? 0 : 1;
	return (REG_GET(GPIO_IO_CFG_STAT_OUTSTATE, v) == REG_CONST(GPIO_IO_CFG_STAT_OUTSTATE, low)) ? 0 : 1;
}

/*
 *  Note logical operations have already been translated before we get here.
 */
static void
_gpio_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(chip, offset);
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);

	if (!IS_ERR(iop)) {
		if (value)
			gpio_iowrite32(priv, REG_SET(GPIO_IO_SET_OUTSTATE, 1),
				       &iop->set);
		else
			gpio_iowrite32(priv, REG_SET(GPIO_IO_CLR_OUTSTATE, 1),
				       &iop->clr);
	}
}

static int
_gpio_get_direction(struct gpio_chip *chip, unsigned int offset)
{
	struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(chip, offset);
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	int rc;
	uint32_t v;

	if (IS_ERR(iop))
		return PTR_ERR(iop);

	rc = gpio_ioread32(priv, &iop->cfg_stat, &v);
	if (rc)
		return rc;

	if (REG_GET(GPIO_IO_CFG_STAT_DIR, v) == REG_CONST(GPIO_IO_CFG_STAT_DIR, input))
		return GPIOF_DIR_IN;
	return GPIOF_DIR_OUT;
}

static int
_gpio_direction_input(struct gpio_chip *chip, unsigned int offset)
{
	struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(chip, offset);
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	int rc;
	uint32_t __iomem *r;
	uint32_t v;

	if (IS_ERR(iop))
		return PTR_ERR(iop);

	r = &iop->cfg_stat;
	rc = gpio_ioread32(priv, r, &v);
	if (rc)
		return rc;

	v = REG_REPLACEe(GPIO_IO_CFG_STAT_DIR, v, input);
	rc = gpio_iowrite32(priv, v, r);
	if (rc)
		return rc;


	rc = gpio_ioread32(priv, r, &v);
	if (rc)
		return rc;

	if (REG_GET(GPIO_IO_CFG_STAT_DIR, v) ==
	    REG_CONST(GPIO_IO_CFG_STAT_DIR, input))
		return 0;

	/*
	 * This is actually quite normal that we can't change direction
	 * from output to input.  But we still allow callers to retrieve
	 * the value that is being output, and we consider that as "input".
	 */
	return 0;
}

static int
_gpio_direction_output(struct gpio_chip *chip,
		       unsigned int offset, int value)
{
	struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(chip, offset);
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	int rc;
	uint32_t __iomem *r;
	uint32_t v;

	if (IS_ERR(iop))
		return PTR_ERR(iop);

	r = &iop->cfg_stat;
	rc = gpio_ioread32(priv, r, &v);
	if (rc)
		return rc;

	v = REG_REPLACEe(GPIO_IO_CFG_STAT_DIR, v, output);
	v = REG_REPLACE(GPIO_IO_CFG_STAT_OUTSTATE, v, !!value);
	v = REG_REPLACEe(GPIO_IO_CFG_STAT_DISOUTPUT, v, enable);
	////////
	// Polaris driver clears input state
	v = REG_REPLACEe(GPIO_IO_CFG_STAT_INSTATE, v, low);
	////////
	rc = gpio_iowrite32(priv, v, r);
	if (rc)
		return rc;

	rc = gpio_ioread32(priv, r, &v);
	if (rc)
		return rc;

	if (REG_GET(GPIO_IO_CFG_STAT_DIR, v) ==
	    REG_CONST(GPIO_IO_CFG_STAT_DIR, output))
		return 0;

	/*
	 * No one should be trying to write out an input-only pin.
	 * We return an error in such cases.
	 */
	dev_warn(chip->parent, "direction_output: offset %u; value %u fails (ignored)\n",
		    offset, value);
	return -EINVAL;
}

#if KERNEL_VERSION(4, 9, 189) < LINUX_VERSION_CODE

static int
_gpio_set_config(struct gpio_chip *chip, unsigned int offset, unsigned long config)
{
	struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(chip, offset);
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	int rc;
	uint32_t __iomem *r;
	uint32_t v;

	if (IS_ERR(iop))
		return PTR_ERR(iop);

	r = &iop->cfg_stat;
	rc = gpio_ioread32(priv, r, &v);
	if (rc)
		return rc;

	switch (pinconf_to_config_param(config)) {
	case PIN_CONFIG_DRIVE_PUSH_PULL:
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_DISOUTPUT, v, enable);
		break;
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_DISOUTPUT, v, tristate);
		break;
	default:
		return -ENOTSUPP;
	}
	return gpio_iowrite32(priv, v, r);
}
#else /* KERNEL_VERSION(4, 9, 189) >= LINUX_VERSION_CODE */

static int
_gpio_set_single_ended(struct gpio_chip *chip, unsigned int offset,
		       enum single_ended_mode config)
{
	struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(chip, offset);
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	int rc;
	uint32_t __iomem *r;
	uint32_t v;

	if (IS_ERR(iop))
		return PTR_ERR(iop);

	r = &iop->cfg_stat;
	rc = gpio_ioread32(priv, r, &v);
	if (rc)
		return rc;

	switch (config) {
	case LINE_MODE_PUSH_PULL:
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_DISOUTPUT, v, enable);
		break;
	case LINE_MODE_OPEN_DRAIN:
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_DISOUTPUT, v, tristate);
		break;
	default:
		return -ENOTSUPP;
	}
	return gpio_iowrite32(priv, v, r);
}

#endif /* KERNEL_VERSION(4, 9, 189) >= LINUX_VERSION_CODE */

#ifdef CONFIG_GPIOLIB_IRQCHIP
static int
_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(chip, hwirq);
	int rc;
	uint32_t __iomem *r;
	uint32_t v;

	if (IS_ERR(iop))
		return PTR_ERR(iop);


	r = &iop->cfg_stat;
	rc = gpio_ioread32(priv, r, &v);
	if (rc)
		return rc;

	v = REG_REPLACE(GPIO_IO_CFG_STAT_INTMSI, v,
			irqd_to_hwirq(irq_get_irq_data(priv->irq)));

	if (type & IRQ_TYPE_LEVEL_HIGH)
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, level_active_high);
	else if (type & IRQ_TYPE_LEVEL_LOW)
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, level_active_low);
	else if (type & IRQ_TYPE_EDGE_BOTH)
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, any_edge);
	else if (type & IRQ_TYPE_EDGE_RISING)
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, positive_edge);
	else if (type & IRQ_TYPE_EDGE_FALLING)
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, negative_edge);
	else
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, disabled);

	return gpio_iowrite32(priv, v, r);
}

static void
_gpio_irq_mask(struct irq_data *d)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(chip, hwirq);
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	int rc;

	if (!IS_ERR(iop)) {
		uint32_t __iomem *r = &iop->cfg_stat;
		uint32_t v;

		rc = gpio_ioread32(priv, r, &v);
		if (rc)
			return;
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTENB, v, disable);
		gpio_iowrite32(priv, v, r);
	}
}

static void
_gpio_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(d);
	irq_hw_number_t hwirq = irqd_to_hwirq(d);
	struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(chip, hwirq);
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	int rc;

	if (!IS_ERR(iop)) {
		uint32_t __iomem *r = &iop->cfg_stat;
		uint32_t v;

		rc = gpio_ioread32(priv, r, &v);
		if (rc)
			return;
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTENB, v, enable);
		gpio_iowrite32(priv, v, r);
	}
}

/*
 * This is a template copied used for each gpio controller.
 * The kernel writes into the associated irq_chip structure
 * so we cannot use the same structure for all controllers.
 */
static const struct irq_chip cisco_fpga_gpio_irq_chip = {
	.name		= "cisco-gpio",
	.irq_set_type	= _gpio_irq_set_type,
	.irq_mask       = _gpio_irq_mask,
	.irq_unmask	= _gpio_irq_unmask,
};

#if KERNEL_VERSION(5, 6, 13) > LINUX_VERSION_CODE
#define IRQ_STYLE 0
#else /* KERNEL_VERSION(5, 6, 13) <= LINUX_VERSION_CODE */
#define IRQ_STYLE 1
#endif /* KERNEL_VERSION(5, 6, 13) <= LINUX_VERSION_CODE */

#if IRQ_STYLE == 1

static void
_gpio_irq_handler(struct irq_desc *desc)
{
	struct gpio_chip *gc_chip = irq_desc_get_handler_data(desc);
	struct irq_chip *irq_chip = irq_desc_get_chip(desc);
	struct irq_domain *irq_domain = gc_chip->irq.domain;
	irq_hw_number_t hwirq;

	chained_irq_enter(irq_chip, desc);
	for (hwirq = 0; hwirq < gc_chip->ngpio; hwirq++) {
		struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(gc_chip, hwirq);
		struct gpio_adapter_t *priv = gpiochip_get_data(gc_chip);

		if (!IS_ERR(iop)) {
			uint32_t v;
			int rc = gpio_ioread32(priv, &iop->cfg_stat, &v);

			if (rc)
				continue;
			if (REG_GET(GPIO_IO_CFG_STAT_INTSTATE, v)) {
				unsigned int irq = irq_find_mapping(irq_domain, hwirq);

				rc = gpio_iowrite32(priv, REG_SET(GPIO_IO_CLR_INTSTATE, 1), &iop->clr);
				if (rc)
					continue;
				generic_handle_irq(irq);
			}
		}
	}
	chained_irq_exit(irq_chip, desc);
}

#else /* IRQ_STYLE != 1 */

static void
_gpio_irq_handler(struct irq_desc *desc)
{
	struct gpio_chip *gc_chip = irq_desc_get_handler_data(desc);
	struct irq_data *irq_data = irq_desc_get_irq_data(desc);
	struct irq_chip *irq_chip = irq_data_get_irq_chip(irq_data);
	irq_hw_number_t hwirq;

	for (hwirq = 0; hwirq < gc_chip->ngpio; hwirq++) {
		struct gpio_io_t __iomem *iop = cisco_fpga_gpio_io(gc_chip, hwirq);
		struct gpio_adapter_t *priv = gpiochip_get_data(gc_chip);

		if (!IS_ERR(iop)) {
			uint32_t v;
			int rc = gpio_ioread32(priv, &iop->cfg_stat, &v);

			if (!rc && REG_GET(GPIO_IO_CFG_STAT_INTSTATE, v)) {
#if KERNEL_VERSION(4, 9, 189) < LINUX_VERSION_CODE
				unsigned int irq = irq_find_mapping(gc_chip->irq.domain, hwirq);
#else /* KERNEL_VERSION(4, 9, 189) < LINUX_VERSION_CODE */
				unsigned int irq = irq_find_mapping(gc_chip->irqdomain, hwirq);
#endif /* KERNEL_VERSION(4, 9, 189) < LINUX_VERSION_CODE */

				rc = gpio_iowrite32(priv, REG_SET(GPIO_IO_CLR_INTSTATE, 1), &iop->clr);
				if (!rc)
					generic_handle_irq(irq);
			}
		}
	}
	irq_chip->irq_eoi(irq_data);
}
#endif /* IRQ_STYLE != 1 */
#endif /* def CONFIG_GPIOLIB_IRQCHIP */

static int
get_reboot_type(struct device *dev)
{
	/* read the gpio scratch to get the reboot type */
	struct regmap *r = dev_get_regmap(dev, NULL);
	u32 sw1;
	int err = regmap_read(r, F(hdr.sw1), &sw1);

	if (!err)
		m_reboot_type = sw1 & REBOOT_TYPE_MASK;
	return err;
}

static int
clear_reboot_type_gpio_scratch(struct device *dev)
{
	struct regmap *r = dev_get_regmap(dev, NULL);

	return regmap_update_bits(r, F(hdr.sw1), REBOOT_TYPE_MASK, 0);
}

static const char*
get_reboot_type_str(void)
{
	enum reboot_type_e reboot_type = m_reboot_type;

	BUG_ON(reboot_type >= MAX_REBOOT_TYPE);
	switch (reboot_type) {
	case COLD_REBOOT:
		return "cold-reboot";
	case FAST_REBOOT:
		return "fast-reboot";
	case WARM_REBOOT:
		return "warm-reboot";
	case UNSET:
	case MAX_REBOOT_TYPE:
		return "unset";
	}
	return "illegal";
}

static int
_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_adapter_t *priv;
	int e, irq;
	u16 ngpio;
	struct gpio_chip *chip;
	const char *label;
	const char **names = 0;
	bool init_descriptors;
	uintptr_t csr;
	static const struct regmap_config r_config = {
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.fast_io = false,
		.max_register = sizeof(struct gpio_regs_v5_t) - 1,
	};
	struct regmap *map;

	e = device_property_read_string_array(dev, "gpio-descriptors", NULL, 0);
	if (e < 0) {
		dbg_dev_info(dev, "no gpio-descriptors property");
		ngpio = GPIO_MAX_GPIOS;
		init_descriptors = 0;
	} else {
		init_descriptors = 1;
		if (e > GPIO_MAX_GPIOS) {
			dbg_dev_warn(dev, "too many entries (%d > %u)in gpio-descriptors property",
				     e, GPIO_MAX_GPIOS);
			ngpio = GPIO_MAX_GPIOS;
		} else {
			ngpio = e;
		}
	}

	e = cisco_fpga_mfd_init(pdev,
				sizeof(*priv) + (ngpio * sizeof(priv->off[0]))
					      + (ngpio * sizeof(*names)),
				&csr,
				&r_config);
	if (e) {
		dev_err(dev, "failed to instantiate regmap; status %d\n", e);
		return e;
	}
	priv = platform_get_drvdata(pdev);
	priv->csr = (typeof(priv->csr))csr;
	map = dev_get_regmap(dev, NULL);
	if (map == NULL) {
		dev_err(dev, "null regmap");
		return -ENODEV;
	}
	priv->map = map;
	priv->dev = dev;

	if (m_reboot_type >= MAX_REBOOT_TYPE)
		m_reboot_type = UNSET;

	/*
	 *  if module param is unset, use the gpio scratch to determine the
	 *  reboot type
	 */
	if (m_reboot_type == UNSET) {
		e = get_reboot_type(dev);
		if (e)
			dev_err(dev, "failed to get reboot type, status %d\n", e);
	}

	if (m_reboot_type == UNSET)
		/*
		 * This could happen if there are issue reading gpio scratch
		 * (or) this was a normal reboot and gpio scratch read returned 0
		 */
		m_reboot_type = COLD_REBOOT;

	dbg_dev_info(dev, "Reboot type %s\n", get_reboot_type_str());

	/* clear reboot type for the subsequent reboot */
	e = clear_reboot_type_gpio_scratch(dev);
	if (e)
		dev_warn(dev, "failed to clear gpio scratch, status %d\n", e);

	names = (typeof(names))&priv->off[ngpio];
	priv->ngpio = ngpio;

	irq = platform_get_irq_optional(pdev, 0);
	priv->irq = irq;
	priv->irqchip = cisco_fpga_gpio_irq_chip;

	if (init_descriptors) {
		e = cisco_fpga_gpio_init(pdev);
		if (e < 0) {
			dev_err(dev, "cisco_fpga_gpio_init failed; status %d\n", e);
			return e;
		}
	} else {
		for (e = 0; e < ngpio; ++e)
			priv->off[e] = e;
	}

	chip = &priv->chip;
	chip->owner = THIS_MODULE;
	e = device_property_read_string(dev, "gpio-chip-label", &label);
	if ((e < 0) || !label)  {
		dbg_dev_info(dev, "no gpio-chip-label; status %d", e);
		chip->label = dev_name(dev);
	} else {
		dbg_dev_info(dev, "gpio-chip-label %s", label);
		chip->label = devm_kstrdup(dev, label, GFP_KERNEL);
	}
	chip->parent = dev;
	chip->base = -1;
	chip->ngpio = ngpio;
	chip->get_direction = _gpio_get_direction;
	chip->direction_input = _gpio_direction_input;
	chip->direction_output = _gpio_direction_output;
#if KERNEL_VERSION(4, 9, 189) < LINUX_VERSION_CODE
	priv->chip.set_config = _gpio_set_config;
#else /* KERNEL_VERSION(4, 9, 189) >= LINUX_VERSION_CODE */
	priv->chip.set_single_ended = _gpio_set_single_ended;
#endif /* KERNEL_VERSION(4, 9, 189) >= LINUX_VERSION_CODE */
	chip->get = _gpio_get;
	chip->set = _gpio_set;
	chip->dbg_show = cisco_fpga_gpio_dbg_show;
	chip->names = names;

	/* Clear remapEn and remapRdWrEn */
	e = gpio_iowrite32(priv, 0, &priv->csr->cfg0);
	if (e)
		return e;

#if defined(CONFIG_GPIOLIB_IRQCHIP) && IRQ_STYLE == 1
	if (priv->irq >= 0) {
		struct gpio_irq_chip *girq = &chip->irq;

		/* Set up the GPIO irqchip */
		girq = &chip->irq;
		girq->chip = &priv->irqchip;
		girq->parent_handler = _gpio_irq_handler;
		girq->num_parents = 1;
		girq->parents = devm_kcalloc(dev, 1,
					     sizeof(*girq->parents),
					     GFP_KERNEL);
		if (!girq->parents)
			return -ENOMEM;
		girq->parents[0] = priv->irq;
		girq->default_type = IRQ_TYPE_NONE;
		girq->handler = handle_simple_irq;
	}
#endif /* defined(CONFIG_GPIOLIB_IRQCHIP) && IRQ_STYLE == 1 */

	e = devm_gpiochip_add_data(dev, chip, priv);
	if (e) {
		dev_err(dev, "devm_gpiochip_add_data failed; status %d\n", e);
		return e;
	}

#if defined(CONFIG_GPIOLIB_IRQCHIP) && IRQ_STYLE == 0
	if (priv->irq >= 0) {
		e = gpiochip_irqchip_add(chip, &priv->irqchip,
					 0, handle_simple_irq, IRQ_TYPE_NONE);
		if (e) {
			dev_err(dev, "gpiochip_irqchip_add failed; status %d\n", e);
			return e;
		}

		gpiochip_set_chained_irqchip(chip, &priv->irqchip,
					     irq, _gpio_irq_handler);
	}
#endif /* defined(CONFIG_GPIOLIB_IRQCHIP) && IRQ_STYLE == 0 */

	e = devm_device_add_groups(dev, _gpio_attr_groups);
	if (e < 0)
		dev_err(dev, "devm_device_add_groups failed; status %d\n", e);
	else
		dev_info(dev, "%s %s @ %p (%s)\n", DRIVER_NAME, DRIVER_VERSION,
			 priv->csr, get_reboot_type_str());
	return e;
}

static const struct platform_device_id cisco_fpga_gpio_id_table[] = {
	{ .name = "gpio-rp",	.driver_data = 1 },
	{ .name = "gpio-lc",	.driver_data = 0 },
	{ .name = "gpio-fc0",	.driver_data = 1 },
	{ .name = "gpio-fc1",	.driver_data = 1 },
	{ .name = "gpio-fc2",	.driver_data = 1 },
	{ .name = "gpio-fc3",	.driver_data = 1 },
	{ .name = "gpio-fc4",	.driver_data = 1 },
	{ .name = "gpio-fc5",	.driver_data = 1 },
	{ .name = "gpio-fc6",	.driver_data = 1 },
	{ .name = "gpio-fc7",	.driver_data = 1 },
	{ .name = "gpio-ft",	.driver_data = 1 },
	{ .name = "gpio",	.driver_data = 1 },
	{ .name = "gpio-pim1",	.driver_data = 1 },
	{ .name = "gpio-pim2",	.driver_data = 1 },
	{ .name = "gpio-pim3",	.driver_data = 1 },
	{ .name = "gpio-pim4",	.driver_data = 1 },
	{ .name = "gpio-pim5",	.driver_data = 1 },
	{ .name = "gpio-pim6",	.driver_data = 1 },
	{ .name = "gpio-pim7",	.driver_data = 1 },
	{ .name = "gpio-pim8",	.driver_data = 1 },
	{ },
};
MODULE_DEVICE_TABLE(platform, cisco_fpga_gpio_id_table);

static struct platform_driver cisco_fpga_gpio_driver = {
	.driver = {
		.name = DRIVER_NAME,
	},
	.probe    = _gpio_probe,
	.id_table = cisco_fpga_gpio_id_table,
};
module_platform_driver(cisco_fpga_gpio_driver);

MODULE_AUTHOR("Cisco Systems, Inc. <ospo-kmod@cisco.com>");
MODULE_DESCRIPTION("Cisco 8000 GPIO Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS(PLATFORM_MODULE_PREFIX DRIVER_NAME);
MODULE_VERSION(DRIVER_VERSION);
