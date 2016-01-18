/*++

Copyright (c) 2010 Evan Green

Module Name:

    sokoban.c

Abstract:

    This module implements the game Sokoban released by Thinking Rabbit in 1982.

Author:

    Evan Green 14-Nov-2010

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "types.h"
#include "mainboard.h"
#include "sokoban.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the location on the screen of the level meter.
//

#define SOKOBAN_LEVEL_METER_Y 23
#define SOKOBAN_LEVEL_METER_X 2

//
// Define the location on the screen to draw the main map.
//

#define SOKOBAN_LEVEL_X 2
#define SOKOBAN_LEVEL_Y 5

//
// Define the color of our hero.
//

#define SOKOBAN_HERO RGB_PIXEL(0x0, 0x1F, 0x0)
#define SOKOBAN_FREE RGB_PIXEL(0x0, 0x0, 0x0)
#define SOKOBAN_WALL RGB_PIXEL(0x1F, 0x1F, 0x1F)
#define SOKOBAN_BEAN RGB_PIXEL(0x1F, 0x0, 0x0)
#define SOKOBAN_GOAL RGB_PIXEL(0x0, 0x0, 0x1F)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
SkpPaintLevelIndicator (
    UCHAR CurrentLevel
    );

VOID
SkpPaintLevel (
    UCHAR Level,
    PUCHAR CharacterX,
    PUCHAR CharacterY
    );

UCHAR
SkpIsLevelComplete (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a global representing the completed levels. This persists across
// application runs so that the user won't lose his or her progress. The first
// one is levels 1-16, the second is levels 16-20.
//

USHORT SokobanCompletedLevels1;
UCHAR SokobanCompletedLevels2;

//
// ------------------------------------------------------------------ Functions
//

APPLICATION
SokobanEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entrypoint for the Sokoban game.

Arguments:

    None.

Return Value:

    Returns the next application to be run.

--*/

{

    UCHAR CharacterX;
    UCHAR CharacterY;
    USHORT CompletedLevels;
    UCHAR CurrentLevel;
    UCHAR LevelComplete;
    USHORT OldValue;
    APPLICATION NextApplication;
    UCHAR NextNextX;
    UCHAR NextNextY;
    USHORT NextSpace;
    UCHAR NextX;
    UCHAR NextY;

    //
    // Determine the current level.
    //

    CurrentLevel = 0;
    CompletedLevels = SokobanCompletedLevels1;
    if (CompletedLevels == 0xFFFF) {
        CompletedLevels = SokobanCompletedLevels2;
        CurrentLevel = 15;
    }

    while ((CompletedLevels & 0x1) != 0) {
        CurrentLevel += 1;
        CompletedLevels = CompletedLevels >> 1;
    }

    if (CurrentLevel > (SOKOBAN_LEVELS - 1)) {
        CurrentLevel = 0;
    }

    LevelComplete = FALSE;
    NextApplication = ApplicationNone;
    KeTrackball2 = 0;
    KeTrackball1 = 0;
    KeWhiteLeds = TRACKBALL2_WHITEPIXEL(0x10);
    while (TRUE) {
        KeClearScreen();

        //
        // Paint the level indicator.
        //

        SkpPaintLevelIndicator(CurrentLevel);

        //
        // Set up the level.
        //

        SkpPaintLevel(CurrentLevel, &CharacterX, &CharacterY);

        //
        // Accept input.
        //

        while (TRUE) {

            //
            // Did the user get bored or frustrated?
            //

            NextApplication = KeRunMenu();
            if (NextApplication != ApplicationNone) {
                return NextApplication;
            }

            //
            // See if the user is attempting to go somewhere.
            //

            NextX = CharacterX;
            NextNextX = CharacterX;
            NextY = CharacterY;
            NextNextY = CharacterY;
            if ((KeInputEdges & INPUT_LEFT2) != 0) {
                KeInputEdges &= ~INPUT_LEFT2;
                NextX -= 1;
                NextNextX -= 2;

            } else if ((KeInputEdges & INPUT_RIGHT2) != 0) {
                KeInputEdges &= ~INPUT_RIGHT2;
                NextX += 1;
                NextNextX += 2;

            } else if ((KeInputEdges & INPUT_UP2) != 0) {
                KeInputEdges &= ~INPUT_UP2;
                NextY -= 1;
                NextNextY -= 2;

            } else if ((KeInputEdges & INPUT_DOWN2) != 0) {
                KeInputEdges &= ~INPUT_DOWN2;
                NextY += 1;
                NextNextY += 2;
            }

            //
            // If that next space is a wall, that move won't be possible.
            //

            NextSpace = KeMatrix[NextY][NextX];
            if (NextSpace == SOKOBAN_WALL) {
                NextX = CharacterX;
                NextY = CharacterY;

            //
            // If there's a bean where the user wants to go, check out what's
            // in front of the bean. If it's a wall or another bean, this move
            // won't be possible.
            //

            } else if ((NextSpace & SOKOBAN_BEAN) == SOKOBAN_BEAN) {
                NextSpace = KeMatrix[NextNextY][NextNextX];
                if ((NextSpace == SOKOBAN_WALL) ||
                    (NextSpace & SOKOBAN_BEAN) == SOKOBAN_BEAN) {

                    NextX = CharacterX;
                    NextY = CharacterY;

                //
                // Move our hero and the bean.
                //

                } else {
                    KeMatrix[NextNextY][NextNextX] |= SOKOBAN_BEAN;
                    OldValue = KeMatrix[NextY][NextX];
                    KeMatrix[NextY][NextX] =
                                    (OldValue & PIXEL_USER_BIT) | SOKOBAN_HERO;
                }

            //
            // The space is free (or a goal), move onto it.
            //

            } else {
                OldValue = KeMatrix[NextY][NextX];
                KeMatrix[NextY][NextX] = (OldValue & PIXEL_USER_BIT) |
                                         SOKOBAN_HERO;
            }

            //
            // If our hero was moved, fill in the old space. This must either
            // be a free space or goal, otherwise our hero could not have been
            // on it.
            //

            if ((NextX != CharacterX) || (NextY != CharacterY)) {
                if ((KeMatrix[CharacterY][CharacterX] & PIXEL_USER_BIT) != 0) {
                    KeMatrix[CharacterY][CharacterX] = SOKOBAN_GOAL |
                                                       PIXEL_USER_BIT;

                } else {
                    KeMatrix[CharacterY][CharacterX] = SOKOBAN_FREE;
                }

                CharacterY = NextY;
                CharacterX = NextX;
            }

            //
            // If the level is complete, switch the light to green.
            //

            LevelComplete = SkpIsLevelComplete();
            if (LevelComplete != FALSE) {
                if (CurrentLevel < 16) {
                    SokobanCompletedLevels1 |= 1 << CurrentLevel;

                } else {
                    SokobanCompletedLevels2 |= 1 << (CurrentLevel - 16);
                }

                KeTrackball1 = RGB_PIXEL(0x0, 0x1F, 0x0);

                //
                // If the first trackball is pressed and the level is complete,
                // move onto the next level.
                //

                if ((KeInputEdges & (INPUT_BUTTON1 | INPUT_BUTTON2)) != 0) {
                    KeInputEdges &= ~(INPUT_BUTTON1 | INPUT_BUTTON2);
                    CurrentLevel += 1;
                    if (CurrentLevel == SOKOBAN_LEVELS) {
                        CurrentLevel = 0;
                    }

                    break;
                }

            //
            // The level is not complete, paint trackball 1 red.
            //

            } else {
                KeTrackball1 = RGB_PIXEL(0x1F, 0x0, 0x0);

                //
                // If the first trackball is pressed and the level is not
                // complete, reset the level.
                //

                if ((KeInputEdges & INPUT_BUTTON1) != 0) {
                    KeInputEdges &= ~INPUT_BUTTON1;
                    break;
                }
            }

            //
            // Check the cheat code. If trackball 1 is scrolled up or down while
            // trackball 2 is pressed, it skips to a different level.
            //

            if ((KeInputEdges & INPUT_UP1) != 0) {
                KeInputEdges &= ~INPUT_UP1;
                if ((KeRawInputs & INPUT_BUTTON2) != 0) {
                    CurrentLevel += 1;
                    if (CurrentLevel == SOKOBAN_LEVELS) {
                        CurrentLevel = 0;
                    }

                    break;
                }
            }

            if ((KeInputEdges & INPUT_DOWN1) != 0) {
                KeInputEdges &= ~INPUT_DOWN1;
                if ((KeRawInputs & INPUT_BUTTON2) != 0) {
                    CurrentLevel -= 1;
                    if (CurrentLevel == 0xFF) {
                        CurrentLevel = SOKOBAN_LEVELS - 1;
                    }

                    break;
                }
            }

            //
            // Finally, if both trackballs are held down at the same time,
            // reset all stats.
            //

            if (((KeRawInputs & INPUT_BUTTON1) != 0) &&
                ((KeRawInputs & INPUT_BUTTON2) != 0)) {

                SokobanCompletedLevels1 = 0;
                SokobanCompletedLevels2 = 0;
                CurrentLevel = 0;
                break;
            }

            //
            // Slow down to debounce.
            //

            KeStall(32 * 50);
        }
    }

    return NextApplication;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
SkpPaintLevelIndicator (
    UCHAR CurrentLevel
    )

/*++

Routine Description:

    This routine paints the level meter indicating which levels have been
    completed.

Arguments:

    CurrentLevel - Supplies the current level being played.

Return Value:

    None.

--*/

{

    USHORT CompletedLevels;
    UCHAR Level;
    USHORT Pixel;

    CompletedLevels = SokobanCompletedLevels1;
    for (Level = 0; Level < 16; Level += 1) {
        if (Level == CurrentLevel) {
            Pixel = RGB_PIXEL(0x10, 0x10, 0x10);

        } else if ((CompletedLevels & 0x1) != 0) {
            Pixel = RGB_PIXEL(0x0, 0x10, 0x0);

        } else {
            Pixel = RGB_PIXEL(0x10, 0x0, 0x0);
        }

        KeMatrix[SOKOBAN_LEVEL_METER_Y][SOKOBAN_LEVEL_METER_X + Level] = Pixel;
        CompletedLevels = CompletedLevels >> 1;
    }

    CompletedLevels = SokobanCompletedLevels2;
    for (Level = 16; Level < SOKOBAN_LEVELS; Level += 1) {
        if (Level == CurrentLevel) {
            Pixel = RGB_PIXEL(0x10, 0x10, 0x10);

        } else if ((CompletedLevels & 0x1) != 0) {
            Pixel = RGB_PIXEL(0x0, 0x10, 0x0);

        } else {
            Pixel = RGB_PIXEL(0x10, 0x0, 0x0);
        }

        KeMatrix[SOKOBAN_LEVEL_METER_Y][SOKOBAN_LEVEL_METER_X + Level] = Pixel;
        CompletedLevels = CompletedLevels >> 1;
    }

    return;
}

VOID
SkpPaintLevel (
    UCHAR Level,
    PUCHAR CharacterX,
    PUCHAR CharacterY
    )

/*++

Routine Description:

    This routine paints the level map onto the matrix.

Arguments:

    Level - Supplies the level to paint.

    CharacterX - Supplies a pointer where the character's X location will be
        returned.

    CharacterY - Supplies a pointer where the character's Y location will be
        returned.

Return Value:

    None.

--*/

{

    UCHAR Byte;
    UCHAR ByteIndex;
    UCHAR PixelIndex;
    USHORT StartingPosition;
    UCHAR XPixel;
    UCHAR YPixel;

    XPixel = SOKOBAN_LEVEL_X;
    YPixel = SOKOBAN_LEVEL_Y;
    for (ByteIndex = 0; ByteIndex < SOKOBAN_LEVEL_SIZE; ByteIndex += 1) {
        Byte = RtlReadProgramSpace8(&(SokobanData[Level][ByteIndex]));
        for (PixelIndex = 0; PixelIndex < 4; PixelIndex += 1) {
            switch ((Byte >> (2 * PixelIndex)) & 0x3) {
            case SOKOBAN_CELL_FREE:
                KeMatrix[YPixel][XPixel] = SOKOBAN_FREE;
                break;

            case SOKOBAN_CELL_WALL:
                KeMatrix[YPixel][XPixel] = SOKOBAN_WALL;
                break;

            case SOKOBAN_CELL_BEAN:
                KeMatrix[YPixel][XPixel] = SOKOBAN_BEAN;
                break;

            case SOKOBAN_CELL_GOAL:
                KeMatrix[YPixel][XPixel] = SOKOBAN_GOAL | PIXEL_USER_BIT;
                break;
            }

            XPixel += 1;
            if (XPixel == (SOKOBAN_LEVEL_X + SOKOBAN_WIDTH)) {
                XPixel = SOKOBAN_LEVEL_X;
                YPixel += 1;
            }
        }
    }

    StartingPosition = RtlReadProgramSpace16(&(SokobanStartingPosition[Level]));
    *CharacterX = (StartingPosition & SOKOBAN_ORIGIN_MASK) + SOKOBAN_LEVEL_X;
    *CharacterY = ((StartingPosition >> SOKOBAN_ORIGIN_Y_SHIFT) &
                   SOKOBAN_ORIGIN_MASK) + SOKOBAN_LEVEL_Y;

    //
    // Paint our hero.
    //

    XPixel = *CharacterX;
    YPixel = *CharacterY;
    KeMatrix[YPixel][XPixel] = SOKOBAN_HERO;
    return;
}

UCHAR
SkpIsLevelComplete (
    VOID
    )

/*++

Routine Description:

    This routine determines if the current level has been completed or not.
    The current level must be painted on the matrix.

Arguments:

    None.

Return Value:

    TRUE if the level is currently complete.

    FALSE if the level is not yet complete as it stands on the board.

--*/

{

    USHORT Pixel;
    UCHAR XPixel;
    UCHAR YPixel;

    for (YPixel = SOKOBAN_LEVEL_Y;
         YPixel < (SOKOBAN_LEVEL_Y + SOKOBAN_HEIGHT);
         YPixel += 1) {

        for (XPixel = SOKOBAN_LEVEL_X;
             XPixel < (SOKOBAN_LEVEL_X + SOKOBAN_WIDTH);
             XPixel += 1) {

            Pixel = KeMatrix[YPixel][XPixel];

            //
            // If the pixel has the user bit set, it's a goal. If there's no
            // bean in it, the level is not complete.
            //

            if ((Pixel & PIXEL_USER_BIT) != 0) {
                if ((Pixel & SOKOBAN_BEAN) != SOKOBAN_BEAN) {
                    return FALSE;
                }
            }
        }
    }

    //
    // No requirements were not met, so the level must be done (you can't not
    // love a good double negative).
    //

    return TRUE;
}

