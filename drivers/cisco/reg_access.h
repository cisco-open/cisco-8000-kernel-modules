/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cisco 8000 register access definitions
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef _CISCO_REG_ACCESS_H
#define _CISCO_REG_ACCESS_H

#include <linux/compiler.h>
#include <linux/io.h>
#include <linux/delay.h>

struct device;

static inline uint32_t
_reg_mask(uint32_t hi, uint32_t lo)
{
	uint32_t width = hi - lo + 1;

	return (width == 32) ? U32_C(~0) : ((1ul << width) - 1);
}

static inline uint32_t
_reg_mask_lo(uint32_t hi, uint32_t lo)
{
	return _reg_mask(hi, lo) << lo;
}

static inline uint32_t
_reg_get(uint32_t d, uint32_t hi, uint32_t lo)
{
	return (d >> lo) & _reg_mask(hi, lo);
}

static inline uint32_t
_reg_set(uint32_t d, uint32_t hi, uint32_t lo)
{
	return (d & _reg_mask(hi, lo)) << lo;
}

static inline uint32_t
_reg_replace(uint32_t v, uint32_t d, uint32_t hi, uint32_t lo)
{
	uint32_t mask = _reg_mask_lo(hi, lo);

	return (v & ~mask) | ((d << lo) & mask);
}

#if DEBUG_REG_TRACE
extern void reg_write32(const struct device *dev,
			uint32_t v, void __iomem *addr);
extern uint32_t reg_read32(const struct device *dev,
			   void __iomem *addr);

#else /* !DEBUG_REG_TRACE */
static inline void
reg_write32(const struct device *dev __always_unused,
	    uint32_t v, void __iomem *addr)
{
	iowrite32(v, addr);
}

static inline uint32_t
reg_read32(const struct device *dev __always_unused,
	   void __iomem *addr)
{
	return ioread32(addr);
}
#endif /* !DEBUG_REG_TRACE */

#define _REG_BLOCK(block, reg, field, hi, lo, type)    (block)
#define _REG_NAME(block, reg, field, hi, lo, type)     (reg)
#define _REG_FIELD(block, reg, field, hi, lo, type)    (field)
#define _REG_HI(block, reg, field, hi, lo, type)	(hi)
#define _REG_LO(block, reg, field, hi, lo, type)	(lo)
#define _REG_WIDTH(block, reg, field, hi, lo, type)	((hi) - (lo) + 1)
#define _REG_GET(block, reg, field, hi, lo, type, d)	_reg_get((d), (hi), (lo))
#define _REG_SET(block, reg, field, hi, lo, type, d)	_reg_set((d), (hi), (lo))
#define _REG_SETc(block, reg, field, hi, lo, type, d)	\
	(((d) & _REG_MASKc(block, reg, field, hi, lo, type)) << (lo))
#define _REG_REPLACE(block, reg, field, hi, lo, type, v, d) \
	_reg_replace((v), (d), (hi), (lo))
#define _REG_MASK(block, reg, field, hi, lo, type)	_reg_mask((hi), (lo))
#define _REG_MASK_LO(block, reg, field, hi, lo, type)	_reg_mask_lo((hi), (lo))
#define _REG_LIMIT(block, reg, field, hi, lo, type)	\
	(1ull << ((hi) - (lo) + 1))
#define _REG_MASKc(block, reg, field, hi, lo, type)	\
	((((hi) - (lo) + 1) == 32) ? U32_C(~0) : ((1ul << ((hi) - (lo) + 1)) - 1))
#define _REG_SETe(block, reg, field, hi, lo, type, d)	\
	_reg_set(block ## _ ## reg ## _ ## field ## __ ## d, (hi), (lo))
#define _REG_SETec(block, reg, field, hi, lo, type, d)	\
	_REG_SETc(block, reg, field, hi, lo, type, block ## _ ## reg ## _ ## field ## __ ## d)
#define _REG_REPLACEe(block, reg, field, hi, lo, type, v, d) \
	_reg_replace((v), block ## _ ## reg ## _ ## field ## __ ## d, (hi), (lo))

#define _REG_CONST(block, reg, field, hi, lo, type, d)	\
	(block ## _ ## reg ## _ ## field ## __ ## d)

#define _REG_UPDATE_BITS(r, block, reg, field, hi, lo, type, d) \
	regmap_update_bits(r, offsetof(struct type, reg), _reg_mask_lo((hi), (lo)), _reg_set((d), (hi), (lo)))

#define _REG_UPDATE_BITSe(r, block, reg, field, hi, lo, type, d) \
	regmap_update_bits(r, offsetof(struct type, reg), _reg_mask_lo((hi), (lo)), \
			   _reg_set(_REG_CONST(block, reg, field, hi, lo, type, d), (hi), (lo)))

#define REG_BLOCK(...)    _REG_BLOCK(__VA_ARGS__)
#define REG_NAME(...)     _REG_NAME(__VA_ARGS__)
#define cREG_FIELD(...)   _REG_FIELD(__VA_ARGS__)
#define REG_HI(...)       _REG_HI(__VA_ARGS__)
#define REG_LO(...)       _REG_LO(__VA_ARGS__)
#define REG_WIDTH(...)    _REG_WIDTH(__VA_ARGS__)
#define REG_GET(...)      _REG_GET(__VA_ARGS__)
#define REG_SET(...)      _REG_SET(__VA_ARGS__)
#define REG_SETc(...)     _REG_SETc(__VA_ARGS__)
#define REG_SETe(...)     _REG_SETe(__VA_ARGS__)
#define REG_SETec(...)    _REG_SETec(__VA_ARGS__)
#define REG_REPLACE(...)  _REG_REPLACE(__VA_ARGS__)
#define REG_CONST(...)    _REG_CONST(__VA_ARGS__)
#define REG_MASK(...)     _REG_MASK(__VA_ARGS__)
#define REG_MASK_LO(...)  _REG_MASK_LO(__VA_ARGS__)
#define REG_MASKc(...)    _REG_MASKc(__VA_ARGS__)
#define REG_REPLACEe(...) _REG_REPLACEe(__VA_ARGS__)
#define REG_LIMIT(...)    _REG_LIMIT(__VA_ARGS__)

#define REG_UPDATE_BITS(...)    _REG_UPDATE_BITS(__VA_ARGS__)
#define REG_UPDATE_BITSe(...)    _REG_UPDATE_BITSe(__VA_ARGS__)

struct reg_field_value_t {
	uint32_t mask;
	uint32_t value;
	const char *description;
};

struct reg_field_layout_t {
	const char *field_name;
	uint8_t hi;
	uint8_t lo;
	const struct reg_field_value_t *values;
};
#define _REG_FIELD_LAYOUT(_block, _reg, _field, _hi, _lo, _type, _values) \
	{ \
		.field_name = # _field, \
		.hi = _hi, \
		.lo = _lo, \
		.values = _values, \
	}
#define REG_FIELD_LAYOUT(...) _REG_FIELD_LAYOUT(__VA_ARGS__)
#define REG_FIELD_LAYOUT_TERMINATOR { 0, 0, 0, 0 }

struct reg_layout_t {
	const char *block;
	const char *reg_name;
	size_t offset;
	const struct reg_field_layout_t *fields;
};
#define _REG_LAYOUT(_block, _reg, _field, _hi, _lo, _type) \
	{ \
		.block = # _block, \
		.reg_name = # _reg, \
		.offset = offsetof(struct _type, _reg), \
		.fields = _type ## _ ## _reg ## _field_layout, \
	}
#define REG_LAYOUT(...) _REG_LAYOUT(__VA_ARGS__)
#define REG_LAYOUT_TERMINATOR       { 0, 0, 0, 0 }

#endif /* ndef _CISCO_REG_ACCESS_H */
