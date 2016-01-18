/*++

Copyright (c) 2011 Evan Green

Module Name:

    atmega8.h

Abstract:

    This header contains definitions for the ATMega48/88/168/328 family of AVR
    Microcontrollers.

Author:

    Evan Green 8-Jan-2011

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write from I/O ports.
//

#define HlReadIo(_Port) (*(volatile unsigned char *)(_Port))
#define HlWriteIo(_Port, _Value) (*(volatile unsigned char *)(_Port)) = (_Value)

//
// These macros unconditionally enable and disable interrupts.
//

#define HlDisableInterrupts() __asm__ __volatile__ ("cli" ::)
#define HlEnableInterrupts() __asm__ __volatile__ ("sei" ::)

//
// This macro is used to create an Interrrupt Service Routine function.
// The Vector parameter must be one of the vector names valid for the
// particular MCU type. Valid attributes are ISR_BLOCK, ISR_NOBLOCK, ISR_NAKED
// and ISR_ALIASOF
//

#define ISR(_Vector, ...) \
    VOID _Vector(VOID) __attribute__ ((signal,__INTR_ATTRS)) __VA_ARGS__; \
    VOID _Vector(VOID)

//
// This macro defines an interrupt vector number. Used internally in this
// header only.
//

#define _VECTOR(_VectorNumber) __vector_ ## _VectorNumber

//
// ---------------------------------------------------------------- Definitions
//

//
// GCC ISR definitions.
//

#define __INTR_ATTRS used, externally_visible

//
// Define the GCC attribute used to specify that the ISR runs with interrupts
// enabled as soon as possible to allow nested interrupts.
//

#define ISR_NOBLOCK __attribute__((interrupt))

//
// Define the GCC attribute used to specify that the ISR should run with
// interrupts disabled the entire time. This prevents additional nesting.
//

#define ISR_BLOCK

//
// Define the GCC attribute used to specify that this ISR should be created with
// no prologue or epilogue code. The ISR is directly responsible for
// preservation of machine state.
//

#define ISR_NAKED __attribute__((naked))

//
// Define the GCC attribute to specify that this ISR should be linked to
// another ISR.
//

#define ISR_ALIASOF(_Vector) __attribute__((alias(#_Vector)))

//
// Hardware I/O ports.
//

#define PORTC_INPUT 0x26
#define PORTC_DATA_DIRECTION 0x27
#define PORTC 0x28
#define PORTD_INPUT 0x29
#define PORTD_DATA_DIRECTION 0x2A
#define PORTD 0x2B
#define TIMER1_INTERRUPT_STATUS 0x36
#define EEPROM_CONTROL 0x3F
#define EEPROM_DATA 0x40
#define EEPROM_ADDRESS_HIGH 0x42
#define EEPROM_ADDRESS_LOW 0x41
#define TIMER1_INTERRUPT_ENABLE 0x6F
#define TIMER1_CONTROL_B 0x81
#define TIMER1_COUNTER_LOW 0x84
#define TIMER1_COMPARE_A_LOW 0x88
#define TIMER1_COMPARE_A_HIGH 0x89

//
// Timer 1 control bits.
//

#define TIMER1_CONTROL_A_PERIODIC_MODE 0x00
#define TIMER1_CONTROL_B_DIVIDE_BY_1 0x01
#define TIMER1_CONTROL_B_DIVIDE_BY_8 0x02
#define TIMER1_CONTROL_B_DIVIDE_BY_64 0x03
#define TIMER1_CONTROL_B_DIVIDE_BY_256 0x04
#define TIMER1_CONTROL_B_DIVIDE_BY_1024 0x05
#define TIMER1_CONTROL_B_PERIODIC_MODE (0x1 << 3)
#define TIMER1_INTERRUPT_OVERFLOW 0x01
#define TIMER1_INTERRUPT_COMPARE_A 0x02

//
// EEPROM control register bits.
//

#define EEPROM_CONTROL_MASTER_WRITE_ENABLE 0x04
#define EEPROM_CONTROL_WRITE_ENABLE 0x02
#define EEPROM_CONTROL_READ_ENABLE 0x01

//
// Valid ISR Vectors.
//

#define INTERRUPT0_VECTOR          _VECTOR(1)
#define INTERRUPT1_VECTOR          _VECTOR(2)
#define PIN_CHANGE_0_VECTOR        _VECTOR(3)
#define PIN_CHANGE_1_VECTOR        _VECTOR(4)
#define PIN_CHANGE_2_VECTOR        _VECTOR(5)
#define WATCHDOG_VECTOR            _VECTOR(6)
#define TIMER2_COMPARE_A_VECTOR    _VECTOR(7)
#define TIMER2_COMPAGE_B_VECTOR    _VECTOR(8)
#define TIMER2_OVERFLOW_VECTOR     _VECTOR(9)
#define TIMER1_CAPTURE_VECTOR      _VECTOR(10)
#define TIMER1_COMPARE_A_VECTOR    _VECTOR(11)
#define TIMER1_COMPARE_B_VECTOR    _VECTOR(12)
#define TIMER1_OVERFLOW_VECTOR     _VECTOR(13)
#define TIMER0_COMPARE_A_VECTOR    _VECTOR(14)
#define TIMER0_COMPARE_B_VECTOR    _VECTOR(15)
#define TIMER0_OVERFLOW_VECTOR     _VECTOR(16)
#define SPI_VECTOR                 _VECTOR(17)
#define USART_RECEIVE_VECTOR       _VECTOR(18)
#define USART_EMPTY_VECTOR         _VECTOR(19)
#define USART_TRANSMIT_VECTOR      _VECTOR(20)
#define ADC_VECTOR                 _VECTOR(21)
#define EEPROM_READY_VECTOR        _VECTOR(22)
#define ANALOG_COMPARE_VECTOR      _VECTOR(23)
#define TWO_WIRE_VECTOR            _VECTOR(24)
#define STORE_PROGRAM_READY_VECTOR _VECTOR(25)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
