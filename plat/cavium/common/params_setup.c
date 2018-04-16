/*
 * Copyright (c) 2016, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <bl_common.h>
#include <debug.h>
#include <plat_params.h>
#include <thunder_private.h>

void params_early_setup(void *plat_param_from_bl2)
{
	struct bl31_plat_param *bl31_param;
	struct bl31_fdt_param *fdt_param;
#if COREBOOT
	struct bl31_u64_param *u64_param;
#endif

	/* keep plat parameters for later processing if need */
	bl31_param = (struct bl31_plat_param *)plat_param_from_bl2;

	while (bl31_param) {
		switch (bl31_param->type) {
		case PARAM_FDT:
			fdt_param = (struct bl31_fdt_param *)bl31_param;
			fdt_ptr = (void *)fdt_param->fdt_ptr;
			break;
#if COREBOOT
		case PARAM_COREBOOT_TABLE:
			u64_param = (struct bl31_u64_param *)bl31_param;
			coreboot_table_setup((void *)u64_param->value);
			break;
#endif
		default:
			ERROR("not expected type found %ld\n", bl31_param->type);
			break;
		}
		bl31_param = bl31_param->next;
	}
}
