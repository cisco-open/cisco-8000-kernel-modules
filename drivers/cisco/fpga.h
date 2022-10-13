/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * fire sensor ip block register definitions
 *
 * Copyright (c) 2020, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef CISCO_FPGA_H_
#define CISCO_FPGA_H_

#include <linux/kernel.h>

#define CISCO_FPGA_MAGIC            (0xc15c0595)

#define CISCO_FPGA_MAX_IRQS_LT_V8   (10)
#define CISCO_FPGA_MAX_IRQS_V8      (12)

struct blkhdr {
	u32	maj:6,
		id:8,
		offset:18;
	u32	minorVer:5,
		fpgaNum:4,
		instNum:7,
		arraySz:8,
		cfgRegsNum:8;
	u32	sw[2];
	u32	magic;
} __packed;

struct info_rom {
	struct blkhdr hdr;
	u16	vendor;
	u16	family;
	u32	fpga_id;
	u32	cfg_info;
	u32	rev_min:16,
		rev_maj:8,
		rev_dbg:8;
	u32	build;
	char	comment[24];
	u32	num_blocks;
	u16	block_offset[];
} __packed;

#endif /* ndef CISCO_FPGA_H_ */
