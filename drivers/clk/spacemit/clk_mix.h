// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 SpacemiT Technology Co. Ltd
 * Copyright (c) 2024-2025 Haylen Chu <heylenay@4d2.org>
 * Copyright (c) 2025 Junhui Liu <junhui.liu@pigmoral.tech>
 *  Authors: Haylen Chu <heylenay@4d2.org>
 */

#ifndef _CLK_MIX_H_
#define _CLK_MIX_H_

#include <linux/clk-provider.h>

#include "clk_common.h"

/**
 * struct ccu_gate_config - Gate configuration
 *
 * @mask:	Mask to enable the gate. Some clocks may have more than one bit
 *		set in this field.
 */
struct ccu_gate_config {
	u32 mask;
};

struct ccu_factor_config {
	u32 div;
	u32 mul;
};

struct ccu_mux_config {
	u8 shift;
	u8 width;
};

struct ccu_div_config {
	u8 shift;
	u8 width;
};

struct ccu_mix {
	struct ccu_factor_config factor;
	struct ccu_gate_config gate;
	struct ccu_div_config div;
	struct ccu_mux_config mux;
	struct ccu_common common;
};

#define CCU_GATE_INIT(_mask)		{ .mask = _mask }
#define CCU_FACTOR_INIT(_div, _mul)	{ .div = _div, .mul = _mul }
#define CCU_MUX_INIT(_shift, _width)	{ .shift = _shift, .width = _width }
#define CCU_DIV_INIT(_shift, _width)	{ .shift = _shift, .width = _width }

#define CCU_GATE_DEFINE(_name, _parent, _reg_ctrl, _mask_gate, _flags)		\
static struct ccu_mix _name = {							\
	.gate	= CCU_GATE_INIT(_mask_gate),					\
	.common	= {								\
		.reg_ctrl	= _reg_ctrl,					\
		CCU_COMMON(_name, _parent, spacemit_gate_init, _flags)		\
	}									\
}

#define CCU_FACTOR_DEFINE(_name, _parent, _div, _mul)				\
static struct ccu_mix _name = {							\
	.factor	= CCU_FACTOR_INIT(_div, _mul),					\
	.common = {								\
		CCU_COMMON(_name, _parent, spacemit_factor_init, 0)		\
	}									\
}

#define CCU_MUX_DEFINE(_name, _parents, _reg_ctrl, _shift, _width, _flags)	\
static struct ccu_mix _name = {							\
	.mux	= CCU_MUX_INIT(_shift, _width),					\
	.common	= {								\
		.reg_ctrl	= _reg_ctrl,					\
		CCU_COMMON_PARENTS(_name, _parents, spacemit_mux_init,		\
				   _flags)					\
	}									\
}

#define CCU_DIV_DEFINE(_name, _parent, _reg_ctrl, _shift, _width, _flags)	\
static struct ccu_mix _name = {							\
	.div	= CCU_DIV_INIT(_shift, _width),					\
	.common = {								\
		.reg_ctrl	= _reg_ctrl,					\
		CCU_COMMON(_name, _parent, spacemit_div_init, _flags)		\
	}									\
}

#define CCU_FACTOR_GATE_FLAGS_DEFINE(_name, _parent, _reg_ctrl, _mask_gate,	\
				     _div, _mul, _flags)			\
static struct ccu_mix _name = {							\
	.gate	= CCU_GATE_INIT(_mask_gate),					\
	.factor	= CCU_FACTOR_INIT(_div, _mul),					\
	.common = {								\
		.reg_ctrl	= _reg_ctrl,					\
		CCU_COMMON(_name, _parent, spacemit_factor_gate_init, _flags)	\
	}									\
}

#define CCU_FACTOR_GATE_DEFINE(_name, _parent, _reg_ctrl, _mask_gate, _div,	\
			       _mul)						\
	CCU_FACTOR_GATE_FLAGS_DEFINE(_name, _parent, _reg_ctrl, _mask_gate,	\
				     _div, _mul, 0)

#define CCU_MUX_GATE_DEFINE(_name, _parents, _reg_ctrl, _shift, _width,		\
			    _mask_gate, _flags)					\
static struct ccu_mix _name = {							\
	.gate	= CCU_GATE_INIT(_mask_gate),					\
	.mux	= CCU_MUX_INIT(_shift, _width),					\
	.common	= {								\
		.reg_ctrl	= _reg_ctrl,					\
		CCU_COMMON_PARENTS(_name, _parents,				\
				   spacemit_mux_gate_init, _flags)		\
	}									\
}

#define CCU_DIV_GATE_DEFINE(_name, _parent, _reg_ctrl, _shift, _width,		\
			    _mask_gate,	_flags)					\
static struct ccu_mix _name = {							\
	.gate	= CCU_GATE_INIT(_mask_gate),					\
	.div	= CCU_DIV_INIT(_shift, _width),					\
	.common = {								\
		.reg_ctrl	= _reg_ctrl,					\
		CCU_COMMON(_name, _parent, spacemit_div_gate_init, _flags)	\
	}									\
}

#define CCU_MUX_DIV_GATE_DEFINE(_name, _parents, _reg_ctrl, _mshift, _mwidth,	\
				 _muxshift, _muxwidth, _mask_gate, _flags)	\
static struct ccu_mix _name = {							\
	.gate	= CCU_GATE_INIT(_mask_gate),					\
	.div	= CCU_DIV_INIT(_mshift, _mwidth),				\
	.mux	= CCU_MUX_INIT(_muxshift, _muxwidth),				\
	.common	= {								\
		.reg_ctrl	= _reg_ctrl,					\
		CCU_COMMON_PARENTS(_name, _parents,				\
				   spacemit_mux_div_gate_init, _flags)		\
	},									\
}

#define CCU_MUX_DIV_GATE_SPLIT_FC_DEFINE(_name, _parents, _reg_ctrl, _reg_fc,	\
					 _mshift, _mwidth, _mask_fc, _muxshift,	\
					 _muxwidth, _mask_gate, _flags)		\
static struct ccu_mix _name = {							\
	.gate	= CCU_GATE_INIT(_mask_gate),					\
	.div	= CCU_DIV_INIT(_mshift, _mwidth),				\
	.mux	= CCU_MUX_INIT(_muxshift, _muxwidth),				\
	.common = {								\
		.reg_ctrl	= _reg_ctrl,					\
		.reg_fc		= _reg_fc,					\
		.mask_fc	= _mask_fc,					\
		CCU_COMMON_PARENTS(_name, _parents,				\
				   spacemit_mux_div_gate_init, _flags)	\
	},									\
}

#define CCU_MUX_DIV_GATE_FC_DEFINE(_name, _parents, _reg_ctrl, _mshift, _mwidth,\
				   _mask_fc, _muxshift, _muxwidth, _mask_gate,	\
				   _flags)					\
CCU_MUX_DIV_GATE_SPLIT_FC_DEFINE(_name, _parents, _reg_ctrl, _reg_ctrl, _mshift,\
				 _mwidth, _mask_fc, _muxshift, _muxwidth,	\
				 _mask_gate, _flags)

#define CCU_MUX_DIV_FC_DEFINE(_name, _parents, _reg_ctrl, _mshift, _mwidth,	\
			      _mask_fc, _muxshift, _muxwidth, _flags)		\
static struct ccu_mix _name = {							\
	.div	= CCU_DIV_INIT(_mshift, _mwidth),				\
	.mux	= CCU_MUX_INIT(_muxshift, _muxwidth),				\
	.common = {								\
		.reg_ctrl	= _reg_ctrl,					\
		.reg_fc		= _reg_ctrl,					\
		.mask_fc	= _mask_fc,					\
		CCU_COMMON_PARENTS(_name, _parents,				\
				   spacemit_mux_div_init, _flags)		\
	},									\
}

#define CCU_MUX_FC_DEFINE(_name, _parents, _reg_ctrl, _mask_fc,	_muxshift,	\
			  _muxwidth, _flags)					\
static struct ccu_mix _name = {							\
	.mux	= CCU_MUX_INIT(_muxshift, _muxwidth),				\
	.common = {								\
		.reg_ctrl	= _reg_ctrl,					\
		.reg_fc		= _reg_ctrl,					\
		.mask_fc	= _mask_fc,					\
		CCU_COMMON_PARENTS(_name, _parents, spacemit_mux_init,	\
				   _flags)					\
	},									\
}

static inline struct ccu_mix *clk_to_ccu_mix(struct clk *clk)
{
	struct ccu_common *common = clk_to_ccu_common(clk);

	return container_of(common, struct ccu_mix, common);
}

int spacemit_gate_init(struct ccu_common *common);
int spacemit_factor_init(struct ccu_common *common);
int spacemit_mux_init(struct ccu_common *common);
int spacemit_div_init(struct ccu_common *common);
int spacemit_factor_gate_init(struct ccu_common *common);
int spacemit_div_gate_init(struct ccu_common *common);
int spacemit_mux_gate_init(struct ccu_common *common);
int spacemit_mux_div_init(struct ccu_common *common);
int spacemit_mux_div_gate_init(struct ccu_common *common);

#endif /* _CLK_MIX_H_ */
