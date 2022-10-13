/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cisco IP block pseq definitions
 *
 * Copyright (C) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef CISCO_8000_PSEQ_H_
#define CISCO_8000_PSEQ_H_

#include <linux/types.h>

#include "cisco/hdr.h"

struct pseq_regs_v4_t {
	struct regblk_hdr_t hdr;	/*        0x0 - 0x14       */
	char pad__0[0xc];		/*       0x14 - 0x20       */
	uint32_t power_en_fit0;		/*       0x20 - 0x24       */
	uint32_t power_en_fit1;		/*       0x24 - 0x28       */
	char pad__1[0x18];		/*       0x28 - 0x40       */
	uint32_t power_good_fit0;	/*       0x40 - 0x44       */
	uint32_t power_good_fit1;	/*       0x44 - 0x48       */
	char pad__2[0x18];		/*       0x48 - 0x60       */
	uint32_t power_ov_fit0;		/*       0x60 - 0x64       */
	uint32_t power_ov_fit1;		/*       0x64 - 0x68       */
	char pad__3[0x18];		/*       0x68 - 0x80       */
	uint32_t misc_fit0;		/*       0x80 - 0x84       */
	uint32_t intr_cfg0;		/*       0x84 - 0x88       */
	uint32_t intr_cfg1;		/*       0x88 - 0x8c       */
	uint32_t gen_cfg;		/*       0x8c - 0x90       */
	uint32_t gen_stat;		/*       0x90 - 0x94       */
	uint32_t power_err0;		/*       0x94 - 0x98       */
	char pad__4[0xc];		/*       0x98 - 0xa4       */
	uint32_t power_en0;		/*       0xa4 - 0xa8       */
	char pad__5[0xc];		/*       0xa8 - 0xb4       */
	uint32_t power_good0;		/*       0xb4 - 0xb8       */
	char pad__6[0xc];		/*       0xb8 - 0xc4       */
	uint32_t power_ov0;		/*       0xc4 - 0xc8       */
	char pad__7[0xc];		/*       0xc8 - 0xd4       */
	uint32_t chkpt_stat0;		/*       0xd4 - 0xd8       */
	char pad__8[0xc];		/*       0xd8 - 0xe4       */
	uint32_t chkpt_ctrl0;		/*       0xe4 - 0xe8       */
	char pad__9[0xc];		/*       0xe8 - 0xf4       */
	uint32_t intr_sts;		/*       0xf4 - 0xf8       */
	uint32_t intr_test;		/*       0xf8 - 0xfc       */
	uint32_t intr_en;		/*       0xfc - 0x100      */
	uint32_t intr_dis;		/*      0x100 - 0x104      */
	uint32_t curr_stat;		/*      0x104 - 0x108      */
};

#define PSEQ_INTR_CFG0        pseq, intr_cfg0,        raw, 31,  0, pseq_regs_v4_t
#define PSEQ_INTR_CFG0_DATA   pseq, intr_cfg0,       data, 23,  0, pseq_regs_v4_t

#define PSEQ_INTR_CFG1        pseq, intr_cfg1,        raw, 31,  0, pseq_regs_v4_t
#define PSEQ_INTR_CFG1_MSI    pseq, intr_cfg1,        msi,  3,  0, pseq_regs_v4_t

#define PSEQ_GEN_CFG                   pseq,   gen_cfg,               raw, 31,  0, pseq_regs_v4_t
#define PSEQ_GEN_CFG_IGNORE_UV         pseq,   gen_cfg,         ignore_uv,  0,  0, pseq_regs_v4_t
#define PSEQ_GEN_CFG_USER_POWER_ON     pseq,   gen_cfg,     user_power_on,  1,  1, pseq_regs_v4_t
#define PSEQ_GEN_CFG_USER_POWER_OFF    pseq,   gen_cfg,    user_power_off,  2,  2, pseq_regs_v4_t
#define PSEQ_GEN_CFG_USER_POWER_CYCLE  pseq,   gen_cfg,  user_power_cycle,  3,  3, pseq_regs_v4_t
#define PSEQ_GEN_CFG_IGNORE_OV         pseq,   gen_cfg,         ignore_ov,  4,  4, pseq_regs_v4_t
#define PSEQ_GEN_CFG_IGNORE_DEVICE_ERR pseq,   gen_cfg, ignore_device_err,  5,  5, pseq_regs_v4_t
#define PSEQ_GEN_CFG_IGNORE_OTHER_ERR  pseq,   gen_cfg,  ignore_other_err,  6,  6, pseq_regs_v4_t

#define PSEQ_GEN_STAT                  pseq,  gen_stat,               raw, 31,  0, pseq_regs_v4_t
#define PSEQ_GEN_STAT_SEQ_STATE_AT_ERR pseq,  gen_stat,  seq_state_at_err, 31, 20, pseq_regs_v4_t
#define PSEQ_GEN_STAT_POWER_DOWN_REASON pseq,  gen_stat, power_down_reason, 13, 11, pseq_regs_v4_t
#define PSEQ_GEN_STAT_POWER_STATE      pseq,  gen_stat,       power_state, 10,  9, pseq_regs_v4_t
enum pseq_gen_stat_power_state {
	pseq_gen_stat_power_state__OFF,
	pseq_gen_stat_power_state__SEQUENCED_ON,
	pseq_gen_stat_power_state__ON,
	pseq_gen_stat_power_state__SEQUENCED_OFF,
};
#define PSEQ_GEN_STAT_POWER_STATUS_LED pseq,  gen_stat,  power_status_led,  8,  0, pseq_regs_v4_t

#define PSEQ_POWER_ERR0                pseq,  power_err0,             raw, 31,  0, pseq_regs_v4_t
#define PSEQ_POWER_EN0                 pseq,   power_en0,             raw, 31,  0, pseq_regs_v4_t
#define PSEQ_POWER_GOOD0               pseq, power_good0,             raw, 31,  0, pseq_regs_v4_t
#define PSEQ_POWER_OV0                 pseq,   power_ov0,             raw, 31,  0, pseq_regs_v4_t

#endif /* ndef CISCO_8000_PSEQ_H_ */
