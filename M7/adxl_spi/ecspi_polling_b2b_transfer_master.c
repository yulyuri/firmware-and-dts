/*
 * ADXL345 over ECSPI2 (SPI Mode 3)
 * i.MX8MP M7 core, NXP SDK
 */

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "fsl_ecspi.h"
#include "board.h"
#include "app.h"
#include "fsl_clock.h"

/*******************************************************************************
 * ADXL345 Definitions
 ******************************************************************************/
#define ADXL_REG_DEVID        0x00
#define ADXL_REG_BW_RATE      0x2C
#define ADXL_REG_POWER_CTL    0x2D
#define ADXL_REG_DATA_FORMAT  0x31
#define ADXL_REG_DATAX0       0x32

#define ECSPI_BAUDRATE        500000U  /* 500 kHz, safe for ADXL345 */

/*******************************************************************************
 * Buffers
 ******************************************************************************/
static uint8_t txBuf[16];
static uint8_t rxBuf[16];

/*******************************************************************************
 * Helpers
 ******************************************************************************/
static status_t spi_txrx(ECSPI_Type *base, uint8_t channel,
                         const uint8_t *tx, uint8_t *rx, size_t nbytes)
{
    ecspi_transfer_t t = {0};
    t.channel  = channel;
    t.txData   = (uint8_t *)tx;
    t.rxData   = rx;
    t.dataSize = (uint32_t)nbytes;
    return ECSPI_MasterTransferBlocking(base, &t);
}

static status_t adxl_spi_write_reg(ECSPI_Type *base, uint8_t channel,
                                   uint8_t reg, uint8_t val)
{
    uint8_t frame[2] = { (uint8_t)(reg & 0x3F), val };
    return spi_txrx(base, channel, frame, NULL, sizeof(frame));
}

static status_t adxl_spi_read_regs(ECSPI_Type *base, uint8_t channel,
                                   uint8_t startReg, uint8_t *buf, size_t len)
{
    uint8_t cmd = (uint8_t)(0x80 | (startReg & 0x3F));
    if (len > 1) cmd |= 0x40;

    txBuf[0] = cmd;
    for (size_t i = 1; i < 1 + len; i++) txBuf[i] = 0;

    status_t st = spi_txrx(base, channel, txBuf, rxBuf, 1 + len);
    if (st != kStatus_Success) return st;

    memcpy(buf, &rxBuf[1], len); /* skip dummy */
    return kStatus_Success;
}

/*******************************************************************************
 * Main
 ******************************************************************************/
int main(void)
{
    BOARD_InitHardware();
    PRINTF("ADXL345 SPI Demo (ECSPI2) build %s %s\r\n", __DATE__, __TIME__);

    /* --- Init ECSPI2 Master --- */
    ecspi_master_config_t masterConfig;
    ECSPI_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = ECSPI_BAUDRATE;
    ECSPI_MasterInit(EXAMPLE_ECSPI_MASTER_BASEADDR, &masterConfig, ECSPI_MASTER_CLK_FREQ);

    /* Configure channel 0: 8-bit, active low, Mode 3 */
    ecspi_channel_config_t ch = {0};
    ch.channel                 = kECSPI_Channel0;
    ch.burstLength             = 8;
    ch.chipSelectActiveState   = kECSPI_ChipSelectActiveLow;
    ch.clockPolarity           = kECSPI_ClockPolarityActiveHigh;
    ch.clockPhase              = kECSPI_ClockPhaseSecondEdge;
    ECSPI_SetChannelConfig(EXAMPLE_ECSPI_MASTER_BASEADDR, kECSPI_Channel0, &ch);

    /* --- 1. Read DEVID --- */
    uint8_t devid = 0;
    if (adxl_spi_read_regs(EXAMPLE_ECSPI_MASTER_BASEADDR, kECSPI_Channel0, ADXL_REG_DEVID, &devid, 1) == kStatus_Success)
    {
        PRINTF("ADXL DEVID = 0x%02X (expect 0xE5)\r\n", devid);
    }
    else
    {
        PRINTF("Failed to read ADXL DEVID\r\n");
    }

    /* --- 2. Configure ADXL345 --- */
    adxl_spi_write_reg(EXAMPLE_ECSPI_MASTER_BASEADDR, kECSPI_Channel0, ADXL_REG_BW_RATE, 0x0C);     // 400 Hz
    adxl_spi_write_reg(EXAMPLE_ECSPI_MASTER_BASEADDR, kECSPI_Channel0, ADXL_REG_DATA_FORMAT, 0x08); // FULL_RES, ±2g
    adxl_spi_write_reg(EXAMPLE_ECSPI_MASTER_BASEADDR, kECSPI_Channel0, ADXL_REG_POWER_CTL, 0x08);   // Measure

    uint8_t pctl=0;
    adxl_spi_read_regs(EXAMPLE_ECSPI_MASTER_BASEADDR, kECSPI_Channel0, ADXL_REG_POWER_CTL, &pctl, 1);
    PRINTF("POWER_CTL=0x%02X (Measure=%d)\r\n", pctl, (pctl>>3)&1);

    /* --- 3. Continuous read XYZ --- */
    while (1)
    {
        uint8_t d[6];
        if (adxl_spi_read_regs(EXAMPLE_ECSPI_MASTER_BASEADDR, kECSPI_Channel0, ADXL_REG_DATAX0, d, 6) == kStatus_Success)
        {
            int16_t x = (int16_t)((d[1] << 8) | d[0]);
            int16_t y = (int16_t)((d[3] << 8) | d[2]);
            int16_t z = (int16_t)((d[5] << 8) | d[4]);

            float x_g = x * 0.0039f; /* full-res scale */
            float y_g = y * 0.0039f;
            float z_g = z * 0.0039f;

            PRINTF("ADXL345 SPI:\r\n  ACC [g]: %7.3f  %7.3f  %7.3f\r\n", x_g, y_g, z_g);
        }
        else
        {
            PRINTF("Read XYZ failed\r\n");
        }

        SDK_DelayAtLeastUs(50 * 1000U, CLOCK_GetFreq(kCLOCK_CpuClk)); /* 50 ms delay */
    }
}
