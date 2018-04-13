/** @file

  Copyright (c) 2016-2017, Cavium Inc. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <arch.h>
#include <platform_def.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <thunder_dt.h>
#include <thunder_private.h>
#include <debug.h>

#include <libfdt.h>

board_fdt_t bfdt;

static void print_board_variables()
{
	INFO("======================\n");
	INFO("BOARD MODEL = %s\n", bfdt.board_model);
	INFO("BMC BOOT TWSI node=%d bus=0x%x, addr=0x%x\n",
		bfdt.bmc_boot_twsi_node,
		bfdt.bmc_boot_twsi_bus,
		bfdt.bmc_boot_twsi_addr);
	INFO("BMC IPMI TWSI node=%d bus=0x%x, addr=0x%x\n",
		bfdt.bmc_ipmi_twsi_node,
		bfdt.bmc_ipmi_twsi_bus,
		bfdt.bmc_ipmi_twsi_addr);
	INFO("GPIO Shutdown pin IN = 0x%x\n",
		bfdt.gpio_shutdown_ctl_in);
	INFO("GPIO Shutdown pin OUT = 0x%x\n",
		bfdt.gpio_shutdown_ctl_out);
#if TRUSTED_BOARD_BOOT
	INFO("TRUST-ROT-ADDR = 0x%lx\n",
		bfdt.trust_rot_addr);
	INFO("TRUST-BSSK-ADDR = 0x%lx\n",
		bfdt.trust_key_addr);
#endif
	INFO("======================\n");

}

#if TRUSTED_BOARD_BOOT
static uint64_t thunder_fdt_get_uint64(const void *fdt, int offset, const char *property, int base)
{
	const char *name;
	int len;

	name = fdt_getprop(fdt, offset, property, &len);
	if (!name) {
		WARN("No %s is found\n", property);
		return 0;
	} else {
		return (uint64_t)strtol(name, NULL, base);
	}
}
#endif

static int thunder_fdt_get(const void *fdt, int offset, const char *property, int base)
{
	const char *name;
	int len;

	name = fdt_getprop(fdt, offset, property, &len);
	if (!name) {
		WARN("No %s is found\n", property);
		return -1;
	} else {
		return (int)strtol(name, NULL, base);
	}

}

int thunder_fill_board_details(int info)
{
	const void *fdt = fdt_ptr;
	const char *name;
	int offset, len, config;

	if (fdt_check_header(fdt)) {
		printf("WARNING: Invalid device tree\n");
		return -1;
	}

	offset = fdt_path_offset(fdt, "/cavium,bdk");
	if (offset < 0) {
		printf("WARNING: FDT node not found\n");
		return offset;
	}

	name = fdt_getprop(fdt, offset, "BOARD-MODEL", &len);
	if (!name) {
		printf("WARNING: No BOARD-MODEL is found\n");
		bfdt.board_model[0] = '\0';
	} else {
		strncpy(bfdt.board_model, name, sizeof(bfdt.board_model));
		/* Terminate, in case strlen(name) > sizeof(board_model) */
		bfdt.board_model[sizeof(bfdt.board_model) - 1] = '\0';
		len = strlen(bfdt.board_model);
		for (int i = 0; i < len; i++)
			bfdt.board_model[i] = tolower(bfdt.board_model[i]);
	}

	config = thunder_fdt_get(fdt, offset, "BMC-BOOT-TWSI-CONFIG", 0);
	if (config != -1) {
		bfdt.bmc_boot_twsi_node = (config >> 16) & 0xff;
		bfdt.bmc_boot_twsi_bus = (config >> 8) & 0xff;
		bfdt.bmc_boot_twsi_addr = config & 0xff;
	} else {
		bfdt.bmc_boot_twsi_node = 0;
		bfdt.bmc_boot_twsi_bus = thunder_fdt_get(fdt, offset, "BMC-BOOT-TWSI-BUS", 10);
		bfdt.bmc_boot_twsi_addr = thunder_fdt_get(fdt, offset, "BMC-BOOT-TWSI-ADDR", 16);
	}
	config = thunder_fdt_get(fdt, offset, "BMC-IPMI-TWSI-CONFIG", 0);
	if (config != -1) {
		bfdt.bmc_ipmi_twsi_node = (config >> 16) & 0xff;
		bfdt.bmc_ipmi_twsi_bus = (config >> 8) & 0xff;
		bfdt.bmc_ipmi_twsi_addr = config & 0xff;
	} else {
		bfdt.bmc_ipmi_twsi_node = 0;
		bfdt.bmc_ipmi_twsi_bus = thunder_fdt_get(fdt, offset, "BMC-IPMI-TWSI-BUS", 10);
		bfdt.bmc_ipmi_twsi_addr = thunder_fdt_get(fdt, offset, "BMC-IPMI-TWSI-ADDR", 16);
	}

	bfdt.gpio_shutdown_ctl_in = thunder_fdt_get(fdt, offset, "GPIO-SHUTDOWN-CTL-IN", 0);
	/* The new format is hex and allows for node id and polarity
	 * packed into the value. We don't use them right now and mask
	 * them out. */
	if (bfdt.gpio_shutdown_ctl_in != -1)
		bfdt.gpio_shutdown_ctl_in &= 0xff;

	bfdt.gpio_shutdown_ctl_out = thunder_fdt_get(fdt, offset, "GPIO-SHUTDOWN-CTL-OUT", 0);
	/* The new format is hex and allows for node id and polarity
	 * packed into the value. We don't use them right now and mask
	 * them out. */
	if (bfdt.gpio_shutdown_ctl_out != -1)
		bfdt.gpio_shutdown_ctl_out &= 0xff;

#if TRUSTED_BOARD_BOOT
	/*
	 * Configuration for Trusted Board Boot received from BDK.
	 * For more information, please refer to bdk-trusted-boot.pdf file
	 * in BDK source repository.
	 */
	bfdt.trust_rot_addr = thunder_fdt_get_uint64(fdt, offset, "TRUST-ROT-ADDR", 16);

	/*
	 * If either thunder_fdt_get_uint64 returned -1 (property not found) or
	 * 0 (property found, but set to 0 indicates non-secure boot), stop secure booting
	 */
	if (bfdt.trust_rot_addr == 0 || bfdt.trust_rot_addr == -1) {
		printf("ERROR: No TRUST-ROT-ADDR in DTS, trusted boot failed\n");
		return -1;
	}

#if CRYPTO_BOARD_BOOT
	/*
	 * TRUST-BSSK-ADDR is set only when HUK parameter was passed to build system.
	 * If so, it contains the address of BSSK key. Firstly, try to get BSSK
	 */
	bfdt.trust_key_addr = thunder_fdt_get_uint64(fdt, offset, "TRUST-BSSK-ADDR", 16);

	/*
	 * If either thunder_fdt_get_uint64 returned -1 (property not found) or
	 * 0 (property found, but set to 0 indicates SSK), print info about it
	 */
	if (bfdt.trust_key_addr == 0 || bfdt.trust_rot_addr == -1) {
		printf("INFO: No TRUST-BSSK-ADDR in DTS, images will be decrypted\n"
		       "      with SSK key from FUSF_SSKX(0..1).\n");
		bfdt.trust_key_addr = 0;
	}
#endif /* CRYPTO_BOARD_BOOT */

#endif /* TRUSTED_BOARD_BOOT */
	if (info)
		print_board_variables();

	return 0;
}

uint64_t thunder_get_ecam_config_addr(int node, int ecam)
{
        const void *fdt = fdt_ptr;
        int offset = 0, dt_node, ret = 0, len;
        uint64_t val = 0, range = 0;
        uint64_t addr = CSR_PA(0, CAVM_ECAMX_PF_BAR2(ecam));
        const char *nodename;
        char name[32], *socname;
        const uint32_t *prop;


        if(fdt_check_header(fdt)) {
                printf("ERROR: Invalid device tree\n");
                return -1;
        }

        socname = node ? "/soc@100000000000" : "/soc@0"; 
        offset = fdt_path_offset(fdt, socname);
        if(offset < 0) {
                printf("ERROR: FDT node /soc not found\n");
                return -1;
        }

        /* Get the ranges property for soc node*/
        if(node) {
                prop = fdt_getprop(fdt, offset, "ranges", &len);
                if(prop) {
                        range = fdt64_to_cpu(*(prop + 2));
                        if(!range) {
                                printf("ERROR : range value not valid 0x%lx\n", range);
                                return -1;
                        }
                }
        }

        for(dt_node = offset; dt_node >= 0; dt_node = fdt_next_node(fdt, dt_node, NULL)) {
		nodename = fdt_get_name(fdt, dt_node, &ret);
                snprintf(name, sizeof(name), "pci@%lx", addr);
                if(!strcmp(nodename, name)) {
                        prop = fdt_getprop(fdt, dt_node, "reg", &len);
                        if(prop) {
                                val = fdt64_to_cpu(*prop) | range;
                                break;
                        }
                }
        }

        if(!val)
                printf("ERROR : No valid ecam config read from DT node %d ecam %d\n",
                                node, ecam);

        return val;
}

uint64_t thunder_get_ecam_config_size(int node, int ecam)
{
        const void *fdt = fdt_ptr;
        int offset = 0, dt_node, ret = 0, len;
        uint64_t val = 0;
        uint64_t addr = CSR_PA(0, CAVM_ECAMX_PF_BAR2(ecam));
        const char *nodename;
        char name[32], *socname;
        const uint32_t *prop;

        if(fdt_check_header(fdt)) {
                printf("ERROR: Invalid device tree\n");
                return -1;
        }

        socname = node ? "/soc@100000000000" : "/soc@0";
        offset = fdt_path_offset(fdt, socname);
        if(offset < 0) {
                printf("ERROR: FDT node /soc not found\n");
                return -1;
        }

        for(dt_node = offset; dt_node >= 0; dt_node = fdt_next_node(fdt, dt_node, NULL)) {
                nodename = fdt_get_name(fdt, dt_node, &ret);
                snprintf(name, sizeof(name), "pci@%lx", addr);
                if(!strcmp(nodename, name))
                {
                        prop = fdt_getprop(fdt, dt_node, "reg", &len);
                        if(prop) {
                                val = fdt32_to_cpu(*(prop + 3));
                                break;
                        }
                }
        }

        if(!val)
                printf("ERROR : No valid ecam config read from DT node %d ecam %d\n",
                                node, ecam);

        return val;
}

