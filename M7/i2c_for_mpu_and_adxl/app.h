/*
 * Copyright 2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _APP_H_
#define _APP_H_

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* I2C5 (MPU6050) */
#define I2C5_MASTER_BASE        I2C5
#define I2C5_MASTER_CLK_FREQ                                                               \
    (CLOCK_GetPllFreq(kCLOCK_SystemPll1Ctrl) / (CLOCK_GetRootPreDivider(kCLOCK_RootI2c5)) / \
     (CLOCK_GetRootPostDivider(kCLOCK_RootI2c5)) / 5) /* SYSTEM PLL1 DIV5 */

/* I2C3 (ADXL345) */
#define I2C3_MASTER_BASE        I2C3
#define I2C3_MASTER_CLK_FREQ                                                               \
    (CLOCK_GetPllFreq(kCLOCK_SystemPll1Ctrl) / (CLOCK_GetRootPreDivider(kCLOCK_RootI2c3)) / \
     (CLOCK_GetRootPostDivider(kCLOCK_RootI2c3)) / 5) /* SYSTEM PLL1 DIV5 */

/* Back-compat: your existing code uses these. Keep them pointing at I2C5. */
#define EXAMPLE_I2C_MASTER_BASEADDR I2C5_MASTER_BASE
#define I2C_MASTER_CLK_FREQ         I2C5_MASTER_CLK_FREQ

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void BOARD_InitHardware(void);

#endif /* _APP_H_ */