/*
 * RPMsg Echo + MU Wake Pinger (CM7, FreeRTOS) — minimal changes
 *
 * Keeps your original echo logic. Adds a second task that prints and
 * triggers MU GIR0 every 2s so you can see CM7 alive and try to wake A-core.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rpmsg_lite.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"
#include "board.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app.h"

#include "fsl_mu.h"   /* <-- only new header */

/* ===== Config ===== */
#define APP_TASK_STACK_SIZE (256)

#ifndef LOCAL_EPT_ADDR
#define LOCAL_EPT_ADDR (30)
#endif

/* If your SDK doesn't define MUB, fall back to the MU at 0x30AA0000 (A<->M7). */
#ifndef MUB
#define MUB ((MU_Type *)0x30AA0000u)
#endif

#ifndef WAKE_PING_PERIOD_MS
#define WAKE_PING_PERIOD_MS (2000U)
#endif

/* ===== Globals ===== */
static TaskHandle_t app_task_handle  = NULL;
static TaskHandle_t wake_task_handle = NULL;

static struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
static struct rpmsg_lite_endpoint *volatile my_ept   = NULL;
static volatile rpmsg_queue_handle my_queue          = NULL;

static char app_buf[512];

/* ===== Forward decls ===== */
static void app_task(void *param);

/* ===== Wake task (new) ===== */
static void wake_task(void *param)
{
    static uint32_t seq = 0;

    /* Init MU once */
    MU_Init(MUB);
    PRINTF("WAKE: MU ready, will ping every %u ms\r\n", (unsigned)WAKE_PING_PERIOD_MS);

    for (;;)
    {
        ++seq;
        PRINTF("WAKE[%u]: sending MU GIR0 -> A-core\r\n", (unsigned)seq);

        /* Fire General Interrupt Request 0 toward the A-core */
        MU_TriggerInterrupts(MUB, kMU_GenInt0InterruptTrigger);

        PRINTF("WAKE[%u]: MU GIR0 sent\r\n", (unsigned)seq);
        vTaskDelay(pdMS_TO_TICKS(WAKE_PING_PERIOD_MS));
    }
}

/* ===== Cleanup (unchanged, with wake task added) ===== */
void app_destroy_task(void)
{
    if (app_task_handle)
    {
        vTaskDelete(app_task_handle);
        app_task_handle = NULL;
    }

    if (my_ept)
    {
        rpmsg_lite_destroy_ept(my_rpmsg, my_ept);
        my_ept = NULL;
    }

    if (my_queue)
    {
        rpmsg_queue_destroy(my_rpmsg, my_queue);
        my_queue = NULL;
    }

    if (my_rpmsg)
    {
        rpmsg_lite_deinit(my_rpmsg);
        my_rpmsg = NULL;
    }

    if (wake_task_handle)
    {
        vTaskDelete(wake_task_handle);
        wake_task_handle = NULL;
    }
}

/* ===== Original echo app task (unchanged behavior) ===== */
void app_task(void *param)
{
    volatile uint32_t remote_addr = 0U;
    void *rx_buf;
    uint32_t len;
    int32_t result;
    void *tx_buf;
    uint32_t size;

    PRINTF("\r\nRPMSG String Echo FreeRTOS RTOS API Demo...\r\n");

#ifdef MCMGR_USED
    uint32_t startupData;
    (void)MCMGR_GetStartupData(kMCMGR_Core1, &startupData, kMCMGR_Core0);
    my_rpmsg = rpmsg_lite_remote_init((void *)startupData, RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
    (void)MCMGR_SignalReady(kMCMGR_Core1);
#else
    my_rpmsg = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
#endif

    rpmsg_lite_wait_for_link_up(my_rpmsg, RL_BLOCK);

    my_queue = rpmsg_queue_create(my_rpmsg);
    my_ept   = rpmsg_lite_create_ept(my_rpmsg, LOCAL_EPT_ADDR, rpmsg_queue_rx_cb, my_queue);

    /* Small delay so A-side nameservice callback is ready */
    SDK_DelayAtLeastUs(1000000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    (void)rpmsg_ns_announce(my_rpmsg, my_ept, RPMSG_LITE_NS_ANNOUNCE_STRING, RL_NS_CREATE);

    PRINTF("\r\nNameservice sent, ready for incoming messages...\r\n");

    /* NEW: start the MU wake pinger task (higher prio than echo) */
    if (wake_task_handle == NULL &&
        xTaskCreate(wake_task, "WAKE_TASK",
                    configMINIMAL_STACK_SIZE + 128, NULL,
                    tskIDLE_PRIORITY + 2, &wake_task_handle) != pdPASS)
    {
        PRINTF("Failed to create WAKE_TASK\r\n");
    }

    for (;;)
    {
        /* Receive from A-core and echo back */
        result = rpmsg_queue_recv_nocopy(my_rpmsg, my_queue,
                                         (uint32_t *)&remote_addr,
                                         (char **)&rx_buf, &len, RL_BLOCK);
        if (result != 0)
        {
            /* keep going */
            continue;
        }

        if (len >= sizeof(app_buf)) len = sizeof(app_buf) - 1U;
        memcpy(app_buf, rx_buf, len);
        app_buf[len] = 0;

        if ((len == 2) && (app_buf[0] == '\r') && (app_buf[1] == '\n'))
            PRINTF("Get New Line From Master Side\r\n");
        else
            PRINTF("Get Message From Master Side : \"%s\" [len : %u]\r\n", app_buf, (unsigned)len);

        tx_buf = rpmsg_lite_alloc_tx_buffer(my_rpmsg, &size, RL_BLOCK);
        if (tx_buf)
        {
            if (len > size) len = size;
            memcpy(tx_buf, app_buf, len);
            (void)rpmsg_lite_send_nocopy(my_rpmsg, my_ept, remote_addr, tx_buf, len);
        }

        (void)rpmsg_queue_nocopy_free(my_rpmsg, rx_buf);
    }
}

/* ===== RTOS bootstrap (unchanged) ===== */
void app_create_task(void)
{
    if (app_task_handle == NULL &&
        xTaskCreate(app_task, "APP_TASK",
                    APP_TASK_STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 1, &app_task_handle) != pdPASS)
    {
        PRINTF("\r\nFailed to create application task\r\n");
        for (;;)
            ;
    }
}

int main(void)
{
    BOARD_InitHardware();

#ifdef MCMGR_USED
    (void)MCMGR_Init();
#endif

    app_create_task();
    vTaskStartScheduler();

    PRINTF("Failed to start FreeRTOS on core0.\n");
    for (;;)
        ;
}
