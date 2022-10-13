/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * info ip block register definitions
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef _CISCO_INFO_H
#define _CISCO_INFO_H

#include <linux/types.h>

#include "cisco/hdr.h"

struct info_regs_v6_t {
	struct regblk_hdr_t hdr;		/*        0x0 - 0x14       */
	uint32_t device;		/*       0x14 - 0x18       */
	uint32_t fpga_id;		/*       0x18 - 0x1c       */
	uint32_t cfg_info;		/*       0x1c - 0x20       */
	uint32_t version;		/*       0x20 - 0x24       */
	uint32_t build;			/*       0x24 - 0x28       */
	uint32_t comment_str[6];	/*       0x28 - 0x40       */
	uint32_t num_ip_block;		/*       0x40 - 0x44       */
	uint32_t block_offset[128];	/*       0x44 - 0x244      */
};

#define INFO_DEVICE           info, device,        raw, 31,  0, info_regs_v6_t
#define INFO_DEVICE_FAMILY    info, device,     family, 31, 16, info_regs_v6_t
#define INFO_DEVICE_VENDOR    info, device,     vendor, 15,  0, info_regs_v6_t

#define INFO_FPGA_ID          info, fpga_id,      raw, 31,  0, info_regs_v6_t
#define INFO_FPGA_ID_FPGA_ID  info, fpga_id,  fpga_id, 31,  0, info_regs_v6_t

#define INFO_CFG_INFO          info, cfg_info,      raw, 31,  0, info_regs_v6_t
#define INFO_CFG_INFO_CFG_INFO info, cfg_info, cfg_info,  1,  0, info_regs_v6_t

#define INFO_VERSION          info, version,      raw, 31,  0, info_regs_v6_t
#define INFO_VERSION_REVDBG   info, version,   revDbg, 31, 24, info_regs_v6_t
#define INFO_VERSION_REVMAJ   info, version,   revMaj, 23, 16, info_regs_v6_t
#define INFO_VERSION_REVMIN   info, version,   revMin, 15,  0, info_regs_v6_t

#define INFO_BUILD            info,   build,      raw, 31,  0, info_regs_v6_t
#define INFO_BUILD_BLDNO      info,   build,    bldNo, 31,  0, info_regs_v6_t

#define INFO_COMMENT_STR            info, comment_str,        raw, 31,  0, info_regs_v6_t
#define INFO_COMMENT_STR_COMMENTSTR info, comment_str, commentStr, 31,  0, info_regs_v6_t

/* Following fields are available in majorVer >= 6 */

#define INFO_NUM_IP_BLOCK          info, num_ip_block,      raw, 31,  0, info_regs_v6_t
#define INFO_NUM_IP_BLOCK_NUMIPBLK info, num_ip_block, numIpBlk, 31,  0, info_regs_v6_t

#define INFO_BLOCK_OFFSET              info, block_offset,          raw, 31,  0, info_regs_v6_t
#define INFO_BLOCK_OFFSET_BLKOFFSET_LO info, block_offset, blkOffset_lo, 31, 16, info_regs_v6_t
#define INFO_BLOCK_OFFSET_BLKOFFSET_HI info, block_offset, blkOffset_hi, 15,  0, info_regs_v6_t

#endif /* ndef _CISCO_INFO_H */
