#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"

// ============================================
// Memory Placement Macros
// ============================================
#define FAST_CODE   /* Default - ITCM */
#define SLOW_CODE   __attribute__((section(".text_ddr"), noinline))
#define SLOW_DATA   __attribute__((section(".rodata_ddr")))
// Don't define DDR_BSS yet since we haven't added .bss_ddr section

// ============================================
// FAST CODE - ITCM
// ============================================
FAST_CODE
uint32_t flight_control_loop(uint32_t input)
{
    volatile uint32_t result = 0;
    for(uint32_t i = 0; i < 10000; i++) {
        result += input * i;
    }
    return result;
}

// ============================================
// SLOW CODE - DDR
// ============================================
SLOW_CODE
void mavlink_handler(void)
{
    volatile uint32_t x = 0;
    for(int i = 0; i < 5000; i++) {
        x += i;
    }
    PRINTF("MAVLink: %lu\r\n", x);
}

SLOW_CODE
void logging_function(void)
{
    PRINTF("Logging to SD card...\r\n");
}

SLOW_DATA
const uint8_t crc_table[256] = {
    [0 ... 255] = 0xAA
};

// ============================================
// Main
// ============================================
int main(void)
{
    char ch;
    
    BOARD_InitHardware();

    PRINTF("\r\n");
    PRINTF("Flight Controller Memory Test\r\n");
    PRINTF("==============================\r\n");
    PRINTF("CRC table: %lu bytes\r\n", sizeof(crc_table));
    PRINTF("\r\n");

    uint32_t result = flight_control_loop(42);
    PRINTF("Flight control: %lu\r\n", result);
    
    mavlink_handler();
    logging_function();
    
    PRINTF("CRC[10]: 0x%02X\r\n", crc_table[10]);
    PRINTF("\r\n");
    PRINTF("Echo mode:\r\n");

    while (1)
    {
        ch = GETCHAR();
        PUTCHAR(ch);
    }
}