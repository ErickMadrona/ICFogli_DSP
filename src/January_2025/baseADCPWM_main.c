/*------------------------------STANDARD LIBRARIES------------------------------*/

#include <math.h>
#include <stdint.h>

/*------------------------------DRIVERLIB------------------------------*/

#include "debug.h"
#include "gpio.h"
#include "dac.h"
#include "adc.h"
#include "epwm.h"
#include "device.h"
#include "inc/hw_types.h"
#include "interrupt.h"
#include "cpu.h"
#include "cputimer.h"
#include "inc/hw_memmap.h" // Bases
#include "inc/hw_ints.h" // Interrupt registers
#include "machine/_types.h"
#include "pin_map.h"
#include "sysctl.h"

/*------------------------------GLOBAL VARIABLES AND MACROS------------------------------*/

#define TWO_PI (2.0 * M_PI)

#define TIMER0_FREQ 10000.0 // 10 kHz, period 100 us
#define TIMER0_PRD 0.000100

#define HALF_DEVICE_SYSCLK_FREQ (DEVICE_SYSCLK_FREQ / 2)

// ADCA buffer
#define BUFFER_LENGTH 167 // From the formula: (TIMER0 frequency / DACA frequency)

uint16_t bufferADCA_SOC0[BUFFER_LENGTH];

// DACA parameters
double amplitudeDACA = 4095.0; // Digital level
double offsetDACA = 2048.0; // Shift wave X levels upward
double frequencyDACA = 60.0; // Sine wave frequency in Hz

// PWM1 parameters
EPWM_TimeBaseCountMode countModePWM1 = EPWM_COUNTER_MODE_UP_DOWN;
uint16_t frequencyPWM1 = 1000U;
float dutyCyclePWM1 = 0.80; // Ranges from 0 to 1
uint16_t phaseShiftPWM1 = 0U;

/*------------------------------EPWM FUNCTIONS------------------------------*/

void setPWMFrequency(uint32_t base, EPWM_TimeBaseCountMode counterMode, uint16_t frequency) {
    // Frequency MUST BE positive
    if (frequency <= 0) {
        frequency = 1U;
    }
    uint16_t timeBasePeriod;
    // Symmetric PWM waveform
    if (counterMode == EPWM_COUNTER_MODE_UP_DOWN) {
        timeBasePeriod = HALF_DEVICE_SYSCLK_FREQ / (2 * frequency);
    // Asymmetric PWM waveform
    } else {
        timeBasePeriod = HALF_DEVICE_SYSCLK_FREQ / frequency - 1;
    }
    EPWM_setTimeBaseCounterMode(base, counterMode);
    EPWM_setTimeBasePeriod(base, timeBasePeriod);
    EPWM_setTimeBaseCounter(base, 0U);
}

void setPWMDutyCycle(uint32_t base, EPWM_TimeBaseCountMode counterMode, EPWM_CounterCompareModule module, float dutyCycle) {
    // Duty cycle MUST BE between 0 and 1
    if (dutyCycle < 0.0) {
        dutyCycle = 0.0;
    } else if (dutyCycle > 1.0) {
        dutyCycle = 1.0;
    }
    uint16_t counterCompareValue;
    float timeBasePeriod = (float)(EPWM_getTimeBasePeriod(base));
    // Symmetric PWM waveform
    if (counterMode == EPWM_COUNTER_MODE_UP_DOWN) {
        counterCompareValue = (uint16_t)(timeBasePeriod * (1.0 - dutyCycle));
    // Asymmetric PWM waveform
    } else {
        counterCompareValue = (uint16_t)((timeBasePeriod + 1.0) * (1.0 - dutyCycle) - 1.0);
    }
    EPWM_setCounterCompareValue(base, module, counterCompareValue);
}

/*------------------------------ISR------------------------------*/

interrupt void timer0_ISR() {
    // Timer 0 frequency check
    GPIO_togglePin(122U);
    // Time elapsed since sine wave began
    static double timeElapsed;
    timeElapsed += TIMER0_PRD;
    // Write to DACA
    uint16_t outputDACA = (uint16_t)(sin(TWO_PI * frequencyDACA * timeElapsed) * amplitudeDACA * 0.5 + offsetDACA);
    DAC_setShadowValue(DACA_BASE, outputDACA);
    // Update PWM1
    if (fmod(timeElapsed, 3.0) < 0.05) {
        setPWMFrequency(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN, frequencyPWM1);
        setPWMDutyCycle(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN, EPWM_COUNTER_COMPARE_A, dutyCyclePWM1);
        setPWMDutyCycle(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN, EPWM_COUNTER_COMPARE_B, dutyCyclePWM1);
        EPWM_setPhaseShift(EPWM1_BASE, phaseShiftPWM1);
    }
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

interrupt void ADCA1_ISR() {
    // Buffer to store SOC0 read result history
    static uint16_t bufferIndex_SOC0;
    bufferIndex_SOC0 ++;
    if (bufferIndex_SOC0 >= BUFFER_LENGTH) {
        bufferIndex_SOC0 = 0;
    }
    // Read result saved in the buffer
    const uint16_t resultADCA_SOC0 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
    bufferADCA_SOC0[bufferIndex_SOC0] = resultADCA_SOC0;
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}

/*------------------------------SETUP------------------------------*/

// GPIO and peripherals configuration
void configureGPIO() {
    // GPIO 122 (pin 17) as output
    GPIO_setPadConfig(122U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(122U, GPIO_DIR_MODE_OUT);
    // GPIO 0 (pin 40) as EPWM1A output
    GPIO_setPinConfig(GPIO_0_EPWM1A);
    GPIO_setPadConfig(0U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(0U, GPIO_DIR_MODE_OUT);
    // GPIO 1 (pin 39) as EPWM1B output
    GPIO_setPinConfig(GPIO_1_EPWM1B);
    GPIO_setPadConfig(1U, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(1U, GPIO_DIR_MODE_OUT);
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

// EPWM configuration on GPIO 0 and 1 (pins 40 and 39)
void configureEPWM1() {
    // Time-base submodule
    // EPWM clock frequency is HALF_DEVICE_SYSCLK_FREQ by default (100 MHz)
    EPWM_setClockPrescaler(EPWM1_BASE, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setPeriodLoadMode(EPWM1_BASE, EPWM_PERIOD_SHADOW_LOAD);
    // Frequency and duty cycle first loaded
    setPWMFrequency(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN, frequencyPWM1);
    setPWMDutyCycle(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN, EPWM_COUNTER_COMPARE_A, dutyCyclePWM1);
    setPWMDutyCycle(EPWM1_BASE, EPWM_COUNTER_MODE_UP_DOWN, EPWM_COUNTER_COMPARE_B, dutyCyclePWM1);
    EPWM_setPhaseShift(EPWM1_BASE, 0U);
    // Counter-compare submodule
    // New counter-compare value loaded at every zero and TBPRD
    EPWM_setCounterCompareShadowLoadMode(EPWM1_BASE, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO_PERIOD); 
    EPWM_setCounterCompareShadowLoadMode(EPWM1_BASE, EPWM_COUNTER_COMPARE_B, EPWM_COMP_LOAD_ON_CNTR_ZERO_PERIOD);
    // Action-qualifier submodule
    EPWM_setActionQualifierShadowLoadMode(EPWM1_BASE, EPWM_ACTION_QUALIFIER_A, EPWM_AQ_LOAD_ON_CNTR_ZERO_PERIOD);
    EPWM_setActionQualifierShadowLoadMode(EPWM1_BASE, EPWM_ACTION_QUALIFIER_B, EPWM_AQ_LOAD_ON_CNTR_ZERO_PERIOD);
    EPWM_setActionQualifierAction(EPWM1_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    EPWM_setActionQualifierAction(EPWM1_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);
    // EPWM1-B as a complement to A (inverse actions)
    EPWM_setActionQualifierAction(EPWM1_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);
    EPWM_setActionQualifierAction(EPWM1_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    // Configuration done, EPWM1 clock enabled
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_EPWM1);
}

// ADCA configuration on input channel ADCIN14 (pin A14)
void configureADCA() {
    // ADCA reset and disabled for security
    SysCtl_resetPeripheral(SYSCTL_PERIPH_RES_ADCA);
    ADC_disableConverter(ADCA_BASE);
    // Resolution, conversion mode and clock settings
    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED); // Single-pin conversion mode
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
    // SOC0 setup on channel ADCIN14
    ADC_setSOCPriority(ADCA_BASE, ADC_PRI_ALL_ROUND_ROBIN); // No SOC priority
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_CPU1_TINT0, ADC_CH_ADCIN14, 20U); // Triggered by timer 0 interrupt
    ADC_setInterruptSOCTrigger(ADCA_BASE, ADC_SOC_NUMBER0, ADC_INT_SOC_TRIGGER_NONE); // SOC0 not triggered by ADC interrupts
    // Interrupt pulse at end of conversion (value ready to read)
    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableContinuousMode(ADCA_BASE, ADC_INT_NUMBER1); // ADCA interrupt 1
    // ADCA interrupt 1 enabled and linked to SOC0
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    Interrupt_register(INT_ADCA1, &ADCA1_ISR);
    Interrupt_enable(INT_ADCA1);
    ADC_enableConverter(ADCA_BASE);
    // Initialisation delay
    DEVICE_DELAY_US(1000);
}

// CPU timer 0 configuration
void configureTimer0() {
    // Timer 0 uses half system clock frequency (100 MHz)
    const uint32_t timerPeriod = HALF_DEVICE_SYSCLK_FREQ / (uint32_t)TIMER0_FREQ;
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_setPreScaler(CPUTIMER0_BASE, CPUTIMER_CLOCK_PRESCALER_1);
    CPUTimer_setPeriod(CPUTIMER0_BASE, timerPeriod);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    // Timer 0 end-of-cycle interrupt
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    Interrupt_register(INT_TIMER0, &timer0_ISR);
    Interrupt_enable(INT_TIMER0);
}

/*------------------------------MAIN------------------------------*/

void main(void) {
    /*------------------------------DEVICE------------------------------*/
    // Device initialisation
    Device_init();
    Device_initGPIO();
    /*------------------------------INTERRUPT------------------------------*/
    // PIE module initialisation
    Interrupt_initModule();
    Interrupt_initVectorTable();
    /*------------------------------MODULES------------------------------*/
    configureGPIO();
    configureDACA();
    configureEPWM1();
    configureADCA();
    configureTimer0();
    // Global interrupts and real-time debugging initialisation
    EINT;
    ERTM;
    // Main loop
    CPUTimer_startTimer(CPUTIMER0_BASE);
    while (1) {
        // No operation
    }
}
