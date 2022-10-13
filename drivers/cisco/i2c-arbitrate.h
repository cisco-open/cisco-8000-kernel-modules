/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Cisco I2C arbitration routines
 *
 * Copyright (c) 2022 by Cisco Systems, Inc.
 * All rights reserved.
 *
 */
#ifndef CISCO_I2C_ARBITRATE_H_
#define CISCO_I2C_ARBITRATE_H_

#include <linux/i2c.h>

struct device;
struct platform_device;
struct attribute_group;
struct regmap;
struct regmap_config;

struct cisco_i2c_arbitrate {
	u32	peer;		/* peer scratch register */
	u32	local;		/* local scratch register */
	u32	index;		/* arbitration index */

	u32	timeout_msecs;
	u32	peer_grant_msecs;
	u32	peer_retry_msecs;

	/* computed fields */
	struct device *info;	/* associated info block */
	u64	timeout_jiffies;
	u64	peer_grant_jiffies;
	u64	peer_retry_jiffies;

	/* statistics */
	u64	disputed;	/* arbitration required */
	u64	undisputed;	/* arbitration not required */

	u64	read_local_err;
	u64	write_local_err;

	u64	read_peer_err;
	u64	write_peer_err;

	u64	read_arb_err;
	u64	write_arb_err;

	u64	expires;
	u64	total_wait_msecs;
	u64	max_wait_msecs;
	u64	min_wait_msecs;
};

struct cisco_fpga_i2c {
	void __iomem *csr;
	struct regmap *regmap;
	struct cisco_i2c_arbitrate arb;
	struct rt_mutex *bus_lock;

	u32 func;

	u8 ver;
	u8 num_adapters;

	/* i2c_ext specific */
	u32 *rdata_ptr;
	u16 bufsize;

	struct i2c_adapter adap[0];	/* dynamic */
};


extern int cisco_i2c_init(struct platform_device *pdev,
			  const struct regmap_config *cfg,
			  const struct i2c_adapter *template,
			  const char *block_name);
extern int cisco_i2c_register(struct platform_device *pdev,
			      int (*reset)(struct i2c_adapter *adap,
					   struct cisco_fpga_i2c *hw));
extern struct attribute_group i2c_arbitrate_attr_group;

#endif /* ifndef CISCO_I2C_ARBITRATE_H_ */
