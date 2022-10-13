/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Generic mdio device access
 *
 * Copyright (c) 2020, 2022 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */

#if !defined(CISCO_MDIO_ACCESS_H_)
#define CISCO_MDIO_ACCESS_H_

#include <linux/mdio-ioctl.h>

#define MDIO_ACCESS_MTK_CL45_REGRD _IOWR('c', 1, struct mdio_access_req)
#define MDIO_ACCESS_MTK_CL45_REGWR _IOWR('c', 2, struct mdio_access_req)
#define MDIO_ACCESS_MTK_REGRD _IOWR('c', 3, struct mdio_access_blk_req)
#define MDIO_ACCESS_MTK_REGWR _IOWR('c', 4, struct mdio_access_blk_req)

#endif /* !defined(CISCO_MDIO_ACCESS_H_) */
