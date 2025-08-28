/*
 * Copyright (c) 2013 - 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017, 2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"
#include <math.h>
#include <stdio.h>
/* Angle simulation */
float t = 0.0f;
float pitch, roll, yaw;
int main(void)
{
    BOARD_InitHardware();
    PRINTF("Starting simulated IMU output...\r\n");

    float t = 0.0f;

float t = 0.0f;

while (1)
{
    float pitch = 0.0f, roll = 0.0f, yaw = 0.0f;
    int phase = ((int)t / 5) % 3;  // Change phase every 5 seconds, total 3 phases

    switch (phase)
    {
        case 0:
            // Diagonal forward
            pitch = fmodf(t * 5.0f, 360.0f);
            roll  = fmodf(t * 5.0f, 360.0f);
            yaw   = fmodf(t * 5.0f, 360.0f);
            break;

        case 1:
            // Reverse diagonal
            pitch = fmodf(-t * 5.0f + 360.0f, 360.0f);
            roll  = fmodf(-t * 5.0f + 360.0f, 360.0f);
            yaw   = fmodf(-t * 5.0f + 360.0f, 360.0f);
            break;

        case 2:
            // Alternate axis spin
            pitch = 0.0f;
            roll  = fmodf(t * 5.0f, 360.0f);
            yaw   = fmodf(-t * 5.0f + 360.0f, 360.0f);
            break;
    }

    PRINTF("%.2f, %.2f, %.2f\r\n", roll, pitch, yaw);

    for (volatile int i = 0; i < 800000; ++i);

    t += 0.05f;
}
}

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
// /*!
//  * @brief Main function
//  */
// int main(void)
// {
//     char ch;

//     /* Init board hardware. */
//     BOARD_InitHardware();

//     PRINTF("hello world.\r\n");

//     while (1)
//     {
//         ch = GETCHAR();
//         PUTCHAR(ch);
//     }
// }
