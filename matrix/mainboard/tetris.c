/*++

Copyright (c) 2010 Evan Green

Module Name:

    tetris.c

Abstract:

    This module implements Tetris, originally written by Alexey Pajitnov in
    1984.

Author:

    Evan Green 14-Nov-2010

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
// Define the borders created to keep the game manageable.
//

#define TETRIS_LEFT_BORDER 5
#define TETRIS_RIGHT_BORDER 16

//
// Define the X coordinate for the level and line indicators.
//

#define TETRIS_INDICATORS_X 18

//
// Define the Y coordinates of the indicators.
//

#define TETRIS_LINE_INDICATOR_Y 8
#define TETRIS_LEVEL_INDICATOR_Y 12

//
// Define the initial drop rate, in ms.
//

#define TETRIS_INITIAL_DROP_RATE (650 * 32)

//
// Define how the drop rate increases with each level.
//

#define TETRIS_DROP_INCREMENT 100

//
// Define the number of lines completed before the level ticks up.
//

#define TETRIS_LINES_PER_LEVEL 10

//
// Define the initial location where pieces show up.
//

#define TETRIS_INITIAL_X 10

//
// Define the "invalid piece", which is no piece at all.
//

#define TETRIS_INVALID_PIECE 7

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

UCHAR
TtpGenerateNewPiece (
    PUCHAR PositionX,
    PUCHAR PositionY
    );

UCHAR
TtpMovePiece (
    PUCHAR PieceX,
    PUCHAR PieceY,
    CHAR VectorX,
    CHAR VectorY
    );

UCHAR
TtpHandlePieceLockdown (
    UCHAR PieceX,
    UCHAR PieceY
    );

VOID
TtpRotatePiece (
    PUCHAR PieceX,
    PUCHAR PieceY
    );

VOID
TtpDrawIndicators (
    UCHAR Level,
    UCHAR LinesCompleted
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

APPLICATION
TetrisEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for Tetris.

Arguments:

    None.

Return Value:

    Returns the next application to be run.

--*/

{

    UCHAR CurrentPiece;
    UCHAR GameRunning;
    ULONG NextUpdateTime;
    UCHAR Level;
    UCHAR LinesCompleted;
    APPLICATION NextApplication;
    UCHAR PieceMoved;
    UCHAR PieceX;
    UCHAR PieceY;
    USHORT UpdateInterval;
    UCHAR YPixel;

    NextApplication = ApplicationNone;
    while (NextApplication == ApplicationNone) {
        KeClearScreen();

        //
        // Draw the borders.
        //

        for (YPixel = 0; YPixel < MATRIX_HEIGHT; YPixel += 1) {
            KeMatrix[YPixel][TETRIS_LEFT_BORDER] =
                        RGB_PIXEL(MAX_INTENSITY, MAX_INTENSITY, MAX_INTENSITY);

            KeMatrix[YPixel][TETRIS_RIGHT_BORDER] =
                        RGB_PIXEL(MAX_INTENSITY, MAX_INTENSITY, MAX_INTENSITY);

        }

        UpdateInterval = TETRIS_INITIAL_DROP_RATE;
        GameRunning = TRUE;
        CurrentPiece = TETRIS_INVALID_PIECE;
        KeTrackball1 = RGB_PIXEL(0, 0, MAX_INTENSITY);
        KeTrackball2 = 0;
        NextUpdateTime = KeRawTime + UpdateInterval;
        LinesCompleted = 0;
        Level = 0;
        while (GameRunning != FALSE) {
            NextApplication = KeRunMenu();
            if (NextApplication != ApplicationNone) {
                GameRunning = FALSE;
                break;
            }

            //
            // If a new piece is needed, generate one.
            //

            if (CurrentPiece == TETRIS_INVALID_PIECE) {
                CurrentPiece = TtpGenerateNewPiece(&PieceX, &PieceY);
                if (CurrentPiece == TETRIS_INVALID_PIECE) {
                    GameRunning = FALSE;
                    break;
                }
            }

            //
            // Stall for 1ms and process input signals.
            //

            KeStall(32);
            if ((KeInputEdges & INPUT_LEFT1) != 0) {
                KeInputEdges &= ~INPUT_LEFT1;
                TtpMovePiece(&PieceX, &PieceY, -1, 0);
            }

            if ((KeInputEdges & INPUT_RIGHT1) != 0) {
                KeInputEdges &= ~INPUT_RIGHT1;
                TtpMovePiece(&PieceX, &PieceY, 1, 0);
            }

            if ((KeInputEdges & INPUT_DOWN1) != 0) {
                KeInputEdges &= ~INPUT_DOWN1;
                NextUpdateTime = KeRawTime + UpdateInterval;
            }

            if ((KeInputEdges & INPUT_UP1) != 0) {
                KeInputEdges &= ~INPUT_UP1;
                TtpRotatePiece(&PieceX, &PieceY);
            }

            //
            // If the update interval has gone by, move the piece down.
            //

            if (KeRawTime >= NextUpdateTime) {
                NextUpdateTime = KeRawTime + UpdateInterval;
                if (NextUpdateTime < KeRawTime) {
                    NextUpdateTime = 0xFFFFFFFFUL;
                }

                PieceMoved = TtpMovePiece(&PieceX, &PieceY, 0, 1);
                if (PieceMoved == FALSE) {
                    LinesCompleted += TtpHandlePieceLockdown(PieceX, PieceY);
                    TtpDrawIndicators(Level, LinesCompleted);
                    if (LinesCompleted == TETRIS_LINES_PER_LEVEL) {
                        LinesCompleted = 0;
                        Level += 1;
                        if (UpdateInterval >= TETRIS_DROP_INCREMENT) {
                            UpdateInterval -= TETRIS_DROP_INCREMENT;
                        }
                    }

                    CurrentPiece = TETRIS_INVALID_PIECE;
                }
            }
        }

        if (NextApplication != ApplicationNone) {
            return NextApplication;
        }

        //
        // Game over. Wait until the user pushes the button.
        //

        KeTrackball1 = RGB_PIXEL(MAX_INTENSITY, 0, 0);
        while ((KeInputEdges & INPUT_BUTTON1) == 0) {
            NextApplication = KeRunMenu();
            if (NextApplication != ApplicationNone) {
                return NextApplication;
            }

            KeStall(1);
        }

        KeInputEdges &= ~INPUT_BUTTON1;
    }

    return NextApplication;
}

//
// --------------------------------------------------------- Internal Functions
//

UCHAR
TtpGenerateNewPiece (
    PUCHAR PositionX,
    PUCHAR PositionY
    )

/*++

Routine Description:

    This routine generates a new tetris piece.

Arguments:

    PositionX - Supplies a pointer where the X coordinate of the top left of
        the new piece will be returned.

    PositionY - Supplies a pointer where the Y coordinate of the top left of
        the new piece will be returned.

Return Value:

    Returns the index of the new piece.

    TETRIS_INVALID_PIECE if the piece could not be generated because there were
    blocks in the way. The game is over.

--*/

{

    UCHAR Piece;
    UCHAR PieceX;
    UCHAR PieceY;
    USHORT Pixel;
    UCHAR XPixel;
    UCHAR YPixel;

    do {
        Piece = HlRandom() & 7;
    } while (Piece == TETRIS_INVALID_PIECE);

    PieceX = TETRIS_INITIAL_X;
    PieceY = 0;
    switch (Piece) {

    //
    // The line (I).
    //

    case 0:
        Pixel = RGB_PIXEL(0x0, MAX_INTENSITY, MAX_INTENSITY) |
                PIXEL_USER_BIT;

        KeMatrix[PieceY][PieceX] |= PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX + 1] |= PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX + 2] |= PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX + 3] |= PIXEL_USER_BIT;
        break;

    //
    // The L poking up on the left (J).
    //

    case 1:
        Pixel = RGB_PIXEL(0x0, 0x0, MAX_INTENSITY) | PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX + 1] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX + 2] |= PIXEL_USER_BIT;
        break;

    //
    // The L poking up on the right (L).
    //

    case 2:
        Pixel = RGB_PIXEL(MAX_INTENSITY, MAX_INTENSITY / 2, 0x0) |
                PIXEL_USER_BIT;

        KeMatrix[PieceY + 1][PieceX] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX + 1] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX + 2] |= PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX + 2] |= PIXEL_USER_BIT;
        break;

    //
    // The box (O).
    //

    case 3:
        Pixel = RGB_PIXEL(MAX_INTENSITY, MAX_INTENSITY, 0x0) |
                PIXEL_USER_BIT;

        KeMatrix[PieceY][PieceX] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX] |= PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX + 1] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX + 1] |= PIXEL_USER_BIT;
        break;

    //
    // The zigzag going up and right (S).
    //

    case 4:
        Pixel = RGB_PIXEL(0x0, MAX_INTENSITY, 0x0) | PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX + 1] |= PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX + 1] |= PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX + 2] |= PIXEL_USER_BIT;
        break;

    //
    // The tee (T).
    //

    case 5:
        Pixel = RGB_PIXEL(MAX_INTENSITY, 0x0, MAX_INTENSITY) |
                PIXEL_USER_BIT;

        KeMatrix[PieceY + 1][PieceX] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX + 1] |= PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX + 1] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX + 2] |= PIXEL_USER_BIT;
        break;

    //
    // The zigzag going down and right (Z).
    //

    case 6:
    default:
        Pixel = RGB_PIXEL(0x0, MAX_INTENSITY, 0x0) | PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX] |= PIXEL_USER_BIT;
        KeMatrix[PieceY][PieceX + 1] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX + 1] |= PIXEL_USER_BIT;
        KeMatrix[PieceY + 1][PieceX + 2] |= PIXEL_USER_BIT;
        break;
    }

    *PositionX = PieceX;
    *PositionY = PieceY;

    //
    // Check to see if any pieces marked were already blocks. If they were,
    // fail.
    //

    for (YPixel = PieceY; YPixel < PieceY + 4; YPixel += 1) {
        for (XPixel = PieceX; XPixel < PieceX + 4; XPixel += 1) {
            if ((KeMatrix[YPixel][XPixel] & PIXEL_USER_BIT) != 0) {
                if ((KeMatrix[YPixel][XPixel] & (~PIXEL_USER_BIT)) != 0) {
                    return TETRIS_INVALID_PIECE;

                } else {
                    KeMatrix[YPixel][XPixel] = Pixel;
                }
            }
        }
    }

    return Piece;
}

UCHAR
TtpMovePiece (
    PUCHAR PieceX,
    PUCHAR PieceY,
    CHAR VectorX,
    CHAR VectorY
    )

/*++

Routine Description:

    This routine moves a tetris piece by one position. It is assumed that the
    piece moves in either the X direction or the Y direction, but not both.

Arguments:

    PieceX - Supplies a pointer to the current piece's X position on input. On
        output, contains the piece's new position.

    PieceY - Supplies a pointer to the current piece's Y position on input. On
        output, contains the piece's new position.

    VectorX - Supplies how far to move the piece in the X direction. Valid
        values are -1, 0, and 1. If this value is non-zero, the Y vector is
        expected to be 0.

    VectorY - Supplies how far to move the piece in the Y direction. Valid
        values are -1, 0, and 1. If this value is non-zero, the X vector is
        expected to be 0.

Return Value:

    Returns TRUE if the piece was successfully moved.

    Returns FALSE if an obstacle blocks that piece from moving in the desired
    direction.

--*/

{

    USHORT NextPixel;
    UCHAR OkayToMove;
    UCHAR XPixel;
    UCHAR YPixel;

    OkayToMove = TRUE;

    //
    // Move the piece right.
    //

    if (VectorX == 1) {

        //
        // Check if every row can handle the move right.
        //

        for (YPixel = *PieceY;
             ((YPixel < *PieceY + 4) && (YPixel < MATRIX_HEIGHT));
             YPixel += 1) {

            for (XPixel = *PieceX + 3; XPixel >= *PieceX; XPixel -= 1) {

                //
                // If this box has a piece in it, then it is the rightmost
                // pixel. If the pixel beyond it is not free, then the move
                // must be vetoed.
                //

                if ((KeMatrix[YPixel][XPixel] & PIXEL_USER_BIT) != 0) {
                    NextPixel = KeMatrix[YPixel][XPixel + 1];
                    if ((NextPixel != 0) &&
                        ((NextPixel & PIXEL_USER_BIT) == 0)) {

                        OkayToMove = FALSE;
                        break;
                    }
                }
            }

            if (OkayToMove == FALSE) {
                break;
            }
        }

        //
        // Move the piece if that's okay.
        //

        if (OkayToMove != FALSE) {
            for (YPixel = *PieceY;
                 ((YPixel < *PieceY + 4) && (YPixel < MATRIX_HEIGHT));
                 YPixel += 1) {

                for (XPixel = *PieceX + 3; XPixel >= *PieceX; XPixel -= 1) {
                    if ((KeMatrix[YPixel][XPixel] & PIXEL_USER_BIT) != 0) {
                        KeMatrix[YPixel][XPixel + 1] = KeMatrix[YPixel][XPixel];
                        KeMatrix[YPixel][XPixel] = 0;
                    }
                }
            }

            *PieceX += 1;
        }

    //
    // Move the piece left.
    //

    } else if (VectorX != 0) {

        //
        // Check if every row can handle the move left.
        //

        for (YPixel = *PieceY;
             ((YPixel < *PieceY + 4) && (YPixel < MATRIX_HEIGHT));
             YPixel += 1) {

            for (XPixel = *PieceX; XPixel < *PieceX + 4; XPixel += 1) {

                //
                // If this box has a piece in it, then it is the leftmost
                // pixel. If the pixel beyond it is not free, then the move
                // must be vetoed.
                //

                if ((KeMatrix[YPixel][XPixel] & PIXEL_USER_BIT) != 0) {
                    NextPixel = KeMatrix[YPixel][XPixel - 1];
                    if ((NextPixel != 0) &&
                        ((NextPixel & PIXEL_USER_BIT) == 0)) {

                        OkayToMove = FALSE;
                        break;
                    }
                }
            }

            if (OkayToMove == FALSE) {
                break;
            }
        }

        //
        // Move the piece if that's okay.
        //

        if (OkayToMove != FALSE) {
            for (YPixel = *PieceY;
                 ((YPixel < *PieceY + 4) && (YPixel < MATRIX_HEIGHT));
                 YPixel += 1) {

                for (XPixel = *PieceX; XPixel < *PieceX + 4; XPixel += 1) {
                    if ((KeMatrix[YPixel][XPixel] & PIXEL_USER_BIT) != 0) {
                        KeMatrix[YPixel][XPixel - 1] = KeMatrix[YPixel][XPixel];
                        KeMatrix[YPixel][XPixel] = 0;
                    }
                }
            }

            *PieceX -= 1;
        }

    //
    // Move the piece down.
    //

    } else if (VectorY == 1) {

        //
        // Check if every column can move down.
        //

        for (XPixel = *PieceX; XPixel < *PieceX + 4; XPixel += 1) {
            YPixel = *PieceY + 3;
            if (YPixel > MATRIX_HEIGHT - 1) {
                YPixel = MATRIX_HEIGHT - 1;
            }

            for (NOTHING;
                 ((YPixel >= *PieceY) && (YPixel != (UCHAR)-1));
                 YPixel -= 1) {

                //
                // If this pixel has a piece in it, then it is the bottom most
                // pixel. If it cannot move down, then the move is vetoed.
                //

                if ((KeMatrix[YPixel][XPixel] & PIXEL_USER_BIT) != 0) {
                    if (YPixel == MATRIX_HEIGHT - 1) {
                        OkayToMove = FALSE;
                        break;
                    }

                    NextPixel = KeMatrix[YPixel + 1][XPixel];
                    if ((NextPixel != 0) &&
                        ((NextPixel & PIXEL_USER_BIT) == 0)) {

                        OkayToMove = FALSE;
                        break;
                    }
                }
            }

            if (OkayToMove == FALSE) {
                break;
            }
        }

        //
        // Move the piece if that's okay.
        //

        if (OkayToMove != FALSE) {
            for (XPixel = *PieceX ; XPixel < *PieceX + 4; XPixel += 1) {
                YPixel = *PieceY + 3;
                if (YPixel > MATRIX_HEIGHT - 1) {
                    YPixel = MATRIX_HEIGHT - 1;
                }

                for (NOTHING;
                     ((YPixel >= *PieceY) && (YPixel != (UCHAR)-1));
                     YPixel -= 1) {

                    if ((KeMatrix[YPixel][XPixel] & PIXEL_USER_BIT) != 0) {
                        KeMatrix[YPixel + 1][XPixel] = KeMatrix[YPixel][XPixel];
                        KeMatrix[YPixel][XPixel] = 0;
                    }
                }
            }

            *PieceY += 1;
        }

    //
    // Move the piece up. Not implemented.
    //

    } else if (VectorY != 0) {
        OkayToMove = FALSE;
    }

    return OkayToMove;
}

UCHAR
TtpHandlePieceLockdown (
    UCHAR PieceX,
    UCHAR PieceY
    )

/*++

Routine Description:

    This routine locks a piece in place and handles any disappearing rows.

Arguments:

    PieceX - Supplies the piece's current X position.

    PieceY - Supplies the piece's current Y position.

Return Value:

    Returns the number of lines that were cleared by this piece setting in.

--*/

{

    UCHAR CopyY;
    UCHAR LineCompleted;
    UCHAR LinesCompleted;
    UCHAR SmallestY;
    UCHAR XPixel;
    UCHAR YPixel;

    LinesCompleted = 0;

    //
    // Unmark the piece as active.
    //

    for (YPixel = PieceY;
         ((YPixel < PieceY + 4) && (YPixel < MATRIX_HEIGHT));
         YPixel += 1) {

        for (XPixel = PieceX; XPixel < PieceX + 4; XPixel += 1) {
            KeMatrix[YPixel][XPixel] &= ~PIXEL_USER_BIT;
        }
    }

    //
    // Look for and mark completed lines.
    //

    for (YPixel = PieceY;
         ((YPixel < PieceY + 4) && (YPixel < MATRIX_HEIGHT));
         YPixel += 1) {

        LineCompleted = TRUE;
        for (XPixel = TETRIS_LEFT_BORDER + 1;
             XPixel < TETRIS_RIGHT_BORDER;
             XPixel += 1) {

            if (KeMatrix[YPixel][XPixel] == 0) {
                LineCompleted = FALSE;
                break;
            }
        }

        if (LineCompleted != FALSE) {
            LinesCompleted += 1;
            for (XPixel = TETRIS_LEFT_BORDER + 1;
                 XPixel < TETRIS_RIGHT_BORDER;
                 XPixel += 1) {

                KeMatrix[YPixel][XPixel] = 0xFFFF;
            }
        }
    }

    if (LinesCompleted == 0) {
        return 0;
    }

    //
    // Let the user see those white lines he or she completed.
    //

    KeStall(32 * 150);

    //
    // If lines are going to be deleted, find the highest (lowest value) line
    // with a valid pixel in it, to optimize the block copies.
    //

    SmallestY = 0;
    while (TRUE) {
        for (XPixel = TETRIS_LEFT_BORDER + 1;
             XPixel < TETRIS_RIGHT_BORDER;
             XPixel += 1) {

            if (KeMatrix[SmallestY][XPixel] != 0) {
                break;
            }
        }

        //
        // If the loop exited early then a filled pixel must have been found,
        // in which case the highest up Y value has been located.
        //

        if (XPixel != TETRIS_RIGHT_BORDER) {
            break;
        }

        SmallestY += 1;
    }

    //
    // Take out any completed lines.
    //

    for (YPixel = PieceY;
         ((YPixel < PieceY + 4) && (YPixel < MATRIX_HEIGHT));
         YPixel += 1) {

        //
        // Delete if the first pixel in the row has been marked for deletion.
        //

        if ((KeMatrix[YPixel][TETRIS_LEFT_BORDER + 1] & PIXEL_USER_BIT) != 0) {

            //
            // Copy each line from the line above it (watch out for the top
            // line).
            //

            for (CopyY = YPixel; CopyY > SmallestY; CopyY -= 1) {
                for (XPixel = TETRIS_LEFT_BORDER + 1;
                     XPixel < TETRIS_RIGHT_BORDER;
                     XPixel += 1) {

                    if (CopyY == 0) {
                        KeMatrix[CopyY][XPixel] = 0;

                    } else {
                        KeMatrix[CopyY][XPixel] = KeMatrix[CopyY - 1][XPixel];
                    }
                }
            }

            //
            // Blank out the highest line.
            //

            for (XPixel = TETRIS_LEFT_BORDER + 1;
                 XPixel < TETRIS_RIGHT_BORDER;
                 XPixel += 1) {

                KeMatrix[SmallestY][XPixel] = 0;
            }
        }
    }

    return LinesCompleted;
}

VOID
TtpRotatePiece (
    PUCHAR PieceX,
    PUCHAR PieceY
    )

/*++

Routine Description:

    This routine rotates a tetris piece 90 degrees clockwise if possible.

Arguments:

    PieceX - Supplies a pointer to the current piece's X position on input. On
        output, contains the piece's new position.

    PieceY - Supplies a pointer to the current piece's Y position on input. On
        output, contains the piece's new position.

Return Value:

    None.

--*/

{

    USHORT DestinationPixel;
    UCHAR DimensionX;
    UCHAR DimensionY;
    USHORT Pixel;
    UCHAR RotatedPiece[4][4];
    USHORT SourcePixel;
    UCHAR XPixel;
    UCHAR YPixel;

    //
    // Determine the dimension of this piece and zero out the rotate local.
    //

    DimensionX = 0;
    DimensionY = 0;
    Pixel = 0;
    for (YPixel = *PieceY;
         ((YPixel < *PieceY + 4) && (YPixel < MATRIX_HEIGHT));
         YPixel += 1) {

        for (XPixel = *PieceX; XPixel < *PieceX + 4; XPixel += 1) {
            RotatedPiece[YPixel - *PieceY][XPixel - *PieceX] = 0;
            if ((KeMatrix[YPixel][XPixel] & PIXEL_USER_BIT) != 0) {
                Pixel = KeMatrix[YPixel][XPixel];
                if ((YPixel - *PieceY) > DimensionY) {
                    DimensionY = YPixel - *PieceY;
                }

                if ((XPixel - *PieceX) > DimensionX) {
                    DimensionX = XPixel - *PieceX;
                }
            }
        }
    }

    //
    // If rotating the image causes the image to fall off the bottom, don't
    // do it.
    //

    if (*PieceY + DimensionX >= MATRIX_HEIGHT) {
        return;
    }

    //
    // Construct the rotated image and check to see if it can be rotated at
    // the same time.
    //

    for (YPixel = 0; YPixel <= DimensionX; YPixel += 1) {
        for (XPixel = 0; XPixel <= DimensionY; XPixel += 1) {

            //
            // If the *source* pixel was originally marked, check the
            // destination pixel.
            //

            SourcePixel = KeMatrix[*PieceY + (DimensionY - XPixel)]
                                  [*PieceX + YPixel];

            if ((SourcePixel & PIXEL_USER_BIT) != 0) {

                //
                // If the destination pixel is not an active pixel but is
                // colored, the block cannot rotate.
                //

                DestinationPixel = KeMatrix[*PieceY + YPixel][*PieceX + XPixel];
                if (((DestinationPixel & PIXEL_USER_BIT) == 0) &&
                    ((DestinationPixel & (~PIXEL_USER_BIT)) != 0)) {

                    return;
                }

                RotatedPiece[YPixel][XPixel] = 1;
            }
        }
    }

    //
    // If execution got here without returning, the piece must be okay to
    // rotate. Light up any new pixels and blank out any old ones.
    //

    for (YPixel = 0; YPixel < 4; YPixel += 1) {
        if (YPixel >= MATRIX_HEIGHT) {
            break;
        }

        for (XPixel = 0; XPixel < 4; XPixel += 1) {

            //
            // If the pixel is set in the destination, just set it.
            //

            if (RotatedPiece[YPixel][XPixel] != 0) {
                KeMatrix[*PieceY + YPixel][*PieceX + XPixel] = Pixel;

            //
            // If the piece wasn't set in the destination but was set in the
            // source as part of the active piece, blank it out.
            //

            } else if ((KeMatrix[*PieceY + YPixel][*PieceX + XPixel] &
                        PIXEL_USER_BIT) != 0) {

                KeMatrix[*PieceY + YPixel][*PieceX + XPixel] = 0;
            }
        }
    }

    return;
}

VOID
TtpDrawIndicators (
    UCHAR Level,
    UCHAR LinesCompleted
    )

/*++

Routine Description:

    This routine draws the lines completed and level indicators.

Arguments:

    Level - Supplies the current level.

    LinesCompleted - Supplies the number of lines completed.

Return Value:

    None.

--*/

{

    UCHAR CurrentIndicator;
    UCHAR Pass;
    UCHAR Value;
    UCHAR YPosition;

    for (Pass = 0; Pass < 2; Pass += 1) {

        //
        // In the first pass, draw the line indicator. In the second pass,
        // draw the level indicator.
        //

        if (Pass == 0) {
            Value = LinesCompleted;
            YPosition = TETRIS_LINE_INDICATOR_Y;

        } else {
            Value = Level;
            YPosition = TETRIS_LEVEL_INDICATOR_Y;
        }

        //
        // Draw the given indicator.
        //

        for (CurrentIndicator = 0;
             CurrentIndicator < 5;
             CurrentIndicator += 1) {

            if (Value > 10 + CurrentIndicator) {
                KeMatrix[YPosition][TETRIS_INDICATORS_X + CurrentIndicator] =
                        RGB_PIXEL(MAX_INTENSITY, MAX_INTENSITY, MAX_INTENSITY);

            } else if (Value > 5 + CurrentIndicator) {
                KeMatrix[YPosition][TETRIS_INDICATORS_X + CurrentIndicator] =
                        RGB_PIXEL(MAX_INTENSITY, 0x0, MAX_INTENSITY);

            } else if (Value > CurrentIndicator) {
                KeMatrix[YPosition][TETRIS_INDICATORS_X + CurrentIndicator] =
                        RGB_PIXEL(0x0, 0x0, MAX_INTENSITY);

            } else {
                KeMatrix[YPosition][TETRIS_INDICATORS_X + CurrentIndicator] =
                        RGB_PIXEL(0x0, 0x0, 0x0);
            }
        }
    }

    return;
}
