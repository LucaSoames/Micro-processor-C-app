#include "uart.h"
#include "timer.h"
#include "sequence.h"
#include "pmw.h"
#include "spi.h"
#include "buttons.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>



//Table of characters used to check if an inputed characters ascii value is = to the indexed characters ascii value, which is indexed based on the characters base64 value. 
static char Decoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};

//an array of uint32_t notes used to index the per value of each notes when octave = 1, derived from the frequency equation in the cra and the clock speed.
volatile uint32_t notes[12] = {60606, 57204, 53994, 50963, 48103, 45403, 42854, 40449, 38179, 36036, 34013, 32104};

//array of byets used to write certain characters to the seven segment display, indexed in ascending order from 0-F.
volatile uint8_t displayValues[] = { 0x08, 0x6B, 0x44, 0x41, 0x23, 0x11,
                                0x10, 0x4B, 0x00, 0x1, 0x2, 0x30, 0x1C, 0x60, 0x14, 0x16, 0x77, 0x7F };

//variables declared for use in the playing of a sequence.
uint16_t octaveNote;
uint32_t brightnesscalc;
uint8_t brightness;
uint8_t octave;
uint8_t note;
uint16_t perValue;

// variables used to decode and descramble bytes from the sequence array in PROGMEM 
uint32_t STATE_LFSR = 0x11047836;
uint32_t LSFR_SHIFT;
uint8_t decodedArray[3];
uint8_t descrambledBytes[3];
uint8_t memcpyArray[4];

//volatile variables used 
volatile uint16_t index = 0;
volatile uint16_t step = 0;
volatile uint16_t counter = 0;
volatile uint8_t display = 0;
volatile uint16_t duration = 1;
volatile uint8_t high = 0x08 |0b10000000;
volatile uint8_t low = 0x08;
volatile uint16_t position;

// initialise pushbutton variables
uint8_t pb = 0xFF;
uint8_t pb_prev;
uint8_t pb_changed;
uint8_t pb_falling;



//state machine typedefinitions of enumerations are declared
typedef enum {
    SELECT,
    SEQUENCING,
    TEST,
} state;

typedef enum {
    START,
    S4
} buttonState;

typedef enum {
    PLAY,
    PAUSE,
} sequencingState;


uint8_t asciitohex(char letter){ // using the ascii value table 
    for (uint8_t i = 0 ; i < 64; i++){ // counter controlled loop to check each element in decoding table
        if (letter == Decoding_table[i]) // corresponding base64 value to ascii table character is returned.
        {
            return i;
        }
    }
}

void DecoderDescramble(uint16_t index){
    LSFR_SHIFT= (index*3 << 3) + (step *3);  
    position = index << 5;
    memcpy_P(memcpyArray, SEQUENCE+(position+(step<<2)), 4);
    //printf("%X", memcpyArray[0]);
    for (uint8_t i = 0; i < 4; i++){
        memcpyArray[i] = asciitohex(memcpyArray[i]);        
    }
    decodedArray[0] = (memcpyArray[0] << 2) + ((memcpyArray[1] & 0x30) >> 4); 
    decodedArray[1] = ((memcpyArray[1] & 0xf) << 4) + ((memcpyArray[2] & 0x3c) >> 2); 
    decodedArray[2] = ((memcpyArray[2] & 0x3) << 6) + memcpyArray[3]; 

    STATE_LFSR = 0x11047836;
    if (LSFR_SHIFT > 0){
        for (uint32_t i = 0 ; i < LSFR_SHIFT; i ++){
            uint8_t bit =  (STATE_LFSR  & 1);
            STATE_LFSR = STATE_LFSR >> 1;
            if (bit == 1){
                STATE_LFSR = STATE_LFSR ^ 0xB4BCD35C;
            }
        }
    }

    for (uint8_t i = 0; i < 3; i++){
        descrambledBytes[i] = (STATE_LFSR & 0xff) ^ decodedArray[i];
        uint8_t bit =  (STATE_LFSR  & 1);
        STATE_LFSR = STATE_LFSR >> 1;
        if (bit == 1){
            STATE_LFSR = STATE_LFSR ^ 0xB4BCD35C;
        }
    }
}

void PlaySequence(){

    octaveNote = descrambledBytes[2];
    duration = descrambledBytes[0];
    brightness = descrambledBytes[1];
    octave = (octaveNote & 0xF0) >> 4; 
    note = octaveNote & 0x0F; 

    if (octave == 0 ){
        PORTB.DIRCLR = PIN0_bm;
    }
    else{
        PORTB.DIRSET = PIN0_bm;
        perValue = notes[note] >> (octave - 1);
    }
    
    duration = duration*13.1;
    uint32_t calc = ((uint32_t)brightness)*(uint32_t)perValue;
    brightnesscalc = calc >> 8;                        

    TCA0.SINGLE.PERBUF = perValue; //tone frequency
    TCA0.SINGLE.CMP1BUF = brightnesscalc;
    TCA0.SINGLE.CMP0BUF = perValue >> 1;
}

void DisplayIndexConversion(){
    high = index & 0xF0; //high byte, so digit displayed on the left hand side.
    low = index & 0x0F; // low byte, displayed on right.
    high = high >> 4; // bit shift right 4 times to be indexed in the array of displayed values
    
    //high and low nibbles are indexed in the array of displayed values for the correct bit mask to display the corresponding hex digit value.
    high = displayValues[high] | 0b10000000; //bit 7 determines which side of the display is displayed using spi write.
    low = displayValues[low];
}

void SamplePushButtonsState(){
    static uint8_t count0 = 0;
    static uint8_t count1 = 0;

    uint8_t pb_previous = PORTA.IN;
    uint8_t pb_changed = pb_previous ^ pb_state;

    //increment if PB state changed
    count1 = (count1 ^ count0) & pb_changed;
    count0 = ~count0 & pb_changed;

    pb_state ^= (count1 & count0) | (pb_changed & pb_state);

    pb_state = PORTA.IN; //port A byte controls push button state. 
}

void DisplayBytes(void){ // function used to displayed bytes in both sides of the seven segment display by alternating display sides at a high frequency.
    if (display == 0) {  // display flag checked, 
        spi_write(high); // high byte global variable is written to the display.
        display = 1; //display flag set to 1
    }
    else { //display flag != 0 so low byte is written to spi, then display returns to 0, causing and alternating loop.
        spi_write(low);
        display = 0;
    }
}

void ResetBuzzerDisplay(){
    TCA0.SINGLE.PERBUF = 100; //random per value
    TCA0.SINGLE.CMP1BUF = 100; // brightness % = (per value/ cmp1 value) *100 thus brightness = 100%
    TCA0.SINGLE.CMP0BUF = 0; // buzzer off, - duty cycle 0
}

void pbStateBitBash(){ // byte manipulation for easier parsing of button state employing multiple button states.
    pb_prev = pb;
    pb = pb_state;
    pb_changed = pb_prev ^ pb;
    pb_falling = pb_changed & ~pb;
}
//state machine states, used in main and in ISR's
volatile state initial_state = SELECT;
volatile buttonState current = START;
volatile sequencingState sequenceState = PLAY;

int main(){
    //all components, peripherals and clocks initialised.
    timer_init(); // initialise timer
    uart_init(); // initialse uart serial communication
    spi_init(); // initialise spi for display communications.
    pwm_init(); // initialise pwm for buzzer and display brightness control
    adc_init(); // initialse potentiometer
    buttons_init(); // initialise buttons

    while(1){ 
        switch (initial_state)
        {
        case SELECT:
            PORTB.DIRCLR = PIN0_bm; // turn off buzzer
            PORTB.OUTSET = PIN5_bm;
            PORTB.DIRSET = PIN5_bm;            
            
            while (initial_state == SELECT){
                pbStateBitBash();
                switch (current)
                {
                case START:
                //printf("select\n");
                TCA0.SINGLE.PERBUF = 100;
                TCA0.SINGLE.CMP1BUF = 100;
                    if(pb_falling & PIN6_bm){ //s3
                        if (index != 0xFF){
                            index++;
                        }
                        current = START;
                        break;
                    }
                    else if(pb_falling & PIN5_bm){ // s2
                        if (index !=0){
                            index--;
                        }
                        current = START;
                        break;
                    }
                    else if(pb_falling & PIN7_bm){ //s4
                        current = S4;
                        break;
                    }
                    else if((PORTA.IN & PIN4_bm) == 0) //checking to see if button s1 is depressed and if it is, the index variable will be equal to the result from the potentiometer
                    {
                        index = ADC0.RESULT;
                    }
                    else current = START;
                    break;
                case S4:
                    duration = 1;
                    initial_state = SEQUENCING;
                    counter = 0;
                    current = START;
                    break;
                }
            }
        break;
        
        case SEQUENCING:                

                switch (sequenceState)
                {
                case PLAY:
                    pbStateBitBash();
                    if (duration != 0)
                    {
                        if(pb_falling & PIN7_bm){ //check if s4 is pressed, if so transition to paused state
                            sequenceState = PAUSE;
                            break;
                        }
                        if (counter >= duration){ // check if clock timer counter has reached duration 
                            step++;
                            counter = 0;
                        }
                        DecoderDescramble(index); // decode and descramble bytes from sequencer based on index and current step
                        PlaySequence();                          
                    }
                    else if(duration == 0){
                        ResetBuzzerDisplay();
                        step = 0;
                        initial_state = SELECT;
                        current = START;
                    }
                    break;
                
                case PAUSE:
                    pbStateBitBash();
                    if (duration != 0)
                    {
                        DecoderDescramble(index); // decode and descramble bytes from sequencer based on index and current step   
                        PlaySequence(); // function called to play sequence                       
                    }
                    else if(duration == 0){
                        ResetBuzzerDisplay(); // display and buzzer reset
                        step = 0;
                        sequenceState = PLAY;
                        initial_state = SELECT;
                        current = START;
                    }
                    if(pb_falling & PIN6_bm){ //button 3 pressed
                        step++; // step is advanced.
                    }
                    else if(pb_falling & PIN5_bm){ // button 2 pressed
                        ResetBuzzerDisplay(); // display and buzzer reset
                        step = 0; //reset step for next sequence to initialse properly.
                        sequenceState = PLAY; //sequencer state reset to play 
                        initial_state = SELECT; // mode changes to select
                    }
                    else if(pb_falling & PIN7_bm){ //button 4 is pressed.
                        step++; //return to sequence from the next step 
                        sequenceState = PLAY; //sequencing state returns to play mode in order to step though sequence
                        counter = 0; // counter set to 0 to ensure full duration of returning step
                    }
                    break;                
                }                        
            break;
        case TEST:
            /* code */
            break;
        }
    }
    
    
} //END Main

void pwm_init() //buzzer and display initialisation, pulse width modulation used on timer A
{
    cli();
    PORTB.DIRSET = PIN0_bm;
    PORTB.DIRSET = PIN1_bm;

    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV2_gc;
    
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc | TCA_SINGLE_CMP0EN_bm | TCA_SINGLE_CMP1EN_bm;

    TCA0.SINGLE.PER = 100;
    TCA0.SINGLE.CMP0 = 0;
    TCA0.SINGLE.CMP1 = 100;
    
    TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm; 
    sei();
}

void timer_init() { //timer function used to initialise the timer clock and interrupts for TCB0
    cli();
    TCB0.CTRLA = TCB_CLKSEL_DIV1_gc;         // Enable
    TCB0.CTRLB = TCB_CNTMODE_INT_gc;    // Configure TCB0 in periodic interrupt mode
    TCB0.CCMP = 3333;                   // Set interval for 1ms (3333 clocks @ 3.3 MHz)
    TCB0.INTCTRL = TCB_CAPT_bm;  
    TCB0.CTRLA |= TCB_ENABLE_bm;      // CAPT interrupt enable

    sei(); 
}

ISR(TCB0_INT_vect) { //ISR executing interrupts based on TCB0's compare register, (1ms intervals) 
    counter++; //clock counter used in sequencing implementation.

    //state machine state determines what is displayed on the seven segment display.
    if (initial_state == SELECT){ 
        DisplayIndexConversion();
        DisplayBytes();
    }
    if (initial_state == SEQUENCING){
        spi_write(0b10000000);
    }
    // get push button state.
    SamplePushButtonsState();

    TCB0.INTFLAGS = TCB_CAPT_bm;        // Acknowledge interrupt
}