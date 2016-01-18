/*++

Copyright (c) 2010 Evan Green

Module Name:

    fontdata.h

Abstract:

    This header contains definitions for the built in font data.

Author:

    Evan Green 20-Nov-2010

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the character offset where the font data array starts. This offset
// must be subtracted from every character before indexing into the font arrays.
//

#define FONT_DATA_CHARACTER_OFFSET 32

//
// Define 3x5 indices into the packed array.
//

#define FONT_3X5_NUMERIC_OFFSET 0
#define FONT_3X5_COLON_OFFSET 10
#define FONT_3X5_EQUALS_OFFSET 11
#define FONT_3X5_SPACE_OFFSET 12
#define FONT_3X5_ALPHA_OFFSET 13

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern UCHAR KeFontData3x5[][2];
extern UCHAR KeFontData5x7[][5];

//
// -------------------------------------------------------- Function Prototypes
//
