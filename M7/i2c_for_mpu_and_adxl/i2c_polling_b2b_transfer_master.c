/*
 * Minimal change: same structure, now reads MPU on I2C5 and ADXL345 on I2C3.
 */

#include <string.h>
#include "board.h"
#include "fsl_debug_console.h"
#include "fsl_i2c.h"
#include "app.h"
#include "fsl_clock.h"  /* CLOCK_GetRootClockFreq */

/* MPU6050 (0x68 on I2C5) */
#define MPU_ADDR_7BIT            0x68U
#define I2C_BAUDRATE_MPU         100000U
#define MPU_REG_PWR1             0x6B
#define MPU_REG_WHOAMI           0x75
#define MPU_REG_ACCEL_XOUT       0x3B   /* 14 bytes */

/* ADXL345 (0x53 on I2C3) */
#define ADXL_ADDR_7BIT           0x53U
#define I2C_BAUDRATE_ADXL        400000U     /* ADXL is happy at 400 kHz; use 100 kHz if you prefer */
#define ADXL_REG_DEVID           0x00
#define ADXL_REG_BWRATE          0x2C
#define ADXL_REG_POWERCTL        0x2D
#define ADXL_REG_DATAFORMAT      0x31
#define ADXL_REG_DATAX0          0x32       /* 6 bytes: X0,X1,Y0,Y1,Z0,Z1 (little-endian per axis) */

#define I2C_DATA_LENGTH          32U

static uint8_t g_master_txBuff[I2C_DATA_LENGTH];
static uint8_t g_master_rxBuff[I2C_DATA_LENGTH];

int main(void)
{
    i2c_master_config_t cfg5, cfg3;
    uint32_t clk5, clk3;
    i2c_master_transfer_t xfer;
    status_t st;

    BOARD_InitHardware();

    PRINTF("\r\nDUAL I2C DEMO test 345 (build %s %s)\r\n", __DATE__, __TIME__);

    /* existing banner data */
    for (uint32_t i = 0; i < I2C_DATA_LENGTH; i++) g_master_txBuff[i] = (uint8_t)i;
    PRINTF("Data sent :");
    for (uint32_t i = 0; i < I2C_DATA_LENGTH; i++) {
        if (i % 8 == 0) PRINTF("\r\n");
        PRINTF("0x%2x  ", g_master_txBuff[i]);
    }
    PRINTF("\r\n\r\n");

    /* ---------- I2C5: init for MPU6050 ---------- */
    I2C_MasterGetDefaultConfig(&cfg5);
    cfg5.baudRate_Bps = I2C_BAUDRATE_MPU;
    clk5 = I2C5_MASTER_CLK_FREQ;
    I2C_MasterInit(I2C5_MASTER_BASE, &cfg5, clk5);

    /* Wake MPU: PWR_MGMT_1 = 0x00 */
    {
        uint8_t zero = 0x00;
        memset(&xfer, 0, sizeof(xfer));
        xfer.slaveAddress   = MPU_ADDR_7BIT;
        xfer.direction      = kI2C_Write;
        xfer.subaddress     = MPU_REG_PWR1;
        xfer.subaddressSize = 1;
        xfer.data           = &zero;
        xfer.dataSize       = 1;
        xfer.flags          = kI2C_TransferDefaultFlag;
        (void)I2C_MasterTransferBlocking(I2C5_MASTER_BASE, &xfer);
    }

    /* WHO_AM_I (MPU) */
    {
        uint8_t who = 0xFF;
        memset(&xfer, 0, sizeof(xfer));
        xfer.slaveAddress   = MPU_ADDR_7BIT;
        xfer.direction      = kI2C_Read;
        xfer.subaddress     = MPU_REG_WHOAMI;
        xfer.subaddressSize = 1;
        xfer.data           = &who;
        xfer.dataSize       = 1;
        xfer.flags          = kI2C_TransferDefaultFlag;
        (void)I2C_MasterTransferBlocking(I2C5_MASTER_BASE, &xfer);
        PRINTF("MPU WHO_AM_I = 0x%02X\r\n", who);
    }

    /* ---------- I2C3: init for ADXL345 ---------- */
    I2C_MasterGetDefaultConfig(&cfg3);
    cfg3.baudRate_Bps = I2C_BAUDRATE_ADXL;
    clk3 = I2C3_MASTER_CLK_FREQ;
    I2C_MasterInit(I2C3_MASTER_BASE, &cfg3, clk3);

    /* ADXL: verify DEVID (expect 0xE5) */
    {
        uint8_t devid = 0x00;
        memset(&xfer, 0, sizeof(xfer));
        xfer.slaveAddress   = ADXL_ADDR_7BIT;
        xfer.direction      = kI2C_Read;
        xfer.subaddress     = ADXL_REG_DEVID;
        xfer.subaddressSize = 1;
        xfer.data           = &devid;
        xfer.dataSize       = 1;
        xfer.flags          = kI2C_TransferDefaultFlag;
        (void)I2C_MasterTransferBlocking(I2C3_MASTER_BASE, &xfer);
        PRINTF("ADXL DEVID = 0x%02X\r\n", devid);
    }

    /* ADXL: set BW_RATE=0x0A (100 Hz), DATA_FORMAT=0x00 (±2g, right-justified), POWER_CTL=0x08 (measure) */
    {
        uint8_t v;

        v = 0x0A; /* 100 Hz */
        memset(&xfer, 0, sizeof(xfer));
        xfer.slaveAddress   = ADXL_ADDR_7BIT;
        xfer.direction      = kI2C_Write;
        xfer.subaddress     = ADXL_REG_BWRATE;
        xfer.subaddressSize = 1;
        xfer.data           = &v;
        xfer.dataSize       = 1;
        xfer.flags          = kI2C_TransferDefaultFlag;
        (void)I2C_MasterTransferBlocking(I2C3_MASTER_BASE, &xfer);

        v = 0x00; /* ±2g, right-justified 10-bit */
        memset(&xfer, 0, sizeof(xfer));
        xfer.slaveAddress   = ADXL_ADDR_7BIT;
        xfer.direction      = kI2C_Write;
        xfer.subaddress     = ADXL_REG_DATAFORMAT;
        xfer.subaddressSize = 1;
        xfer.data           = &v;
        xfer.dataSize       = 1;
        xfer.flags          = kI2C_TransferDefaultFlag;
        (void)I2C_MasterTransferBlocking(I2C3_MASTER_BASE, &xfer);

        v = 0x08; /* measure */
        memset(&xfer, 0, sizeof(xfer));
        xfer.slaveAddress   = ADXL_ADDR_7BIT;
        xfer.direction      = kI2C_Write;
        xfer.subaddress     = ADXL_REG_POWERCTL;
        xfer.subaddressSize = 1;
        xfer.data           = &v;
        xfer.dataSize       = 1;
        xfer.flags          = kI2C_TransferDefaultFlag;
        (void)I2C_MasterTransferBlocking(I2C3_MASTER_BASE, &xfer);
    }

    /* ---- Continuous polling: read both sensors ---- */
    for (;;)
    {
        /* ---- MPU6050 burst (I2C5) ---- */
        {
            uint8_t raw[14];
            memset(&xfer, 0, sizeof(xfer));
            xfer.slaveAddress   = MPU_ADDR_7BIT;
            xfer.direction      = kI2C_Read;
            xfer.subaddress     = MPU_REG_ACCEL_XOUT;   /* 0x3B */
            xfer.subaddressSize = 1;
            xfer.data           = raw;
            xfer.dataSize       = sizeof raw;
            xfer.flags          = kI2C_TransferDefaultFlag;

            st = I2C_MasterTransferBlocking(I2C5_MASTER_BASE, &xfer);
            if (st == kStatus_Success) {
                /* raw -> signed 16, then widen */
                int32_t ax = (int16_t)((raw[0]  << 8) | raw[1]);
                int32_t ay = (int16_t)((raw[2]  << 8) | raw[3]);
                int32_t az = (int16_t)((raw[4]  << 8) | raw[5]);
                int32_t gx = (int16_t)((raw[8]  << 8) | raw[9]);
                int32_t gy = (int16_t)((raw[10] << 8) | raw[11]);
                int32_t gz = (int16_t)((raw[12] << 8) | raw[13]);

                /* scale: ±2g => 16384 LSB/g, ±250 dps => 131 LSB/(°/s) */
                float ax_g = (float)ax / 16384.0f;
                float ay_g = (float)ay / 16384.0f;
                float az_g = (float)az / 16384.0f;
                float gx_dps = (float)gx / 131.0f;
                float gy_dps = (float)gy / 131.0f;
                float gz_dps = (float)gz / 131.0f;

                /* pretty block for MPU */
                PRINTF(
                    "\r\nMPU6050 (I2C5)\r\n"
                    "  ACC [g ] : %8.3f  %8.3f  %8.3f\r\n"
                    "  GYR [dps]: %8.2f  %8.2f  %8.2f\r\n",
                    ax_g, ay_g, az_g, gx_dps, gy_dps, gz_dps
                );
            } else {
                PRINTF("\r\nMPU6050 (I2C5) read err (%d)\r\n", (int)st);
            }
        }

        /* ---- ADXL345 6-byte read (I2C3) ---- */
        {
            uint8_t d[6];
            memset(&xfer, 0, sizeof(xfer));
            xfer.slaveAddress   = ADXL_ADDR_7BIT;
            xfer.direction      = kI2C_Read;
            xfer.subaddress     = ADXL_REG_DATAX0;   /* 0x32 */
            xfer.subaddressSize = 1;
            xfer.data           = d;
            xfer.dataSize       = sizeof d;
            xfer.flags          = kI2C_TransferDefaultFlag;

            st = I2C_MasterTransferBlocking(I2C3_MASTER_BASE, &xfer);
            if (st == kStatus_Success) {
                /* little-endian ? */
                int32_t x = (int16_t)((d[1] << 8) | d[0]);
                int32_t y = (int16_t)((d[3] << 8) | d[2]);
                int32_t z = (int16_t)((d[5] << 8) | d[4]);

                /* ADXL345 default (±2g, right-justified 10-bit) ≈ 256 LSB/g */
                float x_g = (float)x / 256.0f;
                float y_g = (float)y / 256.0f;
                float z_g = (float)z / 256.0f;

                /* pretty block for ADXL */
                PRINTF(
                    "ADXL345 (I2C3)\r\n"
                    "  ACC [g ] : %8.3f  %8.3f  %8.3f\r\n",
                    x_g, y_g, z_g
                );
            } else {
                PRINTF("ADXL345 (I2C3) read err (%d)\r\n", (int)st);
            }
        }

    }
}