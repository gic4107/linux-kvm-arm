/*
 * guest-base.S - world's simplest SMP VM os
 * Copyright (C) 2013 Christoffer Dall <cdall@cs.columbia.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "io_common.h"



	.arm
	.text

	.globl secondary_start
secondary_start:
	@ Set up stack
	ldr	sp, =stacktop_secondary

	@ Enable the GIC CPU interface
	bl	smp_gic_enable

	@ Tell the system we have two CPUs
	bl	smp_init

	@ Enter pen
	cpsie	i	@ Enable Interrupts
1:	wfi
	@bl	smp_test
	b	1b

.ltorg


/* Have some local stack space for the second cpu */
	.rept 1024
	.word 0
	.endr
stacktop_secondary:
	.word 0
