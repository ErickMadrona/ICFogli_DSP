/*------------------------------STANDARD LIBRARIES------------------------------*/
#include <math.h>
#include <stdint.h>

/*------------------------------DRIVERLIB------------------------------*/
#include "gpio.h"
#include "dac.h"
#include "adc.h"
#include "device.h"
#include "interrupt.h"
#include "cpu.h"
#include "cputimer.h"
#include "inc/hw_memmap.h" // Bases
#include "inc/hw_ints.h" // Interrupt registers
#include "sysctl.h"

/*------------------------------MACROS------------------------------*/
#define TWO_PI (2 * M_PI)
#define TIMER0_FREQUENCY 50000000
#define TIMER0_DIVIDER 10000
#define DACA_INCREMENT (M_PI / 32.9) // 200 Hz sine wave

const uint16_t amplitudeDACA = 4095;

/*------------------------------ISR------------------------------*/
interrupt void timer0_ISR() {
    // LED 1 toggle to show program is running
    static uint16_t refreshCountLED1;
    refreshCountLED1 ++;
    if (refreshCountLED1 > TIMER0_DIVIDER) {
        GPIO_togglePin(DEVICE_GPIO_PIN_LED1);
        refreshCountLED1 = 0;
    }
    // Write to DACA
    static double angleDACA;
    angleDACA += DACA_INCREMENT;
    if (angleDACA > TWO_PI) {
        angleDACA = 0;
    }
    uint16_t outputDACA = (uint16_t)((sin(angleDACA) + 1) * amplitudeDACA / 2);
    DAC_setShadowValue(DACA_BASE, outputDACA);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

interrupt void ADCA1_ISR() {
    // LED 2 toggle to show SOC0 IFR is firing
    /*static uint16_t refreshCountLED2;
    refreshCountLED2 ++;
    if (refreshCountLED2 > ADC_CLK_DIV_4_0) {
        GPIO_togglePin(DEVICE_GPIO_PIN_LED2);
        refreshCountLED2 = 0;
    }*/
    //ADC_readResult(ADCA_BASE, ADC_SOC_NUMBER0);
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

/*------------------------------SETUP------------------------------*/
// GPIO and peripherals configuration
void configureGPIO() {
    // LED 1 as output
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);
    // LED 2 as output
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED2, GPIO_DIR_MODE_OUT);
}

// DACA (pin AA0) configuration
void configureDACA() {
    DAC_disableOutput(DACA_BASE);
    DAC_setLoadMode(DACA_BASE, DAC_LOAD_SYSCLK);
    DAC_setReferenceVoltage(DACA_BASE, DAC_REF_ADC_VREFHI);
    DAC_enableOutput(DACA_BASE);
    DAC_setShadowValue(DACA_BASE, 800U);
    // Initialisation delay
    DEVICE_DELAY_US(300);
}

// ADCA configuration on channel ADCIN14 (pin A14)
void configureADCA() {
    // ADCA reset and disabled for security
    SysCtl_resetPeripheral(SYSCTL_PERIPH_RES_ADCA);
    ADC_disableConverter(ADCA_BASE);
    // Resolution, conversion mode and clock settings
    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
    // SOC0 (start of conversion) setup on channel ADCIN14
    ADC_setSOCPriority(ADCA_BASE, ADC_PRI_ALL_ROUND_ROBIN); // No priority over other SOCs
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_CPU1_TINT0, ADC_CH_ADCIN14, 20); // Triggered by timer 0 interrupt
    ADC_setInterruptSOCTrigger(ADCA_BASE, ADC_SOC_NUMBER0, ADC_INT_SOC_TRIGGER_NONE); // SOC0 not triggered by ADC interrupts
    // Continuous interrupt pulse at end of conversion (value ready to read)
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableContinuousMode(ADCA_BASE, ADC_INT_NUMBER1); // ADCA interrupt 1
    // ADCA interrupt 1 enabled and linked to SOC0
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    Interrupt_register(INT_ADCA1, &ADCA1_ISR); // Interrupt registered to ISR
    Interrupt_enable(INT_ADCA1);
    ADC_enableConverter(ADCA_BASE);
    // Initialisation delay
    DEVICE_DELAY_US(1000);
}

// CPU timer 0 configuration
void configureTimer0() {
    const uint32_t timerPeriod = (uint32_t)(TIMER0_FREQUENCY / TIMER0_DIVIDER); // 50 us
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_setPeriod(CPUTIMER0_BASE, timerPeriod);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    // Timer 0 end-of-cycle interrupt
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    Interrupt_register(INT_TIMER0, &timer0_ISR);
    Interrupt_enable(INT_TIMER0);
}

void main(void) {
    /*------------------------------DEVICE------------------------------*/
    // Device initialisation
    Device_init();
    Device_initGPIO();
    /*------------------------------MODULES------------------------------*/
    configureGPIO();
    configureDACA();
    configureADCA();
    configureTimer0();
    /*------------------------------INTERRUPT------------------------------*/
    // PIE module initialisation
    Interrupt_initModule();
    Interrupt_initVectorTable();
    // Global interrupts and real-time debugging
    EINT;
    ERTM;
    // Main loop
    CPUTimer_startTimer(CPUTIMER0_BASE);
    while (true) {
        // Do nothing
    }
}
