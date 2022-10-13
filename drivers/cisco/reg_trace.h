/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cisco 8000 register trace definitions
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */

#ifndef _CISCO_REG_TRACE_H
#define _CISCO_REG_TRACE_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/timekeeping.h>
#include <linux/slab.h>

struct device;

struct reg_trace_hdr_t {
	struct timespec64 ts;
	uint16_t          op;
	uint16_t          len;
};

enum reg_trace_op_t {
	REG_TRACE_OP_DATA,
	REG_TRACE_OP_READ,
	REG_TRACE_OP_WRITE,
	REG_TRACE_OP_NEXT,
};

struct reg_trace_read_t {
	uint32_t __iomem *addr;
	uint32_t          value;
	int               e;
};

struct reg_trace_write_t {
	uint32_t __iomem *addr;
	uint32_t          value;
	int               e;
};

struct reg_trace_t {
	uint8_t *base;
	uint8_t *limit;
	size_t   size;

	size_t read_head;
	size_t write_tail;

	bool   overflow;
	size_t max_size;

	struct timespec64 walk_ts;
};

static inline int
reg_trace_init(struct reg_trace_t *tracep, size_t size)
{
	memset(tracep, 0, sizeof(*tracep));
	tracep->base = kzalloc(size, GFP_KERNEL);
	if (tracep->base) {
		tracep->size = size;
		tracep->limit = tracep->base + size;
		return 0;
	}
	return -ENOMEM;
}

static inline void
reg_trace_free(struct reg_trace_t *tracep)
{
	kfree(tracep->base);
	memset(tracep, 0, sizeof(*tracep));
}

static inline bool
_reg_trace_is_empty(const struct reg_trace_t *tracep)
{
	return tracep->read_head == tracep->write_tail;
}

static inline size_t
_reg_trace_write_space(const struct reg_trace_t *tracep)
{
	if (tracep->write_tail < tracep->read_head)
		return (tracep->read_head - tracep->write_tail) - 1;
	return (tracep->size - tracep->write_tail)
		    + tracep->read_head - 1;
}

static inline size_t
_reg_trace_write_space_nowrap(const struct reg_trace_t *tracep)
{
	if (tracep->write_tail < tracep->read_head)
		return (tracep->read_head - tracep->write_tail) - 1;
	return tracep->size - tracep->write_tail;
}

static inline size_t
_reg_trace_read_space(const struct reg_trace_t *tracep)
{
	if (tracep->write_tail >= tracep->read_head)
		return tracep->write_tail - tracep->read_head;
	return (tracep->size - tracep->read_head) + tracep->write_tail;
}

static inline size_t
_reg_trace_read_space_nowrap(const struct reg_trace_t *tracep)
{
	if (tracep->write_tail >= tracep->read_head)
		return tracep->write_tail - tracep->read_head;
	return tracep->size - tracep->read_head;
}

static inline size_t
_reg_trace_fill(struct reg_trace_t *tracep,
		const void *data,
		size_t data_len)
{
	size_t avail = _reg_trace_write_space_nowrap(tracep);

	avail = min(avail, data_len);
	memcpy(&tracep->base[tracep->write_tail], data, avail);
	tracep->write_tail += avail;
	if (tracep->write_tail == tracep->size)
		tracep->write_tail = 0;
	return avail;
}

static inline void
_reg_trace_read_skip(struct reg_trace_t *tracep, size_t len)
{
	tracep->read_head = (tracep->read_head + len) % tracep->size;
}


static inline void
_reg_trace_write(struct reg_trace_t *tracep,
		 const void *data,
		 size_t data_len)
{
	if (data_len) {
		size_t wrote = _reg_trace_fill(tracep, data, data_len);

		if (wrote != data_len) {
			data += wrote;
			data_len -= wrote;
			(void) _reg_trace_fill(tracep, data, data_len);
		}
	}
}

static inline void
reg_trace(struct reg_trace_t *tracep,
	  uint16_t op,
	  const void *data,
	  size_t data_len)
{
	struct reg_trace_hdr_t hdr;

	if ((sizeof(hdr) + data_len) > _reg_trace_write_space(tracep)) {
		tracep->overflow = 1;
	} else {
		ktime_get_real_ts64(&hdr.ts);
		hdr.op = op;
		hdr.len = data_len;
		_reg_trace_write(tracep, &hdr, sizeof(hdr));
		_reg_trace_write(tracep, data, data_len);
	}
}

static inline void
reg_trace_reset(struct reg_trace_t *tracep)
{
	size_t cur_size = tracep->size - _reg_trace_write_space(tracep);

	if (cur_size > tracep->max_size)
		tracep->max_size = cur_size;

	tracep->read_head = tracep->write_tail = 0;
	tracep->overflow = 0;
	tracep->walk_ts.tv_sec = 0;
	tracep->walk_ts.tv_nsec = 0;
}

typedef void (*reg_trace_walk_fn_t)(struct reg_trace_t *tracep,
				    void *cookie,
				    const struct reg_trace_hdr_t *hdrp,
				    const uint8_t *datap);

extern void reg_trace_walk(struct reg_trace_t *tracep,
			   reg_trace_walk_fn_t fn,
			   void *cookie);
extern void reg_trace_display_buffer(struct device *dev,
				     const char *title,
				     const uint8_t *datap,
				     size_t count);

#endif /* ndef _CISCO_REG_TRACE_H */
