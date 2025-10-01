/*
 * ADXL345 SPI - Direct Register Access (No SDK Blocking)
 */

#include <string.h>
#include "board.h"
#include "fsl_debug_console.h"
#include "fsl_ecspi.h"
#include "app.h"
#include "fsl_clock.h"

#define ADXL_REG_DEVID           0x00
#define ADXL_REG_POWERCTL        0x2D
#define ADXL_REG_DATAFORMAT      0x31
#define ADXL_REG_BWRATE          0x2C
#define ADXL_REG_DATAX0          0x32
#define SPI_BAUDRATE_ADXL        1000000U

/* Direct SPI transfer without SDK blocking function */
static uint8_t SPI_TransferByte(uint8_t txByte)
{
    /* Write to TXDATA */
    EXAMPLE_ECSPI_MASTER_BASEADDR->TXDATA = txByte;
    
    /* Trigger exchange */
    EXAMPLE_ECSPI_MASTER_BASEADDR->CONREG |= (1U << 2);
    
    /* Wait for XCH bit to clear */
    uint32_t timeout = 100000;
    while ((EXAMPLE_ECSPI_MASTER_BASEADDR->CONREG & (1U << 2)) && timeout > 0) {
        timeout--;
    }
    
    if (timeout == 0) {
        PRINTF("SPI timeout!\r\n");
        return 0xFF;
    }
    
    /* Read RXDATA */
    return (uint8_t)(EXAMPLE_ECSPI_MASTER_BASEADDR->RXDATA & 0xFF);
}

static uint8_t ADXL_ReadRegister(uint8_t reg)
{
    SPI_TransferByte(reg | 0x80);  /* Send read command */
    return SPI_TransferByte(0x00);  /* Read response */
}

static void ADXL_WriteRegister(uint8_t reg, uint8_t value)
{
    SPI_TransferByte(reg);    /* Send register address */
    SPI_TransferByte(value);  /* Send data */
}

int main(void)
{
    uint32_t i;

    BOARD_InitHardware();

    PRINTF("\r\n========================================\r\n");
    PRINTF("ADXL345 Direct SPI\r\n");
    PRINTF("Build: %s %s\r\n", __DATE__, __TIME__);
    PRINTF("========================================\r\n\r\n");

    /* Manual ECSPI2 init */
    EXAMPLE_ECSPI_MASTER_BASEADDR->CONREG = 0;
    
    uint32_t conreg = 0;
    conreg |= (1U << 0);
    conreg |= (1U << 1);    /* MASTER mode */
    conreg |= (1U << 3);
    conreg |= (0x7U << 20);
    
    uint32_t srcClock = ECSPI_MASTER_CLK_FREQ;
    uint32_t divider = srcClock / SPI_BAUDRATE_ADXL;
    if (divider > 0) divider--;
    conreg |= ((divider & 0xF) << 12);
    conreg |= (((divider >> 4) & 0xF) << 8);
    
    uint32_t configreg = 0;
    configreg |= (1U << 0);
    configreg |= (1U << 4);
    configreg &= ~(1U << 8);
    configreg &= ~(1U << 12);
    configreg |= (1U << 16);
    
    EXAMPLE_ECSPI_MASTER_BASEADDR->CONREG = conreg;
    EXAMPLE_ECSPI_MASTER_BASEADDR->CONFIGREG = configreg;
    
    PRINTF("ECSPI2 ready. MODE=%u\r\n\r\n", (unsigned int)((EXAMPLE_ECSPI_MASTER_BASEADDR->CONREG >> 1) & 0x1));
    
    for (i = 0; i < 100000; i++);

    PRINTF("Reading DEVID...\r\n");
    uint8_t devid = ADXL_ReadRegister(ADXL_REG_DEVID);
    PRINTF("DEVID = 0x%02X ", devid);
    
    if (devid == 0xE5) {
        PRINTF("OK!\r\n\r\n");
        
        ADXL_WriteRegister(ADXL_REG_POWERCTL, 0x00);
        for (i = 0; i < 50000; i++);
        ADXL_WriteRegister(ADXL_REG_POWERCTL, 0x08);
        for (i = 0; i < 50000; i++);
        ADXL_WriteRegister(ADXL_REG_DATAFORMAT, 0x00);
        ADXL_WriteRegister(ADXL_REG_BWRATE, 0x0A);
        
        PRINTF("Reading data...\r\n");
        
        uint32_t count = 0;
        for (;;)
        {
            SPI_TransferByte(ADXL_REG_DATAX0 | 0xC0);  /* Multi-byte read */
            
            uint8_t x0 = SPI_TransferByte(0);
            uint8_t x1 = SPI_TransferByte(0);
            uint8_t y0 = SPI_TransferByte(0);
            uint8_t y1 = SPI_TransferByte(0);
            uint8_t z0 = SPI_TransferByte(0);
            uint8_t z1 = SPI_TransferByte(0);
            
            int16_t x = (int16_t)((x1 << 8) | x0);
            int16_t y = (int16_t)((y1 << 8) | y0);
            int16_t z = (int16_t)((z1 << 8) | z0);
            
            float x_g = (float)x / 256.0f;
            float y_g = (float)y / 256.0f;
            float z_g = (float)z / 256.0f;
            
            if (count % 5 == 0) {
                PRINTF("[%lu] X=%6.2f Y=%6.2f Z=%6.2f g\r\n", count, x_g, y_g, z_g);
            }
            
            count++;
            for (i = 0; i < 500000; i++);
        }
        
    } else {
        PRINTF("FAIL\r\n");
        while(1);
    }
}