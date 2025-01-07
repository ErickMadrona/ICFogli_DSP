/*------------------------------STANDARD LIBRARIES------------------------------*/
#include <math.h>
#include <stdio.h>

/*------------------------------DRIVERLIB------------------------------*/
#include "cpu.h"
#include "gpio.h"
#include "dac.h"
#include "board.h"
#include "interrupt.h"
#include "device.h"
#include "cputimer.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"

/*------------------------------MACROS------------------------------*/
#define TIMER0_FREQUENCY 50000000 // 50 MHz
#define DAC_AMPLITUDE 4095 // 12-bit DAC
#define SINE_INCREMENT M_PI / 180.0 // 1 degree

/*------------------------------GLOBAL VARIABLES------------------------------*/
uint16_t amplitudeSine0 = DAC_AMPLITUDE;
uint16_t amplitudeSine1 = DAC_AMPLITUDE / 2;

/*------------------------------ISR------------------------------*/
// Timer 0 ISR
interrupt void timer0_ISR(void) {
    // Toggles GPIO 122
    GPIO_togglePin(122);
    // Writes to DACA
    static double angleSine0 = 0;
    if (angleSine0 < M_PI * 2) {
        angleSine0 += SINE_INCREMENT;
    } else {
        angleSine0 = 0;
    }
    uint16_t outputSine0 = (sin(angleSine0) + 1) * amplitudeSine0 / 2;
    DAC_setShadowValue(DACA_BASE, outputSine0);
    // Writes to DACB
    static double angleSine1 = 0;
    if (angleSine1 < M_PI * 2) {
        angleSine1 += SINE_INCREMENT;
    } else {
        angleSine1 = 0;
    }
    uint16_t outputSine1 = (sin(angleSine1) + 1) * amplitudeSine1 / 2;
    DAC_setShadowValue(DACB_BASE, outputSine1);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

void main(void) {
    /*------------------------------DEVICE------------------------------*/
    // Device initialisation
    Device_init();
    Device_initGPIO();
    /*------------------------------GPIO------------------------------*/
    // GPIO 122 configuration
    GPIO_setPadConfig(122, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(122, GPIO_DIR_MODE_OUT);
    /*------------------------------DAC A AND B------------------------------*/
    // Pin AA0 initialisation
    DAC_setReferenceVoltage(DACA_BASE, DAC_REF_ADC_VREFHI);
    DAC_setLoadMode(DACA_BASE, DAC_LOAD_SYSCLK);
    DAC_enableOutput(DACA_BASE);
    DAC_setShadowValue(DACA_BASE, 800U);
    // Pin AA1 initialisation
    DAC_setReferenceVoltage(DACB_BASE, DAC_REF_ADC_VREFHI);
    DAC_setLoadMode(DACB_BASE, DAC_LOAD_SYSCLK);
    DAC_enableOutput(DACB_BASE);
    DAC_setShadowValue(DACB_BASE, 800U);
    // DAC initialisation delay
    DEVICE_DELAY_US(300);
    /*------------------------------INTERRUPT AND TIMER 0------------------------------*/
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
    /*------------------------------MAIN LOOP------------------------------*/
    while (true) {
        // Do nothing
    }
}
