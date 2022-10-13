// SPDX-License-Identifier: GPL-2.0-only
/*
 * GPIO IP Block driver -- sysfs access
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/parser.h>
#include <linux/ctype.h>
#include <linux/seq_file.h>
#include <linux/gpio/driver.h>

#include "cisco/reg_access.h"
#include "cisco/hdr.h"
#include "cisco/gpio.h"
//#include "polaris/cisco-fpga-gpio.h"

static inline void
_byp(char **bufpp)
{
	char *bufp = *bufpp;

	while (isspace(*bufp))
		++bufp;
	*bufpp = bufp;
}

static inline void
_byp_num(char **bufpp)
{
	char *bufp = *bufpp;

	if (*bufp == '0') {
		if (_tolower(bufp[1]) == 'x') {
			bufp += 2;
			while (isxdigit(*bufp))
				++bufp;
		} else {
			while ((*bufp >= '0') && (*bufp <= '7'))
				++bufp;
		}
	} else {
		while (isdigit(*bufp))
			++bufp;
	}

	*bufpp = bufp;
}

void
cisco_fpga_gpio_dbg_show(struct seq_file *s,
			 struct gpio_chip *chip)
{
	struct gpio_adapter_t *priv = gpiochip_get_data(chip);
	uint32_t index;

	static const char * const _intType[REG_LIMIT(GPIO_IO_CFG_STAT_INTTYPE)] = {
	    [0] = "disable",
	    [1] = "level-high",
	    [2] = "level-low",
	    [3] = "positive-edge",
	    [4] = "negative-edge",
	    [5] = "any-edge",
	    [6] = "6",
	    [7] = "7",
	};
	static const char * const _fitSel[REG_LIMIT(GPIO_IO_CFG_STAT_FITSEL)] = {
	    [0] = "disable",
	    [1] = "invert",
	    [2] = "stuck-1",
	    [3] = "stuck-0",
	};
	static const char * const _trigger[REG_LIMIT(GPIO_IO_CFG_STAT_TRIGGER)] = {
	    [0] = "clear-fault",
	    [1] = "insert-fault",
	};
	static const char * const _dir[REG_LIMIT(GPIO_IO_CFG_STAT_DIR)] = {
	    [0] = "input",
	    [1] = "output",
	};
	static const char * const _disOutput[REG_LIMIT(GPIO_IO_CFG_STAT_DISOUTPUT)] = {
	    [0] = "enable",
	    [1] = "tristate",
	};
	static const char * const _intEnb[REG_LIMIT(GPIO_IO_CFG_STAT_INTENB)] = {
	    [0] = "disable",
	    [1] = "enable",
	};
	static const char * const _outState[REG_LIMIT(GPIO_IO_CFG_STAT_OUTSTATE)] = {
	    [0] = "low",
	    [1] = "high",
	};
	static const char * const _inState[REG_LIMIT(GPIO_IO_CFG_STAT_INSTATE)] = {
	    [0] = "low",
	    [1] = "high",
	};

	for (index = 0; index < chip->ngpio; ++index) {
		uint16_t offset = priv->off[index];
		// Value is index to FPGA GPIO block's io array in register map.
		struct gpio_io_t __iomem *io = cisco_fpga_gpio_io(chip, index);
		int rc;
		uint32_t v;
		uint32_t intr_data;
		uint32_t mem;
		const char *intType;
		const char *fitSel;
		const char *trigger;
		const char *dir;
		const char *disOutput;
		const char *intEnb;
		const char *outState;
		const char *inState;
		const char *state;

		if (IS_ERR(io))
			continue;

		rc = gpio_ioread32(priv, &io->cfg_stat, &v);
		if (rc)
			continue;

		rc = gpio_ioread32(priv, &io->intr_data, &intr_data);
		if (rc)
			continue;

		rc = gpio_ioread32(priv, &io->mem[0], &mem);
		if (rc)
			continue;

		intType = _intType[REG_GET(GPIO_IO_CFG_STAT_INTTYPE, v)];
		fitSel = _fitSel[REG_GET(GPIO_IO_CFG_STAT_FITSEL, v)];
		trigger = _trigger[REG_GET(GPIO_IO_CFG_STAT_TRIGGER, v)];
		dir = _dir[REG_GET(GPIO_IO_CFG_STAT_DIR, v)];
		disOutput = _disOutput[REG_GET(GPIO_IO_CFG_STAT_DISOUTPUT, v)];
		intEnb = _intEnb[REG_GET(GPIO_IO_CFG_STAT_INTENB, v)];
		outState = _outState[REG_GET(GPIO_IO_CFG_STAT_OUTSTATE, v)];
		inState = _inState[REG_GET(GPIO_IO_CFG_STAT_INSTATE, v)];

		if (REG_GET(GPIO_IO_CFG_STAT_DIR, v) == REG_CONST(GPIO_IO_CFG_STAT_DIR, input)) {
			state = inState;
			disOutput = NULL;
		} else {
			state = outState;
		}
		seq_printf(s, "- {index: %u, offset: %u", index, offset);

		if (chip->names && chip->names[index])
			seq_printf(s, ", name: %s", chip->names[index]);

		if (dir)
			seq_printf(s, ", dir: %s", dir);

		if (disOutput)
			seq_printf(s, ", output: %s", disOutput);

		if (state)
			seq_printf(s, ", state: %s", state);

		if (intEnb) {
			seq_printf(s, ", intEnb: %s", intEnb);
			if (intType)
				seq_printf(s, ", intType: %s", intType);
			seq_printf(s, ", intData: %#x, intMSI: %d, intPending: %d"
				    , intr_data
				    , REG_GET(GPIO_IO_CFG_STAT_INTMSI, v)
				    , REG_GET(GPIO_IO_CFG_STAT_INTSTATE, v));
		}
		if (fitSel)
			seq_printf(s, ", fitSel: %s", fitSel);

		if (trigger)
			seq_printf(s, ", trigger: %s", trigger);

		if (REG_GET(GPIO_IO_MEM_IS_GROUP, mem))
			seq_printf(s, ", group: %#x, group_instance: %#x, pin_count: %u"
				    , REG_GET(GPIO_IO_MEM_GROUP_ID, mem)
				    , REG_GET(GPIO_IO_MEM_GROUP_INSTANCE, mem)
				    , REG_GET(GPIO_IO_MEM_GROUP_PIN_COUNT, mem));
		else
			seq_printf(s, ", pin_id: %#x, pin_instance: %#x"
				    , REG_GET(GPIO_IO_MEM_PIN_ID, mem)
				    , REG_GET(GPIO_IO_MEM_PIN_INSTANCE, mem));
		seq_puts(s, "}\n");
	}
}

static ssize_t
config_store(struct device *dev,
	     struct device_attribute *attr,
	     const char *buf,
	     size_t buflen)
{
	struct gpio_adapter_t *priv = dev_get_drvdata(dev);
	struct gpio_chip *chip = &priv->chip;
	int token;
	unsigned long mask = 0;
	char *input, *base, *p;
	char *arg, *base_arg = NULL;
	substring_t args[MAX_OPT_ARGS];
	ssize_t result = 0;
	uint32_t cfg_stat;

#define TOKEN_index           0
#define TOKEN_intType         1
#define TOKEN_fitSel          2
#define TOKEN_trigger         3
#define TOKEN_dir             4
#define TOKEN_intMSI          5
#define TOKEN_disOutput       6
#define TOKEN_intEnb          7
#define TOKEN_outState        8
#define TOKEN_error           9
#define TOKEN_group_id       10
#define TOKEN_pin_count      11
#define TOKEN_group_instance 12
#define TOKEN_pin_id         13
#define TOKEN_pin_instance   14
#define TOKEN_name           15
#define TOKEN_MAX            16

#define _vE(_index, _block, _reg, _field, _hi, _lo, _type)[_index] = \
	{				\
		.field = # _field,	\
		.hi = _hi,		\
		.lo = _lo,		\
	}
#define _E(...) _vE(__VA_ARGS__)

	unsigned long value[TOKEN_MAX] = { 0 };
	char *name = 0;
	static const struct parameters {
		const char *field;
		uint8_t hi;
		uint8_t lo;
	} params[TOKEN_MAX] = {
		_E(TOKEN_intType, GPIO_IO_CFG_STAT_INTTYPE),
		_E(TOKEN_fitSel, GPIO_IO_CFG_STAT_FITSEL),
		_E(TOKEN_trigger, GPIO_IO_CFG_STAT_TRIGGER),
		_E(TOKEN_dir, GPIO_IO_CFG_STAT_DIR),
		_E(TOKEN_intMSI, GPIO_IO_CFG_STAT_INTMSI),
		_E(TOKEN_disOutput, GPIO_IO_CFG_STAT_DISOUTPUT),
		_E(TOKEN_intEnb, GPIO_IO_CFG_STAT_INTENB),
		_E(TOKEN_outState, GPIO_IO_CFG_STAT_OUTSTATE),
		_E(TOKEN_group_id, GPIO_IO_MEM_GROUP_ID),
		_E(TOKEN_pin_count, GPIO_IO_MEM_GROUP_PIN_COUNT),
		_E(TOKEN_group_instance, GPIO_IO_MEM_GROUP_INSTANCE),
		_E(TOKEN_pin_id, GPIO_IO_MEM_PIN_ID),
		_E(TOKEN_pin_instance, GPIO_IO_MEM_PIN_INSTANCE),
		[TOKEN_name] = { .field = "name" },
	};

	static const struct match_token _tokens[TOKEN_MAX] = {
		{ TOKEN_index,          "index:%u" },
		{ TOKEN_intType,        "intType:%s" },
		{ TOKEN_fitSel,         "fitSel:%s" },
		{ TOKEN_trigger,        "trigger:%s" },
		{ TOKEN_dir,            "dir:%s" },
		{ TOKEN_intMSI,         "intMSI:%s" },
		{ TOKEN_disOutput,      "output:%s" },
		{ TOKEN_intEnb,         "intEnb:%s" },
		{ TOKEN_outState,       "state:%s" },
		{ TOKEN_group_id,       "group:%u" },
		{ TOKEN_pin_count,      "pin_count:%u" },
		{ TOKEN_pin_instance,   "pin_instance:%u" },
		{ TOKEN_group_instance, "group_instance:%u" },
		{ TOKEN_pin_id,         "pin_id:%u" },
		{ TOKEN_name,           "name:%s" },
		{ TOKEN_error,          NULL },
	};
	static const struct match_token _intType[] = {
		{ 0, "disable" },
		{ 1, "level-high" },
		{ 2, "level-low" },
		{ 3, "positive-edge" },
		{ 4, "negative-edge" },
		{ 5, "any-edge" },
		{ -1, NULL },
	};
	static const struct match_token _fitSel[] = {
		{ 0, "disable" },
		{ 1, "invert" },
		{ 2, "stuck-1" },
		{ 3, "stuck-0" },
		{ -1, NULL },
	};
	static const struct match_token _trigger[] = {
		{ 0, "clear-fault" },
		{ 1, "insert-fault" },
		{ -1, NULL },
	};
	static const struct match_token _dir[] = {
		{ 0, "input" },
		{ 1, "output" },
		{ -1, NULL },
	};
	static const struct match_token _disOutput[] = {
		{ 0, "enable" },
		{ 1, "tristate" },
		{ -1, NULL },
	};
	static const struct match_token _intEnb[] = {
		{ 0, "disable" },
		{ 1, "enable" },
		{ -1, NULL },
	};
	static const struct match_token _outState[] = {
		{ 0, "low" },
		{ 1, "high" },
		{ -1, NULL },
	};
	static const struct match_token * const tokens[TOKEN_MAX] = {
		[TOKEN_index] = _tokens,
		[TOKEN_intType] = _intType,
		[TOKEN_fitSel] = _fitSel,
		[TOKEN_trigger] = _trigger,
		[TOKEN_dir] = _dir,
		[TOKEN_intMSI] = NULL,
		[TOKEN_disOutput] = _disOutput,
		[TOKEN_intEnb] = _intEnb,
		[TOKEN_outState] = _outState,
		[TOKEN_error] = NULL,
	};

	input = kstrdup(buf, GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	base = input;

	for (; (p = strsep(&input, ",\n")); ) {
		_byp(&p);
		if (!*p)
			continue;

		token = match_token(p, _tokens, args);
		if (token == TOKEN_error) {
			dev_err(dev, "%s: bad token\n", p);
			result = -EINVAL;
			break;
		}
		if (test_bit(token, &mask)) {
			dev_err(dev, "%s: token repeated\n", p);
			result = -EINVAL;
			break;
		}
		set_bit(token, &mask);
		kfree(base_arg);

		base_arg = arg = match_strdup(&args[0]);
		if (!base_arg) {
			dev_err(dev, "%s: out of memory\n", p);
			result = -ENOMEM;
			break;
		}
		_byp(&arg);
		switch (token) {
		case TOKEN_index:
		case TOKEN_intMSI:
		case TOKEN_group_id:
		case TOKEN_group_instance:
		case TOKEN_pin_count:
		case TOKEN_pin_id:
		case TOKEN_pin_instance: {
			unsigned long l;

			result = kstrtoul(arg, 0, &l);
			if (result < 0) {
				dev_err(dev, "%s: kstrotul(%s) failed\n", p, arg);
				break;
			}
			value[token] = l;
			_byp_num(&arg);
			break;
		}
		case TOKEN_name: {
			char *space;

			name = kstrdup(arg, GFP_KERNEL);
			if (!name) {
				result = -ENOMEM;
				break;
			}
			space = strchr(name, ' ');
			if (space)
				*space = 0;
			while (*arg && !isspace(*arg))
				++arg;
			break;
		}
		default: {
			int v;

			if (!tokens[token]) {
				dev_err(dev, "%s: internal error: no token table for token %u\n", p, token);
				result = -EINVAL;
				break;
			}

			v = match_token(arg, tokens[token], args);
			if (v < 0) {
				dev_err(dev, "%s: bad value %s\n", p, arg);
				result = -EINVAL;
				break;
			}
			value[token] = v;
			while (*arg && !isspace(*arg))
				++arg;
			break;
		}
		}
		if (result)
			break;
		_byp(&arg);
		if (*arg) {
			dev_err(dev, "%s: unexpected input %s\n", p, arg);
			result = -EINVAL;
			break;
		}
	}
	kfree(base_arg);
	kfree(base);
	while (!result) {
		struct gpio_io_t __iomem *io;
		int rc;
		uint32_t index, mem;

		result = buflen;
		if (!test_bit(TOKEN_index, &mask)) {
			dev_err(dev, "index is required\n");
			result = -EINVAL;
			break;
		}
		index = value[TOKEN_index];
		if (index >= chip->ngpio) {
			dev_err(dev, "index: %#x is out of range [0..%u]\n",
				index, chip->ngpio - 1);
			result = -EINVAL;
			break;
		}
		io = cisco_fpga_gpio_io(chip, index);
		if (IS_ERR(io)) {
			result = PTR_ERR(io);
			break;
		}
		rc = gpio_ioread32(priv, &io->cfg_stat, &cfg_stat);
		if (rc) {
			result = rc;
			break;
		}
		rc = gpio_ioread32(priv, &io->mem[0], &mem);
		if (rc) {
			result = rc;
			break;
		}
		if (test_bit(TOKEN_group_id, &mask) ||
		    test_bit(TOKEN_group_instance, &mask) ||
		    test_bit(TOKEN_pin_count, &mask)) {
			if (!REG_GET(GPIO_IO_MEM_IS_GROUP, mem)) {
				dev_err(dev, "Group match requested for Pin entry\n");
				result = -EINVAL;
				break;
			}
			if (test_bit(TOKEN_pin_id, &mask) ||
			    test_bit(TOKEN_pin_instance, &mask)) {
				dev_err(dev, "cannot specify both group and pin parameters\n");
				result = -EINVAL;
				break;
			}
		} else if (test_bit(TOKEN_pin_id, &mask) ||
			   test_bit(TOKEN_pin_instance, &mask)) {
			if (REG_GET(GPIO_IO_MEM_IS_GROUP, mem)) {
				dev_err(dev, "Pin match requested for Group entry\n");
				result = -EINVAL;
				break;
			}
		}
		for (token = TOKEN_error; ++token < TOKEN_name; ) {
			uint32_t m;

			if (!test_bit(token, &mask))
				continue;
			m = _reg_get(mem, params[token].hi, params[token].lo);
			if (m != value[token]) {
				dev_err(dev, "%s: mismatch; mem %#x; request %#lx\n",
					params[token].field, m, value[token]);
				result = -EINVAL;
				break;
			}
		}
		if (result < 0)
			break;

		for (token = 1; token < TOKEN_error; ++token) {
			uint32_t m;

			if (!test_bit(token, &mask))
				continue;
			m = _reg_mask(params[token].hi,
				      params[token].lo);
			if (value[token] > m) {
				dev_err(dev, "%s: %#lx is out of range [0..%u]\n",
					params[token].field,
					value[token],
					m);
				result = -EINVAL;
				break;
			}
			cfg_stat = _reg_replace(cfg_stat, value[token],
						params[token].hi,
						params[token].lo);
		}
		if (result >= 0) {
			/*
			 * If input, force tri-state and clear interrupts
			 */
			if (REG_GET(GPIO_IO_CFG_STAT_DIR, cfg_stat) ==
				 REG_CONST(GPIO_IO_CFG_STAT_DIR, input)) {
				cfg_stat = REG_REPLACEe(GPIO_IO_CFG_STAT_DISOUTPUT, cfg_stat, tristate);
				rc = gpio_iowrite32(priv,
						    REG_SET(GPIO_IO_CLR_INTENB, 1) |
						    REG_SET(GPIO_IO_CLR_INTSTATE, 1),
						    &io->clr);
				if (rc) {
					result = rc;
					break;
				}
			}
			rc = gpio_iowrite32(priv, cfg_stat, &io->cfg_stat);
			if (rc) {
				result = rc;
				break;
			}
			if (name && chip->names &&
			   (!chip->names[index] || strcmp(chip->names[index], name)))
				*(char **)&chip->names[index] = devm_kstrdup(dev, name, GFP_KERNEL);
			result = buflen;
		}
		break;
	}
	kfree(name);
	return result;
}
static DEVICE_ATTR_WO(config);

static int
_parse_set_clear(struct gpio_adapter_t *priv, const char *buf, unsigned int *index)
{
	int result;
	unsigned long v;

	_byp((char **)&buf);
	if (strncmp(buf, "index:", sizeof("index:") - 1))
		return -EINVAL;
	buf += sizeof("index:") - 1;
	_byp((char **)&buf);
	result = kstrtoul(buf, 0, &v);
	if ((result < 0) || (v >= priv->chip.ngpio))
		return -EINVAL;
	*index = v;
	_byp_num((char **)&buf);
	_byp((char **)&buf);
	if (*buf)
		return -EINVAL;
	return 0;
}

/*
 * This writes a physical value, not a logical value!
 */
static ssize_t
set_store(struct device *dev,
	  struct device_attribute *attr,
	  const char *buf,
	  size_t buflen)
{
	struct gpio_adapter_t *priv = dev_get_drvdata(dev);
	struct gpio_io_t __iomem *io;
	int rc;
	unsigned int index;
	int e = _parse_set_clear(priv, buf, &index);
	uint32_t v;

	if (e < 0)
		return e;
	io = cisco_fpga_gpio_io(&priv->chip, index);
	if (IS_ERR(io))
		return PTR_ERR(io);
	rc = gpio_ioread32(priv, &io->cfg_stat, &v);
	if (rc)
		return rc;
	if (REG_GET(GPIO_IO_CFG_STAT_DIR, v) == REG_CONST(GPIO_IO_CFG_STAT_DIR, input))
		return -EINVAL;
	v = REG_SET(GPIO_IO_SET_OUTSTATE, 1);
	rc = gpio_iowrite32(priv, v, &io->set);
	if (rc)
		return rc;
	return buflen;
}
static DEVICE_ATTR_WO(set);

/*
 * This writes a physical value, not a logical value!
 */
static ssize_t
clear_store(struct device *dev,
	    struct device_attribute *attr,
	    const char *buf,
	    size_t buflen)
{
	struct gpio_adapter_t *priv = dev_get_drvdata(dev);
	struct gpio_io_t __iomem *io;
	int rc;
	unsigned int index;
	int e = _parse_set_clear(priv, buf, &index);
	uint32_t v;

	if (e < 0)
		return e;
	io = cisco_fpga_gpio_io(&priv->chip, index);
	if (IS_ERR(io))
		return PTR_ERR(io);
	rc = gpio_ioread32(priv, &io->cfg_stat, &v);
	if (rc)
		return rc;
	if (REG_GET(GPIO_IO_CFG_STAT_DIR, v) == REG_CONST(GPIO_IO_CFG_STAT_DIR, input))
		return -EINVAL;
	v = REG_SET(GPIO_IO_CLR_OUTSTATE, 1);
	rc = gpio_iowrite32(priv, v, &io->clr);
	if (rc)
		return rc;
	return buflen;
}
static DEVICE_ATTR_WO(clear);

static struct attribute *_gpio_sys_attrs[] = {
	&dev_attr_config.attr,
	&dev_attr_set.attr,
	&dev_attr_clear.attr,
	NULL,
};
static const struct attribute_group _gpio_attr_group = {
	.attrs = _gpio_sys_attrs,
};
const struct attribute_group *_gpio_attr_groups[] = {
	&_gpio_attr_group,
	&cisco_fpga_reghdr_attr_group,
	NULL,
};
