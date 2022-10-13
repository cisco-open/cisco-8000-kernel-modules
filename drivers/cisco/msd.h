/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cisco IP block msd definitions
 *
 * Copyright (C) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef CISCO_8000_MSD_H_
#define CISCO_8000_MSD_H_

#include <linux/types.h>

#include "cisco/hdr.h"

struct msd_regs_v5_t {
	struct regblk_hdr_t hdr;	/*        0x0 - 0x14       */
	char pad__0[0xc];	/*       0x14 - 0x20       */
	uint32_t cfg0;		/*       0x20 - 0x24       */
	uint32_t cfg1;		/*       0x24 - 0x28       */
	uint32_t cfg2;		/*       0x28 - 0x2c       */
	uint32_t cfg3;		/*       0x2c - 0x30       */
	uint32_t cfg4;		/*       0x30 - 0x34       */
	uint32_t cfg5;		/*       0x34 - 0x38       */
	uint32_t cfg6;		/*       0x38 - 0x3c       */
	uint32_t cfg7;		/*       0x3c - 0x40       */
	char pad__1[0x7c];	/*       0x40 - 0xbc       */
	uint32_t status0;	/*       0xbc - 0xc0       */
	uint32_t status1;	/*       0xc0 - 0xc4       */
	uint32_t status2;	/*       0xc4 - 0xc8       */
	uint32_t status3;	/*       0xc8 - 0xcc       */
	uint32_t status4;	/*       0xcc - 0xd0       */
	uint32_t status5;	/*       0xd0 - 0xd4       */
	uint32_t status6;	/*       0xd4 - 0xd8       */
	uint32_t status7;	/*       0xd8 - 0xdc       */
	char pad__2[0x24];	/*       0xdc - 0x100      */
	uint32_t scratchram[256]; /*      0x100 - 0x500      */
	uint32_t arbi[8];	/*      0x500 - 0x520      */
};

#define MSD_STATUS0               msd, status0,           raw, 31,  0, msd_regs_v5_t
#define MSD_STATUS0_PLATFORM_ID   msd, status0,   platform_id, 31, 28, msd_regs_v5_t
enum msd_status0_platform {
	msd_status0_platform_id__FIXED = 1,
	msd_status0_platform_id__DISTRIBUTED = 2,
	msd_status0_platform_id__CENTRAL = 3,
};
#define MSD_STATUS0_FPGA_ID       msd, status0,       fpga_id, 27, 20, msd_regs_v5_t
#define MSD_STATUS0_FPGA_INSTANCE msd, status0, fpga_instance, 19, 18, msd_regs_v5_t
#define MSD_STATUS0_PCIE_ID       msd, status0,       pcie_id, 15,  0, msd_regs_v5_t

#define MSD_CFG5                  msd,    cfg5,           raw, 31,  0, msd_regs_v5_t
#define MSD_CFG5_MASTER_SELECT    msd,    cfg5, master_select, 10, 10, msd_regs_v5_t
enum msd_cfg5_master_select {
	msd_cfg5_master_select__X86 = 0,
	msd_cfg5_master_select__BMC = 1,
};

#define MSD_CFG7                  msd,    cfg7,           raw, 31,  0, msd_regs_v5_t
#define MSD_CFG7_ZONE1_CYCLE      msd,    cfg7,   zone1_cycle,  6,  6, msd_regs_v5_t
#define MSD_CFG7_ZONE1_OFF        msd,    cfg7,     zone1_off,  3,  3, msd_regs_v5_t

#endif /* ndef CISCO_8000_MSD_H_ */
