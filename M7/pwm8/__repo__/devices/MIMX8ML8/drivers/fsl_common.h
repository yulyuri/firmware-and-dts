/*
 * Copyright (c) 2015-2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2022,2024-2025 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FSL_COMMON_H_
#define FSL_COMMON_H_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#if defined(__ICCARM__) || (defined(__CC_ARM) || defined(__ARMCC_VERSION)) || defined(__GNUC__)
#include <stddef.h>
#endif

#include "fsl_device_registers.h"

/*!
 * @addtogroup ksdk_common
 * @{
 */

/*******************************************************************************
 * Configurations
 ******************************************************************************/

/*! @brief Macro to use the default weak IRQ handler in drivers. */
#ifndef FSL_DRIVER_TRANSFER_DOUBLE_WEAK_IRQ
#define FSL_DRIVER_TRANSFER_DOUBLE_WEAK_IRQ 1
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*! @brief Construct a status code value from a group and code number. */
#define MAKE_STATUS(group, code) ((((group)*100L) + (code)))

/*! @brief Construct the version number for drivers.
 *
 * The driver version is a 32-bit number, for both 32-bit platforms(such as Cortex M)
 * and 16-bit platforms(such as DSC).
 *
 * @verbatim

   | Unused    || Major Version || Minor Version ||  Bug Fix    |
   31        25  24           17  16            9  8            0

   @endverbatim
 */
#define MAKE_VERSION(major, minor, bugfix) (((major)*65536L) + ((minor)*256L) + (bugfix))

/*! @name Driver version */
/*! @{ */
/*! @brief common driver version. */
#define FSL_COMMON_DRIVER_VERSION (MAKE_VERSION(2, 6, 0))
/*! @} */
 
/*! \public
 * @brief Generic status return codes.
 */
enum
{
    kStatus_Success         = MAKE_STATUS(kStatusGroup_Generic, 0), /*!< Generic status for Success. */
    kStatus_Fail            = MAKE_STATUS(kStatusGroup_Generic, 1), /*!< Generic status for Fail. */
    kStatus_ReadOnly        = MAKE_STATUS(kStatusGroup_Generic, 2), /*!< Generic status for read only failure. */
    kStatus_OutOfRange      = MAKE_STATUS(kStatusGroup_Generic, 3), /*!< Generic status for out of range access. */
    kStatus_InvalidArgument = MAKE_STATUS(kStatusGroup_Generic, 4), /*!< Generic status for invalid argument check. */
    kStatus_Timeout         = MAKE_STATUS(kStatusGroup_Generic, 5), /*!< Generic status for timeout. */
    kStatus_NoTransferInProgress =
        MAKE_STATUS(kStatusGroup_Generic, 6),            /*!< Generic status for no transfer in progress. */
    kStatus_Busy = MAKE_STATUS(kStatusGroup_Generic, 7), /*!< Generic status for module is busy. */
    kStatus_NoData =
        MAKE_STATUS(kStatusGroup_Generic, 8), /*!< Generic status for no data is found for the operation. */
};

/*! @brief Type used for all status and error return values. */
typedef int32_t status_t;

#ifdef __ZEPHYR__
#include <zephyr/sys/util.h>
#else
/*!
 * @name Min/max macros
 * @{
 */
#if !defined(MIN)
/*! Computes the minimum of \a a and \a b. */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#if !defined(MAX)
/*! Computes the maximum of \a a and \a b. */
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
/*! @} */

/*! @brief Computes the number of elements in an array. */
#if !defined(ARRAY_SIZE)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
#endif /* __ZEPHYR__ */

/*! @name UINT16_MAX/UINT32_MAX value */
/*! @{ */
#if !defined(UINT16_MAX)
/*! Max value of uint16_t type. */
#define UINT16_MAX ((uint16_t)-1)
#endif

#if !defined(UINT32_MAX)
/*! Max value of uint32_t type. */
#define UINT32_MAX ((uint32_t)-1)
#endif
/*! @} */

/*! Macro to get upper 32 bits of a 64-bit value */
#if !defined(UINT64_H)
#define UINT64_H(X)        ((uint32_t)((((uint64_t) (X)) >> 32U) & 0x0FFFFFFFFULL))
#endif

/*! Macro to get lower 32 bits of a 64-bit value */
#if !defined(UINT64_L)
#define UINT64_L(X)        ((uint32_t)(((uint64_t) (X)) & 0x0FFFFFFFFULL))
#endif

/*!
 * @def SUPPRESS_FALL_THROUGH_WARNING()
 *
 * For switch case code block, if case section ends without "break;" statement, there wil be
 * fallthrough warning with compiler flag -Wextra or -Wimplicit-fallthrough=n when using armgcc.
 * To suppress this warning, "SUPPRESS_FALL_THROUGH_WARNING();" need to be added at the end of each
 * case section which misses "break;"statement.
 */
#if defined(__GNUC__) && !defined(__ARMCC_VERSION)
#define SUPPRESS_FALL_THROUGH_WARNING() __attribute__((fallthrough))
#else
#define SUPPRESS_FALL_THROUGH_WARNING()
#endif

/*******************************************************************************
 * API
 ******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif

#if !((defined(__DSC__) && defined(__CW__)))
/*!
 * @brief Allocate memory with given alignment and aligned size.
 *
 * This is provided to support the dynamically allocated memory
 * used in cache-able region.
 * @param size The length required to malloc.
 * @param alignbytes The alignment size.
 * @retval The allocated memory.
 */
void *SDK_Malloc(size_t size, size_t alignbytes);

/*!
 * @brief Free memory.
 *
 * @param ptr The memory to be release.
 */
void SDK_Free(void *ptr);
#endif

/*!
 * @brief Delay at least for some time.
 *  Please note that, this API uses while loop for delay, different run-time environments make the time not precise,
 *  if precise delay count was needed, please implement a new delay function with hardware timer.
 *
 * @param delayTime_us  Delay time in unit of microsecond.
 * @param coreClock_Hz  Core clock frequency with Hz.
 */
void SDK_DelayAtLeastUs(uint32_t delayTime_us, uint32_t coreClock_Hz);

#if defined(__cplusplus)
}
#endif

/*! @} */

#if (defined(__DSC__) && defined(__CW__))
#include "fsl_common_dsc.h"
#elif defined(__XTENSA__)
#include "fsl_common_dsp.h"
#elif defined(__riscv)
#include "fsl_common_riscv.h"
#else
#include "fsl_common_arm.h"
#endif

#endif /* FSL_COMMON_H_ */
