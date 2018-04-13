#/** @file
#
#  Copyright (c) 2016-2017, Cavium Inc. All rights reserved.<BR>
#  This program and the accompanying materials
#  are licensed and made available under the terms and conditions of the BSD License
#  which accompanies this distribution.  The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
#**/

BL32_SOURCES		+=	plat/cavium/common/tsp/thunder_tsp_setup.c	\
				plat/cavium/common/thunder_topology.c		\
				plat/cavium/common/aarch64/thunder_helpers.S	\
				plat/common/aarch64/platform_mp_stack.S		\
				plat/cavium/common/thunder_gicv3.c		\
				plat/common/plat_gicv3.c
