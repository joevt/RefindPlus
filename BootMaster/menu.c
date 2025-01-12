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
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
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
#include "../include/version.h"
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
static UINTN  TileSizes[2] = { 144, 64 };

// Text and icon spacing constants.
#define TEXT_YMARGIN       (2)
#define TITLEICON_SPACING (16)

#define TILE_XSPACING      (8)
#define TILE_YSPACING     (16)

// Alignment values for PaintIcon()
#define ALIGN_RIGHT 1
#define ALIGN_LEFT  0

static EG_IMAGE *SelectionImages[2]      = { NULL, NULL };
static EG_PIXEL SelectionBackgroundPixel = { 0xff, 0xff, 0xff, 0 };

EFI_EVENT *WaitList       = NULL;
UINT64     MainMenuLoad   = 0;
UINTN      WaitListLength = 0;

// Pointer variables
BOOLEAN PointerEnabled    = FALSE;
BOOLEAN PointerActive     = FALSE;
BOOLEAN DrawSelection     = TRUE;

extern EFI_GUID          RefindPlusGuid;

extern CHAR16           *VendorInfo;

extern BOOLEAN           FlushFailedTag;
extern BOOLEAN           FlushFailReset;
extern BOOLEAN           ClearedBuffer;

REFIT_MENU_ENTRY TagMenuEntry[] = {
    #define TAGS_MENU
    #include "tags.include"
};

#if REFIT_DEBUG > 0
STATIC UINTN MinAllocation = 0;
#endif


extern UINT64 GetCurrentMS (VOID);

//
// Graphics helper functions
//

static
VOID InitSelection (VOID) {
    LOGPROCENTRY();
    EG_IMAGE  *TempSmallImage    = NULL;
    EG_IMAGE  *TempBigImage      = NULL;
    BOOLEAN    LoadedSmallImage  = FALSE;
    BOOLEAN    TaintFree         = TRUE;

    if (!AllowGraphicsMode || (SelectionImages[0] != NULL)) {
        LOGPROCEXIT("AllowGraphicsMode:%d SelectionImages[0]:%d", AllowGraphicsMode, SelectionImages[0] != NULL);
        return;
    }

    // load small selection image
    if (GlobalConfig.SelectionSmallFileName != NULL) {
        TempSmallImage = egLoadImage (SelfDir, GlobalConfig.SelectionSmallFileName, TRUE);
    }

    if (TempSmallImage == NULL) {
        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL, L"Using Embedded Selection Image:- 'egemb_back_selected_small'");
        #endif

        TempSmallImage = egPrepareEmbeddedImage (&egemb_back_selected_small, TRUE, NULL);
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
            #if REFIT_DEBUG > 0
            LOG(2, LOG_LINE_NORMAL, L"Scaling Selection Image from LoadedSmallImage");
            #endif

            // calculate big selection image from small one
            TempBigImage = egCopyImage (TempSmallImage);
        }
        else {
            #if REFIT_DEBUG > 0
            LOG(2, LOG_LINE_NORMAL, L"Using Embedded Selection Image:- 'egemb_back_selected_big'");
            #endif

            TempBigImage = egPrepareEmbeddedImage (&egemb_back_selected_big, TRUE, NULL);
        }
    }

    if ((TempBigImage->Width != TileSizes[0]) || (TempBigImage->Height != TileSizes[0])) {
        SelectionImages[0] = egScaleImage (TempBigImage, TileSizes[0], TileSizes[0]);
    }
    else {
        SelectionImages[0] = egCopyImage (TempBigImage);
    }
    LEAKABLEONEIMAGE(SelectionImages[0], "SelectionImages 0");

    MY_FREE_IMAGE(TempSmallImage);
    MY_FREE_IMAGE(TempBigImage);
    LOGPROCEXIT();
} // VOID InitSelection()

//
// Scrolling functions
//

static
VOID InitScroll (
    OUT SCROLL_STATE *State,
    IN UINTN          ItemCount,
    IN UINTN          VisibleSpace
) {
    State->PreviousSelection = State->CurrentSelection = 0;
    State->MaxIndex          = (INTN)ItemCount - 1;
    State->FirstVisible      = 0;
    State->MaxVisible        = (INTN)VisibleSpace;
    State->PaintAll          = TRUE;
    State->PaintSelection    = FALSE;
    State->LastVisible       = State->FirstVisible + State->MaxVisible - 1;
}

// Adjust variables relating to the scrolling of tags, for when a selected icon
// is not visible given the current scrolling condition.
static
VOID AdjustScrollState (
    IN SCROLL_STATE *State
) {
    // Scroll forward
    if (State->CurrentSelection > State->LastVisible) {
        State->LastVisible   = State->CurrentSelection;
        State->FirstVisible  = 1 + State->CurrentSelection - State->MaxVisible;

        if (State->FirstVisible < 0) {
            // should not happen, but just in case.
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
    IN UINTN              Movement
) {
    State->PreviousSelection = State->CurrentSelection;

    switch (Movement) {
        case SCROLL_NONE:       break;
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
                        State->CurrentSelection = State->FirstVisible
                            + (State->LastVisible      - State->FirstVisible)
                            * (State->CurrentSelection - State->InitialRow1)
                            / (State->MaxIndex         - State->InitialRow1);
                    }
                    else {
                        State->CurrentSelection = State->FirstVisible;
                    }
                }
            }
            else {
                if (State->CurrentSelection > 0) {
                    State->CurrentSelection--;
                }
            }

            break;

        case SCROLL_LINE_DOWN:
            if (State->ScrollMode == SCROLL_MODE_ICONS) {
                if (State->CurrentSelection <= State->FinalRow0) {
                    if (State->LastVisible > State->FirstVisible) {
                        // avoid division by 0!
                        State->CurrentSelection = State->InitialRow1 +
                            (State->MaxIndex         - State->InitialRow1) *
                            (State->CurrentSelection - State->FirstVisible) /
                            (State->LastVisible      - State->FirstVisible);
                    }
                    else {
                        State->CurrentSelection = State->InitialRow1;
                    }
                }
            }
            else {
                if (State->CurrentSelection < State->MaxIndex) {
                    State->CurrentSelection++;
                }
            }

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
            if (State->CurrentSelection  < State->FinalRow0) {
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
    } // switch

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

// Returns a constant ... do not free
CHAR16 * MenuExitInfo (
    IN UINTN MenuExit
) {
    CHAR16 *MenuExitData = NULL;

    switch (MenuExit) {
        case 1:  MenuExitData = L"ENTER";   break;
        case 2:  MenuExitData = L"ESCAPE";  break;
        case 3:  MenuExitData = L"DETAILS"; break;
        case 4:  MenuExitData = L"TIMEOUT"; break;
        case 5:  MenuExitData = L"EJECT";   break;
        case 6:  MenuExitData = L"REMOVE";  break;
        default: MenuExitData = L"RETURN";  // Actually '99'
    } // switch

    return MenuExitData;
} // CHAR16 * MenuExitInfo()

VOID AddMenuInfoLine (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16            *InfoLine,
    IN BOOLEAN            Cached
) {
    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL, L"Adding Menu Info Line:- '%s'", InfoLine);
    #endif

    if (!Cached) { if (LOGPOOL (InfoLine)); }
    PoolStr Str;
    Str.Str = InfoLine;
    Str.Cached = Cached;
    AddListElementSized ((VOID **) &(Screen->InfoLines), &(Screen->InfoLineCount), &Str, sizeof(Str));
}

VOID AddMenuInfoLinePool (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16 *InfoLine
) {
    AddMenuInfoLine (Screen, InfoLine, FALSE);
}

VOID AddMenuInfoLineCached (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16 *InfoLine
) {
    AddMenuInfoLine (Screen, InfoLine, TRUE);
}

VOID AddMenuInfoLinePoolStr_PS_ (
    IN REFIT_MENU_SCREEN *Screen,
    IN PoolStr *InfoLine
) {
    if (InfoLine && InfoLine->Str) {
        if (InfoLine->Cached) {
            AddMenuInfoLineCached (Screen, InfoLine->Str);
        }
        else {
            AddMenuInfoLinePool (Screen, StrDuplicate (InfoLine->Str));
        }
    }
    else {
        AddMenuInfoLineCached (Screen, NULL);
    }
}

VOID AddMenuEntry (
    IN REFIT_MENU_SCREEN *Screen,
    IN REFIT_MENU_ENTRY  *Entry
) {
    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Adding Menu Entry to %s - %s",
        Screen && GetPoolStr (&Screen->Title) ? GetPoolStr (&Screen->Title) : L"NULL",
        Entry && GetPoolStr (&Entry->Title ) ? GetPoolStr (&Entry->Title ) : L"NULL"
    );
    #endif

    if (!GetPoolStr (&Screen->Title)) {
        DumpCallStack (NULL, FALSE);
    }
    if (LOGPOOL (Entry));
    AddListElement ((VOID ***) &(Screen->Entries), &(Screen->EntryCount), Entry);
} // VOID AddMenuEntry()

VOID AddMenuEntryCopy (
    IN REFIT_MENU_SCREEN *Screen,
    IN REFIT_MENU_ENTRY  *Entry
) {
    AddMenuEntry (Screen, CopyMenuEntry (Entry));
}

static
INTN FindMenuShortcutEntry (
    IN REFIT_MENU_SCREEN *Screen,
    IN CHAR16            *Defaults
) {
    UINTN   i, j = 0, ShortcutLength;
    CHAR16 *Shortcut;

    while ((Shortcut = FindCommaDelimited (Defaults, j)) != NULL) {
        ShortcutLength = StrLen (Shortcut);
        if (ShortcutLength == 1) {
            if (Shortcut[0] >= 'a' && Shortcut[0] <= 'z') {
                Shortcut[0] -= ('a' - 'A');
            }

            if (Shortcut[0]) {
                for (i = 0; i < Screen->EntryCount; i++) {
                    if (Screen->Entries[i]->ShortcutDigit  == Shortcut[0] ||
                        Screen->Entries[i]->ShortcutLetter == Shortcut[0]
                    ) {
                        MY_FREE_POOL(Shortcut);

                        return i;
                    }
                } // for
            }
        }
        else if (ShortcutLength > 1) {
            for (i = 0; i < Screen->EntryCount; i++) {
                if (StriSubCmp (Shortcut, GetPoolStr (&Screen->Entries[i]->Title))) {
                    MY_FREE_POOL(Shortcut);

                    return i;
                }
            } // for
        }

        MY_FREE_POOL(Shortcut);
        j++;
    } // while

    return -1;
} // static INTN FindMenuShortcutEntry()

// Identify the end of row 0 and the beginning of row 1; store the results in the
// appropriate fields in State. Also reduce MaxVisible if that value is greater
// than the total number of row-0 tags and if we are in an icon-based screen
static
VOID IdentifyRows (
    IN SCROLL_STATE      *State,
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
        }
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
VOID SaveScreen (VOID) {
    LOGPROCENTRY();
    UINTN  retval;
    UINTN  ColourIndex;
    UINT64 TimeWait;
    UINT64 BaseTimeWait = 3750;

    #if REFIT_DEBUG > 0
    LOG2(2, LOG_LINE_NORMAL, L"INFO: ", L" ...", L"Keypress Wait Threshold Exceeded");
    LOG2(1, LOG_LINE_THIN_SEP, L"", L"\n", L"Start Screensaver");
    #endif

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
    LOGBLOCKENTRY("SaveScreen loop");
    for (;;) {
        ColourIndex = ColourIndex + 1;

        if (ColourIndex < 1 || ColourIndex > 30) {
            ColourIndex = 1;
            TimeWait    = TimeWait * 2;

            if (TimeWait > 120000) {
                // Reset TimeWait if greater than 2 minutes
                TimeWait = BaseTimeWait;

                #if REFIT_DEBUG > 0
                LOG(2, LOG_LINE_NORMAL, L"Reset Timeout");
                #endif
            }
            else {
                #if REFIT_DEBUG > 0
                LOG(2, LOG_LINE_NORMAL, L"Extend Timeout");
                #endif
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
    } // for
    LOGBLOCKEXIT("SaveScreen loop");

    #if REFIT_DEBUG > 0
    LOG2(2, LOG_LINE_NORMAL, L"", L" ...\n", L"Detected Keypress");
    LOG2(2, LOG_THREE_STAR_END, L"", L"\n\n", L"Ending Screensaver");
    #endif

    if (AllowGraphicsMode) {
        SwitchToGraphicsAndClear (TRUE);
    }

    ReadAllKeyStrokes();
    LOGPROCEXIT();
} // VOID SaveScreen()

//
// generic menu function
//
#if REFIT_DEBUG > 0
static
CHAR16 * GetScanCodeText (
    IN UINTN ScanCode
) {
    CHAR16 *retval = NULL;

    switch (ScanCode) {
        case SCAN_END:       retval = L"SCROLL_LAST";   break;
        case SCAN_HOME:      retval = L"SCROLL_FIRST";  break;
        case SCAN_PAGE_UP:   retval = L"PAGE_UP";       break;
        case SCAN_PAGE_DOWN: retval = L"PAGE_DOWN";     break;
        case SCAN_UP:        retval = L"ARROW_UP";      break;
        case SCAN_LEFT:      retval = L"ARROW_LEFT";    break;
        case SCAN_DOWN:      retval = L"ARROW_DOWN";    break;
        case SCAN_RIGHT:     retval = L"ARROW_RIGHT";   break;
        case SCAN_ESC:       retval = L"ESC-Rescan";    break;
        case SCAN_DELETE:    retval = L"DEL-Hide";      break;
        case SCAN_INSERT:    retval = L"INS-Details";   break;
        case SCAN_F2:        retval = L"F2-Details";    break;
        case SCAN_F10:       retval = L"F10-ScrnSht";   break; // Using 'ScrnSht' to limit length
        case 0x0016:         retval = L"F12-Eject";     break;
        default:             retval = L"KEY_UNKNOWN";   break;
    } // switch

    return retval;
} // static CHAR16 * GetScanCodeText()
#endif

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
    UINTN          ElapsCount;
    UINTN          MenuExit = 0;
    UINTN          Input;
    UINTN          Item;
    CHAR16        *TimeoutMessage;
    CHAR16         KeyAsString[2];
    SCROLL_STATE   State;
    EFI_INPUT_KEY  key;

    LOGPROCENTRY("'%s'", GetPoolStr (&Screen->Title));

    #if REFIT_DEBUG > 0
    LOG(1, LOG_THREE_STAR_SEP, L"Entering RunGenericMenu");
    LOG(2, LOG_LINE_NORMAL, L"Running Menu Screen:- '%s'", GetPoolStr (&Screen->Title));
    #endif

    if (Screen->TimeoutSeconds > 0) {
        HaveTimeout      = TRUE;
        TimeoutCountdown = Screen->TimeoutSeconds * 10;
    }

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
        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
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

    BOOLEAN Toggled = FALSE;
    while (MenuExit == 0) {
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
            Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
            if (Status == EFI_SUCCESS) {
                // reset, because otherwise the buffer gets queued with keystrokes
                REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, FALSE);
                REFIT_CALL_1_WRAPPER(gBS->Stall, 100000);
            }
            else {
                WaitForRelease = FALSE;
                REFIT_CALL_2_WRAPPER(gST->ConIn->Reset, gST->ConIn, TRUE);
            }

            continue;
        }

        // DA-TAG: Toggle the selection once to workaround failure
        //         to display default selection on load in text mode.
        //         Workaround ... 'Proper' solution needed.
        if (!Toggled) {
            Toggled = TRUE;
            if (State.ScrollMode == SCROLL_MODE_TEXT) {
                if (State.CurrentSelection < State.MaxIndex) {
                    UpdateScroll (&State, SCROLL_LINE_DOWN);
                    REFIT_CALL_1_WRAPPER(gBS->Stall, 5000);
                    UpdateScroll (&State, SCROLL_LINE_UP);
                }
                else if (State.CurrentSelection > 0) {
                    UpdateScroll (&State, SCROLL_LINE_UP);
                    REFIT_CALL_1_WRAPPER(gBS->Stall, 5000);
                    UpdateScroll (&State, SCROLL_LINE_DOWN);
                }
                else {
                    UpdateScroll (&State, SCROLL_NONE);
                }
            }
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

               MY_FREE_POOL(TimeoutMessage);

                PreviousTime = CurrentTime;
            }
        }

        // read key press or pointer event (and wait for them if applicable)
        if (PointerEnabled) {
            PointerStatus = pdUpdateState();
        }
        Status = REFIT_CALL_2_WRAPPER(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);

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

            PointerActive      = TRUE;
            TimeSinceKeystroke = 0;
        }
        else {
            if (HaveTimeout && TimeoutCountdown == 0) {
                // timeout expired
                #if REFIT_DEBUG > 0
                LOG(2, LOG_LINE_NORMAL, L"Menu Timeout Expired:- '%d Seconds'", Screen->TimeoutSeconds);
                #endif

                MenuExit = MENU_EXIT_TIMEOUT;
                break;
            }
            else if (HaveTimeout || GlobalConfig.ScreensaverTime > 0) {
                ElapsCount = 1;
                Input      = WaitForInput (1000); // 1s Timeout

                if (Input == INPUT_KEY || Input == INPUT_POINTER) {
                    TimeSinceKeystroke = 0;
                    continue;
                }
                else if (Input == INPUT_TIMEOUT) {
                    ElapsCount = 10; // always counted as 1s to end of the timeout
                }

                TimeSinceKeystroke += ElapsCount;
                if (HaveTimeout) {
                    TimeoutCountdown = (TimeoutCountdown > ElapsCount)
                        ? TimeoutCountdown - ElapsCount : 0;
                }
                else if (GlobalConfig.ScreensaverTime > 0 &&
                    TimeSinceKeystroke > (GlobalConfig.ScreensaverTime * 10)
                ) {
                    SaveScreen();
                    State.PaintAll     = TRUE;
                    TimeSinceKeystroke = 0;
                }
            }
            else {
                WaitForInput (0);
            } // if/else HaveTimeout

            continue;
        } // if/else Status == EFI_SUCCESS

        if (HaveTimeout) {
            // the user pressed a key, cancel the timeout
            StyleFunc (Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, L"");
            HaveTimeout = FALSE;

            if (GlobalConfig.ScreensaverTime == -1) {
                // cancel start-with-blank-screen coding
                GlobalConfig.ScreensaverTime = 0;

                if (!GlobalConfig.TextOnly) {
                    BltClearScreen (TRUE);
                }
            }
        }

        if (!PointerActive) {
            // react to key press
            switch (key.ScanCode) {
                case SCAN_END:       UpdateScroll (&State, SCROLL_LAST);       break;
                case SCAN_HOME:      UpdateScroll (&State, SCROLL_FIRST);      break;
                case SCAN_PAGE_UP:   UpdateScroll (&State, SCROLL_PAGE_UP);    break;
                case SCAN_PAGE_DOWN: UpdateScroll (&State, SCROLL_PAGE_DOWN);  break;
                case SCAN_UP:        UpdateScroll (&State, SCROLL_LINE_UP);    break;
                case SCAN_LEFT:      UpdateScroll (&State, SCROLL_LINE_LEFT);  break;
                case SCAN_DOWN:      UpdateScroll (&State, SCROLL_LINE_DOWN);  break;
                case SCAN_RIGHT:     UpdateScroll (&State, SCROLL_LINE_RIGHT); break;
                case SCAN_INSERT:
                case SCAN_F2:        MenuExit = MENU_EXIT_DETAILS;             break;
#if REFIT_DEBUG > 0
                case SCAN_F5:        MinAllocation = GetNextAllocationNum ();  break;
                case SCAN_F6:        DumpAllocations (MinAllocation, TRUE, 0); break;
#endif
                case SCAN_ESC:       MenuExit = MENU_EXIT_ESCAPE;              break;
                case SCAN_DELETE:    MenuExit = MENU_EXIT_HIDE;                break;
                case SCAN_F10:       egScreenShot();                           break;
                case SCAN_F12:
                    if (EjectMedia()) {
                        MenuExit = MENU_EXIT_ESCAPE;
                    }

                    break;
            } // switch

            switch (key.UnicodeChar) {
                case ' ':
                case CHAR_LINEFEED:
                case CHAR_CARRIAGE_RETURN: MenuExit = MENU_EXIT_ENTER;   break;
                case CHAR_BACKSPACE:       MenuExit = MENU_EXIT_ESCAPE;  break;
                case '+':
                case CHAR_TAB:             MenuExit = MENU_EXIT_DETAILS; break;
                case '-':                  MenuExit = MENU_EXIT_HIDE;    break;
                default:
                    KeyAsString[0] = key.UnicodeChar;
                    KeyAsString[1] = 0;
                    ShortcutEntry  = FindMenuShortcutEntry (Screen, KeyAsString);

                    if (ShortcutEntry >= 0) {
                        State.CurrentSelection = ShortcutEntry;
                        MenuExit = MENU_EXIT_ENTER;
                    }

                    break;
            } // switch

            #if 0
            #if REFIT_DEBUG > 0
            CHAR16 *KeyTxt = GetScanCodeText (key.ScanCode);
            if (MyStriCmp (KeyTxt, L"KEY_UNKNOWN")) {
                switch (key.UnicodeChar) {
                    case ' ':                  KeyTxt = L"INFER_ENTER    Key=SpaceBar";        break;
                    case CHAR_LINEFEED:        KeyTxt = L"INFER_ENTER    Key=LineFeed";        break;
                    case CHAR_CARRIAGE_RETURN: KeyTxt = L"INFER_ENTER    Key=CarriageReturn";  break;
                    case CHAR_BACKSPACE:       KeyTxt = L"INFER_ESCAPE   Key=BackSpace";       break;
                    case CHAR_TAB:             KeyTxt = L"INFER_DETAILS  Key=Tab";             break;
                    case '+':                  KeyTxt = L"INFER_DETAILS  Key='+'...'Plus'";    break;
                    case '-':                  KeyTxt = L"INFER_REMOVE   Key='-'...'Minus'";   break;
                } // switch
            }
            LOG(2, LOG_LINE_NORMAL,
                L"Processing Keystroke: UnicodeChar = 0x%02X ... ScanCode = 0x%02X - %s",
                key.UnicodeChar, key.ScanCode, KeyTxt
            );
            #endif
            #endif
        }
        else {
            //react to pointer event
            #if REFIT_DEBUG > 0
            LOG(2, LOG_LINE_NORMAL, L"Processing Pointer Event");
            #endif

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
            } // switch
        } // if/else !PointerActive
    } // while

    pdClear();
    StyleFunc (Screen, &State, MENU_FUNCTION_CLEANUP, NULL);

    // Ignore MenuExit if FlushFailedTag is set and not previously reset
    if (FlushFailedTag && !FlushFailReset) {
        #if REFIT_DEBUG > 0
        CHAR16 *MsgStr = StrDuplicate (L"FlushFailedTag is Set ... Ignore MenuExit");
        LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
        MsgLog ("INFO: %s\n\n", MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif

        FlushFailedTag = FALSE;
        FlushFailReset = TRUE;
        MenuExit = 0;
    }

    // Ignore MenuExit if time between loading main menu and detecting an 'Enter' keypress is too low
    // Primed Keystroke Buffer appears to only affect UEFI PC
    if (GlobalConfig.Timeout > -1 &&
        MenuExit == MENU_EXIT_ENTER &&
        !ClearedBuffer && !FlushFailReset &&
        MyStriCmp (GetPoolStr (&Screen->Title), L"Main Menu")
    ) {
        UINT64 MenuExitTime = GetCurrentMS();
        UINT64 MenuExitDiff = MenuExitTime - MainMenuLoad;

        if (MenuExitDiff < 1250) {
            #if REFIT_DEBUG > 0
            MsgLog ("INFO: Invalid Post-Load MenuExit Interval ... Ignoring MenuExit");
            MsgLog ("\n");

            CHAR16 *MsgStr = StrDuplicate (L"Mitigated Potential Persistent Primed Keystroke Buffer");
            LOG(1, LOG_STAR_SEPARATOR, L"%s", MsgStr);
            MsgLog ("      %s", MsgStr);
            MsgLog ("\n\n");
            MY_FREE_POOL(MsgStr);
            #endif

            FlushFailedTag = FALSE;
            FlushFailReset = TRUE;
            MenuExit = 0;
        }
    }

    if (ChosenEntry) {
        *ChosenEntry = Screen->Entries[State.CurrentSelection];
    }

    *DefaultEntryIndex = State.CurrentSelection;

    LOGPROCEXIT("...%d", MenuExit);

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
        REFIT_CALL_2_WRAPPER(
            gST->ConOut->SetAttribute,
            gST->ConOut,
            ATTR_BASIC
        );

        for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
            REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 3, 4 + i);
            REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, GetPoolStr_PS_ (&Screen->InfoLines[i]));
        }
    }
} // VOID ShowTextInfoLines()

// Do most of the work for text-based menus.
VOID TextMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              Function,
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
                // description of the StrSize parameter to SPrint implies it is measured
                // in characters, but in practice both TianoCore and GNU-EFI seem to
                // use bytes instead, resulting in truncated displays. I could just
                // double the size of the StrSize parameter, but that seems unsafe in
                // case a future library change starts treating this as characters, so
                // I'm doing it the hard way in this instance.
                // TODO: Review the above and possibly change other uses of SPrint()
                DisplayStrings[i] = AllocateZeroPool (2 * sizeof (CHAR16));
                DisplayStrings[i][0] = L' ';
                MuteLogger = TRUE;
                MergeStrings (&DisplayStrings[i], GetPoolStr (&Screen->Entries[i]->Title), 0);
                MuteLogger = FALSE;
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
                MY_FREE_POOL(DisplayStrings[i]);
            }
            MY_FREE_POOL(DisplayStrings);

            break;

        case MENU_FUNCTION_PAINT_ALL:
            // paint the whole screen (initially and after scrolling)

            ShowTextInfoLines (Screen);
            for (i = 0; i <= State->MaxIndex; i++) {
                if (i >= State->FirstVisible && i <= State->LastVisible) {
                    REFIT_CALL_3_WRAPPER(
                        gST->ConOut->SetCursorPosition,
                        gST->ConOut,
                        2,
                        MenuPosY + (i - State->FirstVisible)
                    );

                    if (i == State->CurrentSelection) {
                        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_CHOICE_CURRENT);
                    }
                    else if (DisplayStrings[i]) {
                        REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute, gST->ConOut, ATTR_CHOICE_BASIC);
                        REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, DisplayStrings[i]);
                    }
                }
            }

            // scrolling indicators
            REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_SCROLLARROW);
            REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, MenuPosY);

            if (State->FirstVisible > 0) {
                gST->ConOut->OutputString (gST->ConOut, ArrowUp);
            }
            else {
                gST->ConOut->OutputString (gST->ConOut, L" ");
            }

            gST->ConOut->SetCursorPosition (gST->ConOut, 0, MenuPosY + State->MaxVisible);

            if (State->LastVisible < State->MaxIndex) {
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, ArrowDown);
            }
            else {
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString, gST->ConOut, L" ");
            }

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
                if (GetPoolStr (&Screen->Hint1) != NULL) {
                    REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 2);
                    REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, GetPoolStr (&Screen->Hint1));
                }
                if (GetPoolStr (&Screen->Hint2) != NULL) {
                    REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 1);
                    REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, GetPoolStr (&Screen->Hint2));
                }
            }

            break;

        case MENU_FUNCTION_PAINT_SELECTION:
            // redraw selection cursor
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition,
                gST->ConOut, 2,
                MenuPosY + (State->PreviousSelection - State->FirstVisible)
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut,
                ATTR_CHOICE_BASIC
            );
            if (DisplayStrings[State->PreviousSelection] != NULL) {
                REFIT_CALL_2_WRAPPER(
                    gST->ConOut->OutputString,
                    gST->ConOut,
                    DisplayStrings[State->PreviousSelection]
                );
            }
            REFIT_CALL_3_WRAPPER(
                gST->ConOut->SetCursorPosition,
                gST->ConOut, 2,
                MenuPosY + (State->CurrentSelection - State->FirstVisible)
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->SetAttribute,
                gST->ConOut,
                ATTR_CHOICE_CURRENT
            );
            REFIT_CALL_2_WRAPPER(
                gST->ConOut->OutputString,
                gST->ConOut,
                DisplayStrings[State->CurrentSelection]
            );

            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            if (ParamText[0] == 0) {
                // clear message
                REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_BASIC);
                REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 0, ConHeight - 3);
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, BlankLine + 1);
            }
            else {
                // paint or update message
                REFIT_CALL_2_WRAPPER(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_ERROR);
                REFIT_CALL_3_WRAPPER(gST->ConOut->SetCursorPosition, gST->ConOut, 3, ConHeight - 3);
                REFIT_CALL_2_WRAPPER(gST->ConOut->OutputString,      gST->ConOut, ParamText);
            }

            break;
    }
}

//
// graphical generic style
//

inline static
UINTN TextLineHeight (VOID) {
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
    IN BOOLEAN  Selected,
    IN UINTN    FieldWidth,
    IN UINTN    XPos,
    IN UINTN    YPos
) {
    EG_IMAGE *TextBuffer;
    EG_PIXEL  Bg;

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

        // Get Luminance Index
        UINTN FactorFP = 10;
        UINTN Divisor  = 3 * FactorFP;
        UINTN PixelsR  = (UINTN) Bg.r;
        UINTN PixelsG  = (UINTN) Bg.g;
        UINTN PixelsB  = (UINTN) Bg.b;
        UINTN LumIndex = (
            (
                (PixelsR * FactorFP) +
                (PixelsG * FactorFP) +
                (PixelsB * FactorFP) +
                (Divisor / 2) // Added For Rounding
            ) / Divisor
        );

        // render the text
        egRenderText (
            Text,
            TextBuffer,
            egGetFontCellWidth(),
            TEXT_YMARGIN,
            (UINT8) LumIndex
        );

        egDrawImageWithTransparency (
            TextBuffer, NULL,
            XPos, YPos,
            TextBuffer->Width,
            TextBuffer->Height
        );

        MY_FREE_IMAGE(TextBuffer);
    }
} // VOID DrawText()

// Finds the average brightness of the input Image.
// NOTE: Passing an Image that covers the whole screen can strain the
// capacity of a UINTN on a 32-bit system with a very large display.
// Using UINT64 instead is unworkable, as the code will not compile
// on a 32-bit system. As the intended use for this function is to
// handle a single text string's background, this should not be a
// problem, but may need addressing if applied more broadly.
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
    }

    return (UINT8) Sum;
} // UINT8 AverageBrightness()

// Display text against the screen's background image. Special case: If Text is NULL
// or 0-length, clear the line. Does NOT indent the text or reposition it relative
// to the specified XPos and YPos values.
static
VOID DrawTextWithTransparency (
    IN CHAR16 *Text,
    IN UINTN   XPos,
    IN UINTN   YPos
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

    MY_FREE_IMAGE(TextBuffer);
}

/* Compute the size & position of the window that will hold a subscreen's information.

    |<-----Width-------->|
    |IconX |TextX        |
    |      |<-LineWidth->|
____v____________________v___ BannerBottom
F    ________________________ YPos
L   |-------title--------|          ^
L   |       ExtraLine1   |          |
L   |++++++-infoline0-   |___ IconY |
L   |++++++-infoline1-   |         Height <= AllowedHeight
L   |+big++ ExtraLine2   |          |
L   |+icon+-entry0-      |          |
L   |++++++-entry1-      |          |
L   |++++++-entry2-______|__        v
F                   ________
F         hint1              HintTop
F         hint2
F___________________________ ScreenH

L = TextLineHeight = egGetFontHeight() + TEXT_YMARGIN * 2
F = FontCellHeight = egGetFontHeight()
*/

static
VOID ComputeSubScreenWindowSize (
    REFIT_MENU_SCREEN *Screen,
    UINTN *XPos, UINTN *YPos,
    UINTN *Width, UINTN *Height,
    UINTN *LineWidth,
    UINTN *IconX, UINTN *IconY, UINTN *TextX,
    UINTN *MaxVisibleInfoLines, UINTN *MaxVisibleEntries,
    UINTN *BannerBottomEdge, UINTN *HintTop
) {
    LOGPROCENTRY();
    INTN i;
    UINTN FontCellWidth  = egGetFontCellWidth();
    UINTN FontCellHeight = egGetFontHeight();
    UINTN TopBottomBorder;
    INTN AllowedHeight;
    UINTN ImageWidth = 0;
    UINTN ImageHeight = 0;

    { // calculate the height first - we'll remove info lines if they don't fit
        *HintTop = ScreenH - (FontCellHeight * 3); // top of hint text
        if (GlobalConfig.BannerBottomEdge >= *HintTop) {
            *BannerBottomEdge = 0; // probably a full-screen image; treat it as an empty banner
        }
        else {
            *BannerBottomEdge = GlobalConfig.BannerBottomEdge;
        }

        TopBottomBorder = FontCellHeight;
        for (i = 0; ; i++) {
            AllowedHeight = *HintTop - *BannerBottomEdge - TopBottomBorder * 2;
            if (AllowedHeight >= TextLineHeight() * 3) {
                break;
            }
            if (i == 0) { // remove the banner
                *BannerBottomEdge = 0;
            }
            else if (i == 1) { // remove the hints
                *HintTop = ScreenH;
            }
            else if (i == 2) { // remove the border
                TopBottomBorder = 0;
            }
            else {
                break;
            }
        }

        *MaxVisibleEntries = Screen->EntryCount;
        *MaxVisibleInfoLines = Screen->InfoLineCount;

        for (i = 0; ; i++) {
            INTN LinesRemaining = AllowedHeight / TextLineHeight() - 1; // subtract one for the title
            INTN ExtraLine1 = (*MaxVisibleInfoLines) ? 1 : 0;
            INTN NumInfoLines = *MaxVisibleInfoLines;
            INTN ExtraLine2 = (*MaxVisibleEntries) ? 1 : 0;
            INTN NumEntryLines = (*MaxVisibleEntries > 1 ? 2 : *MaxVisibleEntries); // minimum that we need 0, 1, or 2
            if (LinesRemaining >= ExtraLine1 + NumInfoLines + ExtraLine2 + NumEntryLines) {
                LinesRemaining -= ExtraLine1 + NumInfoLines + ExtraLine2; // we have room for all the infolines and a minimum number of entries
                if (LinesRemaining < *MaxVisibleEntries) {
                    *MaxVisibleEntries = LinesRemaining; // if there's not enough room for all the entries then reduce the max
                }
                break;
            }

            if (i == 0) {
                *BannerBottomEdge = 0; // first, try removing the banner
                AllowedHeight = *HintTop - *BannerBottomEdge - TopBottomBorder * 2;
            }
            else if (i == 1) { // second, try removing some info lines
                if (LinesRemaining >= ExtraLine2 + NumEntryLines) { // can we remove some info lines to make the minimum number of entries fit?
                    LinesRemaining -= ExtraLine2 + NumEntryLines;
                    if (LinesRemaining > 0) LinesRemaining--;
                    *MaxVisibleInfoLines = LinesRemaining;
                    *MaxVisibleEntries = NumEntryLines;
                }
                else {
                    *MaxVisibleInfoLines = 0;
                    *MaxVisibleEntries = NumEntryLines;
                }
                break;
            }
        }

        *Height = (1 + (*MaxVisibleInfoLines ? 1 : 0) + *MaxVisibleInfoLines + (*MaxVisibleEntries ? 1 : 0) + *MaxVisibleEntries) * TextLineHeight();
    }

    { // calculate width second
        UINTN MaxWidth;
        #define CheckWidth(_str) do { CHAR16 *_s = (_str); if (_s) { UINTN ItemWidth = StrLen(_s); if (MaxWidth < ItemWidth) MaxWidth = ItemWidth; } } while (0)
        #define CheckImageWidth(_img) do { \
            EG_IMAGE *ImageToDraw = _img; \
            if (ImageToDraw) { \
                if (ImageToDraw->Width > ImageWidth) ImageWidth = ImageToDraw->Width; \
                if (ImageToDraw->Height > ImageHeight) ImageHeight = ImageToDraw->Height; \
            } \
        } while (0)

        MaxWidth = 0; // calculating TitleWidth
        CheckWidth (GetPoolStr (&Screen->Title));
        CheckImageWidth (GetPoolImage (&Screen->TitleImage));
        UINTN TitleWidth = (MaxWidth + 2) * FontCellWidth;

        MaxWidth = 0; // calculating LineWidth
        for (i = 0; i < *MaxVisibleInfoLines; i++) { // InfoLines is not scrollable, so only check the ones that are visible
            CheckWidth (GetPoolStr_PS_ (&Screen->InfoLines[i]));
        }
        for (i = 0; i < Screen->EntryCount; i++) { // check all the entries
            CheckWidth (GetPoolStr (&Screen->Entries[i]->Title));
            CheckImageWidth (GetPoolImage (&Screen->Entries[i]->Image));
        }
        *LineWidth = (MaxWidth + 2) * FontCellWidth; // used for drawing infolines, entries, and timeout

        *Width = (TitleWidth > *LineWidth) ? TitleWidth : *LineWidth;
    }

    // calculate width with the image
    if (ImageWidth) {
        UINTN WidthWithImage = ImageWidth + TITLEICON_SPACING * 2 + *LineWidth;
        if (*Width < WidthWithImage) {
            *Width = WidthWithImage;
        }
    }
    if (*Width > ScreenW) {
        *Width = ScreenW;
    }

    // calculate height with the image
    if (ImageHeight) {
        UINTN HeightWithImage = ImageHeight + TextLineHeight() * 2; // 2 lines for the title above the icon
        if (*Height < HeightWithImage) {
            *Height = HeightWithImage;
        }
    }
    if (*Height > ScreenH) {
        *Height = ScreenH;
    }

    // center the menu
    *XPos = (ScreenW - *Width) / 2;
    *YPos = (ScreenH - *Height) / 2;

    // if the menu obscures the banner or the hints, then center between the banner and hints
    if (*YPos < *BannerBottomEdge || *YPos + *Height > *HintTop) {
        *YPos = *BannerBottomEdge + TopBottomBorder + (AllowedHeight - *Height) / 2;
    }

    *IconX = *XPos + TITLEICON_SPACING;
    *IconY = *YPos + TextLineHeight() * 2; // 2 lines for the title above the icon
    *TextX = *XPos;
    if (ImageWidth) {
        *TextX += ImageWidth + TITLEICON_SPACING * 2;
    }
    if (*TextX + *LineWidth > ScreenW) {
        *LineWidth = ScreenW - *TextX;
    }
    LOGPROCEXIT();
} // VOID ComputeSubScreenWindowSize()

// Displays sub-menus
VOID GraphicsMenuStyle (
    IN REFIT_MENU_SCREEN  *Screen,
    IN SCROLL_STATE       *State,
    IN UINTN               Function,
    IN CHAR16             *ParamText
) {
/*
    LOGPROCENTRY("%a: Selection:%d of %d from %d View:%d-%d Viewable:%d",
        Function == MENU_FUNCTION_INIT            ? "INIT" :
        Function == MENU_FUNCTION_CLEANUP         ? "CLEANUP" :
        Function == MENU_FUNCTION_PAINT_ALL       ? "PAINT_ALL" :
        Function == MENU_FUNCTION_PAINT_SELECTION ? "PAINT_SELECTION" :
        Function == MENU_FUNCTION_PAINT_TIMEOUT   ? "PAINT_TIMEOUT" :
        Function == MENU_FUNCTION_PAINT_HINTS     ? "PAINT_HINTS" :
                                                    "???",
        State->CurrentSelection, State->MaxIndex, State->PreviousSelection,
        State->FirstVisible, State->LastVisible, State->MaxVisible
    );
*/
           INTN      i;
           UINTN     ItemWidth, TextY;
    static UINTN     LineWidth, MenuWidth, MenuHeight;
    static UINTN     EntriesPosX, EntriesPosY;
    static UINTN     TitlePosX, TimeoutPosY, CharWidth;
    static UINTN     IconX, IconY, TextX;
    static UINTN     MaxVisibleEntries, MaxVisibleInfoLines;
    static UINTN     BannerBottomEdge, HintTop;
    static EG_IMAGE *LastImageDrawn = NULL;
           EG_IMAGE *ImageToDraw;

    CharWidth = egGetFontCellWidth();
    State->ScrollMode = SCROLL_MODE_TEXT;

    switch (Function) {
        case MENU_FUNCTION_INIT:
            ComputeSubScreenWindowSize (
                Screen,
                &EntriesPosX, &EntriesPosY,
                &MenuWidth, &MenuHeight,
                &LineWidth,
                &IconX, &IconY, &TextX,
                &MaxVisibleInfoLines, &MaxVisibleEntries,
                &BannerBottomEdge, &HintTop
            );
            MsgLog ("(Banner:%d MenuT:%d IconT:%d MenuB:%d Hint:%d ScreenH:%d) (MenuL:%d IconX:%d TextX:%d MenuR:%d ScreenW:%d) Entries:%d Infos:%d\n",
                BannerBottomEdge,
                EntriesPosY,
                IconY,
                EntriesPosY + MenuHeight,
                HintTop,
                ScreenH,

                EntriesPosX,
                IconX,
                TextX,
                EntriesPosX + MenuWidth,
                ScreenW,

                MaxVisibleEntries,
                MaxVisibleInfoLines
            );

            InitScroll (State, Screen->EntryCount, MaxVisibleEntries);
            MsgLog ("Selection:%d of %d from %d View:%d-%d Viewable:%d\n",
                State->CurrentSelection, State->MaxIndex, State->PreviousSelection,
                State->FirstVisible, State->LastVisible, State->MaxVisible
            );

            TimeoutPosY = EntriesPosY + (Screen->EntryCount + 1) * TextLineHeight();

            // initial painting (this will change GlobalConfig.ScreenBackground->PixelData)
            SwitchToGraphicsAndClear (TRUE);

            EG_PIXEL FillColor = {0,0,0,0};
            #define MakeGray(x) do { FillColor.r = FillColor.g = FillColor.b = x; x -= 0x11; } while (0)
            #define egDrawRect(x,y,w,h) \
                do { \
                    EG_IMAGE *Window = egCreateFilledImage (w, h, FALSE, &FillColor); \
                    if  (Window) { \
                        egDrawImage (Window, x, y); \
                        MY_FREE_IMAGE(Window); \
                    } \
                } while (0)

            FillColor = GlobalConfig.ScreenBackground->PixelData[0];
            egDrawRect (EntriesPosX, EntriesPosY, MenuWidth, MenuHeight);

            #if 0
                // Draw ruler marks which will show how the menu is positioned (centered) on the screen
                // and the relationship of the menu with the hints and banner.
                UINTN MenuRight = ((EntriesPosX + MenuWidth) <= ScreenW) ? EntriesPosX + MenuWidth : ScreenW - 5;
                INTN NumLines = (HintTop - BannerBottomEdge) / egGetFontHeight() / 2;
                UINTN shade;
                shade = 0xFF;
                for (i=1; i <= 16; i++) {
                    MakeGray(shade);
                    egDrawRect (0, ScreenH - egGetFontHeight() * i, EntriesPosX, egGetFontHeight()); // mark bottom lines
                }
                shade = 0xFF;
                for (i=0; i < 16; i++) {
                    MakeGray(shade);
                    egDrawRect (0, egGetFontHeight() * i, EntriesPosX, egGetFontHeight()); // mark top lines
                }
                shade = 0xFF;
                for (i=0; i < NumLines; i++) {
                    MakeGray(shade);
                    egDrawRect (MenuRight, BannerBottomEdge + egGetFontHeight() * i, ScreenW - MenuRight, egGetFontHeight()); // mark banner lines
                }
                shade = 0xFF;
                for (i=1; i <= NumLines; i++) {
                    MakeGray(shade);
                    egDrawRect (MenuRight, HintTop - egGetFontHeight() * i, ScreenW - MenuRight, egGetFontHeight()); // mark bottom lines
                }
                shade = 0x80;
                MakeGray(shade);
                egDrawRect (EntriesPosX - 1, EntriesPosY - 1, MenuWidth + 2, MenuHeight + 2); // mark the menu area
                MakeGray(shade);
                egDrawRect (10, 0, EntriesPosX - 20, BannerBottomEdge); // Mark the banner that is allowed
                MakeGray(shade);
                egDrawRect (MenuRight + 10, 0, ScreenW - MenuRight - 20, GlobalConfig.BannerBottomEdge); // Mark the real banner
            #endif

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
               }
            }

            break;

        case MENU_FUNCTION_CLEANUP:
            // nothing to do
            break;

        case MENU_FUNCTION_PAINT_ALL:
            LastImageDrawn = NULL;

            TextY = EntriesPosY;
            DrawText (
                GetPoolStr (&Screen->Title),
                FALSE,
                (StrLen (GetPoolStr (&Screen->Title)) + 2) * CharWidth,
                TitlePosX,
                TextY
            );
            TextY += TextLineHeight();

            ImageToDraw = GetPoolImage (&Screen->Entries[State->CurrentSelection]->Image);
            if (!ImageToDraw) ImageToDraw = GetPoolImage (&Screen->TitleImage);
            if (ImageToDraw && ImageToDraw != LastImageDrawn) {
                BltImageAlpha (ImageToDraw, IconX, IconY, GlobalConfig.ScreenBackground->PixelData);
                LastImageDrawn = ImageToDraw;
            }

            TextY += TextLineHeight();

            if (MaxVisibleInfoLines > 0) {
                for (i = 0; i < MaxVisibleInfoLines; i++) {
                    DrawText (
                        GetPoolStr_PS_ (&Screen->InfoLines[i]),
                        FALSE, LineWidth,
                        TextX, TextY
                    );
                    TextY += TextLineHeight();
                }
                // also add a blank line
                TextY += TextLineHeight();
            }

            for (i = State->FirstVisible; i <= State->MaxIndex && i <= State->LastVisible; i++) {
                DrawText (
                    GetPoolStr (&Screen->Entries[i]->Title),
                    (i == State->CurrentSelection),
                    LineWidth,
                    TextX,
                    TextY
                );
                TextY += TextLineHeight();
            }

            if (HintTop < ScreenH) {
                if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HINTS)) {
                    if ((GetPoolStr (&Screen->Hint1) != NULL) && (StrLen (GetPoolStr (&Screen->Hint1)) > 0)) {
                        DrawTextWithTransparency (
                            GetPoolStr (&Screen->Hint1),
                            (ScreenW - egComputeTextWidth (GetPoolStr (&Screen->Hint1))) / 2,
                            HintTop
                        );
                    }

                    if ((GetPoolStr (&Screen->Hint2) != NULL) && (StrLen (GetPoolStr (&Screen->Hint2)) > 0)) {
                        DrawTextWithTransparency (
                            GetPoolStr (&Screen->Hint2),
                            (ScreenW - egComputeTextWidth (GetPoolStr (&Screen->Hint2))) / 2,
                            HintTop + egGetFontHeight()
                        );
                    }
                }
            }

            break;

        case MENU_FUNCTION_PAINT_SELECTION:
            // redraw selection cursor
            DrawText (
                GetPoolStr (&Screen->Entries[State->PreviousSelection]->Title),
                FALSE, LineWidth,
                TextX,
                EntriesPosY + (2 + MaxVisibleInfoLines + (MaxVisibleInfoLines ? 1 : 0) + State->PreviousSelection - State->FirstVisible) * TextLineHeight()
            );

            DrawText (
                GetPoolStr (&Screen->Entries[State->CurrentSelection]->Title),
                TRUE, LineWidth,
                TextX,
                EntriesPosY + (2 + MaxVisibleInfoLines + (MaxVisibleInfoLines ? 1 : 0) + State->CurrentSelection - State->FirstVisible) * TextLineHeight()
            );

            ImageToDraw = GetPoolImage (&Screen->Entries[State->CurrentSelection]->Image);
            if (!ImageToDraw) ImageToDraw = GetPoolImage (&Screen->TitleImage);
            if (ImageToDraw && ImageToDraw != LastImageDrawn) {
                BltImageAlpha (ImageToDraw, IconX, IconY, GlobalConfig.ScreenBackground->PixelData);
                LastImageDrawn = ImageToDraw;
            }

            break;

        case MENU_FUNCTION_PAINT_TIMEOUT:
            DrawText (ParamText, FALSE, LineWidth, TextX, TimeoutPosY);

            break;
    }

    //LOGPROCEXIT();
} // static VOID GraphicsMenuStyle()

//
// graphical main menu style
//

static
VOID DrawMainMenuEntry (
    REFIT_MENU_ENTRY *Entry,
    BOOLEAN           selected,
    UINTN             XPos,
    UINTN             YPos
) {
    EG_IMAGE *Background;

    // if using pointer ... do not draw selection image when not hovering
    if (!selected || !DrawSelection) {
        // Image not selected ... copy background
        egDrawImageWithTransparency (
            GetPoolImage (&Entry->Image),
            GetPoolImage (&Entry->BadgeImage),
            XPos, YPos,
            SelectionImages[Entry->Row]->Width,
            SelectionImages[Entry->Row]->Height
        );
    }
    else {
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

            MY_FREE_IMAGE(Background);
        }
    } // if/else !selected
} // VOID DrawMainMenuEntry()

static
VOID PaintAll (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    UINTN                *itemPosX,
    UINTN                 row0PosY,
    UINTN                 row1PosY,
    UINTN                 textPosY
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
            }
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
    }
} // static VOID PaintAll()

// Move the selection to State->CurrentSelection, adjusting icon row if necessary...
static
VOID PaintSelection (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    UINTN                *itemPosX,
    UINTN                 row0PosY,
    UINTN                 row1PosY,
    UINTN                 textPosY
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
        }

        if (Screen->Entries[State->CurrentSelection]->Row == 0) {
            XSelectCur = State->CurrentSelection - State->FirstVisible;
            YPosCur = row0PosY;
        }
        else {
            XSelectCur = State->CurrentSelection;
            YPosCur = row1PosY;
        }

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

static
EG_IMAGE * GetIcon (
    IN EG_EMBEDDED_IMAGE *BuiltInIcon,
    IN CHAR16            *ExternalFilename
) {
    EG_IMAGE *Icon = NULL;

    Icon = egFindIcon (ExternalFilename, GlobalConfig.IconSizes[ICON_SIZE_SMALL]);
    if (Icon == NULL) {
        Icon = egPrepareEmbeddedImage (BuiltInIcon, TRUE, NULL);
    }

    return Icon;
} // static EG_IMAGE * GetIcon()

// Display a 48x48 icon at the specified location. Uses the image specified by
// ExternalFilename if it is available, or BuiltInImage if it is not. The
// Y position is specified as the center value, and so is adjusted by half
// the icon's height. The X position is set along the icon's left
// edge if Alignment == ALIGN_LEFT, and along the right edge if
// Alignment == ALIGN_RIGHT
static
VOID PaintIcon (
    IN EG_IMAGE *Icon,
    UINTN        PosX,
    UINTN        PosY,
    UINTN        Alignment
) {
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
    }
} // static VOID PaintIcon()

UINTN ComputeRow0PosY (VOID) {
    return ((ScreenH / 2) - TileSizes[0] / 2);
} // UINTN ComputeRow0PosY()

// Display (or erase) the arrow icons to the left and right of an icon's row,
// as appropriate.
static
VOID PaintArrows (
    SCROLL_STATE *State,
    UINTN         PosX,
    UINTN         PosY,
    UINTN         row0Loaders
) {
    static EG_IMAGE *LeftArrow       = NULL;
    static EG_IMAGE *RightArrow      = NULL;
    static EG_IMAGE *LeftBackground  = NULL;
    static EG_IMAGE *RightBackground = NULL;
    static BOOLEAN   LoadedArrows    = FALSE;

    UINTN RightX = (ScreenW + (TileSizes[0] + TILE_XSPACING) * State->MaxVisible) / 2 + TILE_XSPACING;

    if (!LoadedArrows && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_ARROWS)) {
        MuteLogger = TRUE;
        LeftArrow  = GetIcon (&egemb_arrow_left , L"arrow_left" );
        RightArrow = GetIcon (&egemb_arrow_right, L"arrow_right");
        MuteLogger = FALSE;

        if (LeftArrow) {
            LeftBackground = egCropImage (
                GlobalConfig.ScreenBackground,
                PosX - LeftArrow->Width,
                PosY - (LeftArrow->Height / 2),
                LeftArrow->Width,
                LeftArrow->Height
            );
        }
        if (RightArrow) {
            RightBackground = egCropImage (
                GlobalConfig.ScreenBackground,
                RightX,
                PosY - (RightArrow->Height / 2),
                RightArrow->Width,
                RightArrow->Height
            );
        }
        LEAKABLEONEIMAGE ( LeftArrow,  "Left Arrow");
        LEAKABLEONEIMAGE (RightArrow, "Right Arrow");
        LEAKABLEONEIMAGE ( LeftBackground,  "Left Background");
        LEAKABLEONEIMAGE (RightBackground, "Right Background");
        LoadedArrows = TRUE;
    }

    // For PaintIcon() calls, the starting Y position is moved to the midpoint
    // of the surrounding row; PaintIcon() adjusts this back up by half the
    // icon's height to properly center it.
    if (LeftArrow && LeftBackground) {
        if (State->FirstVisible > 0) {
            PaintIcon (LeftArrow, PosX, PosY, ALIGN_RIGHT);
        }
        else {
            BltImage (LeftBackground, PosX - LeftArrow->Width, PosY - (LeftArrow->Height / 2));
        }
    }

    if (RightArrow && RightBackground) {
        if (State->LastVisible < row0Loaders - 1) {
            PaintIcon (RightArrow, RightX, PosY, ALIGN_LEFT);
        }
        else {
            BltImage (RightBackground, RightX, PosY - (RightArrow->Height / 2));
        }
    }
} // VOID PaintArrows()

// Display main menu in graphics mode
VOID MainMenuStyle (
    IN REFIT_MENU_SCREEN *Screen,
    IN SCROLL_STATE      *State,
    IN UINTN              Function,
    IN CHAR16            *ParamText
) {
           INTN   i;
           UINTN  row0Count, row1Count, row1PosX, row1PosXRunning;
           UINTN  MainMaxVisible;
    static UINTN  row0PosX, row0PosXRunning, row1PosY, row0Loaders;
    static UINTN *itemPosX;
    static UINTN  row0PosY, textPosY;

    State->ScrollMode = SCROLL_MODE_ICONS;
    switch (Function) {
        case MENU_FUNCTION_INIT:

            MainMaxVisible = ScreenW / (TileSizes[0] + TILE_XSPACING) - 1;
            if (GlobalConfig.MaxTags > 0 && GlobalConfig.MaxTags < MainMaxVisible) {
                MainMaxVisible = GlobalConfig.MaxTags;
            }
            InitScroll (State, Screen->EntryCount, MainMaxVisible);

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
            MY_FREE_POOL(itemPosX);
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
    IN SCROLL_STATE      *State,
    IN UINTN              PosX,
    IN UINTN              PosY
) {
           UINTN  i;
           UINTN  itemRow;
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
            }
        }
        else if (Screen->Entries[i]->Row == 1 && itemRow == 1) {
            if (PosX >= itemPosX[i] && PosX <= itemPosX[i] + TileSizes[1]) {
                ItemIndex = i;
                break;
            }
        }
    }

    MY_FREE_POOL(itemPosX);

    return ItemIndex;
} // VOID FindMainMenuItem()

VOID GenerateWaitList(VOID) {
    if (WaitList == NULL) {
        LOGPROCENTRY();
        UINTN PointerCount = pdCount();

        WaitListLength = 2 + PointerCount;
        WaitList       = AllocatePool (sizeof (EFI_EVENT) * WaitListLength);
        LEAKABLE(WaitList, "GenerateWaitList WaitList");
        WaitList[0]    = gST->ConIn->WaitForKey;
        MsgLog("%p gST->ConIn->WaitForKey\n", WaitList[0]);

        UINTN Index;
        for (Index = 1; Index <= PointerCount; Index++) {
            WaitList[Index] = pdWaitEvent (Index);
            MsgLog("%p pdWaitEvent (%d)\n", WaitList[Index], Index);
        }
        LOGPROCEXIT("WaitList: %p");
    }
} // VOID GenerateWaitList()

UINTN WaitForInput (
    UINTN Timeout
) {
    //LOGPROCENTRY("timeout:%d", Timeout);
    EFI_STATUS  Status;
    UINTN       Length;
    UINTN       Index      = INPUT_TIMEOUT;
    EFI_EVENT   TimerEvent = NULL;

    //DA-TAG: Consider deleting later. Seems more of a distraction than a useful item
    //#if REFIT_DEBUG > 0
    //LOG(3, LOG_THREE_STAR_MID, L"Input Pending: %d", Timeout);
    //#endif

    // Generate WaitList if not already generated.
    GenerateWaitList();

    Length = WaitListLength;

    Status = REFIT_CALL_5_WRAPPER(gBS->CreateEvent, EVT_TIMER, 0, NULL, NULL, &TimerEvent);
    if (Timeout == 0) {
        Length--;
    }
    else {
        if (EFI_ERROR(Status)) {
            REFIT_CALL_1_WRAPPER(gBS->Stall, 100000); // Pause for 100 ms
            //LOGPROCEXIT("INPUT_TIMER_ERROR 1");
            return INPUT_TIMER_ERROR;
        }
        else {
            REFIT_CALL_3_WRAPPER(gBS->SetTimer, TimerEvent, TimerRelative, Timeout * 10000);
            WaitList[Length - 1] = TimerEvent;
        }
    }

    //LOGBLOCKENTRY("WaitForEvent");
    LEAKABLEEXTERNALSTART("WaitForEvent");
    Status = REFIT_CALL_3_WRAPPER(gBS->WaitForEvent, Length, WaitList, &Index);
    LEAKABLEEXTERNALSTOP("WaitForEvent");
    //LOGBLOCKEXIT("WaitForEvent");
    //LOGBLOCKENTRY("CloseEvent");
    REFIT_CALL_1_WRAPPER(gBS->CloseEvent, TimerEvent);
    //LOGBLOCKEXIT("CloseEvent");

    if (EFI_ERROR(Status)) {
        //LOGBLOCKENTRY("Stall");
        REFIT_CALL_1_WRAPPER(gBS->Stall, 100000); // Pause for 100 ms
        //LOGBLOCKEXIT("Stall");
        //LOGPROCEXIT("INPUT_TIMER_ERROR 2");
        return INPUT_TIMER_ERROR;
    }
    else if (Index == 0) {
        //LOGPROCEXIT("INPUT_KEY");
        return INPUT_KEY;
    }
    else if (Index < Length - 1) {
        //LOGPROCEXIT("INPUT_POINTER");
        return INPUT_POINTER;
    }
    //LOGPROCEXIT("INPUT_TIMEOUT");
    return INPUT_TIMEOUT;
} // UINTN WaitForInput()

// Enable the user to edit boot loader options.
// Returns TRUE if the user exited with edited options; FALSE if the user
// pressed Esc to terminate the edit.
static
BOOLEAN EditOptions (
    REFIT_MENU_ENTRY *MenuEntry
) {
    LOGPROCENTRY("MenuEntry:%p", MenuEntry);
    UINTN    x_max, y_max;
    CHAR16  *EditedOptions;
    BOOLEAN  retval = FALSE;

    LOG(2, LOG_LINE_NORMAL, L"EditOptions: %d", ((GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) != 0));
    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
        return FALSE;
    }

    if (!GlobalConfig.TextOnly) {
        SwitchToText (TRUE);
    }

    REFIT_CALL_4_WRAPPER(
        gST->ConOut->QueryMode, gST->ConOut,
        gST->ConOut->Mode->Mode,
        &x_max, &y_max
    );

    ENTRY_TYPE EntryType = GetMenuEntryType (MenuEntry);
    CHAR16 *LoadOptions;
    switch (EntryType) {
        case EntryTypeLoaderEntry: LoadOptions = GetPoolStr (&((LOADER_ENTRY *)MenuEntry)->LoadOptions); break;
        case EntryTypeLegacyEntry: LoadOptions = GetPoolStr (&((LEGACY_ENTRY *)MenuEntry)->LoadOptions); break;
        default: LoadOptions = NULL; break;
    }

    if (line_edit (LoadOptions, &EditedOptions, x_max)) {
        LEAKABLEPATHSET (MenuEntry);
        switch (EntryType) {
            case EntryTypeLoaderEntry: AssignPoolStr (&((LOADER_ENTRY *)MenuEntry)->LoadOptions, EditedOptions); break;
            case EntryTypeLegacyEntry: AssignPoolStr (&((LEGACY_ENTRY *)MenuEntry)->LoadOptions, EditedOptions); break;
            default: break;
        }
        LEAKABLEMENUENTRY ((REFIT_MENU_ENTRY *) MenuEntry);
        LEAKABLEPATHUNSET ();
        retval = TRUE;
    }

    if (!GlobalConfig.TextOnly) {
        SwitchToGraphics();
    }

    LOGPROCEXIT("%d", retval);
    return retval;
} // VOID EditOptions()

//
// user-callable dispatcher functions
//

VOID DisplaySimpleMessage (
    CHAR16 *Title,
    CHAR16 *Message
) {
    LOGPROCENTRY();

    #if REFIT_DEBUG > 0
    LOG(3, LOG_THREE_STAR_MID, L"Entering DisplaySimpleMessage");
    #endif

    if (!Message) {
        LOGPROCEXIT(": No Message!!");
        return;
    }

    MENU_STYLE_FUNC      Style          = TextMenuStyle;
    INTN                 DefaultEntry   = 0;
    UINTN                MenuExit;
    REFIT_MENU_ENTRY    *ChosenOption;
    REFIT_MENU_SCREEN   *SimpleMessageMenu = NULL;
    REFIT_MENU_SCREEN    SimpleMessageMenuSrc = {
        CACHEDPS(Title), NULLPI, 0, NULL, 0, NULL, 0, NULLPS,
        CACHEDPS(L"Press 'Enter' to Return to Main Menu"), EMPTYPS
    };

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    SimpleMessageMenu = CopyMenuScreen (&SimpleMessageMenuSrc);
    if (!SimpleMessageMenu) {
        LOGPROCEXIT(": Copy Menu Fail!!");
        return;
    }

    CopyFromPoolImage_PI_ (&SimpleMessageMenu->TitleImage_PI_, BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT));
    AddMenuInfoLineCached (SimpleMessageMenu, Message);
    AddMenuEntryCopy (SimpleMessageMenu, &TagMenuEntry[TAG_RETURN]);
    MenuExit = RunGenericMenu (SimpleMessageMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    // DA-TAG: Run check on MenuExit for Coverity
    //         L"UNKNOWN!!" is never reached
    //         Constant ... Do Not Free
    CHAR16 *TypeMenuExit = (MenuExit < 1) ? L"UNKNOWN!!" : MenuExitInfo (MenuExit);

    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'DisplaySimpleMessage'",
        MenuExit, TypeMenuExit, GetPoolStr (&ChosenOption->Title)
    );
    #endif

    FreeMenuScreen (&SimpleMessageMenu);
    LOGPROCEXIT();
} // VOID DisplaySimpleMessage()

// Check each filename in FilenameList to be sure it refers to a valid file. If
// not, delete it. This works only on filenames that are complete, with volume,
// path, and filename components; if the filename omits the volume, the search
// is not done and the item is left intact, no matter what.
// Returns TRUE if any files were deleted, FALSE otherwise.
static
BOOLEAN RemoveInvalidFilenames (
    CHAR16 *FilenameList
) {
    EFI_STATUS       Status;
    UINTN            i = 0;
    CHAR16          *Filename, *OneElement, *VolName = NULL;
    BOOLEAN          DeleteIt, DeletedSomething = FALSE;
    REFIT_VOLUME    *Volume = NULL;
    EFI_FILE_HANDLE  FileHandle;

    while ((OneElement = FindCommaDelimited (FilenameList, i)) != NULL) {
        DeleteIt = FALSE;
        Filename = StrDuplicate (OneElement);

        if (SplitVolumeAndFilename (&Filename, &VolName)) {
            DeleteIt = TRUE;

            if (FindVolume (&Volume, VolName) && Volume->RootDir) {
                Status = REFIT_CALL_5_WRAPPER(
                    Volume->RootDir->Open, Volume->RootDir,
                    &FileHandle, Filename,
                    EFI_FILE_MODE_READ, 0
                );

                if (Status == EFI_SUCCESS) {
                    DeleteIt = FALSE;
                    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
                }
            }
        }

        if (DeleteIt) {
            DeleteItemFromCsvList (OneElement, FilenameList);
        }
        else {
            i++;
        }

        MY_FREE_POOL(OneElement);
        MY_FREE_POOL(Filename);
        MY_FREE_POOL(VolName);

        DeletedSomething |= DeleteIt;
    } // while

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

    CheckError (Status, L"in SaveHiddenList!!");
} // VOID SaveHiddenList()

// Present a menu that enables the user to delete hidden tags
//   that is, to un-hide them.
VOID ManageHiddenTags (VOID) {
    LOGPROCENTRY();

    INTN                 DefaultEntry  = 0;
    MENU_STYLE_FUNC      Style         = TextMenuStyle;
    UINTN                i;

    CHAR16             *MenuInfo     = L"Select a Tag and Press 'Enter' to Restore";
    REFIT_MENU_SCREEN  *RestoreItemMenu = NULL;
    REFIT_MENU_SCREEN   RestoreItemMenuSrc = {
        CACHEDPS(L"Manage Hidden Tags"), NULLPI, 0, NULL, 0, NULL, 0, NULLPS,
        CACHEDPS(L"Select an option and press 'Enter' to apply the option"),
        CACHEDPS(L"Press 'Esc' to return to the main menu without changes")
    };

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_THIN_SEP, L"Manage Hidden Tags");
    #endif

    RestoreItemMenu = CopyMenuScreen (&RestoreItemMenuSrc);
    if (!RestoreItemMenu) {
        LOGPROCEXIT("(NULL)");
        return;
    }

    CopyFromPoolImage_PI_ (&RestoreItemMenu->TitleImage_PI_, BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN));
    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    CHAR16 * AllNames[] = {L"HiddenTags", L"HiddenTools", L"HiddenLegacy", L"HiddenFirmware"};
    CONST UINTN HiddenTypeTool = 1; // the second hidden type is HiddenTools
    CONST UINTN NumHiddenTypesWithFilenames = 2; // the first two hidden types have file names to be checked
    CONST UINTN NumHiddenTypes = sizeof(AllNames)/sizeof(*AllNames);
    BOOLEAN AllSaves[NumHiddenTypes];
    CHAR16 * AllTags[NumHiddenTypes];

    for (i = 0; i < NumHiddenTypes; i++) {
        AllSaves[i] = FALSE;
        AllTags[i] = ReadHiddenTags (AllNames[i]);
        if (i < NumHiddenTypesWithFilenames && AllTags[i]) {
            AllSaves[i] = RemoveInvalidFilenames (AllTags[i]);
        }
        CHAR16 *OneElement;
        for (UINTN j = 0; (OneElement = FindCommaDelimited (AllTags[i], j)); j++) {
            if (*OneElement) {
                REFIT_MENU_ENTRY *MenuEntryItem = AllocateZeroPool (sizeof (REFIT_MENU_ENTRY));
                AssignPoolStr (&MenuEntryItem->Title, OneElement);
                MenuEntryItem->Tag = TAG_RETURN;
                MenuEntryItem->Row = i;
                AddMenuEntry (RestoreItemMenu, MenuEntryItem);
            }
            else {
                MY_FREE_POOL (OneElement);
            }
        } // for
    }

    if (!RestoreItemMenu->EntryCount) {
        DisplaySimpleMessage (L"Information", L"No Hidden Tags Found");
    }
    else {
        AddMenuInfoLineCached (RestoreItemMenu, MenuInfo);

        REFIT_MENU_ENTRY *ChosenOption;
        UINTN MenuExit = RunGenericMenu (RestoreItemMenu, Style, &DefaultEntry, &ChosenOption);

        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL,
            L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'ManageHiddenTags'",
            MenuExit, MenuExitInfo (MenuExit), GetPoolStr (&ChosenOption->Title)
        );
        #endif

        if (MenuExit == MENU_EXIT_ENTER) {
            AllSaves[ChosenOption->Row] |= DeleteItemFromCsvList (GetPoolStr (&ChosenOption->Title), AllTags[ChosenOption->Row]);
        }
    } // if !AllTags

    FreeMenuScreen (&RestoreItemMenu);

    BOOLEAN doRescanAll = FALSE;
    for (i = 0; i < NumHiddenTypes; i++) {
        if (AllSaves[i]) {
            SaveHiddenList (AllTags[i], AllNames[i]);
            if (i == HiddenTypeTool) {
                MY_FREE_POOL(gHiddenTools);
            }
            doRescanAll = TRUE;
        }
        MY_FREE_POOL(AllTags[i]);
    }

    if (doRescanAll) {
        RescanAll (FALSE, FALSE);
    }

    LOGPROCEXIT();
} // VOID ManageHiddenTags()

CHAR16 * ReadHiddenTags (
    CHAR16 *VarName
) {
    CHAR16     *Buffer = NULL;
    UINTN       Size;
    EFI_STATUS  Status;

    Status = EfivarGetRaw (&RefindPlusGuid, VarName, (VOID **) &Buffer, &Size);

    #if REFIT_DEBUG > 0
    if ((Status != EFI_SUCCESS) && (Status != EFI_NOT_FOUND)) {
        CHAR16 *CheckErrMsg = PoolPrint (L"in ReadHiddenTags:- '%s'", VarName);
        CheckError (Status, CheckErrMsg);
        MY_FREE_POOL(CheckErrMsg);
    }
    #endif

    if ((Status == EFI_SUCCESS) && (Size == 0)) {
        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL,
            L"Zero Size in ReadHiddenTags ... Clearing Buffer"
        );
        #endif

        MY_FREE_POOL(Buffer);
    }

    return Buffer;
} // CHAR16* ReadHiddenTags()

// Add PathName to the hidden tags variable specified by *VarName.
static
VOID AddToHiddenTags (CHAR16 *VarName, CHAR16 *Pathname) {
    LOGPROCENTRY("Var:%s Path:%s", VarName, Pathname);
    EFI_STATUS  Status;
    CHAR16     *HiddenTags;

    if (Pathname && (StrLen (Pathname) > 0)) {
        HiddenTags = ReadHiddenTags (VarName);
        MsgLog ("old HiddenTags: %s\n", HiddenTags);
        if (!HiddenTags) {
            // Prevent NULL dererencing
            HiddenTags = StrDuplicate (Pathname);
        }
        else {
            MergeStrings (&HiddenTags, Pathname, L',');
        }
        MsgLog ("new HiddenTags: %s\n", HiddenTags);

        Status = EfivarSetRaw (
            &RefindPlusGuid,
            VarName,
            HiddenTags,
            StrLen (HiddenTags) * 2 + 2,
            TRUE
        );

        CheckError (Status, L"in 'AddToHiddenTags'!!");
        MY_FREE_POOL(HiddenTags);
    }
    LOGPROCEXIT();
} // VOID AddToHiddenTags()

// Adds a filename, specified by the *Loader variable, to the *VarName UEFI variable,
// using the mostly-prepared *HideItemMenu structure to prompt the user to confirm
// hiding that item.
// Returns TRUE if item was hidden, FALSE otherwise.
static
BOOLEAN HideEfiTag (
    LOADER_ENTRY      *Loader,
    REFIT_MENU_SCREEN *HideItemMenu,
    CHAR16            *VarName
) {
    REFIT_VOLUME      *TestVolume   = NULL;
    BOOLEAN            TagHidden    = FALSE;
    CHAR16            *FullPath     = NULL;
    CHAR16            *GuidStr      = NULL;
    UINTN              MenuExit;
    INTN               DefaultEntry = 1;
    MENU_STYLE_FUNC    Style        = TextMenuStyle;
    REFIT_MENU_ENTRY  *ChosenOption;

    if (!Loader ||
        !VarName ||
        !HideItemMenu ||
        !Loader->Volume ||
        !GetPoolStr (&Loader->LoaderPath)
    ) {
        return FALSE;
    }

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    if (GetPoolStr (&Loader->Volume->VolName) && (StrLen (GetPoolStr (&Loader->Volume->VolName)) > 0)) {
        FullPath = StrDuplicate (GetPoolStr (&Loader->Volume->VolName));
    }

    MergeStrings (&FullPath, GetPoolStr (&Loader->LoaderPath), L':');
    AddMenuInfoLinePool (HideItemMenu, PoolPrint (L"Are you sure you want to hide %s?", FullPath));
    AddMenuEntryCopy (HideItemMenu, &TagMenuEntry[TAG_YES]);
    AddMenuEntryCopy (HideItemMenu, &TagMenuEntry[TAG_NO]);

    MenuExit = RunGenericMenu (HideItemMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'HideEfiTag'",
        MenuExit, MenuExitInfo (MenuExit), GetPoolStr (&ChosenOption->Title)
    );
    #endif

    if (ChosenOption && MyStriCmp (GetPoolStr (&ChosenOption->Title), L"Yes") && (MenuExit == MENU_EXIT_ENTER)) {
        GuidStr = GuidAsString (&Loader->Volume->PartGuid);
        if (FindVolume (&TestVolume, GuidStr) && TestVolume->RootDir) {
            MY_FREE_POOL(FullPath);
            MergeStrings (&FullPath, GuidStr, L'\0');
            MergeStrings (&FullPath, L":", L'\0');
            MergeStrings (
                &FullPath,
                GetPoolStr (&Loader->LoaderPath),
                (GetPoolStr (&Loader->LoaderPath)[0] == L'\\' ? L'\0' : L'\\')
            );
        }
        AddToHiddenTags (VarName, FullPath);
        TagHidden = TRUE;
        MY_FREE_POOL(GuidStr);
    }

    MY_FREE_POOL(FullPath);

    // since TestVolume started as NULL, free it if it is not NULL
    FreeVolume (&TestVolume);

    return TagHidden;
} // BOOLEAN HideEfiTag()

static
BOOLEAN HideFirmwareTag(
    LOADER_ENTRY      *Loader,
    REFIT_MENU_SCREEN *HideItemMenu
) {
    MENU_STYLE_FUNC     Style = TextMenuStyle;
    REFIT_MENU_ENTRY   *ChosenOption;
    INTN                DefaultEntry = 1;
    UINTN               MenuExit;
    BOOLEAN             TagHidden = FALSE;

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    AddMenuInfoLinePool (HideItemMenu, PoolPrint(L"Really Hide '%s'?", GetPoolStr (&Loader->Title)));
    AddMenuEntryCopy (HideItemMenu, &TagMenuEntry[TAG_YES]);
    AddMenuEntryCopy (HideItemMenu, &TagMenuEntry[TAG_NO]);

    MenuExit = RunGenericMenu(
        HideItemMenu,
        Style,
        &DefaultEntry,
        &ChosenOption
    );

    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'HideFirmwareTag'",
        MenuExit, MenuExitInfo (MenuExit), GetPoolStr (&ChosenOption->Title)
    );
    #endif

    if ((MyStriCmp(GetPoolStr (&ChosenOption->Title), L"Yes")) &&
        (MenuExit == MENU_EXIT_ENTER)
    ) {
        AddToHiddenTags(L"HiddenFirmware", GetPoolStr (&Loader->Title));
        TagHidden = TRUE;
    }

    return TagHidden;
} // BOOLEAN HideFirmwareTag()


static
BOOLEAN HideLegacyTag (
    LEGACY_ENTRY      *LegacyLoader,
    REFIT_MENU_SCREEN *HideItemMenu
) {
    MENU_STYLE_FUNC    Style = TextMenuStyle;
    REFIT_MENU_ENTRY   *ChosenOption;
    INTN                DefaultEntry = 1;
    UINTN               MenuExit;
    CHAR16             *Name;
    BOOLEAN             TagHidden = FALSE;

    if (AllowGraphicsMode)
        Style = GraphicsMenuStyle;

    if ((GlobalConfig.LegacyType == LEGACY_TYPE_MAC) && GetPoolStr (&LegacyLoader->me.Title)) {
        Name = GetPoolStr (&LegacyLoader->me.Title);
    }
    if ((GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) &&
        LegacyLoader->BdsOption && LegacyLoader->BdsOption->Description
    ) {
        Name = LegacyLoader->BdsOption->Description;
    }
    if (!Name) {
        Name = L"Legacy (BIOS) OS";
    }
    AddMenuInfoLinePool (HideItemMenu, PoolPrint (L"Are you sure you want to hide '%s'?", Name));
    AddMenuEntryCopy (HideItemMenu, &TagMenuEntry[TAG_YES]);
    AddMenuEntryCopy (HideItemMenu, &TagMenuEntry[TAG_NO]);
    MenuExit = RunGenericMenu (HideItemMenu, Style, &DefaultEntry, &ChosenOption);

    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'HideLegacyTag'",
        MenuExit, MenuExitInfo (MenuExit), GetPoolStr (&ChosenOption->Title)
    );
    #endif

    if (MyStriCmp (GetPoolStr (&ChosenOption->Title), L"Yes") && (MenuExit == MENU_EXIT_ENTER)) {
        AddToHiddenTags (L"HiddenLegacy", Name);
        TagHidden = TRUE;
    }

    return TagHidden;
} // BOOLEAN HideLegacyTag()

static
VOID HideTag (
    REFIT_MENU_ENTRY *ChosenEntry
) {
    LOGPROCENTRY("%s", GetPoolStr (&ChosenEntry->Title));
    LOADER_ENTRY      *Loader        = (LOADER_ENTRY *) ChosenEntry;
    LEGACY_ENTRY      *LegacyLoader  = (LEGACY_ENTRY *) ChosenEntry;
    REFIT_MENU_SCREEN *HideItemMenu = NULL;
    REFIT_MENU_SCREEN  HideItemMenuSrc = {
        NULLPS, NULLPI, 0, NULL, 0, NULL, 0, NULLPS,
        CACHEDPS(L"Select an Option and Press 'Enter' or"),
        CACHEDPS(L"Press 'Esc' to Return to Main Menu (Without Changes)")
    };

    if (ChosenEntry == NULL) {
        LOGPROCEXIT("(no menu entry)");
        return;
    }

    HideItemMenu = CopyMenuScreen (&HideItemMenuSrc);
    if (!HideItemMenu) {
        LOGPROCEXIT("(no menu screen)");
        return;
    }

    CopyFromPoolImage_PI_ (&HideItemMenu->TitleImage_PI_, BuiltinIcon (BUILTIN_ICON_FUNC_HIDDEN));
    // BUG: The RescanAll() calls should be conditional on successful calls to
    // HideEfiTag() or HideLegacyTag(); but for the former, this causes
    // crashes on a second call hide a tag if the user chose "No" to the first
    // call. This seems to be related to memory management of Volumes; the
    // crash occurs in FindVolumeAndFilename() and lib.c when calling
    // DevicePathToStr(). Calling RescanAll() on all returns from HideEfiTag()
    // seems to be an effective workaround, but there is likely a memory
    // management bug somewhere that is the root cause.
    switch (ChosenEntry->Tag) {
        case TAG_LOADER:
            if (Loader->DiscoveryType != DISCOVERY_TYPE_AUTO) {
                DisplaySimpleMessage (
                    L"Cannot Hide Entry for Manual Boot Stanza",
                    L"You must edit 'config.conf' to remove this entry."
                );
            }
            else {
                AssignCachedPoolStr (&HideItemMenu->Title, L"Hide EFI OS Tag");
                HideEfiTag (Loader, HideItemMenu, L"HiddenTags");

                #if REFIT_DEBUG > 0
                MsgLog ("User Input Received:\n");
                MsgLog ("  - %s\n\n", GetPoolStr (&HideItemMenu->Title));
                #endif

                RescanAll (FALSE, FALSE);
            }
            break;

        case TAG_LEGACY:
        case TAG_LEGACY_UEFI:
            AssignCachedPoolStr (&HideItemMenu->Title, L"Hide Legacy (BIOS) OS Tag");
            if (HideLegacyTag (LegacyLoader, HideItemMenu)) {
                #if REFIT_DEBUG > 0
                MsgLog ("User Input Received:\n");
                MsgLog ("  - %s\n\n", GetPoolStr (&HideItemMenu->Title));
                #endif

                RescanAll (FALSE, FALSE);
            }
            break;

        case TAG_FIRMWARE_LOADER:
            AssignCachedPoolStr (&HideItemMenu->Title, L"Hide Firmware Boot Option Tag");
            if (HideFirmwareTag(Loader, HideItemMenu)) {
                RescanAll(FALSE, FALSE);
            }
            break;

        #define TAGS_BUILTIN
        #include "tags.include"
            DisplaySimpleMessage (
                L"Unable to Comply",
                L"To hide an internal tool, edit the 'showtools' line in config.conf"
            );
            break;

        case TAG_TOOL:
            AssignCachedPoolStr (&HideItemMenu->Title, L"Hide Tool Tag");
            HideEfiTag (Loader, HideItemMenu, L"HiddenTools");
            MY_FREE_POOL(gHiddenTools);

            #if REFIT_DEBUG > 0
            MsgLog ("User Input Received:\n");
            MsgLog ("  - %s\n\n", GetPoolStr (&HideItemMenu->Title));
            #endif

            RescanAll (FALSE, FALSE);
            break;
    } // switch
    FreeMenuScreen (&HideItemMenu);
    LOGPROCEXIT();
} // VOID HideTag()

UINTN RunMenu (
    IN  REFIT_MENU_SCREEN  *Screen,
    OUT REFIT_MENU_ENTRY  **ChosenEntry
) {
    INTN            DefaultEntry = -1;
    UINTN           MenuExit;
    MENU_STYLE_FUNC Style        = TextMenuStyle;

    if (AllowGraphicsMode) {
        Style = GraphicsMenuStyle;
    }

    MenuExit = RunGenericMenu (Screen, Style, &DefaultEntry, ChosenEntry);

    #if REFIT_DEBUG > 0
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'RunMenu'",
        MenuExit, MenuExitInfo (MenuExit), GetPoolStr (&Screen->Title)
    );
    #endif

    return MenuExit;
} // UINTN RunMenu()

UINTN RunMainMenu (
    REFIT_MENU_SCREEN **ScreenPtr,
    CHAR16            **DefaultSelection,
    REFIT_MENU_ENTRY  **ChosenEntry
) {
    REFIT_MENU_SCREEN *Screen = ScreenPtr ? *ScreenPtr : NULL;

    LOGPROCENTRY("'%s'", GetPoolStr (&Screen->Title));

    REFIT_MENU_ENTRY   *TempChosenEntry     = NULL;
    MENU_STYLE_FUNC     Style               = TextMenuStyle;
    MENU_STYLE_FUNC     MainStyle           = TextMenuStyle;
    UINTN               MenuExit            = 0;
    INTN                DefaultEntryIndex   = -1;
    INTN                DefaultSubmenuIndex = -1;

    #if REFIT_DEBUG > 0
    static BOOLEAN  ShowLoaded          = TRUE;
           BOOLEAN  SetSelection = FALSE;
    #endif

    LOG(4, LOG_BLANK_LINE_SEP, L"X");
    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 1 - START");

    TileSizes[0] = (GlobalConfig.IconSizes[ICON_SIZE_BIG] * 9) / 8;
    TileSizes[1] = (GlobalConfig.IconSizes[ICON_SIZE_SMALL] * 4) / 3;

    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 2");

    #if REFIT_DEBUG > 0
    if (ShowLoaded) {
        LOG2(1, LOG_STAR_SEPARATOR, L"INFO: ", L"", L"Loaded RefindPlus v%s on %s Firmware",
            REFINDPLUS_VERSION, VendorInfo
        );
    }
    #endif

    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 3");
    if (DefaultSelection && *DefaultSelection) {
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 3a 1");
        // Find a menu entry that includes *DefaultSelection as a substring
        DefaultEntryIndex = FindMenuShortcutEntry (Screen, *DefaultSelection);

        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 3a 2");
        #if REFIT_DEBUG > 0
        if (ShowLoaded) {
            LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 3a 2a 1");
            SetSelection = TRUE;

            LOG2(2, LOG_LINE_NORMAL, L"\n      ", L"", L"Configured Default Loader:- '%s'", *DefaultSelection);
            LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 3a 2a 2");
        }
        #endif
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 3a 3");
        MY_FREE_POOL(*DefaultSelection);
    }

    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 4");
    #if REFIT_DEBUG > 0
    if (ShowLoaded) {
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 4a 1");
        ShowLoaded  = FALSE;

        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 4a 2");
        if (SetSelection) {
            UINTN EntryPosition = (DefaultEntryIndex < 0) ? 0 : DefaultEntryIndex;
            LOG2(2, LOG_LINE_NORMAL, L"\n      ", L"", L"Highlighted Screen Option:- '%s'",
                GetPoolStr (&Screen->Entries[EntryPosition]->Title)
            );
            LOG(2, LOG_BLANK_LINE_SEP, L"X");
            LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 4a 2a 2");
        }
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 4a 3");
        MsgLog ("\n\n");
    }
    #endif
    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 5");

    // remove any buffered key strokes
    ReadAllKeyStrokes();
    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 6");

    if (AllowGraphicsMode) {
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 6a 1");
        Style     = GraphicsMenuStyle;
        MainStyle = MainMenuStyle;

        PointerEnabled = PointerActive = pdAvailable();
        DrawSelection  = !PointerEnabled;
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 6a 2");
    }

    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 7");
    // Generate WaitList if not already generated.
    GenerateWaitList();

    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 8");
    // Save time elaspsed from start til now
    MainMenuLoad = GetCurrentMS();

    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9");
    do {
        LOG(4, LOG_BLANK_LINE_SEP, L"X");
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 1 START DO LOOP");
        Screen = ScreenPtr ? *ScreenPtr : NULL;
        MenuExit = RunGenericMenu (Screen, MainStyle, &DefaultEntryIndex, &TempChosenEntry);
        if (LOGPOOL (TempChosenEntry));

        #if REFIT_DEBUG > 0
        LOG(2, LOG_LINE_NORMAL,
            L"Returned '%d' (%s) from RunGenericMenu Call on '%s' in 'RunMainMenu'",
            MenuExit, MenuExitInfo (MenuExit), GetPoolStr (&TempChosenEntry->Title)
        );
        #endif

        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 2");
        Screen->TimeoutSeconds = 0;

        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3");
        if (MenuExit == MENU_EXIT_DETAILS) {
            LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 1");
            if (!TempChosenEntry->SubScreen) {
                LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 1a 1");
                // no sub-screen; ignore keypress
                MenuExit = 0;
                LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 1a 2");
            }
            else {
                LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 1b 1");
                MenuExit = RunGenericMenu (
                    TempChosenEntry->SubScreen,
                    Style,
                    &DefaultSubmenuIndex,
                    &TempChosenEntry
                );
                if (LOGPOOL (TempChosenEntry));

                LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 1b 2");
                #if REFIT_DEBUG > 0
                LOG(2, LOG_LINE_NORMAL,
                    L"Returned '%d' (%s) from RunGenericMenu Call on SubScreen '%s' in 'RunMainMenu'",
                    MenuExit, MenuExitInfo (MenuExit), GetPoolStr (&TempChosenEntry->SubScreen->Title)
                );
                #endif

                LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 1b 3");
                if (MenuExit == MENU_EXIT_ESCAPE || TempChosenEntry->Tag == TAG_RETURN) {
                    MenuExit = 0;
                }

                LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 1b 4");
                if (MenuExit == MENU_EXIT_DETAILS) {
                    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 5a 1b 4a 1");
                    if (!EditOptions (TempChosenEntry)) {
                        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 5a 1b 4a 1a 1");
                        MenuExit = 0;
                        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 1b 4a 1a 2");
                    }
                    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 1b 4a 2");
                }
                LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 1b 5");
            }
            LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 3a 2");
        } // if MenuExit == MENU_EXIT_DETAILS

        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 4");
        if (MenuExit == MENU_EXIT_HIDE) {
            LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 4a 1");
            if (GlobalConfig.HiddenTags) {
                LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 4a 1a 1");
                HideTag (TempChosenEntry);
                LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 4a 1a 2");
            }

            LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 4a 2");
            MenuExit = 0;
            LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 4a 3");
        }
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 9a 5 END DO LOOP");
        LOG(4, LOG_BLANK_LINE_SEP, L"X");
    } while (MenuExit == 0);
    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 10");

    // Ignore MenuExit if FlushFailedTag is set and not previously reset
    if (FlushFailedTag && !FlushFailReset) {
        #if REFIT_DEBUG > 0
        LOG2(2, LOG_THREE_STAR_END, L"INFO: ", L"\n\n", L"FlushFailedTag is Set ... Ignore MenuExit");
        #endif

        FlushFailedTag = FALSE;
        FlushFailReset = TRUE;
        MenuExit = 0;
    }

    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 11");
    if (ChosenEntry) {
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 11a 1");
        *ChosenEntry = TempChosenEntry;
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 11a 2");
    }

    LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 12");
    if (DefaultSelection) {
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 12a 1");
        *DefaultSelection = StrDuplicate (GetPoolStr (&TempChosenEntry->Title));
        LOG(4, LOG_LINE_FORENSIC, L"In RunMainMenu ... 12a 2");
    }

    LOG(4, LOG_LINE_FORENSIC,
        L"In RunMainMenu ... 13- END:- return UINTN MenuExit = '%d'",
        MenuExit
    );
    LOG(4, LOG_BLANK_LINE_SEP, L"X");
    LOGPROCEXIT("MenuExit:%d", MenuExit);
    return MenuExit;
} // UINTN RunMainMenu()

VOID FreeMenuScreen (
    REFIT_MENU_SCREEN **Screen
) {
    UINTN i;

    if (Screen && *Screen) {
        LOGPROCENTRY("%p %s", *Screen, GetPoolStr(&(*Screen)->Title) ? GetPoolStr(&(*Screen)->Title) : L"NULL");
        FreePoolStr (&(*Screen)->Title);
        FreePoolImage (&(*Screen)->TitleImage);
        if ((*Screen)->InfoLines) {
            for (i = 0; i < (*Screen)->InfoLineCount; i++) {
                FreePoolStr_PS_ (&(*Screen)->InfoLines[i]);
            }
            MY_FREE_POOL((*Screen)->InfoLines);
            (*Screen)->InfoLineCount = 0;
        }
        if ((*Screen)->Entries) {
            for (i = 0; i < (*Screen)->EntryCount; i++) {
                FreeMenuEntry (&(*Screen)->Entries[i]);
            }
            MY_FREE_POOL((*Screen)->Entries);
            (*Screen)->EntryCount = 0;
        }

        FreePoolStr (&(*Screen)->TimeoutText);
        FreePoolStr (&(*Screen)->Hint1);
        FreePoolStr (&(*Screen)->Hint2);
        MY_FREE_POOL(*Screen);
        LOGPROCEXIT();
    }
} // VOID FreeMenuScreen()

VOID FreeLegacyEntry (
    IN LEGACY_ENTRY **Entry
) {
    FreeMenuEntry ((REFIT_MENU_ENTRY ** )Entry);
} // VOID FreeLegacyEntry()

VOID FreeLoaderEntry (
    IN OUT LOADER_ENTRY **Entry
) {
    FreeMenuEntry ((REFIT_MENU_ENTRY ** )Entry);
} // VOID FreeLoaderEntry()

VOID FreeMenuEntry (
    REFIT_MENU_ENTRY **Entry
) {
    if (Entry && *Entry) {
        LOGPROCENTRY("%p %s", (*Entry), GetPoolStr (&(*Entry)->Title) ? GetPoolStr (&(*Entry)->Title) : L"NULL");
        //LOGPOOLALWAYS ((*Entry));
        FreePoolStr (&(*Entry)->Title);
        FreePoolImage (&(*Entry)->Image);
        FreePoolImage (&(*Entry)->BadgeImage);
        FreeMenuScreen (&(*Entry)->SubScreen);

        ENTRY_TYPE EntryType = GetMenuEntryType (*Entry);
        if (EntryType == EntryTypeLoaderEntry) {
            FreePoolStr (&((LOADER_ENTRY *)(*Entry))->Title);
            FreePoolStr (&((LOADER_ENTRY *)(*Entry))->LoaderPath);
            FreeVolume  (&((LOADER_ENTRY *)(*Entry))->Volume);
            FreePoolStr (&((LOADER_ENTRY *)(*Entry))->LoadOptions);
            FreePoolStr (&((LOADER_ENTRY *)(*Entry))->InitrdPath);
            MY_FREE_POOL (((LOADER_ENTRY *)(*Entry))->EfiLoaderPath);
        }
        else if (EntryType == EntryTypeLegacyEntry) {
            FreeVolume    (&((LEGACY_ENTRY *)(*Entry))->Volume);
            FreeBdsOption (&((LEGACY_ENTRY *)(*Entry))->BdsOption);
            FreePoolStr   (&((LEGACY_ENTRY *)(*Entry))->LoadOptions);
        }

        MY_FREE_POOL(*Entry);
        LOGPROCEXIT();
    }
} // VOID FreeMenuEntry()

BDS_COMMON_OPTION * CopyBdsOption (
    BDS_COMMON_OPTION *BdsOption
) {
    BDS_COMMON_OPTION *NewBdsOption = NULL;

    if (BdsOption) {
        if (LOGPOOL (BdsOption));
        NewBdsOption = AllocateCopyPool (sizeof (*BdsOption), BdsOption);
        LOGPROCENTRY("%p = %p (%d)", NewBdsOption, BdsOption, sizeof (*BdsOption));
        if (NewBdsOption) {
            #define COPYBDSITEM(x,size) if (BdsOption->x) NewBdsOption->x = AllocateCopyPool (size, BdsOption->x); MsgLog ("%p " #x "\n", NewBdsOption->x);
            COPYBDSITEM(DevicePath  , GetDevicePathSize (BdsOption->DevicePath  ))

            COPYBDSITEM(OptionName  ,           StrSize (BdsOption->OptionName  ))

            COPYBDSITEM(Description ,           StrSize (BdsOption->Description ))

            COPYBDSITEM(LoadOptions ,                    BdsOption->LoadOptionsSize)

            COPYBDSITEM(StatusString,           StrSize (BdsOption->StatusString))
        }
        LOGPROCEXIT();
    } // if NewBdsOption()

    return NewBdsOption;
} // BDS_COMMON_OPTION * CopyBdsOption()

VOID FreeBdsOption (
    BDS_COMMON_OPTION **BdsOption
) {
    if (BdsOption && *BdsOption) {
        LOGPROCENTRY("%p -> %p Boot%04x - '%s'", BdsOption, *BdsOption, (*BdsOption)->BootCurrent, (*BdsOption)->Description);
        MY_FREE_POOL((*BdsOption)->DevicePath);
        MY_FREE_POOL((*BdsOption)->OptionName);
        MY_FREE_POOL((*BdsOption)->Description);
        MY_FREE_POOL((*BdsOption)->LoadOptions);
        MY_FREE_POOL((*BdsOption)->StatusString);
        MY_FREE_POOL(*BdsOption);
        LOGPROCEXIT();
    }
} // VOID FreeBdsOption()


ENTRY_TYPE
GetMenuEntryType (
    REFIT_MENU_ENTRY *Entry
) {
    ENTRY_TYPE EntryType = EntryTypeRefitMenuEntry;
    switch (Entry->Tag) {
        #define TAGS_TAG_TO_ENTRY_TYPE
        #include "tags.include"
    }
    return EntryType;
}

// Creates a shallow copy of a menu entry. Intended for an entry that can be modified.
// The entry is not meant to be used by any menu functions.
// This entry should only exist while the original entry exists. Use MY_FREE_POOL to free
// the result since this is a shallow copy only.
REFIT_MENU_ENTRY *
CopyMenuEntryShallow (
    REFIT_MENU_ENTRY *Entry
) {
    ENTRY_TYPE EntryType = GetMenuEntryType (Entry);
    REFIT_MENU_ENTRY *NewEntry = NULL;
    switch (EntryType) {
        case EntryTypeLoaderEntry: NewEntry = (REFIT_MENU_ENTRY *)AllocateCopyPool (sizeof(LOADER_ENTRY    ), Entry); MsgLog ("Copied LOADER_ENTRY\n"    ); break;
        case EntryTypeLegacyEntry: NewEntry = (REFIT_MENU_ENTRY *)AllocateCopyPool (sizeof(LEGACY_ENTRY    ), Entry); MsgLog ("Copied LEGACY_ENTRY\n"    ); break;
        default                  : NewEntry = (REFIT_MENU_ENTRY *)AllocateCopyPool (sizeof(REFIT_MENU_ENTRY), Entry); MsgLog ("Copied REFIT_MENU_ENTRY\n"); break;
    }
    return NewEntry;
}


#if REFIT_DEBUG > 0

VOID
LEAKABLEBDSOPTION (
    BDS_COMMON_OPTION *BdsOption
) {
    if (BdsOption) {
        LEAKABLEPATHINC ();
            LEAKABLEWITHPATH (BdsOption->DevicePath, "DevicePath");
            LEAKABLEWITHPATH (BdsOption->OptionName, "OptionName");
            LEAKABLEWITHPATH (BdsOption->Description, "Description");
            LEAKABLEWITHPATH (BdsOption->LoadOptions, "LoadOptions");
            LEAKABLEWITHPATH (BdsOption->StatusString, "StatusString");
        LEAKABLEPATHDEC ();
    }
    LEAKABLEWITHPATH (BdsOption, "BdsOption");
}


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

            ENTRY_TYPE EntryType = GetMenuEntryType (Entry);
            LEAKABLEPOOLSTR  ((EntryType == EntryTypeLoaderEntry) ? &((LOADER_ENTRY *)Entry)->Title_PS_       : NULL, "Loader Entry Title");
            LEAKABLEPOOLSTR  ((EntryType == EntryTypeLoaderEntry) ? &((LOADER_ENTRY *)Entry)->LoaderPath_PS_  : NULL, "Loader Entry Loader Path");
            LEAKABLEPOOLSTR  ((EntryType == EntryTypeLoaderEntry) ? &((LOADER_ENTRY *)Entry)->LoadOptions_PS_ : NULL, "Loader Entry Loader Options");
            LEAKABLEPOOLSTR  ((EntryType == EntryTypeLoaderEntry) ? &((LOADER_ENTRY *)Entry)->InitrdPath_PS_  : NULL, "Loader Entry Initrd Path");
            LEAKABLEWITHPATH ((EntryType == EntryTypeLoaderEntry) ?  ((LOADER_ENTRY *)Entry)->EfiLoaderPath   : NULL, "Loader Efi Loader Path");

            LEAKABLEBDSOPTION((EntryType == EntryTypeLegacyEntry) ?  ((LEGACY_ENTRY *)Entry)->BdsOption       : NULL);
            LEAKABLEPOOLSTR  ((EntryType == EntryTypeLegacyEntry) ? &((LEGACY_ENTRY *)Entry)->LoadOptions_PS_ : NULL, "Legacy Entry Loader Options");
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
    LOGPROCENTRY();
    LEAKABLEPATHINIT (LeakableObjectID);
    LEAKABLEMENU (Menu);
    LEAKABLEPATHDONE ();
    LOGPROCEXIT();
}

#endif
