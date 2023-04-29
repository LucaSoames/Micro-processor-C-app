#include "uart.h"
#include "timer.h"
#include <stdint.h>
#include "spi.h"
//initialise SPI0 in unbuffered mode


void spi_init() {
    cli();
    // set up PORT multiplexer
    PORTMUX.SPIROUTEA = PORTMUX_SPI0_ALT1_gc;
    //setup pins for SPI (CLK MOSI)
    PORTC.DIRSET |= (PIN0_bm | PIN2_bm);
    //unbuffered mode, master host
    SPI0.CTRLA = SPI_MASTER_bm;
    //disable client (slave select)
    SPI0.CTRLB = SPI_SSD_bm;
    // interrupt
    SPI0.INTCTRL = SPI_IE_bm;

    PORTA.OUTSET = PIN1_bm;
    PORTA.DIRSET = PIN1_bm;

    SPI0.CTRLA |= SPI_ENABLE_bm;

    sei();
}

// write byte b out
spi_write(uint8_t b) {
    SPI0.DATA = b;
}

//ISR for interrupt SPI

ISR(SPI0_INT_vect) {

    PORTA.OUTCLR = PIN1_bm;
    PORTA.OUTSET = PIN1_bm;

    SPI0.INTFLAGS = SPI_IF_bm;
}