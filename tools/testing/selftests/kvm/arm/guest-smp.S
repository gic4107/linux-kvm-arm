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
	ldr	sp, =secondary_stacktop

	@ Enable the MMU
	mov	r0, #1
	bl	enable_mmu

	@ Tell the system we're up
	bl	smp_init

	@ Enable the GIC CPU interface
	mov	r0, r9
	mov	r1, r10
	bl	smp_gic_enable

	@ Enter pen
1:	cpsie	i	@ Enable Interrupts
	wfi
	mov	r0, #1
	bl	smp_test
	b	1b

.ltorg
