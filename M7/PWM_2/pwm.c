/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"
#include "fsl_pwm.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*! @brief PWM period value. PWMO (Hz) = PCLK(Hz) / (period +2) */
#define PWM_PERIOD_VALUE 24000

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
volatile uint32_t pwmDutycycle = 0U;
volatile bool pwmDutyUp        = true; /* Indicate PWM Duty cycle is increase or decrease */

/*******************************************************************************
 * Code
 ******************************************************************************/

// void DEMO_PWM_IRQHandler(void)
// {
//     /* Gets interrupt kPWM_FIFOEmptyFlag */
//     if (PWM_GetStatusFlags(DEMO_PWM_BASEADDR) & kPWM_FIFOEmptyFlag)
//     {
//         if (pwmDutyUp)
//         {
//             /* Increase duty cycle until it reach limited value. */
//             if (++pwmDutycycle > PWM_PERIOD_VALUE)
//             {
//                 pwmDutycycle = PWM_PERIOD_VALUE;
//                 pwmDutyUp    = false;
//             }
//         }
//         else
//         {
//             /* Decrease duty cycle until it reach limited value. */
//             if (--pwmDutycycle == 0U)
//             {
//                 pwmDutyUp = true;
//             }
//         }
//         /* Write duty cycle to PWM sample register.  */
//         PWM_SetSampleValue(DEMO_PWM_BASEADDR, pwmDutycycle);
//         /* Clear kPWM_FIFOEmptyFlag */
//         PWM_clearStatusFlags(DEMO_PWM_BASEADDR, kPWM_FIFOEmptyFlag);
//     }
//     SDK_ISR_EXIT_BARRIER;
// }
void DEMO_PWM_IRQHandler(void)
{
    if (PWM_GetStatusFlags(DEMO_PWM_BASEADDR) & kPWM_FIFOEmptyFlag)
    {
        /* ===== Tunables ===== */
        enum { DUTY_MIN_PCT = 10, DUTY_MAX_PCT = 90 };
        enum { FADE_TIME_MS = 250 };       // time to go min -> max (fast!)
        enum { HOLD_MS      = 80  };       // pause at ends (optional)

        /* At 1 kHz PWM, ISR fires every 1 ms */
        const uint32_t dutyMin = (uint32_t)( (uint64_t)DUTY_MIN_PCT * PWM_PERIOD_VALUE / 100U );
        const uint32_t dutyMax = (uint32_t)( (uint64_t)DUTY_MAX_PCT * PWM_PERIOD_VALUE / 100U );

        /* Step size (counts per ISR) so we complete the fade in FADE_TIME_MS */
        const uint32_t totalSpan = dutyMax - dutyMin;                 // counts
        const uint32_t step = (totalSpan / (FADE_TIME_MS ? FADE_TIME_MS : 1U)) ?: 1U;

        static bool goingUp = true;
        static uint32_t hold = 0;

        if (goingUp)
        {
            if (pwmDutycycle + step >= dutyMax) {
                pwmDutycycle = dutyMax;
                goingUp = false;
                hold = HOLD_MS;               // brief hold at top
            } else {
                pwmDutycycle += step;
            }
        }
        else
        {
            if (pwmDutycycle <= dutyMin + step) {
                pwmDutycycle = dutyMin;
                goingUp = true;
                hold = HOLD_MS;               // brief hold at bottom
            } else {
                pwmDutycycle -= step;
            }
        }

        /* Optional: consume the hold time (1 count per ISR at ~1 ms) */
        if (hold) {
            --hold;
            // While holding, keep reloading same duty so brightness stays put
        }

        PWM_SetSampleValue(DEMO_PWM_BASEADDR, pwmDutycycle);
        PWM_clearStatusFlags(DEMO_PWM_BASEADDR, kPWM_FIFOEmptyFlag);
    }
    SDK_ISR_EXIT_BARRIER;
}


// /*!
//  * @brief Main function
//  */
// int main(void)
// {
//     pwm_config_t pwmConfig;

//     /* Board pin, clock, debug console init */
//     BOARD_InitHardware();

//     PRINTF("\r\nPWM driver example.\r\n");

//     /*!
//      * config->enableStopMode = false;
//      * config->enableDozeMode = false;
//      * config->enableWaitMode = false;
//      * config->enableDebugMode = false;
//      * config->clockSource = kPWM_LowFrequencyClock;
//      * config->prescale = 0U;
//      * config->outputConfig = kPWM_SetAtRolloverAndClearAtcomparison;
//      * config->fifoWater = kPWM_FIFOWaterMark_2;
//      * config->sampleRepeat = kPWM_EachSampleOnce;
//      * config->byteSwap = kPWM_ByteNoSwap;
//      * config->halfWordSwap = kPWM_HalfWordNoSwap;
//      */
//     PWM_GetDefaultConfig(&pwmConfig);

//     /* Initialize PWM module */
//     PWM_Init(DEMO_PWM_BASEADDR, &pwmConfig);

//     /* Enable FIFO empty interrupt */
//     PWM_EnableInterrupts(DEMO_PWM_BASEADDR, kPWM_FIFOEmptyInterruptEnable);

//     /* Three initial samples be written to the PWM Sample Register */
//     for (pwmDutycycle = 0u; pwmDutycycle < 3; pwmDutycycle++)
//     {
//         PWM_SetSampleValue(DEMO_PWM_BASEADDR, pwmDutycycle);
//     }

//     /* Check and Clear interrupt status flags */
//     if (PWM_GetStatusFlags(DEMO_PWM_BASEADDR))
//     {
//         PWM_clearStatusFlags(DEMO_PWM_BASEADDR,
//                              kPWM_FIFOEmptyFlag | kPWM_RolloverFlag | kPWM_CompareFlag | kPWM_FIFOWriteErrorFlag);
//     }

//     /* Write the period to the PWM Period Register */
//     PWM_SetPeriodValue(DEMO_PWM_BASEADDR, PWM_PERIOD_VALUE);

//     /* Enable PWM interrupt request */
//     EnableIRQ(DEMO_PWM_IRQn);

//     PWM_StartTimer(DEMO_PWM_BASEADDR);

//     while (1)
//     {
//     }
// }
int main(void)
{
    pwm_config_t pwmConfig;

    /* Board pin, clock, debug console init */
    BOARD_InitHardware();

    PRINTF("\r\nPWM LED fade test 69 ! :)\r\n");

    /* Load default config, then override what we need */
    PWM_GetDefaultConfig(&pwmConfig);

    pwmConfig.clockSource = kPWM_PeripheralClock;   // Use high-speed peripheral clock (~24 MHz)
    pwmConfig.prescale = 0U;                        // No prescaler

    /* Initialize PWM module with updated config */
    PWM_Init(DEMO_PWM_BASEADDR, &pwmConfig);

    /* Enable FIFO empty interrupt */
    PWM_EnableInterrupts(DEMO_PWM_BASEADDR, kPWM_FIFOEmptyInterruptEnable);

    /* Preload a few samples (start at duty = 0%) */
    PWM_SetSampleValue(DEMO_PWM_BASEADDR, 0);
    PWM_SetSampleValue(DEMO_PWM_BASEADDR, 0);
    PWM_SetSampleValue(DEMO_PWM_BASEADDR, 0);

    /* Clear any pending flags */
    if (PWM_GetStatusFlags(DEMO_PWM_BASEADDR))
    {
        PWM_clearStatusFlags(DEMO_PWM_BASEADDR,
                             kPWM_FIFOEmptyFlag | kPWM_RolloverFlag |
                             kPWM_CompareFlag | kPWM_FIFOWriteErrorFlag);
    }

    /* Set period so frequency = ~1 kHz:
       f = 24e6 / (Period + 2)  => Period ≈ 24000
    */
    PWM_SetPeriodValue(DEMO_PWM_BASEADDR, 24000);

    /* Enable PWM interrupt in NVIC */
    EnableIRQ(DEMO_PWM_IRQn);

    /* Start PWM timer */
    PWM_StartTimer(DEMO_PWM_BASEADDR);

    while (1)
    {
        /* Idle, everything happens in ISR */
    }
}
