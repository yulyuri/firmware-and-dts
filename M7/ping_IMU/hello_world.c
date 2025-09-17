#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"
#include <math.h>
#include <stdio.h>

static inline float wrap360(float a) {
    while (a >= 360.0f) a -= 360.0f;
    while (a <   0.0f) a += 360.0f;
    return a;
}

int main(void)
{
    BOARD_InitHardware();
    PRINTF("Starting simulated IMU output 2...\r\n");

    float t = 0.0f;

    while (1) {
        float pitch = 0.0f, roll = 0.0f, yaw = 0.0f;
        int phase = ((int)t / 5) % 3;  // change every ~5 "seconds" of your loop

        switch (phase) {
            case 0:  // diagonal forward
                pitch = wrap360(t * 5.0f);
                roll  = wrap360(t * 5.0f);
                yaw   = wrap360(t * 5.0f);
                break;
            case 1:  // reverse diagonal
                pitch = wrap360(-t * 5.0f);
                roll  = wrap360(-t * 5.0f);
                yaw   = wrap360(-t * 5.0f);
                break;
            case 2:  // alternate axis spin
                pitch = 0.0f;
                roll  = wrap360(t * 5.0f);
                yaw   = wrap360(-t * 5.0f);
                break;
        }

        PRINTF("%0.2f, %0.2f, %0.2f\r\n", roll, pitch, yaw);

        for (volatile int i = 0; i < 800000; ++i) { __NOP(); } // crude delay
        t += 0.05f;
    }
}
