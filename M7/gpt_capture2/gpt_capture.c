/*
 * PPM Input Capture Decoder - Test Version with Debug
 * Shows real-time edge capture status
 */

#include "fsl_debug_console.h"
#include "board.h"
#include "app.h"
#include "fsl_gpt.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define MAX_CHANNELS           4
#define PPM_SYNC_MIN           30000    /* Sync pulse minimum (µs) - starts new frame */
#define PPM_PULSE_MIN          8000     /* Minimum valid pulse (µs) */
#define PPM_PULSE_MAX          22000    /* Maximum valid pulse (µs) */
#define PPM_PULSE_DEFAULT      15000    /* Default/failsafe value (µs) */

/*******************************************************************************
 * Variables
 ******************************************************************************/
volatile bool gptIsrFlag = false;
volatile uint32_t lastCaptureVal = 0;
volatile uint32_t captureVal = 0;

/* PPM decoding state */
volatile uint32_t channelPulseWidth[MAX_CHANNELS] = {PPM_PULSE_DEFAULT, PPM_PULSE_DEFAULT, 
                                                      PPM_PULSE_DEFAULT, PPM_PULSE_DEFAULT};
volatile uint8_t currentChannel = 0;
volatile bool newFrameComplete = false;
volatile uint32_t frameCounter = 0;
volatile uint32_t edgeCounter = 0;

/* Debug variables */
volatile uint32_t lastTimeDiff_us = 0;
volatile bool newEdgeCaptured = false;

/*******************************************************************************
 * Code
 ******************************************************************************/
void EXAMPLE_GPT_CAPTURE_IRQHandler(void)
{
    uint32_t timeDiff;
    
    /* Clear interrupt flag */
    GPT_ClearStatusFlags(DEMO_GPT_BASE, BOARD_GPT_CHANNEL_FLAG);

    /* Get current capture value */
    captureVal = GPT_GetInputCaptureValue(DEMO_GPT_BASE, BOARD_GPT_INPUT_CAPTURE_CHANNEL);
    
    edgeCounter++;
    
    /* Calculate time difference between edges */
    if (captureVal >= lastCaptureVal)
        timeDiff = captureVal - lastCaptureVal;
    else
        timeDiff = (0xFFFFFFFF - lastCaptureVal) + captureVal; /* Handle counter overflow */
    
    lastCaptureVal = captureVal;
    
    /* Convert to microseconds based on your GPT clock frequency */
    uint32_t timeDiff_us = timeDiff / 24;  /* <-- ADJUST BASED ON YOUR GPT CLOCK */
    
    /* Store for debug output */
    lastTimeDiff_us = timeDiff_us;
    newEdgeCaptured = true;
    
    /* Check if this is a sync pulse (long gap) */
    if (timeDiff_us > PPM_SYNC_MIN)
    {
        /* Sync pulse detected - frame complete */
        if (currentChannel > 0)  /* Only count if we got at least one channel */
        {
            frameCounter++;
            newFrameComplete = true;
        }
        currentChannel = 0;  /* Reset to channel 0 for next frame */
    }
    else if (timeDiff_us >= PPM_PULSE_MIN && timeDiff_us <= PPM_PULSE_MAX)
    {
        /* Valid channel pulse */
        if (currentChannel < MAX_CHANNELS)
        {
            channelPulseWidth[currentChannel] = timeDiff_us;
            currentChannel++;
        }
    }
    /* Else: pulse too short or too long, ignore */
    
    gptIsrFlag = true;
    SDK_ISR_EXIT_BARRIER;
}

/*!
 * @brief Main function
 */
int main(void)
{
    gpt_config_t gptConfig;
    uint32_t lastPrintedFrame = 0;
    uint32_t lastPrintedEdge = 0;

    /* Board pin, clock, debug console init */
    BOARD_InitHardware();

    /* Print header */
    PRINTF("\r\n");
    PRINTF("========================================\r\n");
    PRINTF("   PPM Decode Test 3 (4 Channels)       \r\n");
    PRINTF("========================================\r\n");
    PRINTF("Waiting for PPM input signal...\r\n");
    PRINTF("========================================\r\n\r\n");

    /* Initialize GPT for input capture */
    GPT_GetDefaultConfig(&gptConfig);
    
    /* IMPORTANT: Configure GPT clock/prescaler for microsecond timing */
    /* Adjust gptConfig.divider if needed */
    
    GPT_Init(DEMO_GPT_BASE, &gptConfig);

    /* Setup input capture on rising edge */
    GPT_SetInputOperationMode(DEMO_GPT_BASE, BOARD_GPT_INPUT_CAPTURE_CHANNEL, kGPT_InputOperation_RiseEdge);

    /* Enable GPT interrupt */
    GPT_EnableInterrupts(DEMO_GPT_BASE, BOARD_GPT_CHANNEL_INTERRUPT_ENABLE);
    EnableIRQ(DEMO_GPT_IRQn);

    /* Start GPT timer */
    GPT_StartTimer(DEMO_GPT_BASE);

    PRINTF("GPT Capture started!\r\n");
    PRINTF("Listening for edges...\r\n\r\n");

    while (true)
    {
        /* Show real-time edge detection */
        if (newEdgeCaptured && (edgeCounter != lastPrintedEdge))
        {
            newEdgeCaptured = false;
            lastPrintedEdge = edgeCounter;
            
            /* Print every edge with timing info */
            PRINTF("[Edge #%4d] Time: %6d µs | Ch: %d | ", 
                   (int)edgeCounter, (int)lastTimeDiff_us, (int)currentChannel);
            
            /* Indicate what type of pulse this was */
            if (lastTimeDiff_us > PPM_SYNC_MIN)
                PRINTF("SYNC\r\n");
            else if (lastTimeDiff_us >= PPM_PULSE_MIN && lastTimeDiff_us <= PPM_PULSE_MAX)
                PRINTF("VALID\r\n");
            else if (lastTimeDiff_us < PPM_PULSE_MIN)
                PRINTF("TOO SHORT\r\n");
            else
                PRINTF("TOO LONG\r\n");
        }
        
        /* Check if new complete frame received */
        if (newFrameComplete && (frameCounter != lastPrintedFrame))
        {
            newFrameComplete = false;
            lastPrintedFrame = frameCounter;
            
            /* Print separator */
            PRINTF("\r\n>>> FRAME COMPLETE #%d <<<\r\n", (int)frameCounter);
            
            /* Print frame number and all channel values */
            PRINTF("Frame #%4d | ", (int)frameCounter);
            
            for (int i = 0; i < MAX_CHANNELS; i++)
            {
                PRINTF("CH%d: %5d µs", i+1, (int)channelPulseWidth[i]);
                if (i < MAX_CHANNELS - 1)
                    PRINTF(" | ");
            }
            PRINTF("\r\n\r\n");
        }
        
        /* Activity indicator if no edges at all */
        static uint32_t inactiveCounter = 0;
        if (edgeCounter == lastPrintedEdge)
        {
            inactiveCounter++;
            if (inactiveCounter > 1000000) /* Adjust for your CPU speed */
            {
                PRINTF(".");
                inactiveCounter = 0;
            }
        }
        else
        {
            inactiveCounter = 0;
        }
        
        __WFI(); /* Sleep until next interrupt */
    }
}