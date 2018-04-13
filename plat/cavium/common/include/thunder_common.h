/** @file

  Copyright (c) 2016-2017, Cavium Inc. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __THUNDER_COMMON_H__
#define __THUNDER_COMMON_H__

#include <stdint.h>
#include <xlat_tables.h>

#define MIDR_REVISION_MASK      0xf
#define MIDR_REVISION(midr)     ((midr) & MIDR_REVISION_MASK)
#define MIDR_PARTNUM_SHIFT      4
#define MIDR_PARTNUM_MASK       (0xfff << MIDR_PARTNUM_SHIFT)
#define MIDR_PARTNUM(midr)      \
        (((midr) & MIDR_PARTNUM_MASK) >> MIDR_PARTNUM_SHIFT)

#define T81PARTNUM 0xA2
#define T83PARTNUM 0xA3
#define T98PARTNUM 0xB1

int thunder_get_lmc_per_node(void);

int thunder_get_num_ecams_per_node(void);

int thunder_get_sata_count(void);
int thunder_sata_to_gser(int ctrlr);
int thunder_sata_to_lane(int ctrlr);
int thunder_get_max_sata_gser(void);

void add_map_record(unsigned long addr, unsigned long size, mmap_attr_t attr);

#endif /* __THUDNER_COMMON_H__ */
