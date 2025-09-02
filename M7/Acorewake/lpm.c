/* lpm.c
 *
 * Copyright 2020-2022 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "board.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
#include "lpm.h"
#include "fsl_gpt.h"
#include "fsl_gpc.h"
#include "fsl_debug_console.h"
#include "app_srtm.h"

#define SYSTICK_COUNTER_SOURCE_CLK_FREQ (SYSTICK_CLOCK)
#define SYSTICK_COUNTER_FREQ (8000000U)
#define SYSTICK_COUNT_PER_TICK (SYSTICK_COUNTER_FREQ / configTICK_RATE_HZ)

#ifndef BOARD_MU_IRQ_NUM
#define BOARD_MU_IRQ_NUM BOARD_MU_IRQ
#endif
volatile uint32_t g_tick_isr_hits = 0;
volatile uint32_t g_tick_last_cnt = 0;


static uint32_t s_BlockEventCnt = 0;
extern void xPortSysTickHandler(void);

static LPM_POWER_STATUS_MCORE mcore_lpm_state = LPM_MCORE_STATE_RUN;

extern volatile app_srtm_state_t srtmState;

void vPortSetupTimerInterrupt(void)
{
    gpt_config_t config;
    GPT_GetDefaultConfig(&config);
    config.enableRunInDoze = true;
    config.clockSource     = kGPT_ClockSource_Osc;

    GPT_Init(SYSTICK_BASE, &config);
    GPT_SetOscClockDivider(SYSTICK_BASE, SYSTICK_COUNTER_SOURCE_CLK_FREQ / SYSTICK_COUNTER_FREQ);
    GPT_SetOutputCompareValue(SYSTICK_BASE, kGPT_OutputCompare_Channel1, SYSTICK_COUNT_PER_TICK - 1U);
    GPT_EnableInterrupts(SYSTICK_BASE, kGPT_OutputCompare1InterruptEnable);

    NVIC_SetPriority(SYSTICK_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(SYSTICK_IRQn);
    GPT_StartTimer(SYSTICK_BASE);
}

uint32_t LPM_EnterTicklessIdle(uint32_t timeoutMilliSec, uint64_t *pCounter)
{
    uint32_t counter, expired = 0;
    uint32_t ms, maxMS;
    uint32_t flag;
    uint32_t timeoutTicks;
    uint32_t countPerTick = SYSTICK_COUNT_PER_TICK;

    if ((uint64_t)timeoutMilliSec * configTICK_RATE_HZ < 2 * 1000)
    {
        return 0;
    }

    maxMS = 0xFFFFFFFFU / SYSTICK_COUNTER_FREQ * 1000;
    ms    = timeoutMilliSec > maxMS ? maxMS : timeoutMilliSec;

    timeoutTicks = (uint64_t)ms * configTICK_RATE_HZ / 1000;
    counter      = timeoutTicks * countPerTick;

    GPT_StopTimer(SYSTICK_BASE);
    flag = GPT_GetStatusFlags(SYSTICK_BASE, kGPT_OutputCompare1Flag);
    if (flag)
    {
        GPT_ClearStatusFlags(SYSTICK_BASE, kGPT_OutputCompare1Flag);
        NVIC_ClearPendingIRQ(SYSTICK_IRQn);
        expired = countPerTick;
    }
    expired += GPT_GetCurrentTimerCount(SYSTICK_BASE);
    counter -= expired;

    SYSTICK_BASE->CR |= GPT_CR_FRR_MASK;
    GPT_SetOutputCompareValue(SYSTICK_BASE, kGPT_OutputCompare_Channel1, counter - 1UL);
    GPT_StartTimer(SYSTICK_BASE);

    *pCounter = counter;

    return timeoutTicks;
}

void LPM_ExitTicklessIdle(uint32_t timeoutTicks, uint64_t timeoutCounter)
{
    uint32_t flag, counter, expired, expiredTicks;
    uint32_t completeTicks;
    uint32_t countPerTick = SYSTICK_COUNT_PER_TICK;

    GPT_StopTimer(SYSTICK_BASE);
    flag = GPT_GetStatusFlags(SYSTICK_BASE, kGPT_OutputCompare1Flag);
    expired = GPT_GetCurrentTimerCount(SYSTICK_BASE);

    if (flag)
    {
        expiredTicks  = (expired - timeoutCounter) / countPerTick;
        completeTicks = expiredTicks + timeoutTicks - 1;
        counter = countPerTick - (expired - timeoutCounter) % countPerTick;
    }
    else
    {
        counter       = timeoutCounter - expired;
        completeTicks = timeoutTicks - (counter - 1) / countPerTick - 1;
        counter       = (counter - 1) % countPerTick + 1;
    }

    SYSTICK_BASE->CR &= ~GPT_CR_FRR_MASK;
    GPT_SetOutputCompareValue(SYSTICK_BASE, kGPT_OutputCompare_Channel1, counter - 1UL);
    GPT_StartTimer(SYSTICK_BASE);

    vTaskStepTick(completeTicks);
}

void SYSTICK_HANDLER(void)
{
    /* Clear compare flag first */
    GPT_ClearStatusFlags(SYSTICK_BASE, kGPT_OutputCompare1Flag);

    /* Restore default compare period if needed */
    if (GPT_GetOutputCompareValue(SYSTICK_BASE, kGPT_OutputCompare_Channel1) != SYSTICK_COUNT_PER_TICK - 1)
    {
        GPT_SetOutputCompareValue(SYSTICK_BASE, kGPT_OutputCompare_Channel1, SYSTICK_COUNT_PER_TICK - 1);
    }

    g_tick_isr_hits++;
    g_tick_last_cnt = GPT_GetCurrentTimerCount(SYSTICK_BASE);

    xPortSysTickHandler();
    __DSB();
}


void LPM_IncreseBlockSleepCnt(void) { s_BlockEventCnt++; }
void LPM_DecreaseBlockSleepCnt(void) { s_BlockEventCnt--; }

bool LPM_AllowSleep(void)
{
    return s_BlockEventCnt == 0;
}

void LPM_MCORE_ChangeMCoreClock(LPM_MCORE_CLOCK_SPEED target)
{
    switch (target)
    {
        case LPM_MCORE_LOW_FREQ:
            if (CLOCK_GetRootMux(kCLOCK_RootM7) != kCLOCK_M7RootmuxOsc24M)
            {
                CLOCK_SetRootMux(kCLOCK_RootM7, kCLOCK_M7RootmuxOsc24M);
                CLOCK_SetRootDivider(kCLOCK_RootM7, 1U, 1U);
            }
            break;
        case LPM_MCORE_HIGH_FREQ:
            if (CLOCK_GetRootMux(kCLOCK_RootM7) != kCLOCK_M7RootmuxSysPll1)
            {
                CLOCK_SetRootDivider(kCLOCK_RootM7, 1U, 1U);
                CLOCK_SetRootMux(kCLOCK_RootM7, kCLOCK_M7RootmuxSysPll1);
            }
            break;
        default:
            break;
    }
}

void LPM_MCORE_SetPowerStatus(GPC_Type *base, LPM_POWER_STATUS_MCORE targetPowerMode)
{
    gpc_lpm_config_t config;
    config.enCpuClk              = false;
    config.enFastWakeUp          = false;
    config.enDsmMask             = false;
    config.enWfiMask             = false;
    config.enVirtualPGCPowerdown = true;
    config.enVirtualPGCPowerup   = true;

    switch (targetPowerMode)
    {
        case LPM_MCORE_STATE_RUN:
            GPC->LPCR_M7 = GPC->LPCR_M7 & (~GPC_LPCR_M7_LPM0_MASK);
            break;
        case LPM_MCORE_STATE_WAIT:
            GPC_EnterWaitMode(GPC, &config);
            break;
        case LPM_MCORE_STATE_STOP:
            GPC_EnterStopMode(GPC, &config);
            break;
        default:
            break;
    }
}

void PreSleepProcessing(void)
{
    APP_SRTM_Suspend();
    DbgConsole_Deinit();
}

void PostSleepProcessing(void)
{
    APP_SRTM_Resume();
    DbgConsole_Init(BOARD_DEBUG_UART_INSTANCE, BOARD_DEBUG_UART_BAUDRATE,
                    BOARD_DEBUG_UART_TYPE, BOARD_DEBUG_UART_CLK_FREQ);
}

void ShowMCoreStatus(void)
{
    if (mcore_lpm_state == LPM_MCORE_STATE_STOP)
    {
        PRINTF("\r\nNo audio, M core enters STOP mode!\r\n");
    }
    else if (mcore_lpm_state == LPM_MCORE_STATE_RUN)
    {
        PRINTF("\r\nPlayback or record is running, M core enters RUN mode!\r\n");
    }
}

/* Only target STOP if sleep is allowed (block counter = 0) */
void UpdateTargetPowerStatus(void)
{
    LPM_POWER_STATUS_MCORE mcore_target_lpm;

    if (APP_SRTM_ServiceIdle() && LPM_AllowSleep())
    {
        mcore_target_lpm = LPM_MCORE_STATE_STOP;
    }
    else
    {
        mcore_target_lpm = LPM_MCORE_STATE_RUN;
    }

    if (mcore_target_lpm != mcore_lpm_state)
    {
        mcore_lpm_state = mcore_target_lpm;
        ShowMCoreStatus();
    }
}

void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    uint32_t irqMask;
    uint64_t counter = 0;
    uint32_t timeoutTicks;
    uint32_t timeoutMilliSec = (uint32_t)((uint64_t)1000 * xExpectedIdleTime / configTICK_RATE_HZ);

    irqMask = DisableGlobalIRQ();

    UpdateTargetPowerStatus();

    if (eTaskConfirmSleepModeStatus() != eAbortSleep)
    {
        timeoutTicks = LPM_EnterTicklessIdle(timeoutMilliSec, &counter);
        if (timeoutTicks)
        {
            if (APP_SRTM_ServiceIdle() && LPM_AllowSleep())
            {
                LPM_MCORE_ChangeMCoreClock(LPM_MCORE_LOW_FREQ);
                LPM_MCORE_SetPowerStatus(BOARD_GPC_BASEADDR, LPM_MCORE_STATE_STOP);
                PreSleepProcessing();
                ServiceFlagAddr = ServiceIdle;
                __DSB();
                __ISB();
                __WFI();
                ServiceFlagAddr = ServiceBusy;
                PostSleepProcessing();
                LPM_MCORE_ChangeMCoreClock(LPM_MCORE_HIGH_FREQ);
                LPM_MCORE_SetPowerStatus(BOARD_GPC_BASEADDR, LPM_MCORE_STATE_RUN);
            }
            else
            {
                __DSB();
                __ISB();
                __WFI();
            }
        }
        LPM_ExitTicklessIdle(timeoutTicks, counter);
    }

    EnableGlobalIRQ(irqMask);
}

bool LPM_Init(void)
{
    ServiceFlagAddr = ServiceBusy;

    PRINTF("********************************\r\n");
    PRINTF("Wait the Linux kernel boot up to create the link between M core and A core.\r\n\r\n");
    PRINTF("********************************\r\n");
    while (srtmState != APP_SRTM_StateLinkedUp)
        ;
    PRINTF("The rpmsg channel between M core and A core created!\r\n");
    PRINTF("********************************\r\n\r\n");

    GPC_Init(BOARD_GPC_BASEADDR, APP_PowerUpSlot, APP_PowerDnSlot);
    GPC_EnableIRQ(BOARD_GPC_BASEADDR, BOARD_MU_IRQ_NUM);
    GPC_EnableIRQ(BOARD_GPC_BASEADDR, SYSTICK_IRQn);
    GPC_EnableIRQ(BOARD_GPC_BASEADDR, I2C3_IRQn);

    return true;
}
