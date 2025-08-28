/* low_power_wakeword.c
 *
 * MU wake (diagnostic build) — NO FreeRTOS delays/timers, so it works even if the tick is dead.
 * - Starts MU immediately so it can wake A53 from suspend.
 * - Does the 4x probe with busy-wait.
 * - Then loops: fire GIR periodically with busy-wait + print a heartbeat every ~1s.
 * Use this to prove the tick is your blocker.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "app_srtm.h"
#include "lpm.h"
#include "fsl_debug_console.h"
#include "fsl_device_registers.h"
#include "fsl_mu.h"

/* === Config === */
#define MU_BASE_ADDR   (0x30AB0000u)                 /* M7 -> MU B-side for 30AA0000 */
#define MU_BASE        ((MU_Type *)MU_BASE_ADDR)
#define MU_PERIOD_MS   200u                          /* doorbell period */
#define HB_PERIOD_MS   1000u
#define FIXED_GIR      3                             /* 0/1/2/3; pick what works on your Linux */

/* === Tiny busy-wait (no tick needed, no DWT required) === */
static void busy_delay_us(uint32_t us)
{
    /* ~1 iteration ~= 2–4 cycles on M7; scale 'iters' conservatively. */
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

/* === One FreeRTOS task, but no time APIs used === */
static void MainTask(void *arg)
{
    (void)arg;

    /* Keep M7 out of STOP while diagnosing */
    LPM_IncreseBlockSleepCnt();

    APP_SRTM_Init();

    /* Start MU BEFORE rpmsg link-up so we can wake A53 from suspend */
    MU_Wake_Init();

    /* 4x probe with busy-wait gaps */
    for (int g = 0; g < 4; ++g) {
        MU_Fire_GIR(g);
        busy_delay_us(150000);
    }

    /* Optional: now wait for rpmsg (purely for your log); this can block if A53 sleeps. */
    (void)LPM_Init();
    PRINTF("The rpmsg channel between M core and A core created!\r\n\r\n");

    /* Polled loop: send GIR at MU_PERIOD_MS and print a heartbeat ~1s */
    uint32_t acc = 0, hb = 0;
    for (;;)
{
    PRINTF("pre\r\n");
    MU_Fire_GIR(FIXED_GIR);
    PRINTF("post\r\n");
    busy_delay_us(MU_PERIOD_MS * 1000u);
}
}
//     for (;;)
//     {
// #if (FIXED_GIR >= 0)
//         MU_Fire_GIR(FIXED_GIR);
// #else
//         static int gir = 0;
//         MU_Fire_GIR(gir);
//         gir = (gir + 1) & 3;
// #endif
//         acc += MU_PERIOD_MS;
//         hb  += MU_PERIOD_MS;
//         if (hb >= HB_PERIOD_MS) { PRINTF("HB\r\n"); hb = 0; }

//         busy_delay_us(MU_PERIOD_MS * 1000u);
    


int main(void)
{
    BOARD_InitHardware();
    DbgConsole_Init(BOARD_DEBUG_UART_INSTANCE, BOARD_DEBUG_UART_BAUDRATE,
                    BOARD_DEBUG_UART_TYPE, BOARD_DEBUG_UART_CLK_FREQ);

    PRINTF("\r\n==== MU Wake (diagnostic, no tick) ====\r\n");
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
