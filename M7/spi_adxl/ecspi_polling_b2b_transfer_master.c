#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "fsl_ecspi.h"
#include "board.h"
#include "app.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define TRANSFER_BAUDRATE 500000U /* 500 kHz */

/* ADXL345 Registers */
#define ADXL_REG_DEVID        0x00
#define ADXL_REG_BW_RATE      0x2C
#define ADXL_REG_POWER_CTL    0x2D
#define ADXL_REG_DATA_FORMAT  0x31
#define ADXL_REG_DATAX0       0x32

/*******************************************************************************
 * Variables
 ******************************************************************************/
uint8_t masterTxData[16];
uint8_t masterRxData[16];
volatile uint32_t g_systickCounter = 0U;

/*******************************************************************************
 * Code
 ******************************************************************************/
void SysTick_Handler(void)
{
    if (g_systickCounter != 0U)
    {
        g_systickCounter--;
    }
}

static void DelayMs(uint32_t ms)
{
    g_systickCounter = ms;
    while (g_systickCounter != 0U)
    {
    }
}

int main(void)
{
    BOARD_InitHardware();
    PRINTF("ADXL345 SPI Example (Polling Master)\r\n");

    ecspi_master_config_t masterConfig;
    ecspi_transfer_t masterXfer;

    /* ECSPI master init */
    ECSPI_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Bps = TRANSFER_BAUDRATE;
    masterConfig.channel      = kECSPI_Channel0;

    /* Force Mode 3, CS active low */
    masterConfig.channelConfig.channelMode           = kECSPI_SpiMaster;
    masterConfig.channelConfig.clockInactiveState    = kECSPI_ClockInactiveStateHigh;
    masterConfig.channelConfig.dataLineInactiveState = kECSPI_DataLineInactiveStateHigh;
    masterConfig.channelConfig.chipSlectActiveState  = kECSPI_ChipSelectActiveStateLow;
    masterConfig.channelConfig.polarity              = kECSPI_ClockPolarityActiveHigh;   /* CPOL=1 */
    masterConfig.channelConfig.phase                 = kECSPI_ClockPhaseSecondEdge;     /* CPHA=1 */

    ECSPI_MasterInit(EXAMPLE_ECSPI_MASTER_BASEADDR, &masterConfig, ECSPI_MASTER_CLK_FREQ);

    /* Setup SysTick for 1ms tick */
    if (SysTick_Config(SystemCoreClock / 1000U))
    {
        while (1) {}
    }

    /***********************
     * 1. Read DEVID (0xE5)
     ***********************/
    masterTxData[0] = 0x80 | ADXL_REG_DEVID; /* Read command */
    masterTxData[1] = 0x00;                  /* dummy */

    masterXfer.channel  = kECSPI_Channel0;
    masterXfer.txData   = masterTxData;
    masterXfer.rxData   = masterRxData;
    masterXfer.dataSize = 2;
    ECSPI_MasterTransferBlocking(EXAMPLE_ECSPI_MASTER_BASEADDR, &masterXfer);

    uint8_t devid = masterRxData[1];
    PRINTF("ADXL345 DEVID = 0x%02X (expect 0xE5)\r\n", devid);

    /*******************************
     * 2. Configure ADXL registers
     *******************************/
    /* BW_RATE = 0x0C (400 Hz) */
    masterTxData[0] = ADXL_REG_BW_RATE;
    masterTxData[1] = 0x0C;
    masterXfer.txData   = masterTxData;
    masterXfer.rxData   = NULL;
    masterXfer.dataSize = 2;
    ECSPI_MasterTransferBlocking(EXAMPLE_ECSPI_MASTER_BASEADDR, &masterXfer);

    /* DATA_FORMAT = 0x08 (FULL_RES, ±2g) */
    masterTxData[0] = ADXL_REG_DATA_FORMAT;
    masterTxData[1] = 0x08;
    masterXfer.dataSize = 2;
    ECSPI_MasterTransferBlocking(EXAMPLE_ECSPI_MASTER_BASEADDR, &masterXfer);

    /* POWER_CTL = 0x08 (Measure) */
    masterTxData[0] = ADXL_REG_POWER_CTL;
    masterTxData[1] = 0x08;
    masterXfer.dataSize = 2;
    ECSPI_MasterTransferBlocking(EXAMPLE_ECSPI_MASTER_BASEADDR, &masterXfer);

    /****************************
     * 3. Loop: read XYZ values
     ****************************/
    while (1)
    {
        masterTxData[0] = 0x80 | 0x40 | ADXL_REG_DATAX0; /* Read + MB */
        for (int i = 1; i <= 6; i++) masterTxData[i] = 0x00;

        masterXfer.txData   = masterTxData;
        masterXfer.rxData   = masterRxData;
        masterXfer.dataSize = 7; /* 1 command + 6 data */
        ECSPI_MasterTransferBlocking(EXAMPLE_ECSPI_MASTER_BASEADDR, &masterXfer);

        int16_t x = (int16_t)((masterRxData[2] << 8) | masterRxData[1]);
        int16_t y = (int16_t)((masterRxData[4] << 8) | masterRxData[3]);
        int16_t z = (int16_t)((masterRxData[6] << 8) | masterRxData[5]);

        float x_g = x * 0.0039f;
        float y_g = y * 0.0039f;
        float z_g = z * 0.0039f;

        PRINTF("ADXL345 [g]: X=%7.3f  Y=%7.3f  Z=%7.3f\r\n", x_g, y_g, z_g);

        DelayMs(50);
    }
}
