/** @file

    Copyright (c) 2016-2017, Cavium Inc. All rights reserved.<BR>
    This program and the accompanying materials
    are licensed and made available under the terms and conditions of the BSD License
    which accompanies this distribution.  The full text of the license may be found at
    http://opensource.org/licenses/bsd-license.php

    THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
    WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __THUNDER_IO_H__
#define __THUNDER_IO_H__

#include <stdint.h>

#define PCI_MSIX_CAP_ID		0x11

struct msix_cap {
    uint8_t cap_ID;
    uint8_t next_pointer;
    uint16_t messagecontrol;
    uint32_t message_upper_address;
    uint32_t table_offset_and_bir;
} __attribute__((__packed__));

struct pcie_config {
    uint16_t         vendor_id;
    uint16_t         devid;
    uint16_t         command;
    uint16_t         status;
    int         revision_id : 8;
    int         class_code : 24;
    int         cache_line_size : 8;
    int         lat_timer : 8;
    int         header_type : 8;
    int         bist : 8;
    uint32_t         baseaddress_reg[6];
    int         card_bus_cis_pointer;
    uint16_t         sub_system_vendor_id;
    uint16_t         sub_system_id;
    int         exp_rom_base_address;
    int         cap_pointer : 8;
    int         res0 : 24;
    int         res1;
    int         intterrupt_line : 8;
    int         intterrupt_pin : 8;
    int         max_gnt : 8;
    int         max_lat : 8;
    int         res2[1008];  /* make it 4K boundary */
} __attribute__((__packed__));

#endif
