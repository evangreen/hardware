/*++

Copyright (c) 2010 Evan Green

Module Name:

    sokoban.h

Abstract:

    This header contains definitions related to the Sokoban game.

Author:

    Evan Green 14-Nov-2010

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the playing field width and height.
//

#define SOKOBAN_WIDTH 19
#define SOKOBAN_HEIGHT 16

//
// Define the number of levels in the game.
//

#define SOKOBAN_LEVELS 20

//
// Define the size, in bytes, of one level.
//

#define SOKOBAN_LEVEL_SIZE (SOKOBAN_WIDTH * SOKOBAN_HEIGHT * 2 / 8)

//
// Define the types of cells.
//

#define SOKOBAN_CELL_FREE 0
#define SOKOBAN_CELL_WALL 1
#define SOKOBAN_CELL_BEAN 2
#define SOKOBAN_CELL_GOAL 3

//
// Define how the origin is encoded.
//

#define SOKOBAN_ORIGIN_MASK 0x00FF
#define SOKOBAN_ORIGIN_Y_SHIFT 8

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define a global containing the packed level maps.
//

extern const UCHAR SokobanData[SOKOBAN_LEVELS][SOKOBAN_LEVEL_SIZE] PROGMEM;

//
// Define a global containing the initial user starting position.
//

extern const USHORT SokobanStartingPosition[SOKOBAN_LEVELS] PROGMEM;

//
// -------------------------------------------------------- Function Prototypes
//
