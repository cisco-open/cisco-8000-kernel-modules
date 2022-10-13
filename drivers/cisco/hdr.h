/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Block header register definitions
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef _CISCO_HDR_H
#define _CISCO_HDR_H

#include <linux/types.h>
#include <linux/sysfs.h>

struct regblk_hdr_t {
	uint32_t info0;		/* 0x00 - 0x04 */
	uint32_t info1;		/* 0x04 - 0x08 */
	uint32_t sw0;		/* 0x08 - 0x0c */
	uint32_t sw1;		/* 0x0c - 0x10 */
	uint32_t magicNo;	/* 0x10 - 0x14 */
};

#define HDR_INFO0            hdr, info0,        raw, 31,  0, regblk_hdr_t
#define HDR_INFO0_OFFSET     hdr, info0,     offset, 31, 14, regblk_hdr_t
#define HDR_INFO0_ID         hdr, info0,         id, 13,  6, regblk_hdr_t
#define HDR_INFO0_MAJORVER   hdr, info0,   majorVer,  5,  0, regblk_hdr_t

#define HDR_INFO1            hdr, info1,        raw, 31,  0, regblk_hdr_t
#define HDR_INFO1_CFGREGSNUM hdr, info1, cfgRegsNum, 31, 24, regblk_hdr_t
#define HDR_INFO1_ARRAYSZ    hdr, info1,    arraySz, 23, 16, regblk_hdr_t
#define HDR_INFO1_INSTNUM    hdr, info1,    instNum, 15,  9, regblk_hdr_t
#define HDR_INFO1_FPGANUM    hdr, info1,    fpgaNum,  8,  5, regblk_hdr_t
#define HDR_INFO1_MINORVER   hdr, info1,   minorVer,  4,  0, regblk_hdr_t

#define HDR_SW0              hdr,   sw0,        raw, 31,  0, regblk_hdr_t
#define HDR_SW0_STAT         hdr,   sw0,       stat, 31,  0, regblk_hdr_t

#define HDR_SW1              hdr,   sw1,        raw, 31,  0, regblk_hdr_t
#define HDR_SW1_STAT         hdr,   sw1,       stat, 31,  0, regblk_hdr_t

#define HDR_MAGICNO          hdr, magicNo,      raw, 31,  0, regblk_hdr_t
#define HDR_MAGICNO_MAGICNO  hdr, magicNo,  magicNo, 31,  0, regblk_hdr_t

extern const struct attribute_group cisco_fpga_reghdr_attr_group;

#endif /* ndef _CISCO_HDR_H */
