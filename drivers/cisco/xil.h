/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Block xil register definitions
 *
 * Copyright (c) 2019, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 */
#ifndef _CISCO_XIL_H
#define _CISCO_XIL_H

#include <linux/types.h>
#include <linux/compiler.h>

#include "cisco/hdr.h"

#define xil_regs_t xil_regs_v5_t
struct xil_regs_v5_t {
	struct regblk_hdr_t hdr;		/*        0x0 - 0x14       */
	char pad__0[0x4];		/*       0x14 - 0x18       */
	uint32_t intrCfg0;		/*       0x18 - 0x1c       */
	uint32_t intrCfg1;		/*       0x1c - 0x20       */
	uint32_t cfg0;			/*       0x20 - 0x24       */
	uint32_t cfg1;			/*       0x24 - 0x28       */
	uint32_t cfg2;			/*       0x28 - 0x2c       */
	uint32_t cfg3;			/*       0x2c - 0x30       */
	uint32_t cfg4;			/*       0x30 - 0x34       */
	uint32_t cfg5;			/*       0x34 - 0x38       */
	uint32_t cfg6;			/*       0x38 - 0x3c       */
	uint32_t cfg7;			/*       0x3c - 0x40       */
	uint32_t dev_dna_msw;		/*       0x40 - 0x44       */
	uint32_t dev_dna_lsw;		/*       0x44 - 0x48       */
	uint32_t sysmon_addr;		/*       0x48 - 0x4c       */
	uint32_t sysmon_data;		/*       0x4c - 0x50       */
	uint32_t sem_status;		/*       0x50 - 0x54       */
	uint32_t sem_data;		/*       0x54 - 0x58       */
	uint32_t pconf_status;		/*       0x58 - 0x5c       */
	uint32_t pconf_dis0;		/*       0x5c - 0x60       */
	uint32_t pconf_dis1;		/*       0x60 - 0x64       */
	uint32_t pconf_dis2;		/*       0x64 - 0x68       */
	uint32_t pconf_dis3;		/*       0x68 - 0x6c       */
	uint32_t pconf_dis4;		/*       0x6c - 0x70       */
	uint32_t pconf_dis5;		/*       0x70 - 0x74       */
	uint32_t pconf_dis6;		/*       0x74 - 0x78       */
	uint32_t pconf_dis7;		/*       0x78 - 0x7c       */
	char pad__1[0x4];		/*       0x7c - 0x80       */
	uint32_t pconf_rst0;		/*       0x80 - 0x84       */
	uint32_t pconf_rst1;		/*       0x84 - 0x88       */
	uint32_t pconf_rst2;		/*       0x88 - 0x8c       */
	uint32_t pconf_rst3;		/*       0x8c - 0x90       */
	uint32_t pconf_rst4;		/*       0x90 - 0x94       */
	uint32_t pconf_rst5;		/*       0x94 - 0x98       */
	uint32_t pconf_rst6;		/*       0x98 - 0x9c       */
	uint32_t pconf_rst7;		/*       0x9c - 0xa0       */
	uint32_t icap_data;		/*       0xa0 - 0xa4       */
	uint32_t lpbk;			/*       0xa4 - 0xa8       */
	uint32_t drp_addr;		/*       0xa8 - 0xac       */
	uint32_t drp_data[4];		/*       0xac - 0xbc       */
	uint32_t status0;		/*       0xbc - 0xc0       */
	uint32_t status1;		/*       0xc0 - 0xc4       */
	uint32_t status2;		/*       0xc4 - 0xc8       */
	uint32_t status3;		/*       0xc8 - 0xcc       */
	uint32_t status4;		/*       0xcc - 0xd0       */
	uint32_t status5;		/*       0xd0 - 0xd4       */
	uint32_t status6;		/*       0xd4 - 0xd8       */
	uint32_t status7;		/*       0xd8 - 0xdc       */
	uint32_t icap_bootsts_data;	/*       0xdc - 0xe0       */
	uint32_t intSts;		/*       0xe0 - 0xe4       */
	uint32_t intFrc;		/*       0xe4 - 0xe8       */
	uint32_t intEnb;		/*       0xe8 - 0xec       */
	uint32_t intDis;		/*       0xec - 0xf0       */
	char pad__2[0x10];		/*       0xf0 - 0x100      */
	uint32_t scratchram[256];	/*      0x100 - 0x500      */
	uint32_t arbi[8];		/*      0x500 - 0x520      */
};

#define XIL_STATUS0               xil, status0,           raw, 31,  0, xil_regs_v5_t
#define XIL_STATUS0_PLATFORM_ID   xil, status0,   platform_id, 31, 28, xil_regs_v5_t
enum xil_status0_platform_id {
	xil_status0_platform_id__FIXED = 1,
	xil_status0_platform_id__DISTRIBUTED,
	xil_status0_platform_id__CENTRAL,
};
#define XIL_STATUS0_FPGA_ID       xil, status0,       fpga_id, 27, 20, xil_regs_v5_t
enum xil_status0_fpga_id {
	xil_status0_fpga_id__FIXED_BMC_FPGA = 1,
	xil_status0_fpga_id__FIXED_X86_FPGA = 2,
	xil_status0_fpga_id__FIXED_IOFPGA_SHERMAN = 3,
	xil_status0_fpga_id__FIXED_IOFPGA_KANGAROO = 4,
	xil_status0_fpga_id__FIXED_IOFPGA_PERSHING_BASE = 5,
	xil_status0_fpga_id__FIXED_IOFPGA_PERSHING_MEZZ = 6,
	xil_status0_fpga_id__FIXED_IOFPGA_CHURCHILL = 7,
	xil_status0_fpga_id__FIXED_IOFPGA_VALENTINE = 9,
	xil_status0_fpga_id__FIXED_IOFPGA_MATILDA_64 = 10,
	xil_status0_fpga_id__FIXED_IOFPGA_MATILDA_32 = 11,
	xil_status0_fpga_id__FIXED_IOFPGA_CROCODILE = 12,
	xil_status0_fpga_id__FIXED_IOFPGA_ELMDON = 24,

	xil_status0_fpga_id__DISTRIBUTED_BMC_FPGA_RP = 0x10,
	xil_status0_fpga_id__DISTRIBUTED_BMC_FPGA_LC = 0x11,
	xil_status0_fpga_id__DISTRIBUTED_RP_PEMBREY = 0x20,
	xil_status0_fpga_id__DISTRIBUTED_RP_ZENITH = 0x21,
	xil_status0_fpga_id__DISTRIBUTED_EXETER_GAUNTLET = 0x30,
	xil_status0_fpga_id__DISTRIBUTED_EXETER_CORSAIR = 0x31,
	xil_status0_fpga_id__DISTRIBUTED_EXETER_DAUNTLESS = 0x34,
	xil_status0_fpga_id__DISTRIBUTED_KENLEY_GAUNTLET = 0x40,
	xil_status0_fpga_id__DISTRIBUTED_KENLEY_CORSAIR = 0x41,
	xil_status0_fpga_id__DISTRIBUTED_KIRKWALL_VANGUARD = 0x42,
	xil_status0_fpga_id__DISTRIBUTED_KIRKWALL_LANCER = 0x43,
	xil_status0_fpga_id__DISTRIBUTED_REDCLIFF_DAUNTLESS = 0x44,
	xil_status0_fpga_id__DISTRIBUTED_WARMWELL = 0x50,
	xil_status0_fpga_id__DISTRIBUTED_FABRIC = 0x60,
	xil_status0_fpga_id__DISTRIBUTED_FABRIC_FOWLMERE = 0x61,

	xil_status0_fpga_id__CENTRAL_ALTUS = 1,
	xil_status0_fpga_id__CENTRAL_KOBLER = 2,
	xil_status0_fpga_id__CENTRAL_BFISH = 3,

	xil_status0_fpga_id__CENTRAL_CYCLONUS = 0x19,
	xil_status0_fpga_id__CENTRAL_SILVERBOLT = 0x68,
	xil_status0_fpga_id__CENTRAL_PINPOINTER = 0x69,
};
#define XIL_STATUS0_FPGA_INSTANCE xil, status0, fpga_instance, 19, 18, xil_regs_v5_t
#define XIL_STATUS0_PCIE_ID       xil, status0,       pcie_id, 15,  0, xil_regs_v5_t

#define XIL_STATUS1_BOARD_VER     xil, status1,     board_ver, 15, 12, xil_regs_v5_t
#define XIL_STATUS1_BOARD_TYPE    xil, status1,    board_type, 11,  8, xil_regs_v5_t

#define XIL_CFG1_GEN_CONF             xil, cfg1, raw, 31, 0, xil_regs_v5_t
#define XIL_CFG1_GEN_CONF_OUTSHIFTS   xil, cfg1, outshifts, 8, 8, xil_regs_v5_t
enum xil_cfg1_outshifts {
	xil_cfg1_outshifts__disable = 0,
	xil_cfg1_outshifts__enable = 1,
};
#define XIL_CFG1_GEN_CONF_CONSOLE     xil, cfg1, console, 13, 13, xil_regs_v5_t
enum xil_cfg1_console {
	xil_cfg1_console__jumper = 0,
	xil_cfg1_console__uxbar = 1,
};

#define XIL_CFG5                  xil,    cfg5,           raw, 31,  0, xil_regs_v5_t
#define XIL_CFG5_MASTER_SELECT    xil,    cfg5, master_select, 10, 10, xil_regs_v5_t
enum xil_cfg5_master_select {
	xil_cfg5_master_select__X86 = 0,
	xil_cfg5_master_select__BMC = 1,
};

#define XIL_CFG7                  xil,    cfg7,             raw, 31,  0, xil_regs_v5_t
#define XIL_CFG7_FPGA_WARM_RESET  xil,    cfg7, fpga_warm_reset, 11, 11, xil_regs_v5_t
#define XIL_CFG7_COLD_RESET       xil,    cfg7,      cold_reset, 10, 10, xil_regs_v5_t
#define XIL_CFG7_CPU_WARM_RESET   xil,    cfg7,  cpu_warm_reset,  9,  9, xil_regs_v5_t
#define XIL_CFG7_BMC_WARM_RESET   xil,    cfg7,  bmc_warm_reset,  8,  8, xil_regs_v5_t
#define XIL_CFG7_BMC_CYCLE        xil,    cfg7,       bmc_cycle,  7,  7, xil_regs_v5_t
#define XIL_CFG7_CPU_CYCLE        xil,    cfg7,       cpu_cycle,  6,  6, xil_regs_v5_t
#define XIL_CFG7_CPU_ON           xil,    cfg7,          cpu_on,  5,  5, xil_regs_v5_t
#define XIL_CFG7_BMC_OFF          xil,    cfg7,         bmc_off,  4,  4, xil_regs_v5_t
#define XIL_CFG7_CPU_OFF          xil,    cfg7,         cpu_off,  3,  3, xil_regs_v5_t
#define XIL_CFG7_CPU_BMC_OFF      xil,    cfg7,     cpu_bmc_off,  2,  2, xil_regs_v5_t

#define XIL_SCRATCHRAM            xil, scratchram,        raw, 31,  0, xil_regs_v5_t

/*
 * Overlay of scratchram
 */
struct xil_msd_scratchram_t {
	uint32_t bios_boot_mode;
	uint32_t chassis_info_valid;
	uint32_t chassis_pd_type;
	uint32_t chassis_hw_version;
	char     chassis_pid[20];
	char     chassis_sn[12];
	uint32_t chassis_rack_id;
	uint32_t bios_running_version;
	uint32_t uboot_running_version;
						/* offset 0x3c */
	uint8_t  pad[0x364];
						/* offset 0x3a0 */
	uint32_t idprom_info_valid;
	uint32_t idprom_tan_version;
						/* offset 0x3a8 */
	uint32_t idprom_pd_type;
	uint32_t idprom_hw_version;
	char     idprom_pid[20];
	char     idprom_sn[12];
						/* offset 0x3d0 */
	uint32_t bios_flash_select;
	uint8_t  uboot_mac_addr[12];
};

#endif /* ndef _CISCO_XIL_H */
