/*
 * SIMPLE TCM vs DDR Benchmark with FFT
 * Clean version with easy-to-use macros
 */

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"

#define CPU_MHZ  800
#define PI       3.14159265f

/*EASY MACROS */
#define FAST_CODE   /* Nothing because default is TCM  */
#define SLOW_CODE   __attribute__((section(".text_ddr")))
#define SLOW_DATA   __attribute__((section(".rodata_ddr")))


static inline void start_timer(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0;
}

static inline uint32_t get_cycles(void) {
    return DWT->CYCCNT;
}

/*FFT SETUP*/
typedef struct {
    float real;
    float imag;
} complex_t;

// Fast FFT buffer (TCM - default)
complex_t fft_tcm[256];

// Slow FFT buffer (DDR) - FIXED: added const like array_slow
SLOW_DATA const float fft_ddr_storage[512] = {0};

/*MATH funct*/
float simple_sin(float x) {
    while (x > PI) x -= 2*PI;
    while (x < -PI) x += 2*PI;
    return x * (1.0f - x*x/6.0f);
}

float simple_cos(float x) {
    return simple_sin(x + PI/2.0f);
}

/*BIT reversal*/
uint32_t reverse_bits(uint32_t x, uint32_t bits) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < bits; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

/*FFT ALGO - TCM*/
FAST_CODE
void compute_fft_fast(complex_t *data, uint32_t n) {
    // Bit reversal
    uint32_t bits = 0;
    uint32_t temp = n;
    while (temp > 1) {
        bits++;
        temp >>= 1;
    }
    
    for (uint32_t i = 0; i < n; i++) {
        uint32_t j = reverse_bits(i, bits);
        if (j > i) {
            complex_t tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
    }
    
    // FFT butterfly
    for (uint32_t stage = 1; stage <= bits; stage++) {
        uint32_t m = 1 << stage;
        uint32_t m2 = m >> 1;
        
        for (uint32_t k = 0; k < n; k += m) {
            for (uint32_t j = 0; j < m2; j++) {
                float angle = -2.0f * PI * j / m;
                float wr = simple_cos(angle);
                float wi = simple_sin(angle);
                
                uint32_t idx1 = k + j;
                uint32_t idx2 = k + j + m2;
                
                float tr = wr * data[idx2].real - wi * data[idx2].imag;
                float ti = wr * data[idx2].imag + wi * data[idx2].real;
                
                data[idx2].real = data[idx1].real - tr;
                data[idx2].imag = data[idx1].imag - ti;
                data[idx1].real = data[idx1].real + tr;
                data[idx1].imag = data[idx1].imag + ti;
            }
        }
    }
}

/* FFT ALGO - SLOW (DDR)*/
SLOW_CODE
void compute_fft_slow(complex_t *data, uint32_t n) {
    // Bit reversal
    uint32_t bits = 0;
    uint32_t temp = n;
    while (temp > 1) {
        bits++;
        temp >>= 1;
    }
    
    for (uint32_t i = 0; i < n; i++) {
        uint32_t j = reverse_bits(i, bits);
        if (j > i) {
            complex_t tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
    }
    
    // FFT butterfly
    for (uint32_t stage = 1; stage <= bits; stage++) {
        uint32_t m = 1 << stage;
        uint32_t m2 = m >> 1;
        
        for (uint32_t k = 0; k < n; k += m) {
            for (uint32_t j = 0; j < m2; j++) {
                float angle = -2.0f * PI * j / m;
                float wr = simple_cos(angle);
                float wi = simple_sin(angle);
                
                uint32_t idx1 = k + j;
                uint32_t idx2 = k + j + m2;
                
                float tr = wr * data[idx2].real - wi * data[idx2].imag;
                float ti = wr * data[idx2].imag + wi * data[idx2].real;
                
                data[idx2].real = data[idx1].real - tr;
                data[idx2].imag = data[idx1].imag - ti;
                data[idx1].real = data[idx1].real + tr;
                data[idx1].imag = data[idx1].imag + ti;
            }
        }
    }
}

/* ============================================
 * TEST 1: Simple Loop
 * ============================================ */
FAST_CODE
uint32_t simple_loop_fast(uint32_t count) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        sum += i;
    }
    return sum;
}

SLOW_CODE
uint32_t simple_loop_slow(uint32_t count) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        sum += i;
    }
    return sum;
}

void test_simple_loop(void) {
    PRINTF("\r\n=== Test 1: Simple Loop ===\r\n");
    
    uint32_t start = get_cycles();
    simple_loop_fast(5000);
    uint32_t fast = get_cycles() - start;
    
    start = get_cycles();
    simple_loop_slow(5000);
    uint32_t slow = get_cycles() - start;
    
    PRINTF("FAST (TCM): %u cycles (%u us)\r\n", fast, fast/CPU_MHZ);
    PRINTF("SLOW (DDR): %u cycles (%u us)\r\n", slow, slow/CPU_MHZ);
    PRINTF("Speedup: %ux\r\n", slow/fast);
}

/* ============================================
 * TEST 2: FFT - NOW WITH COMPARISON!
 * ============================================ */
void test_fft(void) {
    PRINTF("\r\n=== Test 2: FFT ===\r\n");
    
    // Copy FFT buffer to local (so we can modify it)
    complex_t fft_ddr_local[256];
    
    // Generate test signal for TCM buffer
    for (uint32_t i = 0; i < 256; i++) {
        float t = (float)i / 1000.0f;
        fft_tcm[i].real = 1.0f +
                          0.5f * simple_sin(2*PI*50*t) +
                          0.3f * simple_sin(2*PI*200*t);
        fft_tcm[i].imag = 0.0f;
        
        // Same for DDR test
        fft_ddr_local[i] = fft_tcm[i];
    }
    
    // Test FAST (TCM)
    uint32_t start = get_cycles();
    compute_fft_fast(fft_tcm, 256);
    uint32_t fast = get_cycles() - start;
    
    // Test SLOW (DDR) - use local buffer
    start = get_cycles();
    compute_fft_slow(fft_ddr_local, 256);
    uint32_t slow = get_cycles() - start;
    
    PRINTF("FAST (TCM): %u cycles (%u us)\r\n", fast, fast/CPU_MHZ);
    PRINTF("SLOW (DDR): %u cycles (%u us)\r\n", slow, slow/CPU_MHZ);
    PRINTF("Speedup: %ux\r\n", slow/fast);
}

/* ============================================
 * TEST 3: Array Access
 * ============================================ */
float array_fast[500];

SLOW_DATA const float array_slow[500] = {[0 ... 499] = 1.0f};

void test_arrays(void) {
    PRINTF("\r\n=== Test 3: Array Access ===\r\n");
    
    // Fill fast array
    for (int i = 0; i < 500; i++) {
        array_fast[i] = (float)i;
    }
    
    // Read fast array
    volatile float sum = 0;
    uint32_t start = get_cycles();
    for (int i = 0; i < 500; i++) {
        sum += array_fast[i];
    }
    uint32_t fast = get_cycles() - start;
    
    // Read slow array
    sum = 0;
    start = get_cycles();
    for (int i = 0; i < 500; i++) {
        sum += array_slow[i];
    }
    uint32_t slow = get_cycles() - start;
    
    PRINTF("FAST (TCM): %u cycles (%u us)\r\n", fast, fast/CPU_MHZ);
    PRINTF("SLOW (DDR): %u cycles (%u us)\r\n", slow, slow/CPU_MHZ);
    
    if (slow > fast) {
        PRINTF("Speedup: %.1fx\r\n", (float)slow/fast);
    } else {
        PRINTF("Note: Cache helps DDR\r\n");
    }
}

/* ============================================
 * MAIN
 * ============================================ */
int main(void)
{
    BOARD_InitHardware();
    start_timer();
    
    PRINTF("\r\n");
    PRINTF("========================================\r\n");
    PRINTF(" TCM vs DDR \r\n");
    PRINTF("========================================\r\n");
    uint32_t cpu_freq = CLOCK_GetFreq(kCLOCK_CoreM7Clk);
    PRINTF("CPU frequency: %u Hz\r\n", cpu_freq);
    // Should print: CPU frequency: 800000000 Hz
    
    // Show memory layout
    PRINTF("\r\nMemory Addresses:\r\n");
    PRINTF("  Fast code:  0x%08X (TCM)\r\n", (uint32_t)simple_loop_fast);
    PRINTF("  Slow code:  0x%08X (DDR)\r\n", (uint32_t)simple_loop_slow);
    PRINTF("  FFT buffer: 0x%08X (TCM)\r\n", (uint32_t)fft_tcm);
    PRINTF("  Fast array: 0x%08X (TCM)\r\n", (uint32_t)array_fast);
    PRINTF("  Slow array: 0x%08X (DDR)\r\n", (uint32_t)array_slow);
    
    // Check placement
    if ((uint32_t)simple_loop_slow >= 0x80000000) {
        PRINTF("\r\n✓ DDR code working!\r\n");
    } else {
        PRINTF("\r\n✗ DDR code failed!\r\n");
    }
    
    if ((uint32_t)array_slow >= 0x80000000) {
        PRINTF("✓ DDR data working!\r\n");
    } else {
        PRINTF("✗ DDR data failed!\r\n");
    }
    
    // Run all tests
    test_simple_loop();
    test_fft();
    test_arrays();
    
    
    while (1) {
        char ch = GETCHAR();
        PUTCHAR(ch);
    }
}