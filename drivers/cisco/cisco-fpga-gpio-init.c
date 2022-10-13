// SPDX-License-Identifier: GPL-2.0-only
/*
 * GPIO IP Block driver
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#include <linux/module.h>
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

#if KERNEL_VERSION(4, 9, 189) >= LINUX_VERSION_CODE
#include <linux/pinctrl/pinconf-generic.h>
#endif

#include "cisco/reg_access.h"
#include "cisco/hdr.h"
#include "cisco/gpio.h"

#define GPIO_SIM_UNINITIALIZED 0xa5a5a5a5

struct k_element_t {
	u32 group_id;
	u32 pin_id;
	u16 offset;
	u16 count;
	const char *name;
};

static int
_k_cmp(const void *vkey, const void *velt)
{
	const struct k_element_t *key = (typeof(key))vkey;
	const struct k_element_t *elt = (typeof(elt))velt;

	return key->pin_id - elt->pin_id;
}

static void
_k_swap(void *va, void *vb, int s)
{
	struct k_element_t *a = (typeof(a))va;
	struct k_element_t *b = (typeof(b))vb;

	struct k_element_t tmp = *a;
	*a = *b;
	*b = tmp;
}

static struct k_element_t *
_gpio_find_pin(u32 group_id, u32 pin_id, struct k_element_t *k_table, size_t k_num)
{
	struct k_element_t search = {
		.group_id = group_id,
		.pin_id = pin_id,
		.offset = 0,
		.count = 0,
		.name = 0,
	};
	struct k_element_t *elt = (typeof(elt))bsearch(&search, k_table, k_num, sizeof(*k_table), _k_cmp);

	return elt;
}

static int
_gpio_parse_descriptor(struct platform_device *pdev, const char *d, const char **namep,
		       struct k_element_t *k_table, size_t k_num)
{
	const char *comma;
	int len;
	int group_id, pin_id, active_low, seen;
	int pin;
	int n;
	u32 v;
	struct device *dev = &pdev->dev;
	struct gpio_adapter_t *priv = platform_get_drvdata(pdev);
	struct gpio_io_t __iomem *iop = priv->csr->io;
	int rc;
	struct k_element_t *elt;

	comma = strchr(d, ',');
	if (!comma)
		return -EINVAL;

	len = comma - d;
	*namep = devm_kasprintf(dev, GFP_KERNEL, "%*.*s", len, len, d);
	d = comma + 1;

	n = sscanf(d, "%i,%i,%i,%n",
		   &group_id, &pin_id, &active_low, &seen);
	if (n < 3)
		return -EINVAL;

	elt = _gpio_find_pin(group_id, pin_id, k_table, k_num);
	if (elt) {
		pin = elt->offset;
		elt->count++;
		if (elt->name)
			dbg_dev_err(dev, "pin %s @ (%#x, %#x) cannot be renamed to %s",
				    elt->name, group_id, pin_id, *namep);
		else
			elt->name = *namep;
	} else {
		dbg_dev_info(dev, "unable to find GPIO pin %s @ (%#x, %#x)",
			     *namep, group_id, pin_id);
		return -ENOENT;
	}
	d += seen;
	rc = gpio_ioread32(priv, &iop[pin].cfg_stat, &v);
	if (rc)
		return rc;

	if (!strncmp(d, "in,", 3)) {
		int intEnb, intType;

		d += 3;
		n = sscanf(d, "%d,%n", &intEnb, &seen);
		if (n < 1)
			return -EINVAL;

		if (intEnb)
			v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTENB, v, enable);
		else
			v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTENB, v, disable);

		d += seen;
		if (!strcmp(d, "disable")) {
			v = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, disabled);
		} else if (!strcmp(d, "level-high")) {
			intType = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, level_active_high);
		} else if (!strcmp(d, "level-low")) {
			intType = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, level_active_low);
		} else if (!strcmp(d, "positive-edge")) {
			intType = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, positive_edge);
		} else if (!strcmp(d, "negative-edge")) {
			intType = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, negative_edge);
		} else if (!strcmp(d, "any-edge")) {
			intType = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, any_edge);
		} else {
			dev_err(dev, "unknown intType '%s' for pin '%s'", d, *namep);
			intType = REG_REPLACEe(GPIO_IO_CFG_STAT_INTTYPE, v, disabled);
		}
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_DIR, v, input);
	} else if (!strncmp(d, "out,", 4)) {
		d += 4;

		comma = strchr(d, ',');
		if (!comma)
			return -EINVAL;

		len = comma - d;
		if (!strncmp(d, "enable", len)) {
			v = REG_REPLACEe(GPIO_IO_CFG_STAT_DISOUTPUT, v, enable);
		} else if (!strncmp(d, "tristate", len)) {
			v = REG_REPLACEe(GPIO_IO_CFG_STAT_DISOUTPUT, v, tristate);
		} else {
			dev_err(dev, "unknown output '%.*s' for pin '%s'", len, d, *namep);
			v = REG_REPLACEe(GPIO_IO_CFG_STAT_DISOUTPUT, v, tristate);
		}
		d = comma + 1;
		if (!strcmp(d, "low")) {
			v = REG_REPLACEe(GPIO_IO_CFG_STAT_OUTSTATE, v, low);
		} else if (!strcmp(d, "high")) {
			v = REG_REPLACEe(GPIO_IO_CFG_STAT_OUTSTATE, v, high);
		} else {
			dev_err(dev, "unknown output state '%s' for pin '%s'",
				d, *namep);
			v = REG_REPLACEe(GPIO_IO_CFG_STAT_OUTSTATE, v, low);
		}
		v = REG_REPLACEe(GPIO_IO_CFG_STAT_DIR, v, output);
	}

	/* init gpio pins only for cold-reboot */
	if (m_reboot_type == COLD_REBOOT) {
		rc = gpio_iowrite32(priv, v, &iop[pin].cfg_stat);
		if (rc)
			return rc;
	}
	return pin;
}

int
cisco_fpga_gpio_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_adapter_t *priv = platform_get_drvdata(pdev);
	int e;
	u16 ngpio = priv->ngpio;
	struct gpio_io_t __iomem *iop = priv->csr->io;
	const char **desc = 0;
	const char **names = (typeof(names))&priv->off[ngpio];
	struct k_element_t *k_table;
	size_t k_num = 0;
	u32 group_id = 0;
	u32 pins = 0;
	int dump_table = 0;

	desc = devm_kzalloc(dev, sizeof(*desc) * ngpio, GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	e = device_property_read_string_array(dev, "gpio-descriptors", desc, ngpio);
	if (e < 0)
		return e;

	k_table = devm_kzalloc(dev, GPIO_MAX_GPIOS * sizeof(*k_table), GFP_KERNEL);
	if (!k_table)
		return -ENOMEM;

	for (e = 0; e < GPIO_MAX_GPIOS; ++e, ++iop) {
		u32 pin_id, pin_instance;
		u32 v;
		int rc = gpio_ioread32(priv, &iop->mem[0], &v);

		if (rc)
			return rc;

		if (v == GPIO_SIM_UNINITIALIZED)
			continue;

		if (REG_GET(GPIO_IO_MEM_IS_GROUP, v)) {
			if (pins)
				dbg_dev_warn(dev, "group %#x truncated @ index %u; %d pins remaining",
					     group_id, e, pins);
			group_id = REG_GET(GPIO_IO_MEM_GROUP_ID, v);
			pins = REG_GET(GPIO_IO_MEM_GROUP_PIN_COUNT, v);
			continue;
		}
		pin_id = REG_GET(GPIO_IO_MEM_PIN_ID, v);
		pin_instance = REG_GET(GPIO_IO_MEM_PIN_INSTANCE, v);
		if (!pins) {
			/*
			 * Unused pins that are not in a group may be marked as pin 0
			 */
			if (!pin_id || pin_id == REG_CONST(GPIO_IO_MEM_PIN_ID, unsupported))
				continue;
			dbg_dev_warn(dev, "ungrouped entry @ index %d; pin_id %#x:%u", e, pin_id, pin_instance);
			pins = 1;
		} else if (pin_id == REG_CONST(GPIO_IO_MEM_PIN_ID, unsupported)) {
			dbg_dev_info(dev, "pin %d [group %#x] is not supported", e, group_id);
			--pins;
			continue;
		} else if (pin_id == REG_CONST(GPIO_IO_MEM_PIN_ID, no_group)) {
			--pins;
			continue;
		}
		k_table[k_num].group_id = group_id;
		k_table[k_num].pin_id = v;  /* includes pin instance */
		k_table[k_num].offset = e;
		++k_num, --pins;
	}
	sort(k_table, k_num, sizeof(*k_table), _k_cmp, _k_swap);

	/*
	 * Walk the table looking for duplicates
	 */
	for (e = 1; e < k_num; ++e) {
		if (k_table[e - 1].pin_id != k_table[e].pin_id)
			continue;
		dbg_dev_err(dev, "duplicate pin %#x:%u @ indices %u and %u",
			    REG_GET(GPIO_IO_MEM_PIN_ID, k_table[e].pin_id),
			    REG_GET(GPIO_IO_MEM_PIN_INSTANCE, k_table[e].pin_id),
			    k_table[e - 1].offset,
			    k_table[e].offset);
		dump_table = 1;
	}

	for (e = 0; e < ngpio; ++e) {
		/*
		 * Note on failure, we mark the entry as invalid (value GPIO_MAX_GPIOS).
		 * We cannot simply skip the entry, since we use physical indices
		 * elsewhere within ACPI
		 */
		int index = _gpio_parse_descriptor(pdev, desc[e], &names[e], k_table, k_num);

		if (index < 0) {
			if (index == -EINVAL)
				dev_warn(dev, "Failed to parse gpio-descriptor '%s'", desc[e]);
			priv->off[e] = GPIO_MAX_GPIOS;
			dump_table = 1;
		} else {
			priv->off[e] = index;
		}
	}
	if (dump_table) {
		dbg_dev_info(dev, "found %zd pins", k_num);
		for (e = 0; e < k_num; ++e) {
			dbg_dev_info(dev, " [%u] %s @ (%#x, %#x:%u) @ %u; references %u", e,
				     k_table[e].name ? k_table[e].name : "unnamed",
				     k_table[e].group_id,
				     REG_GET(GPIO_IO_MEM_PIN_ID, k_table[e].pin_id),
				     REG_GET(GPIO_IO_MEM_PIN_INSTANCE, k_table[e].pin_id),
				     k_table[e].offset,
				     k_table[e].count);
		}
	}
	devm_kfree(dev, k_table);
	devm_kfree(dev, desc);
	return 0;
}
