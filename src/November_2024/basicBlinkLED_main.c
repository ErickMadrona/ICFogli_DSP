#include <stdint.h>
#include "cpu.h"
#include "gpio.h"
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "c2000ware_libraries.h"

void main(void) {
    // Initialises LaunchPad
    Device_init();
    // Initialises GPIO and peripherals
    Device_initGPIO();
    // Configures LED 1 pin as a standard output
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED1, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_LED2, GPIO_PIN_TYPE_STD);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED2, GPIO_DIR_MODE_OUT);
    // Initialises PIE (interrupt module) and clears leftover interrupt data
    Interrupt_initModule();
    // Initialises PIE vector table with pointers to ISRs (interrupt service routines)
    Interrupt_initVectorTable();
    // Enables global interrupts (INTM) and realtime interrupts (DBGM)
    EINT;
    ERTM;
    // Main infinite loop
    const uint32_t blinkDelay = 200000;
    while (true) {
        // Toggles LED 1 on
        GPIO_writePin(DEVICE_GPIO_PIN_LED1, 0);
        // Toggles LED 2 off
        GPIO_writePin(DEVICE_GPIO_PIN_LED2, 1);
        // Delay in us
        DEVICE_DELAY_US(blinkDelay);
        // Toggles LED 1 off
        GPIO_writePin(DEVICE_GPIO_PIN_LED1, 1);
        // Toggles LED 2 on
        GPIO_writePin(DEVICE_GPIO_PIN_LED2, 0);
        DEVICE_DELAY_US(blinkDelay);
    }
}
