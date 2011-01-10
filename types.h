/*++

Copyright (c) 2011 Evan Green

Module Name:

    types.h

Abstract:

    This header contains definitions for basic types.

Author:

    Evan Green 8-Jan-2011

--*/

//
// ------------------------------------------------------------------- Includes
//

#ifdef _AVR_

//
// Define types that point to program memory.
//

#define PROGMEM __attribute__((__progmem__))
typedef const char PROGMEM *PPGM;

#else

#define PROGMEM

//
// Define the type to program memory as just a regular pointer on x86.
//

typedef const void *PPGM;

#endif

//
// --------------------------------------------------------------------- Macros
//

//
// These macros are used to read from program space. Only used internally.
//

#define __LPM_enhanced__(addr)  \
(__extension__({                \
    USHORT __addr16 = (USHORT)(addr); \
    UCHAR __result;             \
    __asm__                     \
    (                           \
        "lpm %0, Z" "\n\t"      \
        : "=r" (__result)       \
        : "z" (__addr16)        \
    );                          \
    __result;                   \
}))

#define __LPM_word_enhanced__(addr)         \
(__extension__({                            \
    USHORT __addr16 = (USHORT)(addr);       \
    USHORT __result;                        \
    __asm__                                 \
    (                                       \
        "lpm %A0, Z+"   "\n\t"              \
        "lpm %B0, Z"    "\n\t"              \
        : "=r" (__result), "=z" (__addr16)  \
        : "1" (__addr16)                    \
    );                                      \
    __result;                               \
}))

#define __LPM_dword_enhanced__(addr)        \
(__extension__({                            \
    USHORT __addr16 = (USHORT)(addr);       \
    ULONG __result;                         \
    __asm__                                 \
    (                                       \
        "lpm %A0, Z+"   "\n\t"              \
        "lpm %B0, Z+"   "\n\t"              \
        "lpm %C0, Z+"   "\n\t"              \
        "lpm %D0, Z"    "\n\t"              \
        : "=r" (__result), "=z" (__addr16)  \
        : "1" (__addr16)                    \
    );                                      \
    __result;                               \
}))

//
// These macros can be used externally for reading program space.
//

#define RtlReadProgramSpace8(_Address) __LPM_enhanced__(_Address)
#define RtlReadProgramSpace16(_Address) __LPM_word_enhanced__(_Address)
#define RtlReadProgramSpace32(_Address) __LPM_dword_enhanced__(_Address)

//
// ---------------------------------------------------------------- Definitions
//

#define BITS_PER_BYTE    (8)
#define MAX_CHAR         (127)
#define MIN_CHAR         (-128)
#define MAX_UCHAR        (0xFF)
#define MAX_USHORT       (0xFFFF)
#define MAX_SHORT        (32767)
#define MIN_SHORT        (-32768)
#define MAX_LONG         (2147483647)
#define MIN_LONG         (-214748648)
#define MAX_ULONG        (0xFFFFFFFF)
#define MAX_LONGLONG     (9223372036854775807LL)
#define MIN_LONGLONG     (01000000000000000000000LL)
#define MAX_ULONGLONG    (0xFFFFFFFFFFFFFFFFULL)

#define NOTHING
#define ANYSIZE_ARRAY 1

#define _1MB (1024 * 1024)

#define TRUE 1
#define FALSE 0

//
// ------------------------------------------------------ Data Type Definitions
//

typedef unsigned char BYTE, *PBYTE;
typedef unsigned short WORD, *PWORD;
typedef unsigned long DWORD, *PDWORD;

typedef char CHAR, *PCHAR;
typedef unsigned char UCHAR, *PUCHAR;
typedef short SHORT, *PSHORT;
typedef unsigned short USHORT, *PUSHORT;
typedef int INT, *PINT;
typedef unsigned int UINT, *PUINT;
typedef long LONG, *PLONG;
typedef unsigned long ULONG, *PULONG;
typedef long long LONGLONG, *PLONGLONG;
typedef unsigned long long ULONGLONG, *PULONGLONG;

typedef long INTN;
typedef unsigned long UINTN;
typedef unsigned long long PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

typedef char *PSTR;
typedef unsigned short *PWSTR;

#if !defined(VOID)
typedef void VOID, *PVOID;
#endif

#if !defined(NULL)
#define NULL (PVOID)0
#endif

//
// --------------------------------------------------------------------- Macros
//

//
// The ALIGN_RANGE_UP macro aligns the given Value to the granularity of Size,
// truncating any remainder. This macro is only valid for Sizes that are powers
// of two.
//

#define ALIGN_RANGE_DOWN(_Value, _Size) \
    ((_Value) & ~((_Size) - 1))

//
// The ALIGN_RANGE_UP macro aligns the given Value to the granularity of Size,
// rounding up to a Size boundary if there is any remainder. This macro is only
// valid for Sizes that are a power of two.
//

#define ALIGN_RANGE_UP(_Value, _Size) \
    ALIGN_RANGE_DOWN((_Value) + (_Size) - 1, (_Size))

//
// The POWER_OF_2 macro returns a non-zero value if the given value is a power
// of 2. 0 will qualify as a power of 2 with this macro.
//

#define POWER_OF_2(_Value) (((_Value) & ((_Value) - 1)) == 0)

