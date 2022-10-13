// SPDX-License-Identifier: GPL-2.0-only
/*
 * Register trace helpers
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */

#include <linux/compiler.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <cisco/reg_trace.h>
#include <linux/slab.h>

void
reg_trace_display_buffer(struct device *dev,
			 const char *title,
			 const uint8_t *datap, size_t count)
{
#define _c(b) ((((b) >= 0x20) && ((b) <= 0x7e)) ? b : '.')
	size_t offset = 0;
	char buffer[128];

	while (count) {
		snprintf(buffer, sizeof(buffer),
			 "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
			 datap[0], datap[1], datap[2], datap[3],
			 datap[4], datap[5], datap[6], datap[7],
			 datap[8], datap[9], datap[10], datap[11],
			 datap[12], datap[13], datap[14], datap[15],
			 _c(datap[0]), _c(datap[1]), _c(datap[2]), _c(datap[3]),
			 _c(datap[4]), _c(datap[5]), _c(datap[6]), _c(datap[7]),
			 _c(datap[8]), _c(datap[9]), _c(datap[10]), _c(datap[11]),
			 _c(datap[12]), _c(datap[13]), _c(datap[14]), _c(datap[15]));
		if (count > 15) {
			dev_err(dev, "%s-%02zx: %s\n", title, offset, buffer);
			count -= 16;
			offset += 16;
			datap += 16;
		} else {
			memset(buffer + count * 3, ' ', 3 * (16 - count));
			memset(buffer + (16 * 3) + count, ' ', 16 - count);
			dev_err(dev, "%s-%02zx: %s\n", title, offset, buffer);
			break;
		}
	}
}
EXPORT_SYMBOL(reg_trace_display_buffer);

static int
_extract(struct reg_trace_t *tracep, void *dst, size_t len)
{
	size_t space = _reg_trace_read_space(tracep);

	if (space < len) {
		pr_err("%s: failed to extract %zu bytes\n", __func__, len);
		return -EINVAL;
	}
	space = _reg_trace_read_space_nowrap(tracep);
	space = min(space, len);
	memcpy(dst, &tracep->base[tracep->read_head], space);
	if (space < len)
		memcpy(dst + space, tracep->base, len - space);
	_reg_trace_read_skip(tracep, len);
	return 0;
}

void
reg_trace_walk(struct reg_trace_t *tracep,
	       reg_trace_walk_fn_t fn,
	       void *cookie)
{
	uint8_t *bufp = kzalloc(tracep->size, GFP_KERNEL);
	struct reg_trace_hdr_t hdr;

	if (!bufp) {
		pr_err("%s: kzalloc(%#zx) failed\n", __func__, tracep->size);
		return;
	}
	for (; !_reg_trace_is_empty(tracep); ) {
		if (_extract(tracep, &hdr, sizeof(hdr)))
			break;
		if (hdr.len > tracep->size)
			break;
		if (_extract(tracep, bufp, hdr.len))
			break;
		fn(tracep, cookie, &hdr, bufp);
	}
	kfree(bufp);
}
EXPORT_SYMBOL(reg_trace_walk);
