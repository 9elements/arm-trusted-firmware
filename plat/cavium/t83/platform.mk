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

# Include common thunder platform's makefile helper
include plat/cavium/common/platform.mk

PLAT_BL_COMMON_SOURCES	+=	plat/cavium/t83/thunder_setup_cn83xx.c		\

BL2_SOURCES		+=	plat/cavium/t83/thunder_ecam_cn83xx.c		\
