/* low_power_wakeword.c - WITH LPA FLAG */

#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "app_srtm.h"
#include "lpm.h"
#include "fsl_debug_console.h"
#include "fsl_device_registers.h"
#include "fsl_mu.h"

/* === Config === */
#define MU_BASE_ADDR   (0x30AB0000u)
#define MU_BASE        ((MU_Type *)MU_BASE_ADDR)
#define MU_PERIOD_MS   200u
#define HB_PERIOD_MS   1000u
#define FIXED_GIR      3

/* === LPA Flag Setup (CRITICAL!) === */
#define SRC_BASE          0x30390000
#define SRC_GPR10_OFFSET  0x98
#define SRC_GPR10         ((volatile uint32_t *)(SRC_BASE + SRC_GPR10_OFFSET))

#define M4_LPA_ACTIVE          0x5555
#define DSP_LPA_ACTIVE         0x000D  
#define DSP_LPA_DRAM_ACTIVE    0x001D
#define M4_LPA_IDLE            0x0000

/* === Tiny busy-wait === */
static void busy_delay_us(uint32_t us)
{
    volatile uint32_t iters = (SystemCoreClock / 8U) * us / 1000000U;
    if (iters == 0) iters = 1;
    while (iters--) { __NOP(); }
}

/* === MU helpers === */
static const uint32_t s_gir_mask[4] = {
    kMU_GenInt0InterruptTrigger,
    kMU_GenInt1InterruptTrigger,
    kMU_GenInt2InterruptTrigger,
    kMU_GenInt3InterruptTrigger,
};
static const char *s_gir_name[4] = { "GIR0", "GIR1", "GIR2", "GIR3" };

static void SetLPAFlag(void)
{
    *SRC_GPR10 = M4_LPA_ACTIVE;
    PRINTF("LPA flag set: SRC_GPR10 = 0x%08X (addr: %p)\r\n", 
           *SRC_GPR10, (void*)SRC_GPR10);
}

static void MU_Wake_Init(void)
{
    MU_Init(MU_BASE);
    PRINTF("MU base %p init done\r\n", (void*)MU_BASE);
}

static void MU_Fire_GIR(int idx)
{
    status_t st = MU_TriggerInterrupts(MU_BASE, s_gir_mask[idx]);
    PRINTF("MU %s -> %s\r\n", s_gir_name[idx], (st == kStatus_Success) ? "OK" : "BUSY");
}

static void MainTask(void *arg)
{
    (void)arg;

    /* Keep M7 out of STOP while diagnosing */
    LPM_IncreseBlockSleepCnt();

    APP_SRTM_Init();

    /* === CRITICAL: Set LPA flag to keep M7 alive === */
    PRINTF("\n===========================================\r\n");
    PRINTF("*** Setting LPA flag for low-power mode ***\r\n");
    PRINTF("===========================================\r\n");
    SetLPAFlag();
    PRINTF("LPA flag confirmed: 0x%08X\r\n", *SRC_GPR10);
    PRINTF("===========================================\r\n\n");

    /* Start MU BEFORE rpmsg link-up so we can wake A53 from suspend */
    MU_Wake_Init();

    /* 4x probe with busy-wait gaps */
    for (int g = 0; g < 4; ++g) {
        MU_Fire_GIR(g);
        busy_delay_us(150000);
    }

    /* Optional: now wait for rpmsg */
    (void)LPM_Init();
    PRINTF("The rpmsg channel between M core and A core created!\r\n\r\n");

    /* Polled loop: send GIR at MU_PERIOD_MS */
    for (;;)
    {
        PRINTF("pre\r\n");
        MU_Fire_GIR(FIXED_GIR);
        PRINTF("post\r\n");
        busy_delay_us(MU_PERIOD_MS * 1000u);
    }
}

int main(void)
{
    BOARD_InitHardware();
    DbgConsole_Init(BOARD_DEBUG_UART_INSTANCE, BOARD_DEBUG_UART_BAUDRATE,
                    BOARD_DEBUG_UART_TYPE, BOARD_DEBUG_UART_CLK_FREQ);

    PRINTF("\r\n==== MU Wake (with LPA flag) ====\r\n");
    PRINTF("Build: %s %s\r\n", __DATE__, __TIME__);

    if (xTaskCreate(MainTask, "Main", configMINIMAL_STACK_SIZE + 256,
                    NULL, tskIDLE_PRIORITY + 1U, NULL) != pdPASS)
    {
        PRINTF("Task creation failed!\r\n");
        for(;;) {}
    }
    vTaskStartScheduler();
    for(;;) {}
}

void vApplicationMallocFailedHook(void) { PRINTF("Malloc Failed!!!\r\n"); for(;;){} }
