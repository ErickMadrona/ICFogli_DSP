#include <math.h>
#include <stdio.h>

#include "cpu.h"
#include "gpio.h"
#include "dac.h"
#include "board.h"
#include "interrupt.h"
#include "device.h"
#include "cputimer.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "pin_map.h"

#define TIMER0_FREQUENCY 50000000 // 50 MHz
#define SINE_POINTS 100 // DACA and DACB | AA0 and AA1
#define DAC_GAIN 8 // Multiplies wave amplitude

// Sine 0 data
uint16_t sine0Output; // DAC output
uint16_t sine0Offset = 0; // Wave offset in relation to X axis
uint16_t sine0Amplitude = 4095; // Wave amplitude
int sine0Table[SINE_POINTS]; // Table with points describing the sine wave

// Sine 1 data
uint16_t sine1Output;
uint16_t sine1Offset = 0;
uint16_t sine1Amplitude = 2047; // Half the first amplitude
int sine1Table[SINE_POINTS];

// Function to populate sine tables
void populateSineTable(int *table, uint16_t amplitude) {
    int point;
    for (point = 0; point < SINE_POINTS; point ++) {
        // Angle for each point
        double angle = (2.0 * M_PI * point) / SINE_POINTS;
        // Creates each point using the sine function
        table[point] = (int)(sin(angle) * amplitude * DAC_GAIN);
    }
}

// Timer 0 ISR
interrupt void timer0_ISR(void) {
    // Toggles GPIO 122
    GPIO_togglePin(122);
    // Writes to DACA
    static uint16_t isine0 = 0;
    // Operators adapt the points to DAC range
    sine0Output = sine0Offset + ((sine0Table[isine0] ^ 0x8000) >> 5);
    // Increments or resets the index (cyclic nature)
    if (isine0 < SINE_POINTS) {
        isine0 ++;
    } else {
        isine0 = 0;
    }
    // New output in queue to update DACA
    DAC_setShadowValue(DACA_BASE, sine0Output);
    // Writes to DACB
    static uint16_t isine1 = 0;
    sine1Output = sine1Offset + ((sine1Table[isine1] ^ 0x8000) >> 5);
    if (isine1 < SINE_POINTS) {
        isine1 ++;
    } else {
        isine1 = 0;
    }
    DAC_setShadowValue(DACB_BASE, sine1Output);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

void main(void) {
    // Device initialisation
    Device_init();
    Device_initGPIO();
    // GPIO 122 configuration
    GPIO_setPadConfig(122, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(122, GPIO_DIR_MODE_OUT);
    // DAC configuration
    populateSineTable(sine0Table, sine0Amplitude);
    populateSineTable(sine1Table, sine1Amplitude);
    // Pin AA0
    DAC_setReferenceVoltage(DACA_BASE, DAC_REF_ADC_VREFHI);
    DAC_setLoadMode(DACA_BASE, DAC_LOAD_SYSCLK);
    DAC_enableOutput(DACA_BASE);
    DAC_setShadowValue(DACA_BASE, 800U);
    // Pin AA1
    DAC_setReferenceVoltage(DACB_BASE, DAC_REF_ADC_VREFHI);
    DAC_setLoadMode(DACB_BASE, DAC_LOAD_SYSCLK);
    DAC_enableOutput(DACB_BASE);
    DAC_setShadowValue(DACB_BASE, 800U);
    // DAC initialisation delay
    DEVICE_DELAY_US(300);
    // PIE configuration and initialisation
    Interrupt_initModule();
    Interrupt_initVectorTable();
    // Timer 0 configuration
    const uint32_t period = (uint32_t)(TIMER0_FREQUENCY / 10000); // 100 us
    CPUTimer_stopTimer(CPUTIMER0_BASE); // Security measure
    CPUTimer_setPeriod(CPUTIMER0_BASE, period);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    // Interrupt configuration
    Interrupt_enable(INT_TIMER0);
    Interrupt_register(INT_TIMER0, &timer0_ISR);
    // Global and real-time interrupt activation
    EINT;
    ERTM;
    // Timer 0 start
    CPUTimer_startTimer(CPUTIMER0_BASE);
    // Main loop
    while (true) {
        // Do nothing
    }
}
