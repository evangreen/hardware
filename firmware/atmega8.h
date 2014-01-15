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
#define HlNoop() __asm__ __volatile__ ("nop" ::)

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
// This macro calculates the baud rate register value given a processor
// frequency in Hertz and a desired baud rate.
//

#define BAUD_RATE_VALUE(_ProcessorFrequency, _BaudRate) \
    ((((_ProcessorFrequency) / ((_BaudRate) * 16UL))) - 1)

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

#define PORTB_INPUT 0x23
#define PORTB_DATA_DIRECTION 0x24
#define PORTB 0x25
#define PORTC_INPUT 0x26
#define PORTC_DATA_DIRECTION 0x27
#define PORTC 0x28
#define PORTD_INPUT 0x29
#define PORTD_DATA_DIRECTION 0x2A
#define PORTD 0x2B
#define TIMER0_INTERRUPT_STATUS 0x35
#define TIMER1_INTERRUPT_STATUS 0x36
#define EEPROM_CONTROL 0x3F
#define EEPROM_DATA 0x40
#define EEPROM_ADDRESS_HIGH 0x42
#define EEPROM_ADDRESS_LOW 0x41
#define SPI_CONTROL 0x4C
#define SPI_STATUS 0x4D
#define SPI_DATA 0x4E
#define TIMER0_CONTROL_A 0x44
#define TIMER0_CONTROL_B 0x45
#define TIMER0_COUNTER 0x46
#define TIMER0_COMPARE_A 0x47
#define TIMER0_COMPARE_B 0x48
#define TIMER0_INTERRUPT_ENABLE 0x6E
#define TIMER1_INTERRUPT_ENABLE 0x6F
#define ADC_CONTROL_A 0x7A
#define ADC_CONTROL_B 0x7B
#define ADC_SELECTOR 0x7C
#define ADC_DIGITAL_INPUT_DISABLE 0x7E
#define ADC_DATA_LOW 0x78
#define ADC_DATA_HIGH 0x79
#define TIMER1_CONTROL_B 0x81
#define TIMER1_COUNTER_LOW 0x84
#define TIMER1_COMPARE_A_LOW 0x88
#define TIMER1_COMPARE_A_HIGH 0x89
#define UART0_CONTROL_A 0xC0
#define UART0_CONTROL_B 0xC1
#define UART0_CONTROL_C 0xC2
#define UART0_BAUD_RATE_LOW 0xC4
#define UART0_BAUD_RATE_HIGH 0xC5
#define UART0_DATA 0xC6

//
// Timer 0 control bits.
//

#define TIMER0_CONTROL_B_DIVIDE_BY_1 0x01
#define TIMER0_CONTROL_B_DIVIDE_BY_8 0x02
#define TIMER0_CONTROL_B_DIVIDE_BY_64 0x03
#define TIMER0_CONTROL_B_DIVIDE_BY_256 0x04
#define TIMER0_CONTROL_B_DIVIDE_BY_1024 0x05

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
// SPI control bits.
//

#define SPI_CONTROL_INTERRUPT_ENABLE 0x80
#define SPI_CONTROL_ENABLE 0x40
#define SPI_CONTROL_MASTER 0x10
#define SPI_CONTROL_DIVIDE_BY_4 0x00
#define SPI_CONTROL_DIVIDE_BY_16 0x01
#define SPI_CONTROL_DIVIDE_BY_64 0x02
#define SPI_CONTROL_DIVIDE_BY_128 0x03

//
// SPI status bits.
//

#define SPI_STATUS_INTERRUPT 0x80

//
// Analog to Digital (ADC) Control A bits.
//

#define ADC_CONTROL_A_GLOBAL_ENABLE 0x80
#define ADC_CONTROL_A_START_CONVERSION 0x40
#define ADC_CONTROL_A_PRESCALE_128 0x7

//
// Analog to Digital (ADC) Control B bits.
//

#define ADC_CONTROL_B_FREE_RUNNING 0x00

//
// Analog to Digital (ADC) Selector bits.
//

#define ADC_SELECTOR_AREF 0x00
#define ADC_SELECTOR_AVCC 0x40
#define ADC_SELECTOR_1V 0xC0

//
// UART Control A bits.
//

#define UART_CONTORL_A_MULTIPROCESSOR_MODE 0x01
#define UART_CONTROL_A_2X_SPEED 0x02
#define UART_CONTROL_A_PARITY_ERROR 0x04
#define UART_CONTROL_A_DATA_OVERRUN 0x08
#define UART_CONTROL_A_FRAME_ERROR 0x10
#define UART_CONTROL_A_DATA_EMPTY 0x20
#define UART_CONTROL_A_TRANSMIT_COMPLETE 0x40
#define UART_CONTROL_A_RECEIVE_COMPLETE 0x80

//
// UART Control B bits.
//

#define UART_CONTROL_B_TRANSMIT_BIT8 0x01
#define UART_CONTROL_B_RECEIVE_BIT8 0x02
#define UART_CONTROL_B_CHARACTER_SIZE2 0x04
#define UART_CONTROL_B_TRANSMIT_ENABLE 0x08
#define UART_CONTROL_B_RECEIVE_ENABLE 0x10
#define UART_CONTROL_B_DATA_EMPTY_INTERRUPT_ENABLE 0x20
#define UART_CONTROL_B_TRANSMIT_COMPLETE_INTERRUPT_ENABLE 0x40
#define UART_CONTROL_B_RECEIVE_COMPLETE_INTERRUPT_ENABLE 0x80

//
// UART Control C bits.
//

#define UART_CONTROL_C_TRANSMIT_RISING_EDGE 0x01
#define UART_CONTROL_C_CHARACTER_SIZE0 0x02
#define UART_CONTROL_C_CHARACTER_SIZE1 0x04
#define UART_CONTROL_C_1_STOP_BIT 0x00
#define UART_CONTROL_C_2_STOP_BITS 0x08
#define UART_CONTROL_C_NO_PARITY 0x00
#define UART_CONTROL_C_EVEN_PARITY (0x2 << 4)
#define UART_CONTROL_C_ODD_PARTIY (0x3 << 4)
#define UART_CONTROL_C_MODE_ASYNCHRONOUS (0x0 << 6)
#define UART_CONTROL_C_MODE_SYNCHRONOUS (0x1 << 6)
#define UART_CONTROL_C_MODE_MASTER_SPI (0x3 << 6)

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
