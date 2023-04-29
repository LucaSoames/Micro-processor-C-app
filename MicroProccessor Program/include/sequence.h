// n11047836, CAB202_2022se2, X36Y06

#include <stdint.h>
#include <avr/pgmspace.h>

// NOTE: As this header files declares variables, it should only be included
//       ONCE in your programme. If you need to make reference to the variables
//       declared below in multiple source files, use of the "extern" keyword is
//       recommended for this purpose.

char const TEST_BASE64[] PROGMEM = "NhtR";
uint8_t TEST_SCRAMBLED[3] = { 0x36, 0x1B, 0x51 };
char const SEQUENCE[] PROGMEM = "\
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";