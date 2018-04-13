#ifndef __CAVM_ARCH_H__
#define __CAVM_ARCH_H__

/**
 * @file
 *
 * Master include file for architecture support. Use cavm.h
 * instead of including this file directly.
 *
 * <hr>$Revision: 49448 $<hr>
 */

#ifndef __BYTE_ORDER
    #if !defined(__ORDER_BIG_ENDIAN__) || !defined(__ORDER_LITTLE_ENDIAN__) || !defined(__BYTE_ORDER__)
        #error Unable to determine Endian mode
    #elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        #define __BYTE_ORDER __ORDER_BIG_ENDIAN__
    #elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        #define __BYTE_ORDER __ORDER_LITTLE_ENDIAN__
    #else
        #error Unable to determine Endian mode
    #endif
    #define __BIG_ENDIAN    __ORDER_BIG_ENDIAN__
    #define __LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#endif

#include "cavm-require.h"
#include "cavm-swap.h"
#ifndef CAVM_BUILD_HOST
#include "cavm-asm.h"
#endif
#include "cavm-model.h"
#include "cavm-numa.h"
#include "cavm-csr.h"
#include "cavm-lmt.h"
#include "cavm-csrs-trickbox.h"
#include "cavm-warn.h"
#include "cavm-csrs.h"
#include "cavm-version.h"
#ifndef CAVM_BUILD_HOST
#include "cavm-fuse.h"
#include "cavm-platform.h"
#endif

#endif
