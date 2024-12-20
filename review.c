#define BUFFER_LENGTH 83 // From the formula: (TIMER0 frequency / DACA frequency)

uint16_t bufferADCA_SOC0[BUFFER_LENGTH];
uint16_t bufferADCA_SOC1[BUFFER_LENGTH];

interrupt void ADCA1_ISR() {
    // Indexes for buffer storage
    static uint16_t bufferIndex_SOC0;
    bufferIndex_SOC0 ++;
    if (bufferIndex_SOC0 >= BUFFER_LENGTH) {
        bufferIndex_SOC0 = 0;
    }
    static uint16_t bufferIndex_SOC1;
    bufferIndex_SOC1 ++;
    if (bufferIndex_SOC1 >= BUFFER_LENGTH) {
        bufferIndex_SOC1 = 0;
    }
    // Storing the new result in the next buffer position
    const uint16_t resultADCA_SOC0 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
    bufferADCA_SOC0[bufferIndex_SOC0] = resultADCA_SOC0;
    const uint16_t resultADCA_SOC1 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1);
    bufferADCA_SOC1[bufferIndex_SOC1] = resultADCA_SOC1;
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}

/*------------------------------ADC FUNCTIONS------------------------------*/

// ADCA configuration on channels ADCIN14 and ADCIN15 (pin A14 and A15)
void configureADCA() {
    // ADCA reset and disabled for security
    SysCtl_resetPeripheral(SYSCTL_PERIPH_RES_ADCA);
    ADC_disableConverter(ADCA_BASE);
    // Resolution, conversion mode and clock settings
    ADC_setMode(ADCA_BASE, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
    // SOC (start of conversion) setup on channel ADCIN14
    ADC_setSOCPriority(ADCA_BASE, ADC_PRI_ALL_ROUND_ROBIN); // No priority over other SOCs
    // SOC0
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_CPU1_TINT0, ADC_CH_ADCIN14, 20U); // Triggered by timer 0 interrupt
    ADC_setInterruptSOCTrigger(ADCA_BASE, ADC_SOC_NUMBER0, ADC_INT_SOC_TRIGGER_NONE); // SOC0 not triggered by ADC interrupts
    // SOC1
    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER1, ADC_TRIGGER_CPU1_TINT0, ADC_CH_ADCIN15, 20U);
    ADC_setInterruptSOCTrigger(ADCA_BASE, ADC_SOC_NUMBER1, ADC_INT_SOC_TRIGGER_NONE);
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
