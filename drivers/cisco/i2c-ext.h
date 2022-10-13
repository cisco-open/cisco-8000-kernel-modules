/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cisco IP block i2c-ext definitions
 *
 * Copyright (C) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef CISCO_8000_I2C_EXT_H_
#define CISCO_8000_I2C_EXT_H_

#include <linux/types.h>

#include "cisco/hdr.h"

struct i2c_ext_regs_v5_t {
	struct regblk_hdr_t hdr;		/*        0x0 - 0x14       */
	uint32_t fit;			/*       0x14 - 0x18       */
	uint32_t intrCfg0;		/*       0x18 - 0x1c       */
	uint32_t intrCfg1;		/*       0x1c - 0x20       */
	uint32_t cfg;			/*       0x20 - 0x24       */
	char pad__0[0x4];		/*       0x24 - 0x28       */
	uint32_t cfg2;			/*       0x28 - 0x2c       */
	uint32_t intSts;		/*       0x2c - 0x30       */
	uint32_t intFrc;		/*       0x30 - 0x34       */
	uint32_t intEnb;		/*       0x34 - 0x38       */
	uint32_t intDis;		/*       0x38 - 0x3c       */
	uint32_t wdata[65];		/*       0x3c - 0x140      */
	uint32_t rdata[63];		/*      0x140 - 0x244      */
	uint32_t rdata_v5[128];		/*      0x23c - 0x43c  rdata for v5    */
};

#define I2C_EXT_INTSTS         i2c_ext, intSts, raw,     31,  0, i2c_ext_regs_v5_t
#define I2C_EXT_INTSTS_ERROR   i2c_ext, intSts, error,    3,  3, i2c_ext_regs_v5_t
#define I2C_EXT_INTSTS_TIMEOUT i2c_ext, intSts, timeout,  2,  2, i2c_ext_regs_v5_t
#define I2C_EXT_INTSTS_DONE    i2c_ext, intSts, done,     1,  1, i2c_ext_regs_v5_t

#define I2C_EXT_INTFRC         i2c_ext, intFrc, raw,     31,  0, i2c_ext_regs_v5_t
#define I2C_EXT_INTFRC_ERROR   i2c_ext, intFrc, error,    3,  3, i2c_ext_regs_v5_t
#define I2C_EXT_INTFRC_TIMEOUT i2c_ext, intFrc, timeout,  2,  2, i2c_ext_regs_v5_t
#define I2C_EXT_INTFRC_DONE    i2c_ext, intFrc, done,     1,  1, i2c_ext_regs_v5_t

#define I2C_EXT_INTENB         i2c_ext, intEnb, raw,     31,  0, i2c_ext_regs_v5_t
#define I2C_EXT_INTENB_ERROR   i2c_ext, intEnb, error,    3,  3, i2c_ext_regs_v5_t
#define I2C_EXT_INTENB_TIMEOUT i2c_ext, intEnb, timeout,  2,  2, i2c_ext_regs_v5_t
#define I2C_EXT_INTENB_DONE    i2c_ext, intEnb, done,     1,  1, i2c_ext_regs_v5_t

#define I2C_EXT_INTDIS         i2c_ext, intDis, raw,     31,  0, i2c_ext_regs_v5_t
#define I2C_EXT_INTDIS_ERROR   i2c_ext, intDis, error,    3,  3, i2c_ext_regs_v5_t
#define I2C_EXT_INTDIS_TIMEOUT i2c_ext, intDis, timeout,  2,  2, i2c_ext_regs_v5_t
#define I2C_EXT_INTDIS_DONE    i2c_ext, intDis, done,     1,  1, i2c_ext_regs_v5_t

#define I2C_EXT_CFG          i2c_ext, cfg,      raw, 31,  0, i2c_ext_regs_v5_t
#define I2C_EXT_CFG_TEST     i2c_ext, cfg,     test, 31, 31, i2c_ext_regs_v5_t
#define I2C_EXT_CFG_DEVADDR  i2c_ext, cfg,  devAddr, 30, 24, i2c_ext_regs_v5_t
#define I2C_EXT_CFG_REGADDR  i2c_ext, cfg,  regAddr, 23, 16, i2c_ext_regs_v5_t
#define I2C_EXT_CFG_SPDCNT   i2c_ext, cfg,   spdCnt, 15, 13, i2c_ext_regs_v5_t
enum i2c_ext_cfg_spdCnt {
	i2c_ext_cfg_spdCnt__fast = 0,
	i2c_ext_cfg_spdCnt__1Mbps = 2,
	i2c_ext_cfg_spdCnt__400Kbps = 3,
	i2c_ext_cfg_spdCnt__100Kbps = 5,
};
#define I2C_EXT_CFG_DATASIZE i2c_ext, cfg, dataSize, 12, 10, i2c_ext_regs_v5_t
enum i2c_ext_cfg_dataSize {
	i2c_ext_cfg_dataSize__1B = 0,
	i2c_ext_cfg_dataSize__2B = 1,
	i2c_ext_cfg_dataSize__3B = 2,
	i2c_ext_cfg_dataSize__4B = 3,
	i2c_ext_cfg_dataSize__8B_EXT = 7,
};
#define I2C_EXT_CFG_DEVSEL   i2c_ext, cfg,   devSel,  9,  6, i2c_ext_regs_v5_t
#define I2C_EXT_CFG_MODE     i2c_ext, cfg,     mode,  5,  4, i2c_ext_regs_v5_t
enum i2c_ext_cfg_mode {
	i2c_ext_cfg_mode__i2c = 0,
	i2c_ext_cfg_mode__ext = 1,
};
#define I2C_EXT_CFG_ACCESSTYPE i2c_ext, cfg, accessType,  3, 2, i2c_ext_regs_v5_t
enum i2c_ext_cfg_accessType {
	i2c_ext_cfg_accessType__seq_write = 0,
	i2c_ext_cfg_accessType__seq_read = 1,
	i2c_ext_cfg_accessType__cur_write = 2,
	i2c_ext_cfg_accessType__cur_read = 3,
};
#define I2C_EXT_CFG_STARTACCESS i2c_ext, cfg, startAccess,  1,  1, i2c_ext_regs_v5_t
#define I2C_EXT_CFG_RST         i2c_ext, cfg,         rst,  0,  0, i2c_ext_regs_v5_t

#define I2C_EXT_CFG2_RDATASIZE i2c_ext, cfg, RdataSize, 26, 16, i2c_ext_regs_v5_t
#define I2C_EXT_CFG2_WDATASIZE i2c_ext, cfg, WdataSize, 9,   0, i2c_ext_regs_v5_t

#endif /* ndef CISCO_8000_I2C_EXT_H_ */
