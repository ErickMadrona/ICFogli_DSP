// C libraries
#include <stdint.h> // Standard integer types 

// DRIVERLIB libraries
#include "cpu.h" // For interrupt macros
#include "device.h" // For device initialisation 
#include "gpio.h" // For GPIO configuration
#include "inc/hw_ints.h" // For interrupt numbers
#include "inc/hw_memmap.h" // For CPU timer numbers
#include "interrupt.h" // For interrupt configuration
#include "cputimer.h" // For timer use

/*------------------------------MACROS------------------------------*/
#define TIMER0_FREQUENCY 50000000 // 50 MHz

// Timer 0 ISR
interrupt void timer0_ISR(void) {
    // Toggles LED 2
    GPIO_togglePin(DEVICE_GPIO_PIN_LED2);
    // Clears group 1 IFR
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

void main(void) {
    /*------------------------------DEVICE.H------------------------------*/
    // Initialises device
    Device_init();
    // Initialises GPIO and peripherals
    Device_initGPIO();
    /*------------------------------GPIO.H------------------------------*/
    // Configures LED1 and LED2 GPIOs
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED1, GPIO_PIN_TYPE_STD);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED2, GPIO_DIR_MODE_OUT);
    /*------------------------------INTERRUPT.H------------------------------*/
    // Initialises PIE module and vector table
    Interrupt_initModule();
    Interrupt_initVectorTable();
    // Stops and configures timer 0
    const uint32_t timerPeriod = (uint32_t)(TIMER0_FREQUENCY / 10000); // About 100 us
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_setPeriod(CPUTIMER0_BASE, timerPeriod);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);
    // Enables the timer 0 IER and registers its respective ISR
    Interrupt_enable(INT_TIMER0);
    Interrupt_register(INT_TIMER0, &timer0_ISR);
    /*------------------------------CPU.H------------------------------*/
    // Enables global interrupts and realtime interrupts
    EINT; // Same as "Interrupt_enableGlobal()"
    ERTM; // Unmasks interrupts during debugging
    // Starts timer 0
    CPUTimer_startTimer(CPUTIMER0_BASE);
    // Main loop
    const uint32_t toggleDelay = 100000;
    while (true) {
        // Periodically toggles LED 1
        GPIO_togglePin(DEVICE_GPIO_PIN_LED1);
        DEVICE_DELAY_US(toggleDelay);
    }
}
