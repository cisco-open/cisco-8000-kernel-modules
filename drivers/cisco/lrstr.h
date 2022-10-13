/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cisco IP block lrstr definitions
 *
 * Copyright (C) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef CISCO_8000_LRSTR_H_
#define CISCO_8000_LRSTR_H_

#include <linux/types.h>

#include "cisco/hdr.h"

struct lrstr_regs_v4_t {
	struct regblk_hdr_t hdr;	/*        0x0 - 0x14       */
	char pad__0[0xc];		/*       0x14 - 0x20       */
	uint32_t intrCfg0;		/*       0x20 - 0x24       */
	uint32_t intrCfg1;		/*       0x24 - 0x28       */
	uint32_t clkperiod;		/*       0x28 - 0x2c       */
	uint32_t timer;			/*       0x2c - 0x30       */
	uint32_t update_timer;		/*       0x30 - 0x34       */
	uint32_t days;			/*       0x34 - 0x38       */
	uint32_t update_days;		/*       0x38 - 0x3c       */
	uint32_t fifo_status;		/*       0x3c - 0x40       */
	uint32_t fifo_out;		/*       0x40 - 0x44       */
	uint32_t intSts;		/*       0x44 - 0x48       */
	uint32_t intFrc;		/*       0x48 - 0x4c       */
	uint32_t intEnb;		/*       0x4c - 0x50       */
	uint32_t intDis;		/*       0x50 - 0x54       */
};

#define LRSTR_INTRCFG0          lrstr, intrCfg0,          raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_INTRCFG0_DATA     lrstr, intrCfg0,         data, 23,  0, lrstr_regs_v4_t

#define LRSTR_INTRCFG1          lrstr, intrCfg1,          raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_INTRCFG1_MSI      lrstr, intrCfg1,          msi,  3,  0, lrstr_regs_v4_t

#define LRSTR_TIMER             lrstr,    timer,          raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_TIMER_HOURS       lrstr,    timer,        hours, 26, 22, lrstr_regs_v4_t
#define LRSTR_TIMER_MINUTES     lrstr,    timer,      minutes, 21, 16, lrstr_regs_v4_t
#define LRSTR_TIMER_SECONDS     lrstr,    timer,      seconds, 15, 10, lrstr_regs_v4_t
#define LRSTR_TIMER_MILLISECONDS lrstr,    timer, milliseconds,  9,  0, lrstr_regs_v4_t

#define LRSTR_UPDATE_TIMER         lrstr, update_timer,          raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_UPDATE_TIMER_HOURS   lrstr, update_timer,        hours, 26, 22, lrstr_regs_v4_t
#define LRSTR_UPDATE_TIMER_MINUTES lrstr, update_timer,      minutes, 21, 16, lrstr_regs_v4_t
#define LRSTR_UPDATE_TIMER_SECONDS lrstr, update_timer,      seconds, 15, 10, lrstr_regs_v4_t
#define LRSTR_UPDATE_TIMER_MILLISECONDS lrstr, update_timer, milliseconds,  9,  0, lrstr_regs_v4_t

#define LRSTR_DAYS              lrstr,     days,          raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_DAYS_DAYS         lrstr,     days,         days, 26,  0, lrstr_regs_v4_t

#define LRSTR_UPDATE_DAYS       lrstr, update_days,          raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_UPDATE_DAYS_DAYS  lrstr, update_days,         days, 26,  0, lrstr_regs_v4_t

#define LRSTR_FIFO_STATUS           lrstr, fifo_status,        raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_FIFO_STATUS_THRESHOLD lrstr, fifo_status,  threshold, 13,  8, lrstr_regs_v4_t
#define LRSTR_FIFO_STATUS_FULLNESS  lrstr, fifo_status,   fullness,  7,  2, lrstr_regs_v4_t
#define LRSTR_FIFO_STATUS_FULL      lrstr, fifo_status,       full,  1,  1, lrstr_regs_v4_t
#define LRSTR_FIFO_STATUS_EMPTY     lrstr, fifo_status,      empty,  0,  0, lrstr_regs_v4_t

#define LRSTR_FIFO_OUT          lrstr, fifo_out,          raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_FIFO_OUT_EVENT    lrstr, fifo_out,        event, 31, 27, lrstr_regs_v4_t
#define LRSTR_FIFO_OUT_HOURS    lrstr, fifo_out,        hours, 26, 22, lrstr_regs_v4_t
#define LRSTR_FIFO_OUT_MINUTES  lrstr, fifo_out,      minutes, 21, 16, lrstr_regs_v4_t
#define LRSTR_FIFO_OUT_SECONDS  lrstr, fifo_out,      seconds, 15, 10, lrstr_regs_v4_t
#define LRSTR_FIFO_OUT_MILLISECONDS  lrstr, fifo_out,     millisec,  9,  0, lrstr_regs_v4_t
#define LRSTR_FIFO_OUT_DAYS     lrstr, fifo_out,        event, 26,  0, lrstr_regs_v4_t

#define LRSTR_INTSTS                lrstr,  intSts,            raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_INTSTS_FIFO_WATERMARK lrstr,  IntSts, fifo_watermark,  4,  4, lrstr_regs_v4_t
#define LRSTR_INTSTS_EON_ROLLOVER   lrstr,  IntSts,   eon_rollover,  3,  3, lrstr_regs_v4_t
#define LRSTR_INTSTS_DAY_WRITE      lrstr,  IntSts,      day_write,  2,  2, lrstr_regs_v4_t
#define LRSTR_INTSTS_TIMER_ROLLOVER lrstr,  IntSts, timer_rollover,  1,  1, lrstr_regs_v4_t
#define LRSTR_INTSTS_TIMER_WRITE    lrstr,  IntSts,    timer_write,  0,  0, lrstr_regs_v4_t

#define LRSTR_INTFRC                lrstr,  intFrc,            raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_INTFRC_FIFO_WATERMARK lrstr,  IntFrc, fifo_watermark,  4,  4, lrstr_regs_v4_t
#define LRSTR_INTFRC_EON_ROLLOVER   lrstr,  IntFrc,   eon_rollover,  3,  3, lrstr_regs_v4_t
#define LRSTR_INTFRC_DAY_WRITE      lrstr,  IntFrc,      day_write,  2,  2, lrstr_regs_v4_t
#define LRSTR_INTFRC_TIMER_ROLLOVER lrstr,  IntFrc, timer_rollover,  1,  1, lrstr_regs_v4_t
#define LRSTR_INTFRC_TIMER_WRITE    lrstr,  IntFrc,    timer_write,  0,  0, lrstr_regs_v4_t

#define LRSTR_INTENB                lrstr,  intEnb,            raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_INTENB_FIFO_WATERMARK lrstr,  IntEnb, fifo_watermark,  4,  4, lrstr_regs_v4_t
#define LRSTR_INTENB_EON_ROLLOVER   lrstr,  IntEnb,   eon_rollover,  3,  3, lrstr_regs_v4_t
#define LRSTR_INTENB_DAY_WRITE      lrstr,  IntEnb,      day_write,  2,  2, lrstr_regs_v4_t
#define LRSTR_INTENB_TIMER_ROLLOVER lrstr,  IntEnb, timer_rollover,  1,  1, lrstr_regs_v4_t
#define LRSTR_INTENB_TIMER_WRITE    lrstr,  IntEnb,    timer_write,  0,  0, lrstr_regs_v4_t

#define LRSTR_INTDIS                lrstr,  intDis,            raw, 31,  0, lrstr_regs_v4_t
#define LRSTR_INTDIS_FIFO_WATERMARK lrstr,  IntDis, fifo_watermark,  4,  4, lrstr_regs_v4_t
#define LRSTR_INTDIS_EON_ROLLOVER   lrstr,  IntDis,   eon_rollover,  3,  3, lrstr_regs_v4_t
#define LRSTR_INTDIS_DAY_WRITE      lrstr,  IntDis,      day_write,  2,  2, lrstr_regs_v4_t
#define LRSTR_INTDIS_TIMER_ROLLOVER lrstr,  IntDis, timer_rollover,  1,  1, lrstr_regs_v4_t
#define LRSTR_INTDIS_TIMER_WRITE    lrstr,  IntDis,    timer_write,  0,  0, lrstr_regs_v4_t

#endif /* ndef CISCO_8000_LRSTR_H_ */
