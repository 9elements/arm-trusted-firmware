/** @file

  Copyright (c) 2016-2017, Cavium Inc. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <bl_common.h>
#include <console.h>
#include <platform_def.h>
#include <platform_tsp.h>
#include <xlat_tables.h>
#include <thunder_private.h>
#include <thunder_io.h>


/*
 * The next 3 constants identify the extents of the code & RO data region and
 * the limit of the BL32 image. These addresses are used by the MMU setup code
 * and therefore they must be page-aligned.  It is the responsibility of the
 * linker script to ensure that __RO_START__, __RO_END__ & & __BL32_END__
 * linker symbols refer to page-aligned addresses.
 */
#define BL32_RO_BASE (unsigned long)(&__RO_START__)
#define BL32_RO_LIMIT (unsigned long)(&__RO_END__)
#define BL32_END (unsigned long)(&__BL32_END__)

#if USE_COHERENT_MEM
/*
 * The next 2 constants identify the extents of the coherent memory region.
 * These addresses are used by the MMU setup code and therefore they must be
 * page-aligned.  It is the responsibility of the linker script to ensure that
 * __COHERENT_RAM_START__ and __COHERENT_RAM_END__ linker symbols refer to
 * page-aligned addresses.
 */
#define BL32_COHERENT_RAM_BASE (unsigned long)(&__COHERENT_RAM_START__)
#define BL32_COHERENT_RAM_LIMIT (unsigned long)(&__COHERENT_RAM_END__)
#endif


/*******************************************************************************
 * Initialize the UART
 ******************************************************************************/

void tsp_early_platform_setup(void)
{
	/*
	 * Initialize a different console than already in use to display
	 * messages from TSP
	 */
	console_init(CSR_PA(0, CAVM_UAAX_PF_BAR0(0)), 0, 0);
}

/*******************************************************************************
 * Perform platform specific setup placeholder
 ******************************************************************************/
void tsp_platform_setup(void)
{
	thunder_gic_driver_init();
}

/*******************************************************************************
 * Perform the very early platform specific architectural setup here. At the
 * moment this is only intializes the MMU
 ******************************************************************************/
void tsp_plat_arch_setup(void)
{
	mmap_add_region(BL32_RO_BASE, BL32_RO_BASE,
			BL32_END - BL32_RO_BASE,
			MT_MEMORY | MT_RW | MT_SECURE);
	mmap_add_region(BL32_RO_BASE, BL32_RO_BASE,
			BL32_RO_LIMIT - BL32_RO_BASE,
			MT_MEMORY | MT_RO | MT_SECURE);

#if USE_COHERENT_MEM
	mmap_add_region(BL32_COHERENT_RAM_BASE, BL32_COHERENT_RAM_BASE,
			BL32_COHERENT_RAM_LIMIT - BL32_COHERENT_RAM_BASE,
			MT_MEMORY | MT_RW | MT_SECURE);
#endif

	plat_add_mmio_map();

	init_xlat_tables();

	enable_mmu_el1(0);
}
