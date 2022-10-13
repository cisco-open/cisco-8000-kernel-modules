/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cisco IP block mdio definitions
 *
 * Copyright (C) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef CISCO_8000_MDIO_H_
#define CISCO_8000_MDIO_H_

#include <linux/types.h>

#include "cisco/hdr.h"

struct mdio_regs_v4_t {
	struct regblk_hdr_t hdr;	/*        0x0 - 0x14       */
	uint32_t fit;			/*       0x14 - 0x18       */
	uint32_t intrCfg0;		/*       0x18 - 0x1c       */
	uint32_t intrCfg1;		/*       0x1c - 0x20       */
	uint32_t cfg;			/*       0x20 - 0x24       */
	uint32_t addr;			/*       0x24 - 0x28       */
	uint32_t wdata;			/*       0x28 - 0x2c       */
	uint32_t rdata;			/*       0x2c - 0x30       */
	uint32_t trist;			/*       0x30 - 0x34       */
	uint32_t doneIntSts;		/*       0x34 - 0x38       */
	uint32_t doneIntFrc;		/*       0x38 - 0x3c       */
	uint32_t doneIntEnb;		/*       0x3c - 0x40       */
	uint32_t doneIntDis;		/*       0x40 - 0x44       */
	uint32_t mtkmode_wdata[16];	/*       0x44 - 0x84       */
	uint32_t mtkmode_rdata[16];	/*       0x84 - 0xc4       */
};

#define MDIO_INTRCFG0          mdio, intrCfg0,          raw, 31, 0, mdio_regs_v4_t
#define MDIO_INTRCFG0_DATA     mdio, intrCfg0,         data, 23, 0, mdio_regs_v4_t

#define MDIO_INTRCFG1          mdio, intrCfg1,          raw, 31, 0, mdio_regs_v4_t
#define MDIO_INTRCFG1_MSI      mdio, intrCfg1,          msi,  3, 0, mdio_regs_v4_t

#define MDIO_CFG               mdio,      cfg,          raw, 31,  0, mdio_regs_v4_t

#define MDIO_CFG_START         mdio,      cfg,        start, 31, 31, mdio_regs_v4_t
enum mdio_cfg_start {
	mdio_cfg_start__DONE  = 0,
	mdio_cfg_start__START = 1,
};

#define MDIO_CFG_PREAMBLEDIS   mdio,      cfg,  preambleDis, 29, 29, mdio_regs_v4_t

#define MDIO_CFG_CTRLMODE      mdio,      cfg,     ctrlMode, 28, 28, mdio_regs_v4_t
enum mdio_cfg_ctrlMode {
	mdio_cfg_ctrlMode__CLAUSE_22 = 0,
	mdio_cfg_ctrlMode__CLAUSE_45 = 1,
};

#define MDIO_CFG_MDIOCLKFREQ   mdio,      cfg,  mdioClkFreq, 24, 20, mdio_regs_v4_t
#define MDIO_CFG_DEVREGADDR    mdio,      cfg,   devRegAddr, 16, 12, mdio_regs_v4_t
#define MDIO_CFG_ACCESSTYPE    mdio,      cfg,   accessType, 11, 11, mdio_regs_v4_t
enum mdio_cfg_accessType {
	 mdio_cfg_accessType__WRITE_OP = 0,
	 mdio_cfg_accessType__READ_OP  = 1,
};
#define MDIO_CFG_ACCESSWIDTH   mdio,      cfg,  accessWidth, 10, 10, mdio_regs_v4_t
#define MDIO_CFG_MTKMODE       mdio,      cfg,      mtkMode,  9,  9, mdio_regs_v4_t
#define MDIO_CFG_PHYADDR       mdio,      cfg,      phyAddr,  8,  4, mdio_regs_v4_t
#define MDIO_CFG_DEVSEL        mdio,      cfg,       devSel,  3,  0, mdio_regs_v4_t

#define MDIO_ADDR               mdio,     addr,        raw, 31,  0, mdio_regs_v4_t

#define MDIO_WDATA              mdio,    wdata,        raw, 31,  0, mdio_regs_v4_t

#define MDIO_RDATA              mdio,    rdata,        raw, 31,  0, mdio_regs_v4_t

#define MDIO_TRIST              mdio,    trist,        raw, 31,  0, mdio_regs_v4_t
#define MDIO_TRIST_FORCELOW     mdio,    trist,   forceLow, 22, 22, mdio_regs_v4_t
#define MDIO_TRIST_CLKFREQ      mdio,    trist,    clkFreq, 21, 16, mdio_regs_v4_t
#define MDIO_TRIST_TRISTATE     mdio,    trist,   triState, 15,  0, mdio_regs_v4_t

#define MDIO_DONEINTSTS         mdio,  doneIntSts,     raw, 31,  0, mdio_regs_v4_t
#define MDIO_DONEINTSTS_DONEINT mdio,  doneIntSts, doneInt,  0,  0, mdio_regs_v4_t

#define MDIO_DONEINTFRC         mdio,  doneIntFrc,     raw, 31,  0, mdio_regs_v4_t
#define MDIO_DONEINTFRC_DONEINT mdio,  doneIntFrc, doneInt,  0,  0, mdio_regs_v4_t

#define MDIO_DONEINTENB         mdio,  doneIntEnb,     raw, 31,  0, mdio_regs_v4_t
#define MDIO_DONEINTENB_DONEIE  mdio,  doneIntEnb,  doneIe,  0,  0, mdio_regs_v4_t

#define MDIO_DONEINTDIS         mdio,  doneIntDis,     raw, 31,  0, mdio_regs_v4_t
#define MDIO_DONEINTDIS_DONEIE  mdio,  doneIntDis,  doneIe,  0,  0, mdio_regs_v4_t

#endif /* ndef CISCO_8000_MDIO_H_ */
