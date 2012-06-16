/*++

Copyright (c) 2010 Evan Green

Module Name:

    life.c

Abstract:

    This module implements a matrix app, Conway's Game of Life.

Author:

    Evan Green 13-Nov-2010

Environment:

    x86/AVR

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "types.h"
#include "mainboard.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the duration of one game, in ms/32.
//

#define GAME_DURATION (32 * 1000UL * 60 * 10)

//
// Define the default duration between updates, in ms/32.
//

#define DEFAULT_UPDATE_INTERVAL (32 * 500UL)

//
// Define the granularity with which the user can adjust the update speed, in
// ms/32.
//

#define UPDATE_INCREMENT (32 * 30)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

UCHAR
LifepGetNeighborCount (
    UCHAR XPixel,
    UCHAR YPixel,
    PUSHORT NewPixelColor
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

APPLICATION
LifeEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for Conway's Game of Life.

Arguments:

    None.

Return Value:

    Returns the index of the next application to launch.

--*/

{

    UCHAR CursorMoved;
    UCHAR CursorX;
    UCHAR CursorY;
    ULONG GameTime;
    UCHAR Neighbors;
    ULONG NextUpdateTime;
    APPLICATION NextApplication;
    USHORT OnPixel;
    ULONG UpdateInterval;
    UCHAR XPixel;
    UCHAR YPixel;

    NextApplication = ApplicationNone;
    UpdateInterval = DEFAULT_UPDATE_INTERVAL;
    while (TRUE) {
        CursorX = MATRIX_WIDTH / 2;
        CursorY = MATRIX_HEIGHT / 2;

        //
        // Decide the ethnicity of today's culture.
        //

        OnPixel = RGB_PIXEL(HlRandom() & 0x1F,
                            HlRandom() & 0x1F,
                            HlRandom() & 0x1F);

        if (OnPixel == 0) {
            OnPixel = RGB_PIXEL(0x1F, 0x1F, 0x1F);
        }

        //
        // Fill the board randomly. Shoot to fill about a quarter of the pixels.
        //

        for (YPixel = 0; YPixel < MATRIX_HEIGHT; YPixel += 1) {
            for (XPixel = 0; XPixel < MATRIX_WIDTH; XPixel += 1) {
                if ((HlRandom() & 0x3) == 0) {
                    KeMatrix[YPixel][XPixel] = OnPixel | PIXEL_USER_BIT;

                } else {
                    KeMatrix[YPixel][XPixel] = 0;
                }
            }
        }

        GameTime = 0;
        while (GameTime < GAME_DURATION) {

            //
            // Stall for a bit. This is the meaning of life.
            //

            NextUpdateTime = KeRawTime + UpdateInterval;
            if (NextUpdateTime < KeRawTime) {
                NextUpdateTime = 0xFFFFFFFFULL;
            }

            while (KeRawTime < NextUpdateTime) {
                KeStall(32 * 10);

                //
                // Process any inputs. The inputs are kind of fun. Trackball 1
                // paints red live cells as it tracks.
                //

                CursorMoved = FALSE;
                if ((KeInputEdges & INPUT_LEFT1) != 0) {
                    KeInputEdges &= ~INPUT_LEFT1;
                    CursorMoved = TRUE;
                    CursorX -= 1;
                    if (CursorX == 0xFF) {
                        CursorX = MATRIX_WIDTH - 1;
                    }
                }

                if ((KeInputEdges & INPUT_RIGHT1) != 0) {
                    KeInputEdges &= ~INPUT_RIGHT1;
                    CursorMoved = TRUE;
                    CursorX += 1;
                    if (CursorX == MATRIX_WIDTH) {
                        CursorX = 0;
                    }
                }

                if ((KeInputEdges & INPUT_UP1) != 0) {
                    KeInputEdges &= ~INPUT_UP1;
                    CursorMoved = TRUE;
                    CursorY -= 1;
                    if (CursorY == 0xFF) {
                        CursorY = MATRIX_HEIGHT - 1;
                    }
                }

                if ((KeInputEdges & INPUT_DOWN1) != 0) {
                    KeInputEdges &= ~INPUT_DOWN1;
                    CursorMoved = TRUE;
                    CursorY += 1;
                    if (CursorY == MATRIX_HEIGHT) {
                        CursorY = 0;
                    }
                }

                if (CursorMoved != FALSE) {
                    KeMatrix[CursorY][CursorX] = RED_PIXEL(0x1F);
                }

                //
                // Did the user get bored?
                //

                NextApplication = KeRunMenu();
                if (NextApplication != ApplicationNone) {
                    return NextApplication;
                }

                //
                // Trackball 2 controls the update speed.
                //

                if ((KeInputEdges & INPUT_UP2) != 0) {
                    KeInputEdges &= ~INPUT_UP2;
                    if (UpdateInterval > UPDATE_INCREMENT) {
                        UpdateInterval -= UPDATE_INCREMENT;
                    }
                }

                if ((KeInputEdges & INPUT_DOWN2) != 0) {
                    KeInputEdges &= ~INPUT_DOWN2;
                    UpdateInterval += UPDATE_INCREMENT;
                }
            }

            //
            // Clicking either trackball resets to a new game.
            //

            if ((KeInputEdges & (INPUT_BUTTON1 | INPUT_BUTTON2)) != 0) {
                KeInputEdges &= ~(INPUT_BUTTON1 | INPUT_BUTTON2);
                break;
            }

            //
            // Update total game time.
            //

            GameTime += UpdateInterval;

            //
            // Process the board to get the next generation of pixels.
            //

            for (YPixel = 0; YPixel < MATRIX_HEIGHT; YPixel += 1) {
                for (XPixel = 0; XPixel < MATRIX_WIDTH; XPixel += 1) {
                    Neighbors = LifepGetNeighborCount(XPixel, YPixel, &OnPixel);

                    //
                    // Handle a pixel that was alive.
                    //

                    if ((KeMatrix[YPixel][XPixel] & PIXEL_USER_BIT) != 0) {

                        //
                        // Any pixel with fewer than two neighbors dies from
                        // lack of love. Any pixel with more than three
                        // neighbors dies from overcrowding.
                        //

                        if ((Neighbors < 2) || (Neighbors > 3)) {
                            KeMatrix[YPixel][XPixel] &= PIXEL_USER_BIT;
                        }

                    //
                    // This pixel was dead.
                    //

                    } else {

                        //
                        // A dead pixel with exactly three neighbors spawns
                        // life.
                        //

                        if (Neighbors == 3) {
                            KeMatrix[YPixel][XPixel] |= OnPixel;
                        }
                    }
                }
            }

            //
            // Now that the next iteration of the board has been calculated,
            // update the "previous state" bit.
            //

            for (YPixel = 0; YPixel < MATRIX_HEIGHT; YPixel += 1) {
                for (XPixel = 0; XPixel < MATRIX_WIDTH; XPixel += 1) {
                    if ((KeMatrix[YPixel][XPixel] & (~PIXEL_USER_BIT)) != 0) {
                        KeMatrix[YPixel][XPixel] |= PIXEL_USER_BIT;

                    } else {
                        KeMatrix[YPixel][XPixel] &= ~PIXEL_USER_BIT;
                    }
                }
            }
        }
    }

    return NextApplication;
}

//
// --------------------------------------------------------- Internal Functions
//

UCHAR
LifepGetNeighborCount (
    UCHAR XPixel,
    UCHAR YPixel,
    PUSHORT NewPixelColor
    )

/*++

Routine Description:

    This routine determines how many neighbors the given pixel has. Boondocks,
    suburbs, or city living.

Arguments:

    XPixel - Supplies the zero-based X coordinate of the pixel (left origin).

    YPixel - Supplies the zero-based Y coordinate of the pixel (top origin).

    NewPixelColor - Supplies the average color of all the pixel's neighbors.

Return Value:

    Returns the number of active cells bordering this cell.

--*/

{

    UCHAR BlueTotal;
    UCHAR CurrentX;
    UCHAR CurrentY;
    UCHAR GreenTotal;
    UCHAR Neighbors;
    USHORT Pixel;
    UCHAR RedTotal;
    UCHAR XNeighbor;
    UCHAR YNeighbor;

    RedTotal = 0;
    GreenTotal = 0;
    BlueTotal = 0;
    Neighbors = 0;
    for (YNeighbor = 0; YNeighbor < 3; YNeighbor += 1) {
        for (XNeighbor = 0; XNeighbor < 3; XNeighbor += 1) {

            //
            // Skip the cell itself.
            //

            if ((XNeighbor == 1) && (YNeighbor == 1)) {
                continue;
            }

            CurrentX = XPixel + XNeighbor - 1;
            CurrentY = YPixel + YNeighbor - 1;

            //
            // Handle wrapping.
            //

            if (CurrentX == 0xFF) {
                CurrentX = MATRIX_WIDTH - 1;
            }

            if (CurrentX == MATRIX_WIDTH) {
                CurrentX = 0;
            }

            if (CurrentY == 0xFF) {
                CurrentY = MATRIX_HEIGHT - 1;
            }

            if (CurrentY == MATRIX_HEIGHT) {
                CurrentY = 0;
            }

            Pixel = KeMatrix[CurrentY][CurrentX];
            if ((Pixel & PIXEL_USER_BIT) != 0) {
                Neighbors += 1;
                RedTotal += PIXEL_RED(Pixel);
                GreenTotal += PIXEL_GREEN(Pixel);
                BlueTotal += PIXEL_BLUE(Pixel);
            }
        }
    }

    //
    // Determine the color of the new pixel, should it be turned on.
    //

    if (Neighbors != 0) {
        *NewPixelColor = RGB_PIXEL((RedTotal / Neighbors),
                                   (GreenTotal / Neighbors),
                                   (BlueTotal / Neighbors));

    }

    return Neighbors;
}

