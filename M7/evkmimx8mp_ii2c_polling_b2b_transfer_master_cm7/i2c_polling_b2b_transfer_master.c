/*
 * Minimal change: same structure, now continuous reads from IMU on I2C3.
 * - No pin mux changes
 * - No instance/clock overrides (uses EXAMPLE_I2C_MASTER_BASEADDR / I2C_MASTER_CLK_FREQ from app.h)
 * - Replaces board-to-board xfers with WHO_AM_I + continuous 14-byte burst reads
 */

#include <string.h>
#include "board.h"
#include "fsl_debug_console.h"
#include "fsl_i2c.h"
#include "app.h"

/* ---------gpio------ (include) */
#include "fsl_clock.h"  /* for CLOCK_GetRootClockFreq / kCLOCK_RootI2c3 */
/* ---------gpio------ (include) */


/* ---------gpio------ (expander defs) */
/* TCA6416 on I2C3 @ 0x20: we only touch Port 0 */
#define TCA6416_ADDR   0x20
#define TCA_REG_IN0    0x00
#define TCA_REG_OUT0   0x02
#define TCA_REG_CFG0   0x06
#define TCA_BIT_I2C5_SEL (1u<<2)   /* Port0 bit2 = "CAN1/I2C5_SEL" */
/* Set to 1 if HIGH selects I2C5 on your board; set to 0 if LOW selects I2C5 */
#ifndef I2C5_SEL_ACTIVE_HIGH
#define I2C5_SEL_ACTIVE_HIGH 1
#endif
/* ---------gpio------ (expander defs) */


/* Your existing example defs (address 0x68; use 0x69 if AD0=1) */
#define I2C_MASTER_SLAVE_ADDR_7BIT 0x68U
#define I2C_BAUDRATE               100000U
#define I2C_DATA_LENGTH            32U

/* IMU regs (MPU60x0/65x0 family) */
#define MPU_REG_PWR1               0x6B
#define MPU_REG_WHOAMI             0x75
#define MPU_REG_ACCEL_XOUT         0x3B   /* 14 bytes: AX..AZ, TEMP, GX..GZ */


/* Example buffers (kept; harmless) */
static uint8_t g_master_txBuff[I2C_DATA_LENGTH];
static uint8_t g_master_rxBuff[I2C_DATA_LENGTH];

int main(void)
{
    i2c_master_config_t masterConfig;
    uint32_t sourceClock;

    BOARD_InitHardware();

    PRINTF("\r\nTEST 3 i2c5! (build %s %s)\r\n", __DATE__, __TIME__);

    /* Fill and print the sample buffer (kept; does not affect IMU transfers) */
    for (uint32_t i = 0; i < I2C_DATA_LENGTH; i++) g_master_txBuff[i] = (uint8_t)i;

    PRINTF("Data sent :");
    for (uint32_t i = 0; i < I2C_DATA_LENGTH; i++) {
        if (i % 8 == 0) PRINTF("\r\n");
        PRINTF("0x%2x  ", g_master_txBuff[i]);
    }
    PRINTF("\r\n\r\n");

    /* I2C master init (uses EXAMPLE_I2C_MASTER_BASEADDR / I2C_MASTER_CLK_FREQ from app.h) */
    I2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = I2C_BAUDRATE;
    sourceClock = I2C_MASTER_CLK_FREQ;
    I2C_MasterInit(EXAMPLE_I2C_MASTER_BASEADDR, &masterConfig, sourceClock);

    /* ---- Talk to the IMU instead of 0x7E board-to-board ---- */
    i2c_master_transfer_t xfer;
    status_t st;

    /* Wake IMU: PWR_MGMT_1 = 0x00 */
    uint8_t zero = 0x00;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress   = I2C_MASTER_SLAVE_ADDR_7BIT;     /* 0x68 or 0x69 */
    xfer.direction      = kI2C_Write;
    xfer.subaddress     = MPU_REG_PWR1;                   /* 0x6B */
    xfer.subaddressSize = 1;
    xfer.data           = &zero;
    xfer.dataSize       = 1;
    xfer.flags          = kI2C_TransferDefaultFlag;
    st = I2C_MasterTransferBlocking(EXAMPLE_I2C_MASTER_BASEADDR, &xfer);
    if (st != kStatus_Success) { PRINTF("PWR_MGMT_1 write fail (%d)\r\n", (int)st); for(;;){} }

    /* Read WHO_AM_I once (note: many InvenSense parts return 0x70/0x71/etc.) */
    uint8_t who = 0xFF;
    memset(&xfer, 0, sizeof(xfer));
    xfer.slaveAddress   = I2C_MASTER_SLAVE_ADDR_7BIT;
    xfer.direction      = kI2C_Read;
    xfer.subaddress     = MPU_REG_WHOAMI;                 /* 0x75 */
    xfer.subaddressSize = 1;
    xfer.data           = &who;
    xfer.dataSize       = 1;
    xfer.flags          = kI2C_TransferDefaultFlag;
    st = I2C_MasterTransferBlocking(EXAMPLE_I2C_MASTER_BASEADDR, &xfer);
    PRINTF("WHO_AM_I = 0x%02X\r\n", who);

    /* ---- Continuous polling: 14-byte burst from ACCEL_XOUT ---- */
    for (;;)
    {
        uint8_t raw[14];
        memset(&xfer, 0, sizeof(xfer));
        xfer.slaveAddress   = I2C_MASTER_SLAVE_ADDR_7BIT;
        xfer.direction      = kI2C_Read;
        xfer.subaddress     = MPU_REG_ACCEL_XOUT;         /* 0x3B */
        xfer.subaddressSize = 1;
        xfer.data           = raw;
        xfer.dataSize       = sizeof raw;
        xfer.flags          = kI2C_TransferDefaultFlag;

        st = I2C_MasterTransferBlocking(EXAMPLE_I2C_MASTER_BASEADDR, &xfer);
        if (st == kStatus_Success) {
            int16_t ax = (int16_t)((raw[0]  << 8) | raw[1]);
            int16_t ay = (int16_t)((raw[2]  << 8) | raw[3]);
            int16_t az = (int16_t)((raw[4]  << 8) | raw[5]);
            /* raw[6..7] = TEMP if you need it */
            int16_t gx = (int16_t)((raw[8]  << 8) | raw[9]);
            int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
            int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

            /* Cast to int so negatives print correctly */
            PRINTF("ACC[%6d %6d %6d]  GYR[%6d %6d %6d]\r\n",
                   (int)ax, (int)ay, (int)az, (int)gx, (int)gy, (int)gz);
        } else {
            PRINTF("Burst read error (%d)\r\n", (int)st);
        }

        /* crude ~100 Hz delay; no extra headers */
        for (volatile uint32_t d = 0; d < 60000U; ++d) { __NOP(); }
    }
}