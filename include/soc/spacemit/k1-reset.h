/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * SpacemiT K1 reset driver — interface to the per-syscon clock drivers.
 *
 * The reset driver lives at drivers/reset/spacemit/reset-spacemit-k1.c
 * and is bound by name from each per-syscon U_BOOT_DRIVER in
 * drivers/clk/spacemit/clk-k1.c via device_bind_driver_to_node(). The
 * clock driver passes a syscon tag here so the reset driver can pick
 * the correct per-syscon signal table.
 *
 * Pattern follows drivers/reset/reset-rockchip.c.
 *
 * Copyright (C) 2026 RISCstar Ltd.
 */

#ifndef _SOC_SPACEMIT_K1_RESET_H
#define _SOC_SPACEMIT_K1_RESET_H

struct udevice;

enum spacemit_k1_reset_syscon {
	SPACEMIT_K1_RESET_MPMU,
	SPACEMIT_K1_RESET_APMU,
	SPACEMIT_K1_RESET_APBC,
	SPACEMIT_K1_RESET_APBC2,
};

int spacemit_k1_reset_bind(struct udevice *parent,
			   enum spacemit_k1_reset_syscon syscon);

#endif /* _SOC_SPACEMIT_K1_RESET_H */
