#include <avr/io.h>
#include <avr/interrupt.h>


void adc_init() // set up ADC for initialisation
{
    ADC0.CTRLA = ADC_ENABLE_bm; 
    ADC0.CTRLB = ADC_PRESC_DIV2_gc; 
    ADC0.CTRLC = (4 << ADC_TIMEBASE_gp) | ADC_REFSEL_VDD_gc; 
    ADC0.CTRLE = 64; 
    ADC0.CTRLF = ADC_FREERUN_bm; 
    ADC0.MUXPOS = ADC_MUXPOS_AIN2_gc;
    ADC0.COMMAND = ADC_MODE_SINGLE_8BIT_gc | ADC_START_IMMEDIATE_gc;
}