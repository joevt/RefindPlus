/*
 * BootMaster/menu.c
 * Menu functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Modifications copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "screenmgt.h"
#include "lib.h"
#include "leaks.h"
#include "menu.h"
#include "config.h"
#include "libeg.h"
#include "libegint.h"
#include "line_edit.h"
#include "mystrings.h"
#include "icns.h"
#include "scan.h"
#include "../include/refit_call_wrapper.h"

#include "../include/egemb_back_selected_small.h"
#include "../include/egemb_back_selected_big.h"
#include "../include/egemb_arrow_left.h"
#include "../include/egemb_arrow_right.h"

// other menu definitions

#define MENU_FUNCTION_INIT            (0)
#define MENU_FUNCTION_CLEANUP         (1)
#define MENU_FUNCTION_PAINT_ALL       (2)
#define MENU_FUNCTION_PAINT_SELECTION (3)
#define MENU_FUNCTION_PAINT_TIMEOUT   (4)
#define MENU_FUNCTION_PAINT_HINTS     (5)

// typedef VOID (*MENU_STYLE_FUNC)(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText);

static CHAR16 ArrowUp[2]   = { ARROW_UP, 0 };
static CHAR16 ArrowDown[2] = { ARROW_DOWN, 0 };
static UINTN TileSizes[2]  = { 144, 64 };

// Text and icon spacing constants....
#define TEXT_YMARGIN (2)
#define TITLEICON_SPACING (16)

#define TILE_XSPACING (8)
#define TILE_YSPACING (16)

// Alignment values for PaintIcon()
#define ALIGN_RIGHT 1
#define ALIGN_LEFT  0

static EG_IMAGE *SelectionImages[2]      = { NULL, NULL };
static EG_PIXEL SelectionBackgroundPixel = { 0xff, 0xff, 0xff, 0 };

EFI_EVENT* WaitList  = NULL;
UINTN WaitListLength = 0;

// Pointer variables
BOOLEAN PointerEnabled = FALSE;
BOOLEAN PointerActive  = FALSE;
BOOLEAN DrawSelection  = TRUE;

extern EFI_GUID          RefindPlusGuid;
extern REFIT_MENU_ENTRY  MenuEntryReturn;

static REFIT_MENU_ENTRY  MenuEntryYes  = { {L"Yes", TRUE}, TAG_RETURN, 1, 0, 0, {NULL, FALSE}, {NULL, FALSE}, NULL };
static REFIT_MENU_ENTRY  MenuEntryNo   = { {L"No" , TRUE}, TAG_RETURN, 1, 0, 0, {NULL, FALSE}, {NULL, FALSE}, NULL };

#if REFIT_DEBUG > 0
STATIC UINTN MinAllocation = 0;
#endif

//
// Graphics helper functions
//

static
VOID InitSelection (
    VOID
) {
    EG_IMAGE  *TempSmallImage   = NULL;
    EG_IMAGE  *TempBigImage     = NULL;
    BOOLEAN   LoadedSmallImage  = FALSE;
    BOOLEAN   TaintFree         = TRUE;

    if (!AllowGraphicsMode || (SelectionImages[0] != NULL)) {
        return;
    }

    // load small selection image
    if (GlobalConfig.SelectionSmallFileName != NULL) {
        TempSmallImage = egLoadImage (SelfDir, GlobalConfig.SelectionSmallFileName, TRUE);
    }
    if (TempSmallImage == NULL) {
        TempSmallImage = egPrepareEmbeddedImage (&egemb_back_selected_small, TRUE);
    }
    else {
        LoadedSmallImage = TRUE;
    }

    if ((TempSmallImage->Width != TileSizes[1]) || (TempSmallImage->Height != TileSizes[1])) {
        SelectionImages[1] = egScaleImage (TempSmallImage, TileSizes[1], TileSizes[1]);
    }
    else {
        SelectionImages[1] = egCopyImage (TempSmallImage);
    }
    LEAKABLEONEIMAGE(SelectionImages[1], "SelectionImages 1");
    
    // load big selection image
    if (GlobalConfig.SelectionBigFileName != NULL) {
        TempBigImage = egLoadImage (SelfDir, GlobalConfig.SelectionBigFileName, TRUE);
    }
    if (TempBigImage == NULL) {
        if (TempSmallImage->Width > 128 || TempSmallImage->Height > 128) {
            TaintFree = FALSE;
        }
        if (TaintFree && LoadedSmallImage) {
           // calculate big selection image from small one
           TempBigImage = egCopyImage (TempSmallImage);
        }
        else {
           TempBigImage = egPrepareEmbeddedImage (&egemb_back_selected_big, TRUE);
        }
    }

    if ((TempBigImage->Width != TileSizes[0]) || (TempBigImage->Height != TileSizes[0])) {
        SelectionImages[0] = egScaleImage (TempBigImage, TileSizes[0], TileSizes[0]);
    }
    else {
        SelectionImages[0] = egCopyImage (TempBigImage);
    }
    LEAKABLEONEIMAGE(SelectionImages[0], "SelectionImages 0");
    
    if (TempSmallImage) {
        egFreeImage (TempSmallImage);
    }
    if (TempBigImage) {
        egFreeImage (TempBigImage);
    }
} // VOID InitSelection()

//
// Scrolling functions
//

static
VOID InitScroll (
    OUT SCROLL_STATE *State,
    IN UINTN ItemCount,
    IN UINTN VisibleSpace
) {
    State->PreviousSelection = State->CurrentSelection = 0;
    State->MaxIndex = (INTN)ItemCount - 1;
    State->FirstVisible = 0;
    if (AllowGraphicsMode) {
        State->MaxVisible = ScreenW / (TileSizes[0] + TILE_XSPACING) - 1;
    }
    else {
        State->MaxVisible = ConHeight - 4;
    }
    if ((VisibleSpace > 0) && (VisibleSpace < State->MaxVisible)) {
        State->MaxVisible = (INTN)VisibleSpace;
    }
    State->PaintAll        = TRUE;
    State->PaintSelection  = FALSE;
    State->LastVisible     = State->FirstVisible + State->MaxVisible - 1;
}

// Adjust variables relating to the scrolling of tags, for when a selected icon isn't
// visible given the current scrolling condition....
static
VOID AdjustScrollState (
    IN SCROLL_STATE *State
) {
    // Scroll forward
    if (State->CurrentSelection > State->LastVisible) {
        State->LastVisible   = State->CurrentSelection;
        State->FirstVisible  = 1 + State->CurrentSelection - State->MaxVisible;
        if (State->FirstVisible < 0) {
            // should not happen, but just in case...
            State->FirstVisible = 0;
        }
        State->PaintAll = TRUE;
    }

    // Scroll backward
    if (State->CurrentSelection < State->FirstVisible) {
        State->FirstVisible  = State->CurrentSelection;
        State->LastVisible   = State->CurrentSelection + State->MaxVisible - 1;
        State->PaintAll      = TRUE;
    }
} // static VOID AdjustScrollState

static
VOID UpdateScroll (
    IN OUT SCROLL_STATE  *State,
    IN UINTN             Movement
) {
    State->PreviousSelection = State->CurrentSelection;

    switch (Movement) {
        case SCROLL_LINE_LEFT:
            if (State->CurrentSelection > 0) {
                State->CurrentSelection --;
            }
            break;

        case SCROLL_LINE_RIGHT:
            if (State->CurrentSelection < State->MaxIndex) {
                State->CurrentSelection ++;
            }
            break;

        case SCROLL_LINE_UP:
            if (State->ScrollMode == SCROLL_MODE_ICONS) {
                if (State->CurrentSelection >= State->InitialRow1) {
                    if (State->MaxIndex > State->InitialRow1) {
                        // avoid division by 0!
                        State->CurrentSelection = State->FirstVisible +
                            (State->LastVisible - State->FirstVisible) *
                            (State->CurrentSelection - State->InitialRow1) /
                            (State->MaxIndex - State->InitialRow1);
                    }
                    else {
                        State->CurrentSelection = State->FirstVisible;
                    } // if/else
                } // if in second row
            }
            else {
                if (State->CurrentSelection > 0) {
                    State->CurrentSelection--;
                }
            } // if/else
            break;

        case SCROLL_LINE_DOWN:
            if (State->ScrollMode == SCROLL_MODE_ICONS) {
                if (State->CurrentSelection <= State->FinalRow0) {
                    if (State->LastVisible > State->FirstVisible) {
                        // avoid division by 0!
                        State->CurrentSelection = State->InitialRow1 +
                            (State->MaxIndex - State->InitialRow1) *
                            (State->CurrentSelection - State->FirstVisible) /
                            (State->LastVisible - State->FirstVisible);
                    }
                    else {
                        State->CurrentSelection = State->InitialRow1;
                    } // if/else
                } // if in first row
            }
            else {
                if (State->CurrentSelection < State->MaxIndex)
                    State->CurrentSelection++;
            } // if/else
            break;

        case SCROLL_PAGE_UP:
            if (State->CurrentSelection <= State->FinalRow0) {
                State->CurrentSelection -= State->MaxVisible;
            }
            else if (State->CurrentSelection == State->InitialRow1) {
                State->CurrentSelection = State->FinalRow0;
            }
            else {
                State->CurrentSelection = State->InitialRow1;
            }
            if (State->CurrentSelection < 0) {
                State->CurrentSelection = 0;
            }
            break;

        case SCROLL_FIRST:
            if (State->CurrentSelection > 0) {
                State->PaintAll = TRUE;
                State->CurrentSelection = 0;
            }
            break;

        case SCROLL_PAGE_DOWN:
            if (State->CurrentSelection < State->FinalRow0) {
                State->CurrentSelection += State->MaxVisible;
                if (State->CurrentSelection > State->FinalRow0) {
                    State->CurrentSelection = State->FinalRow0;
                }
            }
            else if (State->CurrentSelection == State->FinalRow0) {
                State->CurrentSelection++;
            }
            else {
                State->CurrentSelection = State->MaxIndex;
            }
            if (State->CurrentSelection > State->MaxIndex) {
                State->CurrentSelection = State->MaxIndex;
            }
            break;

        case SCROLL_LAST:
            if (State->CurrentSelection < State->MaxIndex) {
                State->PaintAll = TRUE;
                State->CurrentSelection = State->MaxIndex;
            }
            break;

        case SCROLL_NONE:
            break;
    } // switch()

    if (State->ScrollMode == SCROLL_MODE_TEXT) {
        AdjustScrollState (State);
    }

    if (!State->PaintAll && State->CurrentSelection != State->PreviousSelection) {
        State->PaintSelection = TRUE;
    }
    State->LastVisible = State->FirstVisible + State->MaxVisible - 1;
} // static VOID UpdateScroll()


//
// menu helper functions
//

VOID AddMenuInfoLine (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16 *InfoLine,
    IN BOOLEAN Cached
) {
    LOG(4, LOG_LINE_NORMAL, L"Adding Menu Info Line: '%s'", InfoLine);
    if (!Cached) { LOGPOOL (InfoLine); }
    PoolStr Str;
    Str.Str = InfoLine;
    Str.Cached = Cached;
    AddListElementSized ((VOID **) &(Screen->InfoLines), &(Screen->InfoLineCount), &Str, sizeof(Str));
}

VOID
AddMenuInfoLinePool (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16 *InfoLine
) {
    AddMenuInfoLine (Screen, InfoLine, FALSE);
}

VOID
AddMenuInfoLineCached (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16 *InfoLine
) {
    AddMenuInfoLine (Screen, InfoLine, TRUE);
}

VOID AddMenuEntry (
    IN REFIT_MENU_SCREEN *Screen,
    IN REFIT_MENU_ENTRY  *Entry
) {
    MsgLog ("[ AddMenuEntry Screen:%p '%s' Entry:%p '%s'\n",
        Screen, Screen && GetPoolStr (&Screen->Title) ? GetPoolStr (&Screen->Title) : L"NULL",
        Entry, Entry && GetPoolStr (&Entry->Title ) ? GetPoolStr (&Entry->Title ) : L"NULL"
    );
    if (!GetPoolStr (&Screen->Title)) {
        DumpCallStack (NULL, FALSE);
    }
    LOGPOOL (Entry);
    AddListElement ((VOID ***) &(Screen->Entries), &(Screen->EntryCount), Entry);
    MsgLog ("] AddMenuEntry\n");
}

VOID
AddMenuEntryCopy (
    IN REFIT_MENU_SCREEN *Screen,
    IN REFIT_MENU_ENTRY  *Entry
) {
    AddMenuEntry (Screen, CopyMenuEntry (Entry));
}

static
INTN FindMenuShortcutEntry (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16 *Defaults
) {
    UINTN i, j = 0, ShortcutLength;
    CHAR16 *Shortcut;

    while ((Shortcut = FindCommaDelimited (Defaults, j)) != NULL) {
        ShortcutLength = StrLen (Shortcut);
        if (ShortcutLength == 1) {
            if (Shortcut[0] >= 'a' && Shortcut[0] <= 'z') {
                Shortcut[0] -= ('a' - 'A');
            }
            if (Shortcut[0]) {
                for (i = 0; i < Screen->EntryCount; i++) {
                    if (Screen->Entries[i]->ShortcutDigit == Shortcut[0] ||
                        Screen->Entries[i]->ShortcutLetter == Shortcut[0]
                    ) {
                        MyFreePool (&Shortcut);
                        return i;
                    } // if
                } // for
            } // if
        }
        else if (ShortcutLength > 1) {
            for (i = 0; i < Screen->EntryCount; i++) {
                if (StriSubCmp (Shortcut, GetPoolStr (&Screen->Entries[i]->Title))) {
                    MyFreePool (&Shortcut);
                    return i;
                } // if
            } // for
        }
        MyFreePool (&Shortcut);
        j++;
    } // while()
    return -1;
} // static INTN FindMenuShortcutEntry()

// Identify the end of row 0 and the beginning of row 1; store the results in the
// appropriate fields in State. Also reduce MaxVisible if that value is greater
// than the total number of row-0 tags and if we're in an icon-based screen
static
VOID IdentifyRows (
    IN SCROLL_STATE *State,
    IN REFIT_MENU_SCREEN *Screen
) {
    UINTN i;

    State->FinalRow0 = 0;
    State->InitialRow1 = State->MaxIndex;
    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0) {
            State->FinalRow0 = i;
        }
        else if ((Screen->Entries[i]->Row == 1) && (State->InitialRow1 > i)) {
            State->InitialRow1 = i;
        } // if/else
    } // for
    if ((State->ScrollMode == SCROLL_MODE_ICONS) &&
        (State->MaxVisible > (State->FinalRow0 + 1))
    ) {
        State->MaxVisible = State->FinalRow0 + 1;
    }
} // static VOID IdentifyRows()

// Blank the screen, wait for a keypress or pointer event, and restore banner/background.
// Screen may still require redrawing of text and icons on return.
// TODO: Support more sophisticated screen savers, such as power-saving
// mode and dynamic images.
static
VOID SaveScreen (
    VOID
) {
    UINTN retval;
    UINTN  ColourIndex;
    UINT64 TimeWait;
    UINT64 BaseTimeWait = 1875;

    LOG2(3, LOG_LINE_NORMAL, L"INFO: ", L" ...", L"Threshold Exceeded");
    LOG2(2, LOG_LINE_THIN_SEP, L"", L"\n", L"Start Screensaver");

    EG_PIXEL OUR_COLOUR;
    EG_PIXEL COLOUR_01 = { 0, 51, 51, 0 };
    EG_PIXEL COLOUR_02 = { 0, 102, 102, 0 };
    EG_PIXEL COLOUR_03 = { 0, 153, 153, 0 };
    EG_PIXEL COLOUR_04 = { 0, 204, 204, 0 };
    EG_PIXEL COLOUR_05 = { 0, 255, 255, 0 };
    EG_PIXEL COLOUR_06 = { 51, 0, 204, 0 };
    EG_PIXEL COLOUR_07 = { 51, 51, 153, 0 };
    EG_PIXEL COLOUR_08 = { 51, 102, 102, 0 };
    EG_PIXEL COLOUR_09 = { 51, 153, 51, 0 };
    EG_PIXEL COLOUR_10 = { 51, 204, 0, 0 };
    EG_PIXEL COLOUR_11 = { 51, 255, 51, 0 };
    EG_PIXEL COLOUR_12 = { 102, 0, 102, 0 };
    EG_PIXEL COLOUR_13 = { 102, 51, 153, 0 };
    EG_PIXEL COLOUR_14 = { 102, 102, 204, 0 };
    EG_PIXEL COLOUR_15 = { 102, 153, 255, 0 };
    EG_PIXEL COLOUR_16 = { 102, 204, 204, 0 };
    EG_PIXEL COLOUR_17 = { 102, 255, 153, 0 };
    EG_PIXEL COLOUR_18 = { 153, 0, 102, 0 };
    EG_PIXEL COLOUR_19 = { 153, 51, 51, 0 };
    EG_PIXEL COLOUR_20 = { 153, 102, 0, 0 };
    EG_PIXEL COLOUR_21 = { 153, 153, 51, 0 };
    EG_PIXEL COLOUR_22 = { 153, 204, 102, 0 };
    EG_PIXEL COLOUR_23 = { 153, 255, 153, 0 };
    EG_PIXEL COLOUR_24 = { 204, 0, 204, 0 };
    EG_PIXEL COLOUR_25 = { 204, 51, 255, 0 };
    EG_PIXEL COLOUR_26 = { 204, 102, 204, 0 };
    EG_PIXEL COLOUR_27 = { 204, 153, 153, 0 };
    EG_PIXEL COLOUR_28 = { 204, 204, 102, 0 };
    EG_PIXEL COLOUR_29 = { 204, 255, 51, 0 };
    EG_PIXEL COLOUR_30 = { 255, 0, 0, 0 };

    // Start with COLOUR_01
    ColourIndex = 0;

    // Start with BaseTimeWait
    TimeWait = BaseTimeWait;
    for (;;) {
        ColourIndex = ColourIndex + 1;

        if (ColourIndex < 1 || ColourIndex > 30) {
            ColourIndex = 1;
            TimeWait    = TimeWait * 2;

            if (TimeWait > 120000) {
                // Reset TimeWait if greater than 2 minutes
                TimeWait = BaseTimeWait;

                LOG(4, LOG_LINE_NORMAL, L"Reset Timeout");
            }
            else {
                LOG(4, LOG_LINE_NORMAL, L"Extend Timeout");
            }
        }

        switch (ColourIndex) {
            case 1:  OUR_COLOUR = COLOUR_01; break;
            case 2:  OUR_COLOUR = COLOUR_02; break;
            case 3:  OUR_COLOUR = COLOUR_03; break;
            case 4:  OUR_COLOUR = COLOUR_04; break;
            case 5:  OUR_COLOUR = COLOUR_05; break;
            case 6:  OUR_COLOUR = COLOUR_06; break;
            case 7:  OUR_COLOUR = COLOUR_07; break;
            case 8:  OUR_COLOUR = COLOUR_08; break;
            case 9:  OUR_COLOUR = COLOUR_09; break;
            case 10: OUR_COLOUR = COLOUR_10; break;
            case 11: OUR_COLOUR = COLOUR_11; break;
            case 12: OUR_COLOUR = COLOUR_12; break;
            case 13: OUR_COLOUR = COLOUR_13; break;
            case 14: OUR_COLOUR = COLOUR_14; break;
            case 15: OUR_COLOUR = COLOUR_15; break;
            case 16: OUR_COLOUR = COLOUR_16; break;
            case 17: OUR_COLOUR = COLOUR_17; break;
            case 18: OUR_COLOUR = COLOUR_18; break;
            case 19: OUR_COLOUR = COLOUR_19; break;
            case 20: OUR_COLOUR = COLOUR_20; break;
            case 21: OUR_COLOUR = COLOUR_21; break;
            case 22: OUR_COLOUR = COLOUR_22; break;
            case 23: OUR_COLOUR = COLOUR_23; break;
            case 24: OUR_COLOUR = COLOUR_24; break;
            case 25: OUR_COLOUR = COLOUR_25; break;
            case 26: OUR_COLOUR = COLOUR_26; break;
            case 27: OUR_COLOUR = COLOUR_27; break;
            case 28: OUR_COLOUR = COLOUR_28; break;
            case 29: OUR_COLOUR = COLOUR_29; break;
            default: OUR_COLOUR = COLOUR_30; break;
        }

        egClearScreen (&OUR_COLOUR);
        retval = WaitForInput (TimeWait);
        if (retval == INPUT_KEY || retval == INPUT_TIMER_ERROR) {
            break;
        }
    } // for ;;

    LOG2(3, LOG_LINE_NORMAL, L"      ", L" ...", L"Detected Keypress");
    LOG2(2, LOG_THREE_STAR_END, L"", L"\n\n", L"Ending Screensaver");

    if (AllowGraphicsMode) {
        SwitchToGraphicsAndClear (TRUE);
    }
    ReadAllKeyStrokes();
} // VOID SaveScreen()

//
// generic menu function
//
UINTN RunGenericMenu (
    IN REFIT_MENU_SCREEN  *Screen,
    IN MENU_STYLE_FUNC     StyleFunc,
    IN OUT INTN           *DefaultEntryIndex,
    OUT REFIT_MENU_ENTRY **ChosenEntry
) {
    EFI_STATUS     Status;
    EFI_STATUS     PointerStatus      = EFI_NOT_READY;
    BOOLEAN        HaveTimeout        = FALSE;
    BOOLEAN        WaitForRelease     = FALSE;
    UINTN          TimeoutCountdown   = 0;
    INTN           TimeSinceKeystroke = 0;
    INTN           PreviousTime       = -1;
    INTN           CurrentTime;
    INTN           ShortcutEntry;
    UINTN          MenuExit;
    UINTN          Item;
    CHAR16        *TimeoutMessage;
    CHAR16         KeyAsString[2];
    SCROLL_STATE   State;
    EFI_INPUT_KEY  key;

    LOG(2, LOG_LINE_NORMAL, L"[ RunGenericMenu '%s'", GetPoolStr (&Screen->Title));

    if (Screen->TimeoutSeconds > 0) {
        HaveTimeout      = TRUE;
        TimeoutCountdown = Screen->TimeoutSeconds * 10;
    }
    MenuExit = 0;

    StyleFunc (Screen, &State, MENU_FUNCTION_INIT, NULL);
    IdentifyRows (&State, Screen);
    // override the starting selection with the default index, if any
    if (*DefaultEntryIndex >= 0 && *DefaultEntryIndex <= State.MaxIndex) {
        State.CurrentSelection = *DefaultEntryIndex;
        if (GlobalConfig.ScreensaverTime != -1) {
            UpdateScroll (&State, SCROLL_NONE);
        }
    }

    if (Screen->TimeoutSeconds == -1) {
        Status = refit_call2_wrapper(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            MenuExit = MENU_EXIT_TIMEOUT;
        }
        else {
            KeyAsString[0] = key.UnicodeChar;
            KeyAsString[1] = 0;
            ShortcutEntry  = FindMenuShortcutEntry (Screen, KeyAsString);
            if (ShortcutEntry >= 0) {
                State.CurrentSelection = ShortcutEntry;
                MenuExit = MENU_EXIT_ENTER;
            }
            else {
                WaitForRelease = TRUE;
                HaveTimeout    = FALSE;
            }
        }
    }

    if (GlobalConfig.ScreensaverTime != -1) {
        State.PaintAll = TRUE;
    }

    while (!MenuExit) {
        // update the screen
        pdClear();
        if (State.PaintAll && (GlobalConfig.ScreensaverTime != -1)) {
            StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_ALL, NULL);
            State.PaintAll = FALSE;
        }
        else if (State.PaintSelection) {
            StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_SELECTION, NULL);
            State.PaintSelection = FALSE;
        }
        pdDraw();

        if (WaitForRelease) {
            Status = refit_call2_wrapper(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
            if (Status == EFI_SUCCESS) {
                // reset, because otherwise the buffer gets queued with keystrokes
                refit_call2_wrapper(gST->ConIn->Reset, gST->ConIn, FALSE);
                refit_call1_wrapper(gBS->Stall, 100000);
            }
            else {
                WaitForRelease = FALSE;
                refit_call2_wrapper(gST->ConIn->Reset, gST->ConIn, TRUE);
            }
            continue;
        }

        if (HaveTimeout) {
            CurrentTime = (TimeoutCountdown + 5) / 10;
            if (CurrentTime != PreviousTime) {
               TimeoutMessage = PoolPrint (
                   L"%s in %d seconds",
                   GetPoolStr (&Screen->TimeoutText),
                   CurrentTime
               );

               if (GlobalConfig.ScreensaverTime != -1) {
                   StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, TimeoutMessage);
               }
               MyFreePool (&TimeoutMessage);

               PreviousTime = CurrentTime;
            }
        }

        // read key press or pointer event (and wait for them if applicable)
        if (PointerEnabled) {
            PointerStatus = pdUpdateState();
        }
        Status = refit_call2_wrapper(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);

        if (Status == EFI_SUCCESS) {
            PointerActive      = FALSE;
            DrawSelection      = TRUE;
            TimeSinceKeystroke = 0;
        }
        else if (PointerStatus == EFI_SUCCESS) {
            if (StyleFunc != MainMenuStyle && pdGetState().Press) {
                // prevent user from getting stuck on submenus
                // (the only one currently reachable without a keyboard is the about screen)
                MenuExit = MENU_EXIT_ENTER;
                break;
            }

            PointerActive = TRUE;
            TimeSinceKeystroke = 0;
        }
        else {
            if (HaveTimeout && TimeoutCountdown == 0) {
                // timeout expired
                LOG(1, LOG_LINE_NORMAL, L"Menu timeout expired");

                MenuExit = MENU_EXIT_TIMEOUT;
                break;
            }
            else if (HaveTimeout || GlobalConfig.ScreensaverTime > 0) {
                UINTN ElapsCount = 1;
                UINTN Input      = WaitForInput (1000); // 1s Timeout

                if (Input == INPUT_KEY || Input == INPUT_POINTER) {
                    continue;
                }
                else if (Input == INPUT_TIMEOUT) {
                    ElapsCount = 10; // always counted as 1s to end of the timeout
                }

                TimeSinceKeystroke += ElapsCount;
                if (HaveTimeout) {
                    TimeoutCountdown = (TimeoutCountdown > ElapsCount)
                    ? TimeoutCountdown - ElapsCount
                    : 0;
                }
                else if (GlobalConfig.ScreensaverTime > 0 &&
                    TimeSinceKeystroke > (GlobalConfig.ScreensaverTime * 10)
                ) {
                    SaveScreen();
                    State.PaintAll     = TRUE;
                    TimeSinceKeystroke = 0;
                } // if
            }
            else {
                WaitForInput (0);
            }
            continue;
        } // if/else !read keystroke

        if (HaveTimeout) {
            // the user pressed a key, cancel the timeout
            StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, L"");
            HaveTimeout = FALSE;
            if (GlobalConfig.ScreensaverTime == -1) { // cancel start-with-blank-screen coding
               GlobalConfig.ScreensaverTime = 0;
               if (!GlobalConfig.TextOnly) {
                   BltClearScreen (TRUE);
               }
            }
        }

        if (!PointerActive) {
            // react to key press
            LOG(3, LOG_LINE_NORMAL,
                L"Processing Keystroke: ScanCode = %d ... UnicodeChar = %d",
                key.ScanCode,
                key.UnicodeChar
            );
            
            switch (key.ScanCode) {
                case SCAN_UP:
                    UpdateScroll (&State, SCROLL_LINE_UP);
                    break;
                case SCAN_LEFT:
                    UpdateScroll (&State, SCROLL_LINE_LEFT);
                    break;
                case SCAN_DOWN:
                    UpdateScroll (&State, SCROLL_LINE_DOWN);
                    break;
                case SCAN_RIGHT:
                    UpdateScroll (&State, SCROLL_LINE_RIGHT);
                    break;
                case SCAN_HOME:
                    UpdateScroll (&State, SCROLL_FIRST);
                    break;
                case SCAN_END:
                    UpdateScroll (&State, SCROLL_LAST);
                    break;
                case SCAN_PAGE_UP:
                    UpdateScroll (&State, SCROLL_PAGE_UP);
                    break;
                case SCAN_PAGE_DOWN:
                    UpdateScroll (&State, SCROLL_PAGE_DOWN);
                    break;
                case SCAN_ESC:
                    MenuExit = MENU_EXIT_ESCAPE;
                    break;
                case SCAN_INSERT:
                case SCAN_F2:
                    MenuExit = MENU_EXIT_DETAILS;
                    break;
#if REFIT_DEBUG > 0
                case SCAN_F5:
                    MinAllocation = GetNextAllocationNum ();
                    break;
                case SCAN_F6:
                    DumpAllocations (MinAllocation, TRUE, 4);
                    break;
#endif
                case SCAN_DELETE:
                    MenuExit = MENU_EXIT_HIDE;
                    break;
                case SCAN_F10:
                    egScreenShot();
                    break;
                case SCAN_F12:
                    if (EjectMedia())
                        MenuExit = MENU_EXIT_ESCAPE;
                    break;
            } // switch()

            switch (key.UnicodeChar) {
                case CHAR_LINEFEED:
                case CHAR_CARRIAGE_RETURN:
                case ' ':
                    MenuExit = MENU_EXIT_ENTER;
                    break;
                case CHAR_BACKSPACE:
                    MenuExit = MENU_EXIT_ESCAPE;
                    break;
                case '+':
                case CHAR_TAB:
                    MenuExit = MENU_EXIT_DETAILS;
                    break;
                case '-':
                    MenuExit = MENU_EXIT_HIDE;
                    break;
                default:
                    KeyAsString[0] = key.UnicodeChar;
                    KeyAsString[1] = 0;
                    ShortcutEntry = FindMenuShortcutEntry (Screen, KeyAsString);
                    if (ShortcutEntry >= 0) {
                        State.CurrentSelection = ShortcutEntry;
                        MenuExit = MENU_EXIT_ENTER;
                    }
                    break;
            } // switch()
        }
        else {
            //react to pointer event
            LOG(4, LOG_LINE_NORMAL, L"Processing Pointer Event");

            if (StyleFunc != MainMenuStyle) {
                // nothing to find on submenus
                continue;
            }

            State.PreviousSelection = State.CurrentSelection;
            POINTER_STATE PointerState = pdGetState();
            Item = FindMainMenuItem (Screen, &State, PointerState.X, PointerState.Y);
            switch (Item) {
                case POINTER_NO_ITEM:
                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection = TRUE;
                    }
                    break;
                case POINTER_LEFT_ARROW:
                    if (PointerState.Press) {
                        UpdateScroll (&State, SCROLL_PAGE_UP);
                    }
                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection = TRUE;
                    }
                    break;
                case POINTER_RIGHT_ARROW:
                    if (PointerState.Press) {
                        UpdateScroll (&State, SCROLL_PAGE_DOWN);
                    }
                    if (DrawSelection) {
                        DrawSelection        = FALSE;
                        State.PaintSelection = TRUE;
                    }
                    break;
                default:
                    if (!DrawSelection || Item != State.CurrentSelection) {
                        DrawSelection          = TRUE;
                        State.PaintSelection   = TRUE;
                        State.CurrentSelection = Item;
                    }
                    if (PointerState.Press) {
                        MenuExit = MENU_EXIT_ENTER;
                    }
                    break;
            } // switch()
        } // if/else
    } // while()

    pdClear();
    StyleFunc (Screen, &State, MENU_FUNCTION_CLEANUP, NULL);

    if (ChosenEntry) {
        *ChosenEntry = Screen->Entries[State.CurrentSelection];
    }
    *DefaultEntryIndex = State.CurrentSelection;

    LOG(2, LOG_LINE_NORMAL, L"] RunGenericMenu ...%d", MenuExit);

    return MenuExit;
} // UINTN RunGenericMenu()

//
// text-mode generic style
//

// Show information lines in text mode.
static
VOID ShowTextInfoLines (
    IN REFIT_MENU_SCREEN *Screen
) {
    INTN i;

    BeginTextScreen (GetPoolStr (&Screen->Title));
    if (Screen->InfoLineCount > 0) {
        refit_call2_wrapper(
            gST->ConOut->SetAttribute,
            gST->ConOut,
            ATTR_BASIC
        );
        for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
            refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 3, 4 + i);
            refit_call2_wrapper(gST->ConOut->OutputString, gST->ConOut, GetPoolStr_PS_ (&Screen->InfoLines[i]));
        }
    }
} // VOID ShowTextInfoLines()

// Do most of the work for text-based menus....
VOID TextMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN             Function,
    IN CHAR16            *ParamText
) {
    INTN  i;
    UINTN MenuWidth;
    UINTN ItemWidth;
    UINTN MenuHeight;

    static UINTN    MenuPosY;
    static CHAR16 **DisplayStrings;

    State->ScrollMode = SCROLL_MODE_TEXT;

    switch (Function) {
        case MENU_FUNCTION_INIT:
            // vertical layout
            MenuPosY = 4;
            if (Screen->InfoLineCount > 0) {
                MenuPosY += Screen->InfoLineCount + 1;
            }

            MenuHeight = ConHeight - MenuPosY - 3;
            if (Screen->TimeoutSeconds > 0) {
                MenuHeight -= 2;
            }
            InitScroll (State, Screen->EntryCount, MenuHeight);

            // determine width of the menu
            MenuWidth = 20;  // minimum

            for (i = 0; i <= State->MaxIndex; i++) {
                ItemWidth = StrLen (GetPoolStr (&Screen->Entries[i]->Title));
                if (MenuWidth < ItemWidth) {
                    MenuWidth = ItemWidth;
                }
            }

            MenuWidth += 2;
            if (MenuWidth > ConWidth - 3) {
                MenuWidth = ConWidth - 3;
            }

            // prepare strings for display
            DisplayStrings = AllocatePool (sizeof (CHAR16 *) * Screen->EntryCount);
            for (i = 0; i <= State->MaxIndex; i++) {
                // Note: Theoretically, SPrint() is a cleaner way to do this; but the
                // description of the StrSize parameter to SPrint implies it's measured
                // in characters, but in practice both TianoCore and GNU-EFI seem to
                // use bytes instead, resulting in truncated displays. I could just
                // double the size of the StrSize parameter, but that seems unsafe in
                // case a future library change starts treating this as characters, so
                // I'm doing it the hard way in this instance.
                // TODO: Review the above and possibly change other uses of SPrint()
                DisplayStrings[i] = AllocateZeroPool (2 * sizeof (CHAR16));
                DisplayStrings[i][0] = L' ';
                MergeStrings (&DisplayStrings[i], GetPoolStr (&Screen->Entries[i]->Title), 0);
                if (StrLen (DisplayStrings[i]) > MenuWidth) {
                    DisplayStrings[i][MenuWidth - 1] = 0;
                }
                // TODO: use more elaborate techniques for shortening too long strings (ellipses in the middle)
                // TODO: account for double-width characters
            } // for

            break;

        case MENU_FUNCTION_CLEANUP:
            // release temporary memory
            for (i = 0; i <= State->MaxIndex; i++) {
                MyFreePool (&DisplayStrings[i]);
            }
            MyFreePool (&DisplayStrings);
            break;

        case MENU_FUNCTION_PAINT_ALL:
            // paint the whole screen (initially and after scrolling)

            ShowTextInfoLines (Screen);
            for (i = 0; i <= State->MaxIndex; i++) {
                if (i >= State->FirstVisible && i <= State->LastVisible) {
                    refit_call3_wrapper(
                        gST->ConOut->SetCursorPosition,
                        gST->ConOut,
                        2,
                        MenuPosY + (i - State->FirstVisible)
                    );
                    if (i == State->CurrentSelection) {
                        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_CHOICE_CURRENT);
                    }
                    else if (DisplayStrings[i]) {
                        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_CHOICE_BASIC);
                        refit_call2_wrapper(gST->ConOut->OutputString, gST->ConOut, DisplayStrings[i]);
                    }
                }
            }

            // scrolling indicators
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_SCROLLARROW);
            refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 0, MenuPosY);

            if (State->FirstVisible > 0) {
                gST->ConOut->OutputString (gST->ConOut, ArrowUp);
            }
            else {
                gST->ConOut->OutputString (gST->ConOut, L" ");
            }
            gST->ConOut->SetCursorPosition (gST->ConOut, 0, MenuPosY + State->MaxVisible);
            if (State->LastVisible < State->MaxIndex) {
                refit_call2_wrapper(gST->ConOut->OutputString, gST->ConOut, ArrowDown);
            }
            else {
                refit_call2_wrapper(gST->ConOut->OutputString, gST->ConOut, L" ");
            }
            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
               if (GetPoolStr (&Screen->Hint1) != NULL) {
                   refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 2);
                   refit_call2_wrapper(gST->ConOut->OutputString, gST->ConOut, GetPoolStr (&Screen->Hint1));
               }
               if (GetPoolStr (&Screen->Hint2) != NULL) {
                   refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 1);
                   refit_call2_wrapper(gST->ConOut->OutputString, gST->ConOut, GetPoolStr (&Screen->Hint2));
               }
            }
            break;

        case MENU_FUNCTION_PAINT_SELECTION:
            // redraw selection cursor
            refit_call3_wrapper(
                gST->ConOut->SetCursorPosition,
                gST->ConOut, 2,
                MenuPosY + (State->PreviousSelection - State->FirstVisible)
            );
            refit_call2_wrapper(
                gST->ConOut->SetAttribute,
                gST->ConOut,
                ATTR_CHOICE_BASIC
            );
            if (DisplayStrings[State->PreviousSelection] != NULL) {
                refit_call2_wrapper(
                    gST->ConOut->OutputString,
                    gST->ConOut,
                    DisplayStrings[State->PreviousSelection]
                );
            }
            refit_call3_wrapper(
                gST->ConOut->SetCursorPosition,
                gST->ConOut, 2,
                MenuPosY + (State->CurrentSelection - State->FirstVisible)
            );
            refit_call2_wrapper(
                gST->ConOut->SetAttribute,
                gST->ConOut,
                ATTR_CHOICE_CURRENT
            );
            refit_call2_wrapper(
                gST->ConOut->OutputString,
                gST->ConOut,
                DisplayStrings[State->CurrentSelection]
            );
            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (ParamText[0] == 0) {
                // clear message
                refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
                refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 3);
                refit_call2_wrapper(gST->ConOut->OutputString, gST->ConOut, BlankLine + 1);
            }
            else {
                // paint or update message
                refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
                refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 3, ConHeight - 3);
                refit_call2_wrapper(gST->ConOut->OutputString, gST->ConOut, ParamText);
            }
            break;
    }
}

//
// graphical generic style
//

inline static
UINTN TextLineHeight (
    VOID
) {
    return egGetFontHeight() + TEXT_YMARGIN * 2;
} // UINTN TextLineHeight()

//
// Display a submenu
//

// Display text with a solid background (MenuBackgroundPixel or SelectionBackgroundPixel).
// Indents text by one character and placed TEXT_YMARGIN pixels down from the
// specified XPos and YPos locations.
static
VOID DrawText (
    IN CHAR16  *Text,
    IN BOOLEAN Selected,
    IN UINTN   FieldWidth,
    IN UINTN   XPos,
    IN UINTN   YPos
) {
    EG_IMAGE *TextBuffer;
    EG_PIXEL Bg;

    TextBuffer = egCreateFilledImage (
        FieldWidth,
        TextLineHeight(),
        FALSE,
        &MenuBackgroundPixel
    );

    if (TextBuffer) {
        Bg = MenuBackgroundPixel;
        if (Selected) {
            // draw selection bar background
            egFillImageArea (
                TextBuffer,
                0, 0,
                FieldWidth,
                TextBuffer->Height,
                &SelectionBackgroundPixel
            );
            Bg = SelectionBackgroundPixel;
        }

        // render the text
        egRenderText (
            Text,
            TextBuffer,
            egGetFontCellWidth(),
            TEXT_YMARGIN,
            (Bg.r + Bg.g + Bg.b) / 3
        );
        egDrawImageWithTransparency (
            TextBuffer, NULL,
            XPos, YPos,
            TextBuffer->Width,
            TextBuffer->Height
        );
        egFreeImage (TextBuffer);
    }
} /* VOID DrawText() */

// Finds the average brightness of the input Image.
// NOTE: Passing an Image that covers the whole screen can strain the
// capacity of a UINTN on a 32-bit system with a very large display.
// Using UINT64 instead is unworkable, since the code won't compile
// on a 32-bit system. As the intended use for this function is to handle
// a single text string's background, this shouldn't be a problem, but it
// may need addressing if it's applied more broadly....
static
UINT8 AverageBrightness (
    EG_IMAGE *Image
) {
    UINTN i;
    UINTN Sum = 0;

    if ((Image != NULL) && ((Image->Width * Image->Height) != 0)) {
        for (i = 0; i < (Image->Width * Image->Height); i++) {
            Sum += (Image->PixelData[i].r + Image->PixelData[i].g + Image->PixelData[i].b);
        }
        Sum /= (Image->Width * Image->Height * 3);
    } // if

    return (UINT8) Sum;
} // UINT8 AverageBrightness()

// Display text against the screen's background image. Special case: If Text is NULL
// or 0-length, clear the line. Does NOT indent the text or reposition it relative
// to the specified XPos and YPos values.
static
VOID DrawTextWithTransparency (
    IN CHAR16 *Text,
    IN UINTN XPos,
    IN UINTN YPos
) {
    UINTN TextWidth;
    EG_IMAGE *TextBuffer = NULL;

    if (Text == NULL) {
        Text = L"";
    }

    egMeasureText (Text, &TextWidth, NULL);
    if (TextWidth == 0) {
       TextWidth = ScreenW;
       XPos      = 0;
    }

    TextBuffer = egCropImage (
        GlobalConfig.ScreenBackground,
        XPos, YPos,
        TextWidth,
        TextLineHeight()
    );

    if (TextBuffer == NULL) {
        return;
    }

    // render the text
    egRenderText (
        Text,
        TextBuffer,
        0, 0,
        AverageBrightness (TextBuffer)
    );
    egDrawImageWithTransparency (
        TextBuffer, NULL,
        XPos, YPos,
        TextBuffer->Width,
        TextBuffer->Height
    );
    egFreeImage (TextBuffer);
}

// Compute the size & position of the window that will hold a subscreen's information.
static
VOID ComputeSubScreenWindowSize (
    REFIT_MENU_SCREEN *Screen,
    SCROLL_STATE      *State,
    UINTN *XPos,
    UINTN *YPos,
    UINTN *Width,
    UINTN *Height,
    UINTN *LineWidth
) {
    UINTN i, ItemWidth, HintTop, BannerBottomEdge, TitleWidth;
    UINTN FontCellWidth  = egGetFontCellWidth();
    UINTN FontCellHeight = egGetFontHeight();

    *Width     = 20;
    *Height    = 5;
    TitleWidth = egComputeTextWidth (GetPoolStr (&Screen->Title));

    for (i = 0; i < Screen->InfoLineCount; i++) {
        ItemWidth = StrLen (GetPoolStr_PS_ (&Screen->InfoLines[i]));
        if (*Width < ItemWidth) {
            *Width = ItemWidth;
        }
        (*Height)++;
    }

    for (i = 0; i <= State->MaxIndex; i++) {
        ItemWidth = StrLen (GetPoolStr (&Screen->Entries[i]->Title));
        if (*Width < ItemWidth) {
            *Width = ItemWidth;
        }
        (*Height)++;
    }

    *Width = (*Width + 2) * FontCellWidth;
    *LineWidth = *Width;

    if (GetPoolImage (&Screen->TitleImage)) {
        *Width += (GetPoolImage (&Screen->TitleImage)->Width + TITLEICON_SPACING * 2 + FontCellWidth);
    }
    else {
        *Width += FontCellWidth;
    }

    if (*Width < TitleWidth) {
        *Width = TitleWidth + 2 * FontCellWidth;
    }

    // Keep it within the bounds of the screen, or 2/3 of the screen's width
    // for screens over 800 pixels wide
    if (*Width > ScreenW) {
        *Width = ScreenW;
    }

    *XPos = (ScreenW - *Width) / 2;

    // top of hint text
    HintTop  = ScreenH - (FontCellHeight * 3);
    *Height *= TextLineHeight();

    if (GetPoolImage (&Screen->TitleImage) &&
        (*Height < (GetPoolImage (&Screen->TitleImage)->Height + TextLineHeight() * 4))
    ) {
        *Height = GetPoolImage (&Screen->TitleImage)->Height + TextLineHeight() * 4;
    }

    if (GlobalConfig.BannerBottomEdge >= HintTop) {
        // probably a full-screen image; treat it as an empty banner
        BannerBottomEdge = 0;
    }
    else {
        BannerBottomEdge = GlobalConfig.BannerBottomEdge;
    }
    if (*Height > (HintTop - BannerBottomEdge - FontCellHeight * 2)) {
        BannerBottomEdge = 0;
    }
    if (*Height > (HintTop - BannerBottomEdge - FontCellHeight * 2)) {
        // TODO: Implement scrolling in text screen.
        *Height = (HintTop - BannerBottomEdge - FontCellHeight * 2);
    }

    *YPos = ((ScreenH - *Height) / 2);
    if (*YPos < BannerBottomEdge) {
        *YPos = BannerBottomEdge +
            FontCellHeight +
            (HintTop - BannerBottomEdge - *Height) / 2;
    }
} // VOID ComputeSubScreenWindowSize()

// Displays sub-menus
VOID GraphicsMenuStyle (
    IN REFIT_MENU_SCREEN  *Screen,
    IN SCROLL_STATE       *State,
    IN UINTN              Function,
    IN CHAR16             *ParamText
) {
    INTN i;
    UINTN ItemWidth;
    static UINTN LineWidth, MenuWidth, MenuHeight;
    static UINTN EntriesPosX, EntriesPosY;
    static UINTN TitlePosX, TimeoutPosY, CharWidth;
    EG_IMAGE *Window;
    EG_PIXEL *BackgroundPixel = &(GlobalConfig.ScreenBackground->PixelData[0]);

    CharWidth = egGetFontCellWidth();
    State->ScrollMode = SCROLL_MODE_TEXT;

    switch (Function) {
        case MENU_FUNCTION_INIT:
            InitScroll (State, Screen->EntryCount, 0);
            ComputeSubScreenWindowSize (
                Screen, State,
                &EntriesPosX, &EntriesPosY,
                &MenuWidth, &MenuHeight,
                &LineWidth
            );
            TimeoutPosY = EntriesPosY + (Screen->EntryCount + 1) * TextLineHeight();

            // initial painting
            SwitchToGraphicsAndClear (TRUE);

            Window = egCreateFilledImage (MenuWidth, MenuHeight, FALSE, BackgroundPixel);
            if (Window) {
                egDrawImage (Window, EntriesPosX, EntriesPosY);
                egFreeImage (Window);
            }

            ItemWidth = egComputeTextWidth (GetPoolStr (&Screen->Title));
            if (MenuWidth > ItemWidth) {
                TitlePosX = EntriesPosX + (MenuWidth - ItemWidth) / 2 - CharWidth;
            }
            else {
               TitlePosX = EntriesPosX;
               if (CharWidth > 0) {
                  i = MenuWidth / CharWidth - 2;
                  if (i > 0) {
                      CHAR16 *ScreenTitleNew = StrDuplicate(GetPoolStr(&Screen->Title));
                      ScreenTitleNew[i] = 0;
                      AssignPoolStr(&Screen->Title, ScreenTitleNew);
                  }
               } // if
            } // if/else
            break;

        case MENU_FUNCTION_CLEANUP:
            // nothing to do
            break;

        case MENU_FUNCTION_PAINT_ALL:
            ComputeSubScreenWindowSize (
                Screen, State,
                &EntriesPosX, &EntriesPosY,
                &MenuWidth, &MenuHeight,
                &LineWidth
            );
            DrawText (
                GetPoolStr (&Screen->Title),
                FALSE,
                (StrLen (GetPoolStr (&Screen->Title)) + 2) * CharWidth,
                TitlePosX,
                EntriesPosY += TextLineHeight()
            );
            if (GetPoolImage (&Screen->TitleImage)) {
                BltImageAlpha (
                    GetPoolImage (&Screen->TitleImage),
                    EntriesPosX + TITLEICON_SPACING,
                    EntriesPosY + TextLineHeight() * 2,
                    BackgroundPixel
                );
                EntriesPosX += (GetPoolImage (&Screen->TitleImage)->Width + TITLEICON_SPACING * 2);
            }

            EntriesPosY += (TextLineHeight() * 2);
            if (Screen->InfoLineCount > 0) {
                for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
                    DrawText (
                        GetPoolStr_PS_ (&Screen->InfoLines[i]),
                        FALSE, LineWidth,
                        EntriesPosX, EntriesPosY
                    );
                    EntriesPosY += TextLineHeight();
                }
                // also add a blank line
                EntriesPosY += TextLineHeight();
            }

            for (i = 0; i <= State->MaxIndex; i++) {
                DrawText (
                    GetPoolStr (&Screen->Entries[i]->Title),
                    (i == State->CurrentSelection),
                    LineWidth,
                    EntriesPosX,
                    EntriesPosY + i * TextLineHeight()
                );
            }

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
                if ((GetPoolStr (&Screen->Hint1) != NULL) && (StrLen (GetPoolStr (&Screen->Hint1)) > 0)) {
                    DrawTextWithTransparency (
                        GetPoolStr (&Screen->Hint1),
                        (ScreenW - egComputeTextWidth (GetPoolStr (&Screen->Hint1))) / 2,
                        ScreenH - (egGetFontHeight() * 3)
                    );
                }
                if ((GetPoolStr (&Screen->Hint2) != NULL) && (StrLen (GetPoolStr (&Screen->Hint2)) > 0)) {
                    DrawTextWithTransparency (
                        GetPoolStr (&Screen->Hint2),
                        (ScreenW - egComputeTextWidth (GetPoolStr (&Screen->Hint2))) / 2,
                        ScreenH - (egGetFontHeight() * 2)
                    );
                }
            } // if
            break;

        case MENU_FUNCTION_PAINT_SELECTION:
            // redraw selection cursor
            DrawText (
                GetPoolStr (&Screen->Entries[State->PreviousSelection]->Title),
                FALSE, LineWidth,
                EntriesPosX,
                EntriesPosY + State->PreviousSelection * TextLineHeight()
            );
            DrawText (
                GetPoolStr (&Screen->Entries[State->CurrentSelection]->Title),
                TRUE, LineWidth,
                EntriesPosX,
                EntriesPosY + State->CurrentSelection * TextLineHeight()
            );
            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            DrawText (ParamText, FALSE, LineWidth, EntriesPosX, TimeoutPosY);
            break;
    }
} // static VOID GraphicsMenuStyle()

//
// graphical main menu style
//

static
VOID DrawMainMenuEntry (
    REFIT_MENU_ENTRY *Entry,
    BOOLEAN selected,
    UINTN XPos,
    UINTN YPos
) {
    EG_IMAGE *Background;

    // if using pointer, don't draw selection image when not hovering
    if (selected && DrawSelection) {
        Background = egCropImage (
            GlobalConfig.ScreenBackground,
            XPos, YPos,
            SelectionImages[Entry->Row]->Width,
            SelectionImages[Entry->Row]->Height
        );

        if (Background) {
            egComposeImage (
                Background,
                SelectionImages[Entry->Row],
                0, 0
            );
            BltImageCompositeBadge (
                Background,
                GetPoolImage (&Entry->Image),
                GetPoolImage (&Entry->BadgeImage),
                XPos, YPos
            );
            egFreeImage (Background);
        } // if
    }
    else {
        // Image not selected; copy background
        egDrawImageWithTransparency (
            GetPoolImage (&Entry->Image),
            GetPoolImage (&Entry->BadgeImage),
            XPos, YPos,
            SelectionImages[Entry->Row]->Width,
            SelectionImages[Entry->Row]->Height
        );
    } // if/else
} // VOID DrawMainMenuEntry()

static
VOID PaintAll (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE *State,
    UINTN *itemPosX,
    UINTN row0PosY,
    UINTN row1PosY,
    UINTN textPosY
) {
    INTN i;

    if (Screen->Entries[State->CurrentSelection]->Row == 0) {
        AdjustScrollState (State);
    }

    for (i = State->FirstVisible; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0) {
            if (i <= State->LastVisible) {
                DrawMainMenuEntry (
                    Screen->Entries[i],
                    (i == State->CurrentSelection) ? TRUE : FALSE,
                    itemPosX[i - State->FirstVisible],
                    row0PosY
                );
            } // if
        }
        else {
            DrawMainMenuEntry (
                Screen->Entries[i],
                (i == State->CurrentSelection) ? TRUE : FALSE,
                itemPosX[i],
                row1PosY
            );
        }
    }

    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL) &&
        (!PointerActive || (PointerActive && DrawSelection))
    ) {
        DrawTextWithTransparency (L"", 0, textPosY);
        DrawTextWithTransparency (
            GetPoolStr (&Screen->Entries[State->CurrentSelection]->Title),
            (ScreenW - egComputeTextWidth (GetPoolStr (&Screen->Entries[State->CurrentSelection]->Title))) >> 1,
            textPosY
        );
    }
    else {
        DrawTextWithTransparency (L"", 0, textPosY);
    }

    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
        DrawTextWithTransparency (
            GetPoolStr (&Screen->Hint1),
            (ScreenW - egComputeTextWidth (GetPoolStr (&Screen->Hint1))) / 2,
            ScreenH - (egGetFontHeight() * 3)
        );
        DrawTextWithTransparency (
            GetPoolStr (&Screen->Hint2),
            (ScreenW - egComputeTextWidth (GetPoolStr (&Screen->Hint2))) / 2,
            ScreenH - (egGetFontHeight() * 2)
        );
    } // if
} // static VOID PaintAll()

// Move the selection to State->CurrentSelection, adjusting icon row if necessary...
static
VOID PaintSelection (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE *State,
    UINTN *itemPosX,
    UINTN row0PosY,
    UINTN row1PosY,
    UINTN textPosY
) {
    UINTN XSelectPrev, XSelectCur, YPosPrev, YPosCur;

    if (((State->CurrentSelection <= State->LastVisible) &&
        (State->CurrentSelection >= State->FirstVisible)) ||
        (State->CurrentSelection >= State->InitialRow1)
    ) {
        if (Screen->Entries[State->PreviousSelection]->Row == 0) {
            XSelectPrev = State->PreviousSelection - State->FirstVisible;
            YPosPrev = row0PosY;
        }
        else {
            XSelectPrev = State->PreviousSelection;
            YPosPrev = row1PosY;
        } // if/else

        if (Screen->Entries[State->CurrentSelection]->Row == 0) {
            XSelectCur = State->CurrentSelection - State->FirstVisible;
            YPosCur = row0PosY;
        }
        else {
            XSelectCur = State->CurrentSelection;
            YPosCur = row1PosY;
        } // if/else

        DrawMainMenuEntry (
            Screen->Entries[State->PreviousSelection],
            FALSE,
            itemPosX[XSelectPrev],
            YPosPrev
        );
        DrawMainMenuEntry (
            Screen->Entries[State->CurrentSelection],
            TRUE,
            itemPosX[XSelectCur],
            YPosCur
        );
        if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL) &&
            (!PointerActive || (PointerActive && DrawSelection))
        ) {
            DrawTextWithTransparency (L"", 0, textPosY);
            DrawTextWithTransparency (
                GetPoolStr (&Screen->Entries[State->CurrentSelection]->Title),
                (ScreenW - egComputeTextWidth (GetPoolStr (&Screen->Entries[State->CurrentSelection]->Title))) >> 1,
                textPosY
            );
        }
        else {
            DrawTextWithTransparency (L"", 0, textPosY);
        }
    }
    else {
        // Current selection not visible; must redraw the menu...
        MainMenuStyle (Screen, State, MENU_FUNCTION_PAINT_ALL, NULL);
    }
} // static VOID MoveSelection (VOID)

// Display a 48x48 icon at the specified location. Uses the image specified by
// ExternalFilename if it's available, or BuiltInImage if it's not. The
// Y position is specified as the center value, and so is adjusted by half
// the icon's height. The X position is set along the icon's left
// edge if Alignment == ALIGN_LEFT, and along the right edge if
// Alignment == ALIGN_RIGHT
static
VOID PaintIcon (
    IN EG_EMBEDDED_IMAGE *BuiltInIcon,
    IN CHAR16 *ExternalFilename,
    UINTN PosX,
    UINTN PosY,
    UINTN Alignment
) {
    EG_IMAGE *Icon = NULL;

    Icon = egFindIcon (ExternalFilename, GlobalConfig.IconSizes[ICON_SIZE_SMALL]);
    if (Icon == NULL) {
        Icon = egPrepareEmbeddedImage (BuiltInIcon, TRUE);
    }

    if (Icon != NULL) {
        if (Alignment == ALIGN_RIGHT) {
            PosX -= Icon->Width;
        }
        egDrawImageWithTransparency (
            Icon,
            NULL,
            PosX,
            PosY - (Icon->Height / 2),
            Icon->Width,
            Icon->Height
        );
        egFreeImage (Icon);
    }
} // static VOID()

UINTN ComputeRow0PosY (
    VOID
) {
    return ((ScreenH / 2) - TileSizes[0] / 2);
} // UINTN ComputeRow0PosY()

// Display (or erase) the arrow icons to the left and right of an icon's row,
// as appropriate.
static
VOID PaintArrows (
    SCROLL_STATE *State,
    UINTN PosX,
    UINTN PosY,
    UINTN row0Loaders
) {
    EG_IMAGE *TempImage;
    UINTN Width, Height, RightX, AdjPosY;

    // NOTE: Assume that left and right arrows are of the same size...
     Width   = egemb_arrow_left.Width;
     Height  = egemb_arrow_left.Height;
     RightX  = (ScreenW + (TileSizes[0] + TILE_XSPACING) * State->MaxVisible) / 2 + TILE_XSPACING;
     AdjPosY = PosY - (Height / 2);

     // For PaintIcon() calls, the starting Y position is moved to the midpoint
     // of the surrounding row; PaintIcon() adjusts this back up by half the
     // icon's height to properly center it.
     if ((State->FirstVisible > 0) &&
        (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_ARROWS))
    ) {
        PaintIcon (&egemb_arrow_left, L"arrow_left", PosX, PosY, ALIGN_RIGHT);
    }
    else {
        TempImage = egCropImage (GlobalConfig.ScreenBackground, PosX - Width, AdjPosY, Width, Height);
        BltImage (TempImage, PosX - Width, AdjPosY);
        egFreeImage (TempImage);
    } // if/else

    if ((State->LastVisible < (row0Loaders - 1)) &&
        (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_ARROWS))
    ) {
        PaintIcon (&egemb_arrow_right, L"arrow_right", RightX, PosY, ALIGN_LEFT);
    }
    else {
        TempImage = egCropImage (GlobalConfig.ScreenBackground, RightX, AdjPosY, Width, Height);
        BltImage (TempImage, RightX, AdjPosY);
        egFreeImage (TempImage);
    } // if/else
} // VOID PaintArrows()

// Display main menu in graphics mode
VOID MainMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE *State,
    IN UINTN Function,
    IN CHAR16 *ParamText
) {
    INTN i;
    UINTN row0Count, row1Count, row1PosX, row1PosXRunning;
    static UINTN  row0PosX, row0PosXRunning, row1PosY, row0Loaders;
    static UINTN *itemPosX;
    static UINTN row0PosY, textPosY;

    State->ScrollMode = SCROLL_MODE_ICONS;
    switch (Function) {
        case MENU_FUNCTION_INIT:
            InitScroll (State, Screen->EntryCount, GlobalConfig.MaxTags);

            // layout
            row0Count = 0;
            row1Count = 0;
            row0Loaders = 0;
            for (i = 0; i <= State->MaxIndex; i++) {
                if (Screen->Entries[i]->Row == 1) {
                    row1Count++;
                }
                else {
                    row0Loaders++;
                    if (row0Count < State->MaxVisible) {
                        row0Count++;
                    }
                }
            }
            row0PosX = (ScreenW + TILE_XSPACING - (TileSizes[0] + TILE_XSPACING) * row0Count) >> 1;
            row0PosY = ComputeRow0PosY();
            row1PosX = (ScreenW + TILE_XSPACING - (TileSizes[1] + TILE_XSPACING) * row1Count) >> 1;
            row1PosY = row0PosY + TileSizes[0] + TILE_YSPACING;
            if (row1Count > 0) {
                textPosY = row1PosY + TileSizes[1] + TILE_YSPACING;
            }
            else {
                textPosY = row1PosY;
            }

            itemPosX = AllocatePool (sizeof (UINTN) * Screen->EntryCount);
            LEAKABLE(itemPosX, "MainMenuStyle itemPosX");
            row0PosXRunning = row0PosX;
            row1PosXRunning = row1PosX;
            for (i = 0; i <= State->MaxIndex; i++) {
                if (Screen->Entries[i]->Row == 0) {
                    itemPosX[i] = row0PosXRunning;
                    row0PosXRunning += TileSizes[0] + TILE_XSPACING;
                }
                else {
                    itemPosX[i] = row1PosXRunning;
                    row1PosXRunning += TileSizes[1] + TILE_XSPACING;
                }
            }
            // initial painting
            InitSelection();
            SwitchToGraphicsAndClear (TRUE);
            break;

        case MENU_FUNCTION_CLEANUP:
            MyFreePool (&itemPosX);
            break;

        case MENU_FUNCTION_PAINT_ALL:
            PaintAll (Screen, State, itemPosX, row0PosY, row1PosY, textPosY);
            // For PaintArrows(), the starting Y position is moved to the midpoint
            // of the surrounding row; PaintIcon() adjusts this back up by half the
            // icon's height to properly center it.
            PaintArrows (State, row0PosX - TILE_XSPACING, row0PosY + (TileSizes[0] / 2), row0Loaders);
            break;

        case MENU_FUNCTION_PAINT_SELECTION:
            PaintSelection (Screen, State, itemPosX, row0PosY, row1PosY, textPosY);
            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) {
               DrawTextWithTransparency (L"", 0, textPosY + TextLineHeight());
               DrawTextWithTransparency (
                   ParamText,
                   (ScreenW - egComputeTextWidth (ParamText)) >> 1,
                   textPosY + TextLineHeight()
               );
            }
            break;
    }
} // VOID MainMenuStyle()

// Determines the index of the main menu item at the given coordinates.
UINTN FindMainMenuItem (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE *State,
    IN UINTN PosX,
    IN UINTN PosY
) {
    UINTN i;
    UINTN itemRow;
           UINTN  row0Count, row1Count, row1PosX, row1PosXRunning;
    static UINTN  row0PosX, row0PosXRunning, row1PosY, row0Loaders;
    static UINTN *itemPosX;
    static UINTN  row0PosY;

    row0Count = 0;
    row1Count = 0;
    row0Loaders = 0;
    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 1) {
            row1Count++;
        }
        else {
            row0Loaders++;
            if (row0Count < State->MaxVisible) {
                row0Count++;
            }
        }
    }
    row0PosX = (ScreenW + TILE_XSPACING - (TileSizes[0] + TILE_XSPACING) * row0Count) >> 1;
    row0PosY = ComputeRow0PosY();
    row1PosX = (ScreenW + TILE_XSPACING - (TileSizes[1] + TILE_XSPACING) * row1Count) >> 1;
    row1PosY = row0PosY + TileSizes[0] + TILE_YSPACING;

    if (PosY >= row0PosY && PosY <= row0PosY + TileSizes[0]) {
        itemRow = 0;
        if (PosX <= row0PosX) {
            return POINTER_LEFT_ARROW;
        }
        else if (PosX >= (ScreenW - row0PosX)) {
            return POINTER_RIGHT_ARROW;
        }
    }
    else if (PosY >= row1PosY && PosY <= row1PosY + TileSizes[1]) {
        itemRow = 1;
    }
    else {
        // Y coordinate is outside of either row
        return POINTER_NO_ITEM;
    }

    UINTN ItemIndex = POINTER_NO_ITEM;

    itemPosX = AllocatePool (sizeof (UINTN) * Screen->EntryCount);
    row0PosXRunning = row0PosX;
    row1PosXRunning = row1PosX;
    for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0) {
            itemPosX[i] = row0PosXRunning;
            row0PosXRunning += TileSizes[0] + TILE_XSPACING;
        }
        else {
            itemPosX[i] = row1PosXRunning;
            row1PosXRunning += TileSizes[1] + TILE_XSPACING;
        }
    }

    for (i = State->FirstVisible; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0 && itemRow == 0) {
            if (i <= State->LastVisible) {
                if (PosX >= itemPosX[i - State->FirstVisible] &&
                    PosX <= itemPosX[i - State->FirstVisible] + TileSizes[0]
                ) {
                    ItemIndex = i;
                    break;
                }
            } // if i <= State->LastVisible
        }
        else if (Screen->Entries[i]->Row == 1 && itemRow == 1) {
            if (PosX >= itemPosX[i] && PosX <= itemPosX[i] + TileSizes[1]) {
                ItemIndex = i;
                break;
            }
        } // if/else Screen->Entries[i]->Row
    }

    MyFreePool (&itemPosX);

    return ItemIndex;
} // VOID FindMainMenuItem()

VOID GenerateWaitList() {
    UINTN PointerCount = pdCount();

    WaitListLength = 2 + PointerCount;
    MyFreePool (&WaitList);
    WaitList = AllocatePool (sizeof (EFI_EVENT) * WaitListLength);
    LEAKABLE(WaitList, "GenerateWaitList WaitList");
    WaitList[0] = gST->ConIn->WaitForKey;

    UINTN Index;
    for (Index = 0; Index < PointerCount; Index++) {
        WaitList[Index + 1] = pdWaitEvent (Index);
    }
} // VOID GenerateWaitList()

UINTN WaitForInput (UINTN Timeout) {
    UINTN       Index      = INPUT_TIMEOUT;
    UINTN       Length     = WaitListLength;
    EFI_EVENT   TimerEvent = NULL;
    EFI_STATUS  Status;

    LOG(4, LOG_THREE_STAR_MID, L"Input Pending: %d", Timeout);

    Status = refit_call5_wrapper(gBS->CreateEvent, EVT_TIMER, 0, NULL, NULL, &TimerEvent);
    if (Timeout == 0) {
        Length--;
    }
    else {
        if (EFI_ERROR (Status)) {
            refit_call1_wrapper(gBS->Stall, 100000); // Pause for 100 ms
            return INPUT_TIMER_ERROR;
        }
        else {
            refit_call3_wrapper(gBS->SetTimer, TimerEvent, TimerRelative, Timeout * 10000);
            WaitList[Length - 1] = TimerEvent;
        }
    }

    Status = refit_call3_wrapper(gBS->WaitForEvent, Length, WaitList, &Index);
    refit_call1_wrapper(gBS->CloseEvent, TimerEvent);

    if (EFI_ERROR (Status)) {
        refit_call1_wrapper(gBS->Stall, 100000); // Pause for 100 ms
        return INPUT_TIMER_ERROR;
    }
    else if (Index == 0) {
        return INPUT_KEY;
    }
    else if (Index < Length - 1) {
        return INPUT_POINTER;
    }
    return INPUT_TIMEOUT;
} // UINTN WaitForInput()

// Enable the user to edit boot loader options.
// Returns TRUE if the user exited with edited options; FALSE if the user
// pressed Esc to terminate the edit.
static
BOOLEAN EditOptions (LOADER_ENTRY *MenuEntry) {
   MsgLog ("[ EditOptions\n");
   UINTN x_max, y_max;
   CHAR16 *EditedOptions;
   BOOLEAN retval = FALSE;

   LOG(2, LOG_LINE_NORMAL, L"EditOptions: %d", ((GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) != 0));
   if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
      return FALSE;
   }

   refit_call4_wrapper(gST->ConOut->QueryMode, gST->ConOut, gST->ConOut->Mode->Mode, &x_max, &y_max);

   if (!GlobalConfig.TextOnly)
      SwitchToText (TRUE);

   if (line_edit (GetPoolStr (&MenuEntry->LoadOptions), &EditedOptions, x_max)) {
      AssignPoolStr (&MenuEntry->LoadOptions, EditedOptions);
      LEAKABLE(EditedOptions, "EditOptions EditedOptions(Many)");
      retval = TRUE;
   } // if
   if (!GlobalConfig.TextOnly) {
       SwitchToGraphics();
   }

   MsgLog ("] EditOptions %d\n", retval);
   return retval;
} // VOID EditOptions()

//
// user-callable dispatcher functions
//

VOID DisplaySimpleMessage (
    CHAR16* Title,
    CHAR16 *Message
) {
    LOG(4, LOG_THREE_STAR_MID, L"Entering DisplaySimpleMessage");

    if (!Message) {
        return;
    }

    MENU_STYLE_FUNC Style = TextMenuStyle;
    INTN DefaultEntry = 0;
    UINTN MenuExit;
    REFIT_MENU_ENTRY *ChosenOption;
    REFIT_MENU_SCREEN *SimpleMessageMenu = NULL;
    REFIT_MENU_SCREEN SimpleMessageMenuSrc = {
        {Title, TRUE}, {NULL, FALSE}, 0, NULL, 0, NULL, 0, {NULL, FALSE},
        {L"Press Enter to return to main menu", TRUE}, {L"", TRUE}
    };
    CopyFromPoolImage_PI_ (&SimpleMessageMenuSrc.TitleImage_PI_, BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT));

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }
    
    SimpleMessageMenu = CopyMenuScreen (&SimpleMessageMenuSrc);
    if (!SimpleMessageMenu) {
        return;
    }

    AddMenuInfoLineCached (SimpleMessageMenu, Message);
    AddMenuEntryCopy (SimpleMessageMenu, &MenuEntryReturn);
    MenuExit = RunGenericMenu (SimpleMessageMenu, Style, &DefaultEntry, &ChosenOption);
    FreeMenuScreen (&SimpleMessageMenu);

    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' from RunGenericMenu call on '%s' in 'DisplaySimpleMessage'",
        MenuExit, GetPoolStr (&ChosenOption->Title)
    );

    // DA-TAG: Tick box to run check after 'RunGenericMenu'
    if (MenuExit == 0) {
        LOG(1, LOG_LINE_NORMAL, L"%s - %s", Title, Message);
    }
    else {
        LOG(1, LOG_LINE_NORMAL, L"%s - %s: MenuExit = %d", Title, Message, MenuExit);
    }
    #endif
} // VOID DisplaySimpleMessage()

// Check each filename in FilenameList to be sure it refers to a valid file. If
// not, delete it. This works only on filenames that are complete, with volume,
// path, and filename components; if the filename omits the volume, the search
// is not done and the item is left intact, no matter what.
// Returns TRUE if any files were deleted, FALSE otherwise.
static
BOOLEAN RemoveInvalidFilenames (CHAR16 *FilenameList, CHAR16 *VarName) {
    EFI_STATUS       Status;
    UINTN i = 0;
    CHAR16 *Filename, *OneElement, *VolName = NULL;
    BOOLEAN          DeleteIt, DeletedSomething = FALSE;
    REFIT_VOLUME    *Volume = NULL;
    EFI_FILE_HANDLE FileHandle;

    while ((OneElement = FindCommaDelimited (FilenameList, i)) != NULL) {
        DeleteIt = FALSE;
        Filename = StrDuplicate (OneElement);
        if (SplitVolumeAndFilename (&Filename, &VolName)) {
            DeleteIt = TRUE;
            if (FindVolume (&Volume, VolName) && Volume->RootDir) {
                Status = refit_call5_wrapper(
                    Volume->RootDir->Open, Volume->RootDir,
                    &FileHandle, Filename,
                    EFI_FILE_MODE_READ, 0
                );
                if (Status == EFI_SUCCESS) {
                    DeleteIt = FALSE;
                    refit_call1_wrapper(FileHandle->Close, FileHandle);
                } // if file exists
            } // if volume exists
        } // if list item includes volume

        if (DeleteIt) {
            DeleteItemFromCsvList (OneElement, FilenameList);
        }
        else {
            i++;
        }

        MyFreePool (&OneElement);
        MyFreePool (&Filename);
        MyFreePool (&VolName);

        VolName = NULL;

        DeletedSomething |= DeleteIt;
    } // while()

    // since Volume started as NULL, free it if it is not NULL
    FreeVolume (&Volume);

    return DeletedSomething;
} // BOOLEAN RemoveInvalidFilenames()

// Save a list of items to be hidden to NVRAM or disk,
// as determined by GlobalConfig.UseNvram.
static
VOID SaveHiddenList (
    IN CHAR16 *HiddenList,
    IN CHAR16 *VarName
) {
    EFI_STATUS Status;
    UINTN      ListLen;

    if (!HiddenList || !VarName) {
        // Prevent NULL dererencing
        Status = EFI_INVALID_PARAMETER;
    }
    else {
        ListLen = StrSize (HiddenList);

        Status = EfivarSetRaw (
            &RefindPlusGuid,
            VarName,
            HiddenList,
            ListLen * (ListLen > 0),
            TRUE
        );
    }

    CheckError(Status, L"in SaveHiddenList!!");
} // VOID SaveHiddenList()

// Present a menu that enables the user to delete hidden tags (that is, to
// un-hide them).
VOID ManageHiddenTags (
    VOID
) {
    EFI_STATUS           Status  = EFI_SUCCESS;
    CHAR16              *AllTags = NULL, *OneElement = NULL;
    CHAR16              *HiddenLegacy, *HiddenFirmware, *HiddenTags, *HiddenTools;
    INTN                 DefaultEntry  = 0;
    UINTN                i             = 0;
    UINTN                MenuExit      = 0;
    BOOLEAN              SaveTags      = FALSE;
    BOOLEAN              SaveTools     = FALSE;
    BOOLEAN              SaveLegacy    = FALSE;
    BOOLEAN              SaveFirmware  = FALSE;
    MENU_STYLE_FUNC      Style         = TextMenuStyle;
    REFIT_MENU_ENTRY    *ChosenOption;
    REFIT_MENU_ENTRY    *MenuEntryItem;

    REFIT_MENU_SCREEN   *RestoreItemMenu = NULL;
    REFIT_MENU_SCREEN   RestoreItemMenuSrc = {
        {L"Manage Hidden Tags", TRUE}, {NULL, FALSE}, 0, NULL, 0, NULL, 0, {NULL, FALSE},
        {L"Select an option and press Enter or", TRUE},
        {L"press Esc to return to main menu without changes", TRUE}
    };

    LOG(1, LOG_LINE_SEPARATOR, L"Managing hidden tags");
    
    RestoreItemMenu = CopyMenuScreen (&RestoreItemMenuSrc);
    if (!RestoreItemMenu) {
        return;
    }

    CopyFromPoolImage_PI_ (&RestoreItemMenu->TitleImage_PI_, BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN));
    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    HiddenTags = ReadHiddenTags (L"HiddenTags");
    if (HiddenTags) {
    SaveTags   = RemoveInvalidFilenames (HiddenTags, L"HiddenTags");
    if (HiddenTags && (HiddenTags[0] != L'\0')) {
        AllTags = StrDuplicate (HiddenTags);
    }
    }

    HiddenTools = ReadHiddenTags (L"HiddenTools");
    if (HiddenTools) {
    SaveTools   = RemoveInvalidFilenames (HiddenTools, L"HiddenTools");
    if (HiddenTools && (HiddenTools[0] != L'\0')) {
        MergeStrings (&AllTags, HiddenTools, L',');
    }
    }

    HiddenLegacy = ReadHiddenTags (L"HiddenLegacy");
    if (HiddenLegacy && (HiddenLegacy[0] != L'\0')) {
        MergeStrings (&AllTags, HiddenLegacy, L',');
    }

    HiddenFirmware = ReadHiddenTags (L"HiddenFirmware");
    if (HiddenFirmware && (HiddenFirmware[0] != L'\0')) {
        MergeStrings (&AllTags, HiddenFirmware, L',');
    }

    if ((AllTags) && (StrLen (AllTags) > 0)) {
        AddMenuInfoLineCached (RestoreItemMenu, L"Select a tag and press Enter to restore it");
        while ((OneElement = FindCommaDelimited (AllTags, i++)) != NULL) {
            MenuEntryItem = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY)); // do not free
            AssignPoolStr (&MenuEntryItem->Title, OneElement);
            MenuEntryItem->Tag = TAG_RETURN;
            MenuEntryItem->Row = 1;
            AddMenuEntry (RestoreItemMenu, MenuEntryItem);
        } // while

        MenuExit = RunGenericMenu (RestoreItemMenu, Style, &DefaultEntry, &ChosenOption);
        LOG(2, LOG_LINE_NORMAL,
            L"Returned '%d' from RunGenericMenu call on '%s' in 'ManageHiddenTags'",
            MenuExit, GetPoolStr (&ChosenOption->Title)
        );
        
        if (MenuExit == MENU_EXIT_ENTER) {
            SaveTags     |= DeleteItemFromCsvList (GetPoolStr (&ChosenOption->Title), HiddenTags);
            SaveTools    |= DeleteItemFromCsvList (GetPoolStr (&ChosenOption->Title), HiddenTools);
            SaveFirmware |= DeleteItemFromCsvList (GetPoolStr (&ChosenOption->Title), HiddenFirmware);
            SaveLegacy   |= DeleteItemFromCsvList (GetPoolStr (&ChosenOption->Title), HiddenLegacy);

            if (DeleteItemFromCsvList (GetPoolStr (&ChosenOption->Title), HiddenLegacy)) {
                i = HiddenLegacy ? StrSize (HiddenLegacy) : 0;
                Status = EfivarSetRaw (
                    &RefindPlusGuid,
                    L"HiddenLegacy",
                    HiddenLegacy,
                    i * (i > 2),
                    TRUE
                );
                SaveLegacy = TRUE;
                CheckError (Status, L"in ManageHiddenTags!!");
            } // if
        } // if

        if (SaveTags) {
            SaveHiddenList(HiddenTags, L"HiddenTags");
        }
        if (SaveLegacy) {
            SaveHiddenList(HiddenLegacy, L"HiddenLegacy");
        }
        if (SaveTools) {
            SaveHiddenList(HiddenTools, L"HiddenTools");
            MyFreePool (&gHiddenTools);
        }
        if (SaveFirmware) {
            SaveHiddenList(HiddenFirmware, L"HiddenFirmware");
        }
        if (SaveTags || SaveTools || SaveLegacy || SaveFirmware) {
            RescanAll (FALSE, FALSE);
        }
    }
    else {
        DisplaySimpleMessage (L"Information", L"No hidden tags found");
    }

    FreeMenuScreen (&RestoreItemMenu);
    MyFreePool (&AllTags);
    MyFreePool (&HiddenTags);
    MyFreePool (&HiddenTools);
    MyFreePool (&HiddenLegacy);
    MyFreePool (&HiddenFirmware);
} // VOID ManageHiddenTags()

CHAR16* ReadHiddenTags (CHAR16 *VarName) {
    CHAR16     *Buffer = NULL;
    UINTN       Size;
    EFI_STATUS  Status;

    Status = EfivarGetRaw (&RefindPlusGuid, VarName, (VOID **)&Buffer, &Size);
    if ((Status != EFI_SUCCESS) && (Status != EFI_NOT_FOUND)) {
        #if REFIT_DEBUG > 0
        CHAR16 *CheckErrMsg = PoolPrint (L"in ReadHiddenTags ('%s')", VarName);
        CheckError (Status, CheckErrMsg);
        MyFreePool (&CheckErrMsg);
        #endif
    }
    if ((Status == EFI_SUCCESS) && (Size == 0)) {
        LOG(2, LOG_LINE_NORMAL,
            L"Zero Size in ReadHiddenTags ... Clearing Buffer"
        );

        MyFreePool (&Buffer);
        Buffer = NULL;
    }
    return Buffer;
} // CHAR16* ReadHiddenTags()

// Add PathName to the hidden tags variable specified by *VarName.
static
VOID AddToHiddenTags (CHAR16 *VarName, CHAR16 *Pathname) {
    EFI_STATUS  Status;
    CHAR16      *HiddenTags;

    if (Pathname && (StrLen (Pathname) > 0)) {
        HiddenTags = ReadHiddenTags (VarName);
        if (!HiddenTags) {
            // Prevent NULL dererencing
            HiddenTags = StrDuplicate (Pathname);
        }
        else {
        MergeStrings (&HiddenTags, Pathname, L',');
        }

        Status = EfivarSetRaw (
            &RefindPlusGuid,
            VarName,
            (CHAR8 *) HiddenTags,
            StrSize (HiddenTags),
            TRUE
        );

        CheckError (Status, L"in AddToHiddenTags!!");
        MyFreePool (&HiddenTags);
    } // if
} // VOID AddToHiddenTags()

// Adds a filename, specified by the *Loader variable, to the *VarName EFI variable,
// using the mostly-prepared *HideItemMenu structure to prompt the user to confirm
// hiding that item.
// Returns TRUE if item was hidden, FALSE otherwise.
static
BOOLEAN HideEfiTag (
    LOADER_ENTRY *Loader,
    REFIT_MENU_SCREEN *HideItemMenu,
    CHAR16 *VarName
) {
    REFIT_VOLUME      *TestVolume   = NULL;
    BOOLEAN            TagHidden    = FALSE;
    CHAR16            *FullPath     = NULL;
    CHAR16            *GuidStr      = NULL;
    UINTN              MenuExit;
    INTN               DefaultEntry = 1;
    MENU_STYLE_FUNC    Style        = TextMenuStyle;
    REFIT_MENU_ENTRY  *ChosenOption;

    if ((!Loader) || (!(Loader->Volume)) || (!(GetPoolStr (&Loader->LoaderPath))) || (!HideItemMenu) || (!VarName))
        return FALSE;

    if (AllowGraphicsMode)
        Style = GraphicsMenuStyle;

    if (GetPoolStr (&Loader->Volume->VolName) && (StrLen (GetPoolStr (&Loader->Volume->VolName)) > 0)) {
        FullPath = StrDuplicate (GetPoolStr (&Loader->Volume->VolName));
    }
    MergeStrings (&FullPath, GetPoolStr (&Loader->LoaderPath), L':');
    AddMenuInfoLinePool (HideItemMenu, PoolPrint (L"Are you sure you want to hide %s?", FullPath));
    AddMenuEntryCopy (HideItemMenu, &MenuEntryYes);
    AddMenuEntryCopy (HideItemMenu, &MenuEntryNo);

    MenuExit = RunGenericMenu (HideItemMenu, Style, &DefaultEntry, &ChosenOption);
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' from RunGenericMenu call on '%s' in 'HideEfiTag'",
        MenuExit, GetPoolStr (&ChosenOption->Title)
    );

    if (ChosenOption && MyStriCmp (GetPoolStr (&ChosenOption->Title), L"Yes") && (MenuExit == MENU_EXIT_ENTER)) {
        GuidStr = GuidAsString (&Loader->Volume->PartGuid);
        if (FindVolume (&TestVolume, GuidStr) && TestVolume->RootDir) {
            MyFreePool (&FullPath);
            FullPath = NULL;
            MergeStrings (&FullPath, GuidAsString (&Loader->Volume->PartGuid), L'\0');
            MergeStrings (&FullPath, L":", L'\0');
            MergeStrings (
                &FullPath,
                GetPoolStr (&Loader->LoaderPath),
                (GetPoolStr (&Loader->LoaderPath)[0] == L'\\' ? L'\0' : L'\\')
            );
        }
        AddToHiddenTags (VarName, FullPath);
        TagHidden = TRUE;
        MyFreePool (&GuidStr);
    } // if

    MyFreePool (&FullPath);

    // since TestVolume started as NULL, free it if it is not NULL
    FreeVolume (&TestVolume);

    return TagHidden;
} // BOOLEAN HideEfiTag()

static
BOOLEAN HideFirmwareTag(
    LOADER_ENTRY *Loader,
    REFIT_MENU_SCREEN *HideItemMenu
) {
    MENU_STYLE_FUNC    Style = TextMenuStyle;
    REFIT_MENU_ENTRY   *ChosenOption;
    INTN               DefaultEntry = 1;
    UINTN              MenuExit;
    BOOLEAN            TagHidden = FALSE;

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    AddMenuInfoLinePool (HideItemMenu, PoolPrint(L"Really hide '%s'?", GetPoolStr (&Loader->Title)));
    AddMenuEntryCopy (HideItemMenu, &MenuEntryYes);
    AddMenuEntryCopy (HideItemMenu, &MenuEntryNo);

    MenuExit = RunGenericMenu(HideItemMenu, Style, &DefaultEntry, &ChosenOption);

    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' from RunGenericMenu call on '%s' in 'HideFirmwareTag'",
        MenuExit, GetPoolStr (&ChosenOption->Title)
        );

    if ((MyStriCmp(GetPoolStr (&ChosenOption->Title), L"Yes")) &&
        (MenuExit == MENU_EXIT_ENTER)
    ) {
        AddToHiddenTags(L"HiddenFirmware", GetPoolStr (&Loader->Title));
        TagHidden = TRUE;
    } // if

    return TagHidden;
} // BOOLEAN HideFirmwareTag()


static
BOOLEAN HideLegacyTag (
    LEGACY_ENTRY *LegacyLoader,
    REFIT_MENU_SCREEN *HideItemMenu
) {
    MENU_STYLE_FUNC    Style = TextMenuStyle;
    REFIT_MENU_ENTRY   *ChosenOption;
    INTN               DefaultEntry = 1;
    UINTN              MenuExit;
    CHAR16             *Name = NULL;
    BOOLEAN            TagHidden = FALSE;

    if (AllowGraphicsMode)
        Style = GraphicsMenuStyle;

    if ((GlobalConfig.LegacyType == LEGACY_TYPE_MAC) && GetPoolStr (&LegacyLoader->me.Title)) {
        Name = StrDuplicate (GetPoolStr (&LegacyLoader->me.Title));
    }
    if ((GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) &&
        LegacyLoader->BdsOption && LegacyLoader->BdsOption->Description
    ) {
        Name = StrDuplicate (LegacyLoader->BdsOption->Description);
    }
    if (!Name) {
        Name = StrDuplicate (L"Legacy OS");
    }
    AddMenuInfoLinePool (HideItemMenu, PoolPrint (L"Are you sure you want to hide '%s'?", Name));
    AddMenuEntryCopy (HideItemMenu, &MenuEntryYes);
    AddMenuEntryCopy (HideItemMenu, &MenuEntryNo);
    MenuExit = RunGenericMenu (HideItemMenu, Style, &DefaultEntry, &ChosenOption);

    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' from RunGenericMenu call on '%s' in 'HideLegacyTag'",
        MenuExit, GetPoolStr (&ChosenOption->Title)
    );
    
    if (MyStriCmp (GetPoolStr (&ChosenOption->Title), L"Yes") && (MenuExit == MENU_EXIT_ENTER)) {
        AddToHiddenTags (L"HiddenLegacy", Name);
        TagHidden = TRUE;
    }
    MyFreePool (&Name);

    return TagHidden;
} // BOOLEAN HideLegacyTag()

static
VOID HideTag (
    REFIT_MENU_ENTRY *ChosenEntry
) {
    LOADER_ENTRY       *Loader       = (LOADER_ENTRY *) ChosenEntry;
    LEGACY_ENTRY       *LegacyLoader = (LEGACY_ENTRY *) ChosenEntry;
    REFIT_MENU_SCREEN  *HideItemMenu = NULL;
    REFIT_MENU_SCREEN  HideItemMenuSrc = {
        {NULL, FALSE}, {NULL, FALSE}, 0, NULL, 0, NULL, 0, {NULL, FALSE},
        {L"Select an option and press 'Enter' or", TRUE},
        {L"press 'Esc' to return to main menu without changes", TRUE}
    };

    if (ChosenEntry == NULL) {
        return;
    }
    
    HideItemMenu = CopyMenuScreen (&HideItemMenuSrc);
    if (!HideItemMenu) {
        return;
    }

    CopyFromPoolImage_PI_ (&HideItemMenu->TitleImage_PI_, BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN));
    // BUG: The RescanAll() calls should be conditional on successful calls to
    // HideEfiTag() or HideLegacyTag(); but for the former, this causes
    // crashes on a second call hide a tag if the user chose "No" to the first
    // call. This seems to be related to memory management of Volumes; the
    // crash occurs in FindVolumeAndFilename() and lib.c when calling
    // DevicePathToStr(). Calling RescanAll() on all returns from HideEfiTag()
    // seems to be an effective workaround, but there's likely a memory
    // management bug somewhere that's the root cause.
    switch (ChosenEntry->Tag) {
        case TAG_LOADER:
            if (Loader->DiscoveryType == DISCOVERY_TYPE_AUTO) {
                AssignCachedPoolStr (&HideItemMenu->Title, L"Hide EFI OS Tag");
                HideEfiTag (Loader, HideItemMenu, L"HiddenTags");

                MsgLog ("User Input Received:\n");
                MsgLog ("  - %s\n\n", GetPoolStr (&HideItemMenu->Title));

                RescanAll (FALSE, FALSE);
            }
            else {
                DisplaySimpleMessage (
                    L"Cannot Hide Entry for Manual Boot Stanza",
                    L"You must edit 'config.conf' to remove this entry."
                );
            }
            break;

        case TAG_LEGACY:
        case TAG_LEGACY_UEFI:
            AssignCachedPoolStr (&HideItemMenu->Title, L"Hide Legacy OS Tag");
            if (HideLegacyTag (LegacyLoader, HideItemMenu)) {
                MsgLog ("User Input Received:\n");
                MsgLog ("  - %s\n\n", GetPoolStr (&HideItemMenu->Title));

                RescanAll (FALSE, FALSE);
            }
            break;

        case TAG_FIRMWARE_LOADER:
            AssignCachedPoolStr (&HideItemMenu->Title, L"Hide Firmware Boot Option Tag");
            if (HideFirmwareTag(Loader, HideItemMenu)) {
                RescanAll(FALSE, FALSE);
            }
            break;

        case TAG_ABOUT:
        case TAG_REBOOT:
        case TAG_SHUTDOWN:
        case TAG_EXIT:
        case TAG_FIRMWARE:
        case TAG_CSR_ROTATE:
        case TAG_INSTALL:
        case TAG_HIDDEN:
            DisplaySimpleMessage (
                L"Unable to Comply",
                L"To hide an internal tool, edit the 'showtools' line in config.conf"
            );
            break;

        case TAG_TOOL:
            AssignCachedPoolStr (&HideItemMenu->Title, L"Hide Tool Tag");
            HideEfiTag (Loader, HideItemMenu, L"HiddenTools");
            MyFreePool (&gHiddenTools);

            MsgLog ("User Input Received:\n");
            MsgLog ("  - %s\n\n", GetPoolStr (&HideItemMenu->Title));

            RescanAll (FALSE, FALSE);
            break;
    } // switch()
    FreeMenuScreen (&HideItemMenu);
} // VOID HideTag()

UINTN RunMenu (
    IN  REFIT_MENU_SCREEN  *Screen,
    OUT REFIT_MENU_ENTRY  **ChosenEntry
) {
    INTN            DefaultEntry = -1;
    UINTN           MenuExit;
    MENU_STYLE_FUNC Style = TextMenuStyle;

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    MenuExit = RunGenericMenu (Screen, Style, &DefaultEntry, ChosenEntry);

    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' from RunGenericMenu call on '%s' in 'RunMenu'",
        MenuExit, GetPoolStr (&Screen->Title)
    );

    return MenuExit;
}

UINTN RunMainMenu (
    REFIT_MENU_SCREEN  *Screen,
    CHAR16            **DefaultSelection,
    REFIT_MENU_ENTRY  **ChosenEntry
) {
    LOG(2, LOG_LINE_NORMAL, L"[ RunMainMenu '%s'", GetPoolStr (&Screen->Title));

    MENU_STYLE_FUNC Style     = TextMenuStyle;
    MENU_STYLE_FUNC MainStyle = TextMenuStyle;
    REFIT_MENU_ENTRY *TempChosenEntry = NULL;
    CHAR16 *MenuTitle;
    UINTN MenuExit           = 0;
    INTN DefaultEntryIndex   = -1;
    INTN DefaultSubmenuIndex = -1;

    // remove any buffered key strokes
    ReadAllKeyStrokes();

    TileSizes[0] = (GlobalConfig.IconSizes[ICON_SIZE_BIG] * 9) / 8;
    TileSizes[1] = (GlobalConfig.IconSizes[ICON_SIZE_SMALL] * 4) / 3;

    if ((DefaultSelection != NULL) && (*DefaultSelection != NULL)) {
        // Find a menu entry that includes *DefaultSelection as a substring
        DefaultEntryIndex = FindMenuShortcutEntry (Screen, *DefaultSelection);
    }

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
        MainStyle = MainMenuStyle;

        PointerEnabled = PointerActive = pdAvailable();
        DrawSelection = !PointerEnabled;
    }

    // Generate this now and keep it around forever, since it's likely to be
    // used after this function terminates....
    GenerateWaitList();
    MenuTitle = StrDuplicate (L"Unknown");
    LEAKABLE(MenuTitle, "RunMainMenu MenuTitle");

    while (!MenuExit) {
        MenuExit = RunGenericMenu (Screen, MainStyle, &DefaultEntryIndex, &TempChosenEntry);

        LOG(2, LOG_LINE_NORMAL,
            L"Returned '%d' from RunGenericMenu call on '%s' in 'RunMainMenu'",
            MenuExit, GetPoolStr (&TempChosenEntry->Title)
        );

        Screen->TimeoutSeconds = 0;

        MyFreePool (&MenuTitle);
        MenuTitle = StrDuplicate (GetPoolStr (&TempChosenEntry->Title));
        LEAKABLE(MenuTitle, "RunMainMenu MenuTitle");
        if (MenuExit == MENU_EXIT_DETAILS) {
            if (TempChosenEntry->SubScreen != NULL) {
                MenuExit = RunGenericMenu (
                    TempChosenEntry->SubScreen,
                    Style,
                    &DefaultSubmenuIndex,
                    &TempChosenEntry
                );

                LOG(2, LOG_LINE_NORMAL,
                    L"Returned '%d' from RunGenericMenu call on '%s' in 'RunMainMenu'",
                    MenuExit, GetPoolStr (&TempChosenEntry->SubScreen->Title)
                );

               if (MenuExit == MENU_EXIT_ESCAPE || TempChosenEntry->Tag == TAG_RETURN) {
                   MenuExit = 0;
               }

               if (MenuExit == MENU_EXIT_DETAILS) {
                  if (!EditOptions ((LOADER_ENTRY *) TempChosenEntry)) {
                      MenuExit = 0;
                  }
               }
            }
            else {
                // no sub-screen; ignore keypress
               MenuExit = 0;
            }
        } // Enter sub-screen

        if (MenuExit == MENU_EXIT_HIDE) {
            if (GlobalConfig.HiddenTags) {
                HideTag (TempChosenEntry);
            }
            MenuExit = 0;
        } // Hide launcher
    } // while

    if (ChosenEntry) {
        *ChosenEntry = TempChosenEntry;
    }

    if (DefaultSelection) {
       MyFreePool (DefaultSelection);
       *DefaultSelection = MenuTitle;
    }
    else {
        MyFreePool (&MenuTitle);
    }

    LOG(2, LOG_LINE_NORMAL, L"] RunMainMenu MenuExit:%d", MenuExit);
    return MenuExit;
} // RunMainMenu

// Creates a copy of a menu screen.
// Returns a pointer to the copy of the menu screen.
REFIT_MENU_SCREEN*
CopyMenuScreen (
    REFIT_MENU_SCREEN *Entry
) {
    REFIT_MENU_SCREEN *NewEntry = NULL;
    
    if (Entry) {
        NewEntry = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
        if (NewEntry) {
            UINTN i;
        
            CopyFromPoolStr (&NewEntry->Title, &Entry->Title);
            CopyFromPoolImage (&NewEntry->TitleImage, &Entry->TitleImage);

            NewEntry->InfoLineCount = Entry->InfoLineCount;
            if (NewEntry->InfoLineCount) {
                NewEntry->InfoLines = (PoolStr*) AllocateZeroPool (Entry->InfoLineCount * sizeof (PoolStr));
                for (i = 0; i < Entry->InfoLineCount && NewEntry->InfoLines; i++) {
                    CopyFromPoolStr_PS_ (&NewEntry->InfoLines[i], &Entry->InfoLines[i]);
                } // for
            }

            for (i = 0; i < Entry->EntryCount && NewEntry->Entries; i++) {
                AddMenuEntryCopy (NewEntry, Entry->Entries[i]);
            } // for

            NewEntry->TimeoutSeconds = Entry->TimeoutSeconds;
            CopyFromPoolStr (&NewEntry->TimeoutText, &Entry->TimeoutText);

            CopyFromPoolStr (&NewEntry->Hint1, &Entry->Hint1);
            CopyFromPoolStr (&NewEntry->Hint2, &Entry->Hint2);
        } // if NewEntry
    } // if Entry

    return (NewEntry);
} // REFIT_MENU_SCREEN* CopyMenuScreen()

// Creates a copy of a menu entry. Intended to enable moving a stack-based
// menu entry (such as the ones for the "reboot" and "exit" functions) to
// to the heap. This enables easier deletion of the whole set of menu
// entries when re-scanning.
// Returns a pointer to the copy of the menu entry.
REFIT_MENU_ENTRY *
CopyMenuEntry (
    REFIT_MENU_ENTRY *Entry
) {
    REFIT_MENU_ENTRY *NewEntry;

    NewEntry = AllocateCopyPool (sizeof (REFIT_MENU_ENTRY), Entry);
    if (Entry && NewEntry) {
        ZeroPoolStr (&NewEntry->Title); // set to null so CopyPoolStr doesn't try to free it
        ZeroPoolImage (&NewEntry->BadgeImage);
        ZeroPoolImage (&NewEntry->Image);
#if REFIT_DEBUG > 0
        switch (Entry->Tag) {
            case TAG_NVRAMCLEAN: 
            case TAG_SHOW_BOOTKICKER: 
            case TAG_LOADER: 
            case TAG_TOOL: 
            case TAG_FIRMWARE_LOADER: 
            case TAG_LEGACY: 
            case TAG_LEGACY_UEFI:
                DumpCallStack (NULL, FALSE);
                break;
        }
#endif
        CopyPoolStr (&NewEntry->Title, Entry->Title_PS_.Str); // the source string might not be a pool str
        CopyFromPoolImage (&NewEntry->BadgeImage, &Entry->BadgeImage);
        CopyFromPoolImage (&NewEntry->Image, &Entry->Image);

        NewEntry->SubScreen = CopyMenuScreen (Entry->SubScreen);
    } // if
    return (NewEntry);
} // REFIT_MENU_ENTRY* CopyMenuEntry()

VOID
FreeMenuEntry (
    REFIT_MENU_ENTRY **Entry
) {
    if (Entry && *Entry) {
        MsgLog ("[ FreeMenuEntry %p %s\n", (*Entry), GetPoolStr (&(*Entry)->Title));
        //LOGPOOLALWAYS ((*Entry));
        FreePoolStr (&(*Entry)->Title);
        FreePoolImage (&(*Entry)->BadgeImage);
        FreePoolImage (&(*Entry)->Image);
        FreeMenuScreen (&(*Entry)->SubScreen);

        switch ((*Entry)->Tag) {
            case TAG_NVRAMCLEAN:
            case TAG_SHOW_BOOTKICKER:
            case TAG_LOADER:
            case TAG_TOOL:
            case TAG_FIRMWARE_LOADER:
                FreePoolStr (&((LOADER_ENTRY *)(*Entry))->Title);
                FreePoolStr (&((LOADER_ENTRY *)(*Entry))->LoaderPath);
                FreePoolStr (&((LOADER_ENTRY *)(*Entry))->LoadOptions);
                FreePoolStr (&((LOADER_ENTRY *)(*Entry))->InitrdPath);
                MyFreePool (&((LOADER_ENTRY *)(*Entry))->EfiLoaderPath);
                break;

            case TAG_LEGACY:
            case TAG_LEGACY_UEFI:
                FreePoolStr (&((LEGACY_ENTRY *)(*Entry))->LoadOptions);
                break;
        }

        MyFreePool (Entry);
        MsgLog ("] FreeMenuEntry\n");
    }
}

VOID
FreeMenuScreen (
    REFIT_MENU_SCREEN **Menu
) {
    if (Menu && *Menu) {
        MsgLog ("[ FreeMenuScreen %s\n", GetPoolStr(&(*Menu)->Title) ? GetPoolStr(&(*Menu)->Title) : L"NULL");

        FreePoolStr (&(*Menu)->Title);

        FreePoolImage (&(*Menu)->TitleImage);
        
        if ((*Menu)->InfoLines) {
            UINTN i;
            for (i = 0; i < (*Menu)->InfoLineCount; i++) {
                FreePoolStr_PS_ (&(*Menu)->InfoLines[i]);
            }
            MyFreePool (&(*Menu)->InfoLines);
            (*Menu)->InfoLineCount = 0;
        }
    
        if ((*Menu)->Entries) {
            UINTN i;
            for (i = 0; i < (*Menu)->EntryCount; i++) {
                FreeMenuEntry (&(*Menu)->Entries[i]);
            }
            MyFreePool (&(*Menu)->Entries);
            (*Menu)->EntryCount = 0;
        }
        
        FreePoolStr (&(*Menu)->TimeoutText);
        FreePoolStr (&(*Menu)->Hint1);
        FreePoolStr (&(*Menu)->Hint2);

        MyFreePool (Menu);
        MsgLog ("] FreeMenuScreen\n");
    }
}


#if REFIT_DEBUG > 0

VOID
LEAKABLEMENUENTRY (
    REFIT_MENU_ENTRY *Entry
) {
    if (Entry) {
        LEAKABLEPATHINC ();
            LEAKABLEPOOLSTR (&Entry->Title_PS_, "Menu Entry Title");
            LEAKABLEPOOLIMAGE (&Entry->BadgeImage_PI_);
            LEAKABLEPOOLIMAGE (&Entry->Image_PI_);
            LEAKABLEMENU (Entry->SubScreen);

            BOOLEAN IsLoader = FALSE;
            BOOLEAN IsLegacy = FALSE;

            switch (Entry->Tag) {
                case TAG_NVRAMCLEAN:
                case TAG_SHOW_BOOTKICKER:
                case TAG_LOADER:
                case TAG_TOOL:
                case TAG_FIRMWARE_LOADER:
                    IsLoader = TRUE;
                    break;

                case TAG_LEGACY:
                case TAG_LEGACY_UEFI:
                    IsLegacy = TRUE;
                    break;
            }

            LEAKABLEPOOLSTR  (IsLoader ? &((LOADER_ENTRY *)Entry)->Title_PS_       : NULL, "Loader Entry Title");
            LEAKABLEPOOLSTR  (IsLoader ? &((LOADER_ENTRY *)Entry)->LoaderPath_PS_  : NULL, "Loader Entry Loader Path");
            LEAKABLEPOOLSTR  (IsLoader ? &((LOADER_ENTRY *)Entry)->LoadOptions_PS_ : NULL, "Loader Entry Loader Options");
            LEAKABLEPOOLSTR  (IsLoader ? &((LOADER_ENTRY *)Entry)->InitrdPath_PS_  : NULL, "Loader Entry Initrd Path");
            LEAKABLEWITHPATH (IsLoader ? ((LOADER_ENTRY *)Entry)->EfiLoaderPath    : NULL, "Loader Efi Loader Path");

            LEAKABLEPOOLSTR  (IsLegacy ? &((LEGACY_ENTRY *)Entry)->LoadOptions_PS_ : NULL, "Legacy Entry Loader Options");
        LEAKABLEPATHDEC ();
    }
    LEAKABLEWITHPATH (Entry, "Menu Entry");
}


VOID
LEAKABLEMENU (
    REFIT_MENU_SCREEN *Menu
) {
    if (Menu) {
        LEAKABLEPATHINC ();

            LEAKABLEPOOLSTR (&Menu->Title_PS_, "Menu Title");
            LEAKABLEPOOLIMAGE (&Menu->TitleImage_PI_);

            if (Menu->InfoLines) {
                UINTN i;
                LEAKABLEPATHINC ();
                    for (i = 0; i < Menu->InfoLineCount; i++) {
                        LEAKABLEPOOLSTR (&Menu->InfoLines[i], "Menu Info Lines Info Line");
                    }
                LEAKABLEPATHDEC ();
            }
            LEAKABLEWITHPATH (Menu->InfoLines, "Menu Info Lines");
    
            if (Menu->Entries) {
                UINTN i;
                LEAKABLEPATHINC ();
                    for (i = 0; i < Menu->EntryCount; i++) {
                        LEAKABLEMENUENTRY (Menu->Entries[i]);
                    }
                LEAKABLEPATHDEC ();
            }
            LEAKABLEWITHPATH (Menu->Entries, "Menu Entries");

            LEAKABLEPOOLSTR (&Menu->TimeoutText_PS_, "Menu TimeoutText");
            LEAKABLEPOOLSTR (&Menu->Hint1_PS_, "Menu Hint1");
            LEAKABLEPOOLSTR (&Menu->Hint2_PS_, "Menu Hint2");

        LEAKABLEPATHDEC ();
    }
    LEAKABLEWITHPATH (Menu, "Menu");
}


VOID
LEAKABLEROOTMENU (
    UINT16 LeakableObjectID,
    REFIT_MENU_SCREEN *Menu
) {
    MsgLog ("[ LEAKABLEROOTMENU\n");
    LEAKABLEPATHINIT (LeakableObjectID);
    LEAKABLEMENU (Menu);
    LEAKABLEPATHDONE ();
    MsgLog ("] LEAKABLEROOTMENU\n");
}

#endif
