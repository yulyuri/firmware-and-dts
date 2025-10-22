/*
 * TCM vs DDR FFT Benchmark
 * FIXED: Ensures DDR data is actually allocated
 */

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"
#include <string.h>

/* ============================================
 * Configuration
 * ============================================ */
#define CPU_FREQ_MHZ    800
#define PI              3.14159265358979f

/* ============================================
 * Cycle Counter
 * ============================================ */
static inline void init_cycle_counter(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t get_cycles(void) {
    return DWT->CYCCNT;
}

void print_speedup(uint32_t cycles_tcm, uint32_t cycles_ddr) {
    uint32_t speedup_x100 = (cycles_ddr * 100) / cycles_tcm;
    uint32_t whole = speedup_x100 / 100;
    uint32_t frac = speedup_x100 % 100;
    PRINTF("Speedup: %u.%02ux\r\n", whole, frac);
}

/* ============================================
 * Simple Complex Number Type
 * ============================================ */
typedef struct {
    float real;
    float imag;
} complex_t;

/* ============================================
 * TCM FFT Buffers (Default)
 * ============================================ */
complex_t fft_buffer_tcm_256[256];  // 4 KB

/* ============================================
 * DDR FFT Buffers (Explicit) - VOLATILE!
 * Use volatile to prevent optimization
 * ============================================ */
__attribute__((section(".data_ddr")))
volatile complex_t fft_buffer_ddr_256[256];  // 4 KB

__attribute__((section(".data_ddr")))
volatile complex_t fft_buffer_ddr_512[512];  // 8 KB

__attribute__((section(".data_ddr")))
volatile float large_log_buffer[5000];  // 20 KB - clearly in DDR

/* ============================================
 * Fast Trig Functions
 * ============================================ */
float fast_sin(float x) {
    while (x > PI) x -= 2*PI;
    while (x < -PI) x += 2*PI;
    float x2 = x * x;
    return x * (1.0f - x2/6.0f * (1.0f - x2/20.0f));
}

float fast_cos(float x) {
    return fast_sin(x + PI/2.0f);
}

/* ============================================
 * Bit Reversal
 * ============================================ */
uint32_t reverse_bits(uint32_t x, uint32_t bits) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < bits; i++) {
        result = (result << 1) | (x & 1);
        x >>= 1;
    }
    return result;
}

/* ============================================
 * FFT for TCM buffers
 * ============================================ */
void fft_radix2_tcm(complex_t *data, uint32_t n) {
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
    
    // FFT computation
    for (uint32_t stage = 1; stage <= bits; stage++) {
        uint32_t m = 1 << stage;
        uint32_t m2 = m >> 1;
        
        for (uint32_t k = 0; k < n; k += m) {
            for (uint32_t j = 0; j < m2; j++) {
                float angle = -2.0f * PI * j / m;
                float wr = fast_cos(angle);
                float wi = fast_sin(angle);
                
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
 * FFT for DDR buffers (separate function to prevent inlining)
 * ============================================ */
__attribute__((section(".text_ddr"), noinline))
void fft_radix2_ddr(volatile complex_t *data, uint32_t n) {
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
            complex_t tmp;
            tmp.real = data[i].real;
            tmp.imag = data[i].imag;
            data[i].real = data[j].real;
            data[i].imag = data[j].imag;
            data[j].real = tmp.real;
            data[j].imag = tmp.imag;
        }
    }
    
    // FFT computation
    for (uint32_t stage = 1; stage <= bits; stage++) {
        uint32_t m = 1 << stage;
        uint32_t m2 = m >> 1;
        
        for (uint32_t k = 0; k < n; k += m) {
            for (uint32_t j = 0; j < m2; j++) {
                float angle = -2.0f * PI * j / m;
                float wr = fast_cos(angle);
                float wi = fast_sin(angle);
                
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
 * Generate Test Signal
 * ============================================ */
void generate_gyro_signal_tcm(complex_t *buffer, uint32_t size) {
    float fs = 1000.0f;
    for (uint32_t i = 0; i < size; i++) {
        float t = (float)i / fs;
        buffer[i].real = 1.0f + 
                        0.5f * fast_sin(2*PI*50*t) +
                        0.3f * fast_sin(2*PI*200*t);
        buffer[i].imag = 0.0f;
    }
}

void generate_gyro_signal_ddr(volatile complex_t *buffer, uint32_t size) {
    float fs = 1000.0f;
    for (uint32_t i = 0; i < size; i++) {
        float t = (float)i / fs;
        buffer[i].real = 1.0f + 
                        0.5f * fast_sin(2*PI*50*t) +
                        0.3f * fast_sin(2*PI*200*t);
        buffer[i].imag = 0.0f;
    }
}

/* ============================================
 * Test: 256-point FFT
 * ============================================ */
void test_fft_256(void) {
    uint32_t cycles_tcm, cycles_ddr;
    
    PRINTF("\r\n=== FFT 256-point (4 KB) ===\r\n");
    PRINTF("Use case: Gyro vibration filtering\r\n\r\n");
    
    // Generate signals
    generate_gyro_signal_tcm(fft_buffer_tcm_256, 256);
    generate_gyro_signal_ddr(fft_buffer_ddr_256, 256);
    
    // TCM FFT
    uint32_t start = get_cycles();
    fft_radix2_tcm(fft_buffer_tcm_256, 256);
    cycles_tcm = get_cycles() - start;
    
    // DDR FFT
    start = get_cycles();
    fft_radix2_ddr(fft_buffer_ddr_256, 256);
    cycles_ddr = get_cycles() - start;
    
    uint32_t time_tcm = (cycles_tcm * 100) / CPU_FREQ_MHZ;
    uint32_t time_ddr = (cycles_ddr * 100) / CPU_FREQ_MHZ;
    
    PRINTF("TCM: %u cycles (%u.%02u us)\r\n", 
           cycles_tcm, time_tcm / 100, time_tcm % 100);
    PRINTF("DDR: %u cycles (%u.%02u us)\r\n", 
           cycles_ddr, time_ddr / 100, time_ddr % 100);
    print_speedup(cycles_tcm, cycles_ddr);
    
    // Verify result
    PRINTF("DC bin[0]: %.1f\r\n", fft_buffer_tcm_256[0].real);
}

/* ============================================
 * Test: Array Processing (Sensor Logging)
 * ============================================ */
void test_sensor_logging(void) {
    PRINTF("\r\n=== Sensor Data Logging ===\r\n");
    PRINTF("Logging 5000 samples\r\n\r\n");
    
    // Write sensor data to DDR log
    uint32_t start = get_cycles();
    for (uint32_t i = 0; i < 5000; i++) {
        // Simulate sensor reading and logging
        large_log_buffer[i] = (float)i * 0.01f;
    }
    uint32_t cycles_write = get_cycles() - start;
    
    // Read back from log
    volatile float sum = 0;
    start = get_cycles();
    for (uint32_t i = 0; i < 5000; i++) {
        sum += large_log_buffer[i];
    }
    uint32_t cycles_read = get_cycles() - start;
    
    uint32_t time_write = (cycles_write * 100) / CPU_FREQ_MHZ;
    uint32_t time_read = (cycles_read * 100) / CPU_FREQ_MHZ;
    
    PRINTF("Write: %u cycles (%u.%02u us)\r\n",
           cycles_write, time_write / 100, time_write % 100);
    PRINTF("Read:  %u cycles (%u.%02u us)\r\n",
           cycles_read, time_read / 100, time_read % 100);
    PRINTF("Sum: %u (verify)\r\n", (uint32_t)sum);
}

/* ============================================
 * Test: Matrix Operations (Kalman Filter)
 * ============================================ */
#define MAT_SIZE 4

void matrix_multiply(float A[MAT_SIZE][MAT_SIZE],
                     float B[MAT_SIZE][MAT_SIZE],
                     float C[MAT_SIZE][MAT_SIZE]) {
    for (int i = 0; i < MAT_SIZE; i++) {
        for (int j = 0; j < MAT_SIZE; j++) {
            C[i][j] = 0;
            for (int k = 0; k < MAT_SIZE; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

void test_matrix_kalman(void) {
    PRINTF("\r\n=== Matrix Ops (EKF 4x4) ===\r\n");
    PRINTF("100 iterations @ 100 Hz\r\n\r\n");
    
    // TCM matrices
    float A_tcm[MAT_SIZE][MAT_SIZE];
    float B_tcm[MAT_SIZE][MAT_SIZE];
    float C_tcm[MAT_SIZE][MAT_SIZE];
    
    // Initialize
    for (int i = 0; i < MAT_SIZE; i++) {
        for (int j = 0; j < MAT_SIZE; j++) {
            A_tcm[i][j] = (float)(i + j + 1);
            B_tcm[i][j] = (float)(i * j + 1);
        }
    }
    
    // Benchmark
    uint32_t start = get_cycles();
    for (int iter = 0; iter < 100; iter++) {
        matrix_multiply(A_tcm, B_tcm, C_tcm);
    }
    uint32_t cycles = get_cycles() - start;
    
    uint32_t time = (cycles * 100) / CPU_FREQ_MHZ;
    uint32_t per_iter = (cycles * 100) / (CPU_FREQ_MHZ * 100);
    
    PRINTF("Total: %u cycles (%u.%02u us)\r\n",
           cycles, time / 100, time % 100);
    PRINTF("Per iteration: %u.%02u us\r\n",
           per_iter / 100, per_iter % 100);
    PRINTF("Result[0][0]: %.1f\r\n", C_tcm[0][0]);
}

/* ============================================
 * Main
 * ============================================ */
int main(void)
{
    char ch;
    
    BOARD_InitHardware();
    
    PRINTF("\r\n");
    PRINTF("========================================\r\n");
    PRINTF("  Flight Controller FFT Benchmark\r\n");
    PRINTF("  TCM vs DDR Performance\r\n");
    PRINTF("  Cortex-M7 @ 800 MHz\r\n");
    PRINTF("========================================\r\n");
    
    PRINTF("\r\nMemory Layout:\r\n");
    PRINTF("  ITCM: 128 KB (code)\r\n");
    PRINTF("  DTCM: 128 KB (data)\r\n");
    PRINTF("  DDR:  16 MB (large buffers)\r\n");
    
    // Show memory addresses
    PRINTF("\r\nBuffer Locations:\r\n");
    PRINTF("  TCM buffer: 0x%08X\r\n", (uint32_t)fft_buffer_tcm_256);
    PRINTF("  DDR buffer: 0x%08X\r\n", (uint32_t)fft_buffer_ddr_256);
    PRINTF("  DDR log:    0x%08X\r\n", (uint32_t)large_log_buffer);
    
    init_cycle_counter();
    
    // Run tests
    test_fft_256();
    test_sensor_logging();
    test_matrix_kalman();
    
    // Summary
    PRINTF("\r\n========================================\r\n");
    PRINTF("Flight Controller Recommendations\r\n");
    PRINTF("========================================\r\n");
    PRINTF("\r\n");
    PRINTF("Real-time FFT (< 1 ms):\r\n");
    PRINTF("  - Use TCM for ~5x speedup\r\n");
    PRINTF("  - 256-point fits comfortably\r\n");
    PRINTF("\r\n");
    PRINTF("Large buffers (logs, telemetry):\r\n");
    PRINTF("  - Use DDR (20+ KB buffers)\r\n");
    PRINTF("  - Sequential access is fast enough\r\n");
    PRINTF("\r\n");
    PRINTF("Matrix operations (EKF):\r\n");
    PRINTF("  - Keep in TCM (small size)\r\n");
    PRINTF("  - Critical for sensor fusion\r\n");
    PRINTF("\r\n");
    PRINTF("Echo mode:\r\n");

    while (1)
    {
        ch = GETCHAR();
        PUTCHAR(ch);
    }
}