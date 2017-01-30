/*++

Copyright (c) 2017 Evan Green. All Rights Reserved.

Module Name:

    lib.h

Abstract:

    This header contains some standard library like definitions.

Author:

    Evan Green 22-Jan-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _PRINT_CONTEXT PRINT_CONTEXT, *PPRINT_CONTEXT;

typedef
void
(*PPRINT_CHARACTER) (
    PPRINT_CONTEXT Context,
    char Character
    );

/*++

Routine Description:

    This routine outputs a character via the print context.

Arguments:

    Context - Supplies a pointer to the context.

    Character - Supplies the character to write.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores context for a print function.

Members:

    PrintCharacter - Stores a pointer to the function called to output each
        character.

    Context - Stores an unused context pointer that can be used by the print
        character function.

    Size - Stores an unused integer size that can be used by the print character
        function.

    Written - Stores an integer that must be updated by the print character
        function intended to return the number of bytes written.

--*/

struct _PRINT_CONTEXT {
    PPRINT_CHARACTER PrintCharacter;
    void *Context;
    uint32_t Size;
    uint32_t Written;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

void
DbgInititialize (
    void
    );

/*++

Routine Description:

    This routine initializes debugging via SWO.

Arguments:

    None.

Return Value:

    None.

--*/

void
DbgPrint (
    const char *Format,
    ...
    );

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

int32_t
LibStringPrint (
    char *Buffer,
    int32_t Size,
    const char *Format,
    ...
    );

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

void
LibPrintv (
    PPRINT_CONTEXT Context,
    const char *Format,
    va_list ArgumentList
    );

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

void
LibSetMemory (
    void *Buffer,
    int Value,
    uint32_t Size
    );

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

uint32_t
LibStringLength (
    const char *String
    );

/*++

Routine Description:

    This routine returns the length of the given string, not including the
    null terminator.

Arguments:

    String - Supplies a pointer to the string.

Return Value:

    Returns the length of the string in bytes.

--*/

int
LibStringCompare (
    const char *String1,
    const char *String2,
    int32_t CharacterCount
    );

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

int32_t
LibScanInt (
    char **String
    );

/*++

Routine Description:

    This routine reads an integer from a string.

Arguments:

    String - Supplies a pointer that on input points to the string to read.
        On output, this will be advanced beyond the end of the string.

Return Value:

    Returns the integer value.

--*/
