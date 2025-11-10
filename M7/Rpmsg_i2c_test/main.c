/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpmsg_lite.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"
#include "board.h"
#include "fsl_debug_console.h"
#include "fsl_i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define APP_TASK_STACK_SIZE (512)
#ifndef LOCAL_EPT_ADDR
#define LOCAL_EPT_ADDR (30)
#endif

/* ADXL345 on I2C3 */
#define ADXL_ADDR_7BIT      0x53U
#define I2C_BAUDRATE        400000U
#define ADXL_REG_DEVID      0x00
#define ADXL_REG_BWRATE     0x2C
#define ADXL_REG_POWERCTL   0x2D
#define ADXL_REG_DATAFORMAT 0x31
#define ADXL_REG_DATAX0     0x32

/* Accelerometer Data Packet - sent via RPMsg */
struct AccelPacket {
    uint32_t timestamp_ms;
    float accel_x;
    float accel_y;
    float accel_z;
} __attribute__((packed));

/*******************************************************************************
 * Globals
 ******************************************************************************/
static TaskHandle_t app_task_handle = NULL;
static struct rpmsg_lite_instance *volatile my_rpmsg = NULL;
static struct rpmsg_lite_endpoint *volatile my_ept = NULL;
static volatile rpmsg_queue_handle my_queue = NULL;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void InitADXL345(void);
static bool ReadADXL345(struct AccelPacket *accel);

/*******************************************************************************
 * Code
 ******************************************************************************/

static void InitADXL345(void)
{
    i2c_master_config_t cfg;
    i2c_master_transfer_t xfer;
    uint8_t devid = 0x00;
    uint8_t val;
    status_t st;

    PRINTF("\r\n=== Initializing ADXL345 on I2C3 ===\r\n");

    /* Initialize I2C3 */
    I2C_MasterGetDefaultConfig(&cfg);
    cfg.baudRate_Bps = I2C_BAUDRATE;
    I2C_MasterInit(I2C3_MASTER_BASE, &cfg, I2C3_MASTER_CLK_FREQ);

    /* Read DEVID (should be 0xE5) */
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress   = ADXL_ADDR_7BIT;
    xfer.direction      = kI2C_Read;
    xfer.subaddress     = ADXL_REG_DEVID;
    xfer.subaddressSize = 1;
    xfer.data           = &devid;
    xfer.dataSize       = 1;
    xfer.flags          = kI2C_TransferDefaultFlag;
    
    st = I2C_MasterTransferBlocking(I2C3_MASTER_BASE, &xfer);
    if (st == kStatus_Success) {
        PRINTF("ADXL345 DEVID = 0x%02X ", devid);
        if (devid == 0xE5) {
            PRINTF("(OK)\r\n");
        } else {
            PRINTF("(UNEXPECTED! Should be 0xE5)\r\n");
        }
    } else {
        PRINTF("ERROR: Failed to read DEVID (status=%d)\r\n", st);
        return;
    }

    /* Set BW_RATE = 0x0A (100 Hz output data rate) */
    val = 0x0A;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress   = ADXL_ADDR_7BIT;
    xfer.direction      = kI2C_Write;
    xfer.subaddress     = ADXL_REG_BWRATE;
    xfer.subaddressSize = 1;
    xfer.data           = &val;
    xfer.dataSize       = 1;
    xfer.flags          = kI2C_TransferDefaultFlag;
    I2C_MasterTransferBlocking(I2C3_MASTER_BASE, &xfer);

    /* Set DATA_FORMAT = 0x00 (±2g range, 10-bit resolution) */
    val = 0x00;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress   = ADXL_ADDR_7BIT;
    xfer.direction      = kI2C_Write;
    xfer.subaddress     = ADXL_REG_DATAFORMAT;
    xfer.subaddressSize = 1;
    xfer.data           = &val;
    xfer.dataSize       = 1;
    xfer.flags          = kI2C_TransferDefaultFlag;
    I2C_MasterTransferBlocking(I2C3_MASTER_BASE, &xfer);

    /* Set POWER_CTL = 0x08 (measurement mode) */
    val = 0x08;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress   = ADXL_ADDR_7BIT;
    xfer.direction      = kI2C_Write;
    xfer.subaddress     = ADXL_REG_POWERCTL;
    xfer.subaddressSize = 1;
    xfer.data           = &val;
    xfer.dataSize       = 1;
    xfer.flags          = kI2C_TransferDefaultFlag;
    I2C_MasterTransferBlocking(I2C3_MASTER_BASE, &xfer);
    
    PRINTF("=== ADXL345 Init Complete ===\r\n\r\n");
}

static bool ReadADXL345(struct AccelPacket *accel)
{
    i2c_master_transfer_t xfer;
    uint8_t data[6];
    status_t st;

    /* Read 6 bytes starting from DATAX0 */
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress   = ADXL_ADDR_7BIT;
    xfer.direction      = kI2C_Read;
    xfer.subaddress     = ADXL_REG_DATAX0;
    xfer.subaddressSize = 1;
    xfer.data           = data;
    xfer.dataSize       = sizeof(data);
    xfer.flags          = kI2C_TransferDefaultFlag;

    st = I2C_MasterTransferBlocking(I2C3_MASTER_BASE, &xfer);
    if (st != kStatus_Success) {
        return false;
    }

    /* Parse raw data (little-endian) */
    int16_t x = (int16_t)((data[1] << 8) | data[0]);
    int16_t y = (int16_t)((data[3] << 8) | data[2]);
    int16_t z = (int16_t)((data[5] << 8) | data[4]);

    /* Convert to g (±2g range, 10-bit resolution = 256 LSB/g) */
    accel->accel_x = (float)x / 256.0f;
    accel->accel_y = (float)y / 256.0f;
    accel->accel_z = (float)z / 256.0f;
    accel->timestamp_ms = xTaskGetTickCount();

    return true;
}



void app_task(void *param)
{
    uint32_t remote_addr = 0U;
    int32_t result;
    struct AccelPacket accel;
    uint32_t packet_count = 0;
    uint32_t error_count = 0;
    void *rx_buf;
    uint32_t len;

    PRINTF("\r\n========================================\r\n");
    PRINTF("RPMsg Accelerometer Streaming Demo\r\n");
    PRINTF("ADXL345 on I2C3\r\n");
    PRINTF("========================================\r\n");

    /* Initialize RPMsg */
#ifdef MCMGR_USED
    uint32_t startupData;
    (void)MCMGR_GetStartupData(kMCMGR_Core1, &startupData, kMCMGR_Core0);
    my_rpmsg = rpmsg_lite_remote_init((void *)startupData, RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
    (void)MCMGR_SignalReady(kMCMGR_Core1);
#else
    my_rpmsg = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE, RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
#endif

    PRINTF("Waiting for RPMsg link...\r\n");
    rpmsg_lite_wait_for_link_up(my_rpmsg, RL_BLOCK);

    my_queue = rpmsg_queue_create(my_rpmsg);
    my_ept = rpmsg_lite_create_ept(my_rpmsg, LOCAL_EPT_ADDR, rpmsg_queue_rx_cb, my_queue);

    SDK_DelayAtLeastUs(1000000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
    (void)rpmsg_ns_announce(my_rpmsg, my_ept, RPMSG_LITE_NS_ANNOUNCE_STRING, RL_NS_CREATE);

    PRINTF("RPMsg link established!\r\n");
    PRINTF("Endpoint: %d\r\n", LOCAL_EPT_ADDR);
    
    /* Set standard Linux RPMsg endpoint address */
    remote_addr = 0x400;
    
    PRINTF("Target address: 0x%X\r\n", remote_addr);
    PRINTF("Packet size: %d bytes\r\n", sizeof(struct AccelPacket));
    PRINTF("\r\nStarting accel streaming at 100 Hz...\r\n");
    PRINTF("========================================\r\n\r\n");

    
        // ... rest of code
    /* Main loop: Read accelerometer and stream via RPMsg */
    for (;;)
    {
        if (ReadADXL345(&accel))
        {
            /* Send via RPMsg */
            result = rpmsg_lite_send(my_rpmsg, my_ept, remote_addr,
                                    (char*)&accel, sizeof(accel), RL_BLOCK);
            
            if (result == RL_SUCCESS)
            {
                packet_count++;
                
                /* Print every 100 packets */
                if (packet_count % 100 == 0)
                {
                    PRINTF("[%6d] Accel: [%6.3f, %6.3f, %6.3f] g  (Errors: %d)\r\n",
                           packet_count,
                           accel.accel_x, accel.accel_y, accel.accel_z,
                           error_count);
                }
            }
            else
            {
                error_count++;
                if (error_count % 10 == 1)
                {
                    PRINTF("ERROR: RPMsg send failed (code=%d, errors=%d)\r\n",
                           result, error_count);
                }
            }
        }
        else
        {
            error_count++;
            if (error_count % 10 == 1)
            {
                PRINTF("ERROR: I2C read failed (errors=%d)\r\n", error_count);
            }
        }

        /* 100Hz = 10ms delay */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

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
}

void app_create_task(void)
{
    if (app_task_handle == NULL &&
        xTaskCreate(app_task, "APP_TASK", APP_TASK_STACK_SIZE, NULL, 
                   tskIDLE_PRIORITY + 1, &app_task_handle) != pdPASS)
    {
        PRINTF("\r\nFailed to create application task\r\n");
        for (;;);
    }
}

int main(void)
{
    BOARD_InitHardware();

#ifdef MCMGR_USED
    (void)MCMGR_Init();
#endif

    /* Initialize ADXL345 before starting RTOS */
    InitADXL345();

    app_create_task();
    vTaskStartScheduler();

    PRINTF("Failed to start FreeRTOS on core0.\n");
    for (;;);
}