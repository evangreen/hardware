/*++

Copyright (c) 2017 Evan Green. All Rights Reserved.

Module Name:

    lib.c

Abstract:

    This module implements some standard library functions.

Author:

    Evan Green 22-Jan-2017

Environment:

    STM32

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "stm32f1xx_hal.h"
#include "lib.h"
#include <stdarg.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void
LibpPrintInteger (
    PPRINT_CONTEXT Context,
    int32_t Integer
    );

void
LibpPrintHexInteger (
    PPRINT_CONTEXT Context,
    uint32_t Value
    );

void
LibpDbgPrintCharacter (
    PPRINT_CONTEXT Context,
    char Character
    );

void
LibpStringPrintCharacter (
    PPRINT_CONTEXT Context,
    char Character
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

void
DbgInititialize (
    void
    )

/*++

Routine Description:

    This routine initializes debugging via SWO.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == 0) ||
        ((DBGMCU->CR & DBGMCU_CR_TRACE_IOEN) == 0)) {

        return;
    }

    //
    // STM32 specific configuration to enable the TRACESWO pin.
    //

    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

    //
    // Disable JTAG to release TRACESWO.
    //

    AFIO->MAPR |= (2 << 24);

    //
    // Enable I/O trace pins.
    //

    DBGMCU->CR |= DBGMCU_CR_TRACE_IOEN;

    //
    // Enable access to registers.
    //

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    //
    // Trace clock = HCLK / (APCR + 1) = 8MHz.
    //

    TPI->ACPR = 35;

    //
    // Pin protocol is NRZ (UART).
    //

    TPI->SPPR = 2;
    TPI->FFCR = 0x102;

    //
    // Enable access to the trace macroblock.
    //

    ITM->LAR = 0xC5ACCE55;
    ITM->TCR = (1 << ITM_TCR_TraceBusID_Pos) |
               ITM_TCR_SYNCENA_Msk | ITM_TCR_ITMENA_Msk;

    ITM->TER = 0xFFFFFFFF;
    ITM->PORT[0].u8 = 'q';
    return;
}

void
DbgPrint (
    const char *Format,
    ...
    )

/*++

Routine Description:

    This routine prints to the debug console. It can only accept %d, %x, %c,
    and %s specifiers.

Arguments:

    Format - Supplies the format string to print.

    ... -- Supplies the remaining arguments.

Return Value:

    None.

--*/

{

    va_list ArgumentList;
    PRINT_CONTEXT Context;

    Context.PrintCharacter = LibpDbgPrintCharacter;
    va_start(ArgumentList, Format);
    LibPrintv(&Context, Format, ArgumentList);
    va_end(ArgumentList);
    return;
}

int32_t
LibStringPrint (
    char *Buffer,
    int32_t Size,
    const char *Format,
    ...
    )

/*++

Routine Description:

    This routine prints to a string. It can only accept %d, %x, %c, and %s
    specifiers.

Arguments:

    Buffer - Supplies a pointer where the written string will be returned.

    Size - Supplies the size of the buffer.

    Format - Supplies the format string to print.

    ... -- Supplies the remaining arguments.

Return Value:

    Returns the number of bytes written, not including the null terminator.
    This may be larger than the string size, indicating that would have been
    needed to print all characters. The ending buffer is always null terminated.

--*/

{

    va_list ArgumentList;
    PRINT_CONTEXT Context;

    Context.PrintCharacter = LibpStringPrintCharacter;
    Context.Context = Buffer;
    Context.Size = Size;
    Context.Written = 0;
    va_start(ArgumentList, Format);
    LibPrintv(&Context, Format, ArgumentList);
    va_end(ArgumentList);
    if (Size != 0) {
        if (Context.Written < Size) {
            Buffer[Context.Written] = '\0';

        } else {
            Buffer[Size - 1] = '\0';
        }
    }

    return Context.Written;
}

void
LibPrintv (
    PPRINT_CONTEXT Context,
    const char *Format,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine performs limited print capabilities. It can only accept %d,
    %x, %c, and %s specifiers, with no modifications.

Arguments:

    Context - Supplies a pointer to the print context.

    Format - Supplies the format string to print.

    ... -- Supplies the remaining arguments.

Return Value:

    None.

--*/

{

    uint32_t HexInteger;
    int32_t Integer;
    PPRINT_CHARACTER Out;
    const char *String;

    Out = Context->PrintCharacter;
    while (*Format != '\0') {
        if (*Format == '%') {
            Format += 1;
            switch (*Format) {
            case 'd':
                Integer = va_arg(ArgumentList, int32_t);
                LibpPrintInteger(Context, Integer);
                break;

            case 'x':
                HexInteger = va_arg(ArgumentList, uint32_t);
                LibpPrintHexInteger(Context, HexInteger);
                break;

            case 'c':
                Integer = va_arg(ArgumentList, int);
                Out(Context, (char)Integer);
                break;

            case 's':
                String = va_arg(ArgumentList, const char *);
                if (String == NULL) {
                    String = "(null)";
                }

                while (*String != '\0') {
                    Out(Context, *String);
                    String += 1;
                }

                break;

            case '%':
                Out(Context, '%');
                break;

            default:
                Out(Context, '%');
                Out(Context, *Format);
                break;
            }

        } else {
            Out(Context, *Format);
        }

        Format += 1;
    }

    return;
}

void
LibSetMemory (
    void *Buffer,
    int Value,
    uint32_t Size
    )

/*++

Routine Description:

    This routine acts as a memset function, setting the given memory buffer to
    the given byte value.

Arguments:

    Buffer - Supplies a pointer to the buffer to set.

    Value - Supplies the value to set.

    Size - Supplies the number of bytes to set.

Return Value:

    None.

--*/

{

    uint8_t *Bytes;
    uint32_t Index;

    Bytes = Buffer;
    for (Index = 0; Index < Size; Index += 1) {
        Bytes[Index] = Value;
    }

    return;
}

uint32_t
LibStringLength (
    const char *String
    )

/*++

Routine Description:

    This routine returns the length of the given string, not including the
    null terminator.

Arguments:

    String - Supplies a pointer to the string.

Return Value:

    Returns the length of the string in bytes.

--*/

{

    uint32_t Size;

    Size = 0;
    while (*String != '\0') {
        Size += 1;
        String += 1;
    }

    return Size;
}

int
LibStringCompare (
    const char *String1,
    const char *String2,
    int32_t CharacterCount
    )

/*++

Routine Description:

    This routine compares strings for equality.

Arguments:

    String1 - Supplies the first string to compare.

    String2 - Supplies the second string to compare.

    CharacterCount - Supplies the maximum number of characters to compare.
        Characters after a null terminator in either string are not compared.

Return Value:

    0 if the strings are equal all the way through their null terminators or
    character count.

    Non-zero if the strings are different. The sign of the return value will be
    determined by the sign of the difference between the values of the first
    pair of bytes (both interpreted as type unsigned char) that differ in the
    strings being compared.

--*/

{

    while (CharacterCount != 0) {
        if (*String1 != *String2) {
            return (unsigned char)*String1 - (unsigned char)*String2;
        }

        if (*String1 == '\0') {
            break;
        }

        String1 += 1;
        String2 += 1;
        CharacterCount -= 1;
    }

    return 0;
}

int32_t
LibScanInt (
    char **String
    )

/*++

Routine Description:

    This routine reads an integer from a string.

Arguments:

    String - Supplies a pointer that on input points to the string to read.
        On output, this will be advanced beyond the end of the string.

Return Value:

    Returns the integer value.

--*/

{

    char *Current;
    int Negative;
    int32_t Value;

    Negative = 0;
    Current = *String;
    if (*Current == '-') {
        Negative = 1;
        Current += 1;
    }

    Value = 0;
    while ((*Current >= '0') && (*Current <= '9')) {
        Value = (Value * 10) + (*Current - '0');
        Current += 1;
    }

    if (Negative != 0) {
        Value = -Value;
    }

    *String = Current;
    return Value;
}

uint32_t
LibScanHexInt (
    char **String
    )

/*++

Routine Description:

    This routine reads an hexadecimal integer from a string.

Arguments:

    String - Supplies a pointer that on input points to the string to read.
        On output, this will be advanced beyond the end of the string.

Return Value:

    Returns the integer value.

--*/

{

    char *Current;
    uint32_t Digit;
    uint32_t Value;

    Current = *String;
    Value = 0;
    while (1) {
        if ((*Current >= 'a') && (*Current <= 'f')) {
            Digit = *Current - 'a' + 0xA;

        } else if ((*Current >= 'A') && (*Current <= 'F')) {
            Digit = *Current - 'A' + 0xA;

        } else if ((*Current >= '0') && (*Current <= '9')) {
            Digit = *Current - '0';

        } else {
            break;
        }

        Value = (Value << 4) | Digit;
        Current += 1;
    }

    *String = Current;
    return Value;
}

//
// --------------------------------------------------------- Internal Functions
//

void
LibpPrintInteger (
    PPRINT_CONTEXT Context,
    int32_t Integer
    )

/*++

Routine Description:

    This routine prints a signed decimal integer.

Arguments:

    Context - Supplies a pointer to the print context.

    Integer - Supplies the value to print.

Return Value:

    None.

--*/

{

    char Buffer[13];
    int Index;
    int Negative;
    PPRINT_CHARACTER Out;

    Out = Context->PrintCharacter;
    if (Integer == 0) {
        Out(Context, '0');
        return;
    }

    //
    // This doesn't handle the most negative value, oh well.
    //

    Negative = 0;
    if (Integer < 0) {
        Negative = 1;
        Integer = -Integer;
    }

    Index = 0;
    while (Integer != 0) {
        Buffer[Index] = (Integer % 10) + '0';
        Integer /= 10;
        Index += 1;
    }

    if (Negative != 0) {
        Out(Context, '-');
    }

    while (Index > 0) {
        Index -= 1;
        Out(Context, Buffer[Index]);
    }

    return;
}

void
LibpPrintHexInteger (
    PPRINT_CONTEXT Context,
    uint32_t Value
    )

/*++

Routine Description:

    This routine prints a hex integer.

Arguments:

    Context - Supplies a pointer to the print context.

    Value - Supplies the value to print.

Return Value:

    None.

--*/

{

    uint32_t Digit;
    int Nybble;
    PPRINT_CHARACTER Out;

    Out = Context->PrintCharacter;
    if (Value == 0) {
        Out(Context, '0');
        return;
    }

    for (Nybble = 7; Nybble >= 0; Nybble -= 1) {
        Digit = (Value >> (Nybble * 4)) & 0xF;
        if (Digit >= 0xA) {
            Out(Context, Digit - 0xA + 'A');

        } else {
            Out(Context, Digit + '0');
        }
    }

    return;
}

void
LibpDbgPrintCharacter (
    PPRINT_CONTEXT Context,
    char Character
    )

/*++

Routine Description:

    This routine outputs a character via the print context to the debug console.

Arguments:

    Context - Supplies a pointer to the context.

    Character - Supplies the character to write.

Return Value:

    None.

--*/

{

    ITM_SendChar(Character);
    return;
}

void
LibpStringPrintCharacter (
    PPRINT_CONTEXT Context,
    char Character
    )

/*++

Routine Description:

    This routine outputs a character via the print context to a destination
    buffer.

Arguments:

    Context - Supplies a pointer to the context.

    Character - Supplies the character to write.

Return Value:

    None.

--*/

{

    char *Buffer;

    Buffer = Context->Context;
    if (Context->Written < Context->Size) {
        Buffer[Context->Written] = Character;
    }

    Context->Written += 1;
    return;
}

