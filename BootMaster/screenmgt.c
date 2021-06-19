/*
 * BootMaster/screenmgt.c
 * Screen handling functions
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
 * Modifications copyright (c) 2012-2021 Roderick W. Smith
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
#include "config.h"
#include "libegint.h"
#include "lib.h"
#include "leaks.h"
#include "menu.h"
#include "mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "../include/egemb_refindplus_banner.h"

// Console defines and variables

UINTN ConWidth;
UINTN ConHeight;
CHAR16 *BlankLine = NULL;

// UGA defines and variables

UINTN   ScreenW;
UINTN   ScreenH;

BOOLEAN AllowGraphicsMode;
BOOLEAN IsBoot = FALSE;

EG_PIXEL StdBackgroundPixel  = { 0xbf, 0xbf, 0xbf, 0 };
EG_PIXEL MenuBackgroundPixel = { 0xbf, 0xbf, 0xbf, 0 };
EG_PIXEL DarkBackgroundPixel = { 0x0,  0x0,  0x0,  0 };

// general defines and variables
static BOOLEAN GraphicsScreenDirty;
static BOOLEAN haveError = FALSE;


static VOID
PrepareBlankLine (
    VOID
) {
    UINTN i;

    MyFreePool (&BlankLine);
    // make a buffer for a whole text line
    BlankLine = AllocatePool ((ConWidth + 1) * sizeof (CHAR16));
    LEAKABLE(BlankLine, "PrepareBlankLine");
    for (i = 0; i < ConWidth; i++) {
        BlankLine[i] = ' ';
    }
    BlankLine[i] = 0;
}

//
// Screen initialization and switching
//

VOID InitScreen (
    VOID
) {
    MsgLog ("[ InitScreen\n");

    // initialize libeg
    egInitScreen();

    if (egHasGraphicsMode()) {
        LOG(4, LOG_LINE_NORMAL, L"Graphics mode detected; getting screen size");

        egGetScreenSize (&ScreenW, &ScreenH);
        AllowGraphicsMode = TRUE;

    }
    else {
        LOG(2, LOG_LINE_NORMAL, L"No graphics mode detected; setting text mode");

        AllowGraphicsMode = FALSE;
        egSetTextMode (GlobalConfig.RequestedTextMode);
        egSetGraphicsModeEnabled (FALSE);   // just to be sure we are in text mode
    }
    GraphicsScreenDirty = TRUE;

    // disable cursor
    refit_call2_wrapper(gST->ConOut->EnableCursor, gST->ConOut, FALSE);

    // get size of text console
    if (refit_call4_wrapper(
        gST->ConOut->QueryMode,
        gST->ConOut,
        gST->ConOut->Mode->Mode,
        &ConWidth,
        &ConHeight) != EFI_SUCCESS
    ) {
        // use default values on error
        ConWidth = 80;
        ConHeight = 25;
    }

    PrepareBlankLine();

    // show the banner if in text mode
    if (GlobalConfig.TextOnly && (GlobalConfig.ScreensaverTime != -1)) {
        DrawScreenHeader (L"Initializing...");
    }
    MsgLog ("] InitScreen\n");
}

// Set the screen resolution and mode (text vs. graphics).
VOID SetupScreen (
    VOID
) {
    UINTN          NewWidth;
    UINTN          NewHeight;
    BOOLEAN        gotGraphics;
    static BOOLEAN BannerLoaded = FALSE;
    static BOOLEAN ScaledIcons  = FALSE;

    MsgLog ("[ SetupScreen\n");

    // Convert mode number to horizontal & vertical resolution values
    if ((GlobalConfig.RequestedScreenWidth > 0) &&
        (GlobalConfig.RequestedScreenHeight == 0)
    ) {
        MsgLog ("Get Resolution From Mode:\n");

        egGetResFromMode (
            &(GlobalConfig.RequestedScreenWidth),
            &(GlobalConfig.RequestedScreenHeight)
        );
    }

    // Set the believed-to-be current resolution to the LOWER of the current
    // believed-to-be resolution and the requested resolution. This is done to
    // enable setting a lower-than-default resolution.
    if ((GlobalConfig.RequestedScreenWidth > 0) &&
        (GlobalConfig.RequestedScreenHeight > 0)
    ) {
        MsgLog ("Sync Resolution:\n");

        ScreenW = (ScreenW < GlobalConfig.RequestedScreenWidth)
            ? ScreenW
            : GlobalConfig.RequestedScreenWidth;

        ScreenH = (ScreenH < GlobalConfig.RequestedScreenHeight)
            ? ScreenH
            : GlobalConfig.RequestedScreenHeight;

        LOG(2, LOG_LINE_NORMAL,
            L"Recording current resolution as %d x %d",
            ScreenW, ScreenH
        );
    }

    // Set text mode. If this requires increasing the size of the graphics mode, do so.
    if (egSetTextMode (GlobalConfig.RequestedTextMode)) {

        MsgLog ("Set Text Mode:\n");

        egGetScreenSize (&NewWidth, &NewHeight);
        if ((NewWidth > ScreenW) || (NewHeight > ScreenH)) {
            ScreenW = NewWidth;
            ScreenH = NewHeight;
        }

        LOG(2, LOG_LINE_NORMAL,
            L"After setting text mode, recording new current resolution as %d x %d",
            ScreenW, ScreenH
        );

        if ((ScreenW > GlobalConfig.RequestedScreenWidth) ||
            (ScreenH > GlobalConfig.RequestedScreenHeight)
        ) {
            LOG2(2, LOG_LINE_NORMAL, L"  - ", L"\n", L"Adjusting requested screen size based on actual screen size");

            // Requested text mode forces us to use a bigger graphics mode
            GlobalConfig.RequestedScreenWidth  = ScreenW;
            GlobalConfig.RequestedScreenHeight = ScreenH;
        } // if

        if (GlobalConfig.RequestedScreenWidth > 0) {

            MsgLog ("Set to User Requested Screen Size:\n");

            egSetScreenSize (
                &(GlobalConfig.RequestedScreenWidth),
                &(GlobalConfig.RequestedScreenHeight)
            );
            egGetScreenSize (&ScreenW, &ScreenH);
        } // if user requested a particular screen resolution
    }

    if (GlobalConfig.TextOnly) {
        // Set text mode if requested
        AllowGraphicsMode = FALSE;
        SwitchToText (FALSE);

        LOG2(2, LOG_LINE_NORMAL, L"INFO: ", L"\n\n", L"Screen Set to Text Mode");
    }
    else if (AllowGraphicsMode) {
        gotGraphics = egIsGraphicsModeEnabled();
        if (!gotGraphics || !BannerLoaded) {
            LOG2(2, LOG_LINE_NORMAL, L"", L":\n", L"%s", gotGraphics ? L"Prepare Placeholder Display" : L"Prepare Graphics Mode Switch");
            LOG2(2, LOG_LINE_NORMAL, L"  - ", L"\n", L"Screen Vertical Resolution:- '%dpx'", ScreenH);

            // scale icons up for HiDPI monitors if required
            if (GlobalConfig.ScaleUI == -1) {
                LOG2(3, LOG_LINE_NORMAL, L"    * ", L"\n\n", L"UI Scaling Disabled ... Maintain Icon Scale");
            }
            else if ((GlobalConfig.ScaleUI == 1) || ScreenH >= HIDPI_MIN) {
                if (!ScaledIcons) {
                    LOG2(3, LOG_LINE_NORMAL, L"", L"\n\n", L"Scale Icons Up");

                    GlobalConfig.IconSizes[ICON_SIZE_BADGE] *= 2;
                    GlobalConfig.IconSizes[ICON_SIZE_SMALL] *= 2;
                    GlobalConfig.IconSizes[ICON_SIZE_BIG]   *= 2;
                    GlobalConfig.IconSizes[ICON_SIZE_MOUSE] *= 2;

                    ScaledIcons = TRUE;
                }
                LOG2(3, LOG_LINE_NORMAL, L"    * ", L"\n\n", L"%s ... %s",
                    ScreenH >= HIDPI_MIN ? L"HiDPI Monitor" : L"HiDPI Flag",
                    ScaledIcons ? L"Maintain Scaled Icons" : L"Scale Icons Up"
                );
            }
            else {
                LOG2(3, LOG_LINE_NORMAL, L"    * ", L"\n\n", L"LoDPI Monitor ... Maintain Icon Scale");
            } // if GlobalConfig.ScaleUI

            if (!gotGraphics) {
                LOG2(4, LOG_LINE_NORMAL, L"INFO: ", L"\n\n", L"Running Graphics Mode Switch");

                // clear screen and show banner
                // (now we know we will stay in graphics mode)
                SwitchToGraphics();
            }
            else {
                LOG2(4, LOG_LINE_NORMAL, L"INFO: ", L"\n\n", L"Loading Placeholder Display");
            }

            if (GlobalConfig.ScreensaverTime != -1) {
                BltClearScreen (TRUE);

                #if REFIT_DEBUG > 0
                if (gotGraphics) {
                    LOG2(2, LOG_THREE_STAR_SEP, L"INFO: ", L"\n\n", L"Displayed Placeholder");
                }
                else {
                    LOG2(2, LOG_THREE_STAR_SEP, L"INFO: ", L"\n\n", L"Switch to Graphics Mode ... Success");
                }
                #endif
            }
            else {
                LOG2(1, LOG_THREE_STAR_MID, L"INFO: ", L"\n\n", L"Configured to Start with Screensaver");

                // start with screen blanked
                GraphicsScreenDirty = TRUE;
            }
            BannerLoaded = TRUE;
        }
    }
    else {
        LOG2(1, LOG_THREE_STAR_MID, L"WARN: ", L"\n\n", L"Invalid Screen Mode ... Switching to Text Mode");

        AllowGraphicsMode     = FALSE;
        GlobalConfig.TextOnly = TRUE;
        SwitchToText (FALSE);
    }
    MsgLog ("] SetupScreen\n");
} // VOID SetupScreen()


BOOLEAN HaveOverriden = FALSE;

VOID SwitchToText (
    IN BOOLEAN CursorEnabled
) {
    MsgLog ("[ SwitchToText CursorEnabled:%d\n", CursorEnabled);
    EFI_STATUS     Status;

    MsgLog ("HaveOverriden:%d IsBoot:%d\n", HaveOverriden, IsBoot);
    if (!GlobalConfig.TextRenderer && !HaveOverriden && !IsBoot) {
        // Override Text Renderer Setting
        Status = OcUseBuiltinTextOutput (EfiConsoleControlScreenText);
        HaveOverriden = TRUE;

        if (!EFI_ERROR (Status)) {
            // Condition inside to silence 'Dead Store' flags
            MsgLog ("INFO: 'text_renderer' Config Setting Overriden\n\n");
        }
    }

    egSetGraphicsModeEnabled (FALSE);
    refit_call2_wrapper(gST->ConOut->EnableCursor, gST->ConOut, CursorEnabled);

    MsgLog ("Determine Text Console Size:\n");

    // get size of text console
    Status = refit_call4_wrapper(
        gST->ConOut->QueryMode,
        gST->ConOut,
        gST->ConOut->Mode->Mode,
        &ConWidth,
        &ConHeight
    );

    if (EFI_ERROR (Status)) {
        // use default values on error
        ConWidth  = 80;
        ConHeight = 25;

        MsgLog (
            "  Could Not Get Text Console Size ...Using Default: %d x %d\n\n",
            ConHeight,
            ConWidth
        );
    }
    else {
        MsgLog (
            "  Text Console Size = %d x %d\n\n",
            ConWidth,
            ConHeight
        );
    }
    PrepareBlankLine();

    MsgLog ("INFO: Switch to Text Mode ...Success\n\n");

    IsBoot = FALSE;
    
    MsgLog ("] SwitchToText\n");
}

EFI_STATUS SwitchToGraphics (
    VOID
) {
    MsgLog ("[ SwitchToGraphics\n");
    if (AllowGraphicsMode) {
        if (!egIsGraphicsModeEnabled()) {
            egSetGraphicsModeEnabled (TRUE);
            GraphicsScreenDirty = TRUE;

            MsgLog ("] SwitchToGraphics EFI_SUCCESS\n");
            return EFI_SUCCESS;
        }

        MsgLog ("] SwitchToGraphics EFI_ALREADY_STARTED\n");
        return EFI_ALREADY_STARTED;
    }

    MsgLog ("] SwitchToGraphics EFI_NOT_FOUND\n");
    return EFI_NOT_FOUND;
}

//
// Screen control for running tools
//
VOID BeginTextScreen (
    IN CHAR16 *Title
) {
    MsgLog ("[ BeginTextScreen Title:%s\n", Title ? Title : L"NULL");
    DrawScreenHeader (Title);
    SwitchToText (FALSE);

    // reset error flag
    haveError = FALSE;
    MsgLog ("] BeginTextScreen\n");
}

VOID FinishTextScreen (
    IN BOOLEAN WaitAlways
) {
    MsgLog ("[ FinishTextScreen WaitAlways:%d\n", WaitAlways);
    if (haveError || WaitAlways) {
        SwitchToText (FALSE);
        PauseForKey();
    }

    // reset error flag
    haveError = FALSE;
    MsgLog ("] FinishTextScreen\n");
}

VOID BeginExternalScreen (
    IN BOOLEAN UseGraphicsMode,
    IN CHAR16 *Title
) {
    MsgLog ("[ BeginExternalScreen UseGraphicsMode:%d Title:%s\n", UseGraphicsMode, Title ? Title : L"NULL");
    if (!AllowGraphicsMode) {
        UseGraphicsMode = FALSE;
    }

    if (UseGraphicsMode) {
        SwitchToGraphicsAndClear (FALSE);
    }
    else {
        // clear to dark background
        egClearScreen (&DarkBackgroundPixel);
        DrawScreenHeader (Title);
        SwitchToText (TRUE);
    }

    // reset error flag
    haveError = FALSE;
    MsgLog ("] BeginExternalScreen\n");
}

VOID FinishExternalScreen (
    VOID
) {
    MsgLog ("[ FinishExternalScreen\n");
    // make sure we clean up later
    GraphicsScreenDirty = TRUE;

    if (haveError) {
        SwitchToText (FALSE);
        PauseForKey();
    }

    // Reset the screen resolution, in case external program changed it....
    SetupScreen();

    // reset error flag
    haveError = FALSE;
    MsgLog ("] FinishExternalScreen\n");
}

VOID TerminateScreen (
    VOID
) {
    MsgLog ("[ TerminateScreen\n");
    // clear text screen
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    refit_call1_wrapper(gST->ConOut->ClearScreen,  gST->ConOut);

    // enable cursor
    refit_call2_wrapper(gST->ConOut->EnableCursor, gST->ConOut, TRUE);
    MsgLog ("] TerminateScreen\n");
}

VOID DrawScreenHeader (
    IN CHAR16 *Title
) {
    MsgLog ("[ DrawScreenHeader Title:%s\n", Title ? Title : L"NULL");
    UINTN y;

    // clear to black background
    egClearScreen (&DarkBackgroundPixel); // first clear in graphics mode
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    refit_call1_wrapper(gST->ConOut->ClearScreen,  gST->ConOut); // then clear in text mode

    // paint header background
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BANNER);
    for (y = 0; y < 3; y++) {
        refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 0, y);
        Print (BlankLine);
    }

    MsgLog ("RefindPlus - %s\n", Title);
    // print header text
    refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 3, 1);
    Print (L"RefindPlus - %s", Title);

    // reposition cursor
    refit_call2_wrapper(gST->ConOut->SetAttribute,      gST->ConOut, ATTR_BASIC);
    refit_call3_wrapper(gST->ConOut->SetCursorPosition, gST->ConOut, 0, 4);
    MsgLog ("] DrawScreenHeader\n");
}

//
// Keyboard input
//

BOOLEAN ReadAllKeyStrokes (
    VOID
) {
    BOOLEAN       GotKeyStrokes;
    EFI_STATUS    Status;
    EFI_INPUT_KEY key;

    GotKeyStrokes = FALSE;
    for (;;) {
        Status = refit_call2_wrapper(gST->ConIn->ReadKeyStroke, gST->ConIn, &key);
        if (Status == EFI_SUCCESS) {
            GotKeyStrokes = TRUE;
            continue;
        }
        break;
    }

    #if REFIT_DEBUG > 0
    if (GotKeyStrokes) {
        Status = EFI_SUCCESS;
    }
    else {
        Status = EFI_ALREADY_STARTED;
    }

    LOG(4, LOG_LINE_NORMAL, L"Clear Keystroke Buffer ... %r", Status);
    #endif

    return GotKeyStrokes;
}

// Displays *Text without regard to appearances. Used mainly for debugging
// and rare error messages.
// Position code is used only in graphics mode.
// TODO: Improve to handle multi-line text.
VOID PrintUglyText (
    IN CHAR16 *Text,
    IN UINTN PositionCode
) {
    EG_PIXEL BGColor = COLOR_RED;

    if (Text) {
        if (AllowGraphicsMode &&
            MyStriCmp (L"Apple", gST->FirmwareVendor) &&
            egIsGraphicsModeEnabled()
        ) {
            egDisplayMessage (Text, &BGColor, PositionCode);
            GraphicsScreenDirty = TRUE;
        }
        else {
            // non-Mac or in text mode; a Print() statement will work
            Print (Text);
            Print (L"\n");
        } // if/else
    } // if
} // VOID PrintUglyText()

VOID HaltForKey (
    VOID
) {
    UINTN index;
    BOOLEAN ClearKeystrokes;

    Print (L"\n");

    PrintUglyText (L"", NEXTLINE);
    PrintUglyText (L"* Halted: Press Any Key to Continue *", NEXTLINE);

    ClearKeystrokes = ReadAllKeyStrokes();
    if (ClearKeystrokes) {
        // remove buffered key strokes
        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID, L"Waiting 5 Seconds");
        #endif

        refit_call1_wrapper(gBS->Stall, 5000000);     // 5 seconds delay
        ReadAllKeyStrokes();    // empty the buffer again
    }

    refit_call3_wrapper(gBS->WaitForEvent, 1, &gST->ConIn->WaitForKey, &index);

    GraphicsScreenDirty = TRUE;
    ReadAllKeyStrokes();        // empty the buffer to protect the menu
}

VOID PauseForKey (
    VOID
) {
    UINTN index;
    BOOLEAN ClearKeystrokes;

    Print (L"\n");

    if (GlobalConfig.ContinueOnWarning) {
        PrintUglyText (L"", NEXTLINE);
        PrintUglyText (L"* Paused for Error/Warning. Wait 3 Seconds *", NEXTLINE);
    }
    else {
        PrintUglyText (L"", NEXTLINE);
        PrintUglyText (L"* Paused: Press Any Key to Continue *", NEXTLINE);
    }

    if (GlobalConfig.ContinueOnWarning) {
        refit_call1_wrapper(gBS->Stall, 3000000);
    }
    else {
        ClearKeystrokes = ReadAllKeyStrokes();
        if (ClearKeystrokes) {
            // remove buffered key strokes
            #if REFIT_DEBUG > 0
            LOG(4, LOG_THREE_STAR_MID, L"Waiting 5 Seconds");
            #endif

            refit_call1_wrapper(gBS->Stall, 5000000);     // 5 seconds delay
            ReadAllKeyStrokes();    // empty the buffer again
        }

        refit_call3_wrapper(gBS->WaitForEvent, 1, &gST->ConIn->WaitForKey, &index);
    }

    GraphicsScreenDirty = TRUE;
    // empty the buffer to protect the menu
    ReadAllKeyStrokes();
}

// Pause a specified number of seconds
VOID PauseSeconds (
    UINTN Seconds
) {
    #if REFIT_DEBUG > 0
    LOG(4, LOG_THREE_STAR_MID, L"Pausing for %d Seconds", Seconds);
    #endif

    refit_call1_wrapper(gBS->Stall, 1000000 * Seconds);
} // VOID PauseSeconds()

#if REFIT_DEBUG > 0
VOID DebugPause (
    VOID
) {
    // show console and wait for key
    SwitchToText (FALSE);
    PauseForKey();

    // reset error flag
    haveError = FALSE;
}
#endif

VOID EndlessIdleLoop (
    VOID
) {
    UINTN index;

    for (;;) {
        ReadAllKeyStrokes();
        refit_call3_wrapper(gBS->WaitForEvent, 1, &gST->ConIn->WaitForKey, &index);
    }
}

//
// Error handling
//

BOOLEAN CheckFatalError (
    IN EFI_STATUS Status,
    IN CHAR16 *where
) {
    CHAR16 *Temp = NULL;

    if (!EFI_ERROR (Status)) {
        return FALSE;
    }

#ifdef __MAKEWITH_GNUEFI
    CHAR16 ErrorName[64];
    StatusToString (ErrorName, Status);

    MsgLog ("** FATAL ERROR: %s %s\n", ErrorName, where);

    Temp = PoolPrint (L"Fatal Error: %s %s", ErrorName, where);
#else
    Temp = PoolPrint (L"Fatal Error: %s %s", Status, where);

    MsgLog ("** FATAL ERROR: %r %s\n", Status, where);
#endif
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (Temp, NEXTLINE);
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);
    haveError = TRUE;

    LOG(1, LOG_LINE_NORMAL, L"%s", Temp);

    MyFreePool (&Temp);

    return TRUE;
} // BOOLEAN CheckFatalError()

BOOLEAN CheckError (
    IN EFI_STATUS Status,
    IN CHAR16 *where
) {
    CHAR16 *Temp = NULL;

    if (!EFI_ERROR (Status)) {
        return FALSE;
    }

#ifdef __MAKEWITH_GNUEFI
    CHAR16 ErrorName[64];
    StatusToString (ErrorName, Status);

    MsgLog ("** WARN: %s %s\n", ErrorName, where);

    Temp = PoolPrint (L"Error: %s %s", ErrorName, where);
#else
    Temp = PoolPrint (L"Error: %r %s", Status, where);

    MsgLog ("** WARN: %r %s\n", Status, where);
#endif

    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
    PrintUglyText (Temp, NEXTLINE);
    refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

    // Defeat need to "Press a key to continue" in debug mode
    if (MyStrStr (where, L"While Reading Boot Sector") != NULL ||
        MyStrStr (where, L"in ReadHiddenTags") != NULL
    ) {
        haveError = FALSE;
    }
    else {
        haveError = TRUE;
    }

    LOG(1, LOG_THREE_STAR_SEP, L"%s", Temp);

    MyFreePool (&Temp);

    return haveError;
} // BOOLEAN CheckError()

//
// Graphics functions
//

VOID SwitchToGraphicsAndClear (
    IN BOOLEAN ShowBanner
) {
    MsgLog ("[ SwitchToGraphicsAndClear ShowBanner:%d\n", ShowBanner);
    EFI_STATUS Status;

    #if REFIT_DEBUG > 0
    BOOLEAN    gotGraphics = egIsGraphicsModeEnabled();
    #endif

    Status = SwitchToGraphics();
    if (GraphicsScreenDirty) {
        BltClearScreen (ShowBanner);
    }

    #if REFIT_DEBUG > 0
    if (!gotGraphics) {
        MsgLog ("INFO: Restore Graphics Mode ...%r\n\n", Status);
    }
    #endif
    MsgLog ("] SwitchToGraphicsAndClear\n");
} // VOID SwitchToGraphicsAndClear()

VOID BltClearScreen (
    BOOLEAN ShowBanner
) {
    MsgLog ("[ BltClearScreen ShowBanner:%d\n", ShowBanner);

    static EG_IMAGE *Banner = NULL;
    EG_IMAGE *NewBanner = NULL;
    INTN BannerPosX, BannerPosY;
    EG_PIXEL Black = { 0x0, 0x0, 0x0, 0 };

    #if REFIT_DEBUG > 0
    static BOOLEAN LoggedBanner;
    #endif


    if (ShowBanner && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_BANNER)) {
        MsgLog ("Refresh Screen:\n");
        // load banner on first call
        if (Banner == NULL) {
            MsgLog ("  - Get Banner\n");

            if (GlobalConfig.BannerFileName) {
                Banner = egLoadImage (SelfDir, GlobalConfig.BannerFileName, FALSE);
            }

            #if REFIT_DEBUG > 0
            if (!LoggedBanner) {
                LOG2(3, LOG_LINE_NORMAL, L"    * ", L"\n", L"Using %s", Banner ? L"Custom Title Banner" : L"Default Title Banner");
                LoggedBanner = TRUE;
            }
            #endif
            if (Banner == NULL) {
                Banner = egPrepareEmbeddedImage (&egemb_refindplus_banner, FALSE);
            }
            LEAKABLEONEIMAGE (Banner, "Banner");
        }

        if (Banner) {
            MsgLog ("  - Scale Banner\n");

           if (GlobalConfig.BannerScale == BANNER_FILLSCREEN) {
              if (Banner->Width != ScreenW || Banner->Height != ScreenH) {
                 NewBanner = egScaleImage (Banner, ScreenW, ScreenH);
              }
           }
           else if (Banner->Width > ScreenW || Banner->Height > ScreenH) {
              NewBanner = egCropImage (
                  Banner, 0, 0,
                  (Banner->Width  > ScreenW) ? ScreenW : Banner->Width,
                  (Banner->Height > ScreenH) ? ScreenH : Banner->Height
              );
          } // if GlobalConfig.BannerScale else if Banner->Width

           if (NewBanner != NULL) {
               // DA_TAG: Permit Banner->PixelData Memory Leak on Qemu
               //         Apparent Memory Conflict ... Needs Investigation.
               //         See: sf.net/p/refind/discussion/general/thread/4dfcdfdd16/
               if (1 || DetectedDevices) {
                   egFreeImage (Banner);
               }
               else {
                   MyFreePool (&Banner);
               }

              Banner = NewBanner;
           } // if NewBanner

           MenuBackgroundPixel = Banner->PixelData[0];
        } // if Banner

        // clear and draw banner
        MsgLog ("  - Clear Screen\n");

        if (GlobalConfig.ScreensaverTime != -1) {
            egClearScreen (&MenuBackgroundPixel);
        }
        else {
            egClearScreen (&Black);
        }

        if (Banner != NULL) {
            MsgLog ("  - Show Banner\n\n");

            BannerPosX = (Banner->Width < ScreenW) ? ((ScreenW - Banner->Width) / 2) : 0;
            BannerPosY = (INTN) (ComputeRow0PosY() / 2) - (INTN) Banner->Height;
            if (BannerPosY < 0) {
                BannerPosY = 0;
            }

            GlobalConfig.BannerBottomEdge = BannerPosY + Banner->Height;

            if (GlobalConfig.ScreensaverTime != -1) {
                BltImage (Banner, (UINTN) BannerPosX, (UINTN) BannerPosY);
            }
        }
    }
    else {
        // not showing banner
        // clear to menu background color
        egClearScreen (&MenuBackgroundPixel);
    }

    GraphicsScreenDirty = FALSE;

    // DA_TAG: Permit ScreenBackground->PixelData Memory Leak on items without Devices Detected
    //         Apparent Memory Conflict ... Needs Investigation.
    //         Likely related to Qemu Specific Issue.
    if (1 || DetectedDevices) {
        egFreeImage (GlobalConfig.ScreenBackground);
    }
    else {
        MyFreePool (&GlobalConfig.ScreenBackground);
    }

    GlobalConfig.ScreenBackground = egCopyScreen();
    LEAKABLEONEIMAGE(GlobalConfig.ScreenBackground, "ScreenBackground image");

    MsgLog ("] BltClearScreen\n");
} // VOID BltClearScreen()


VOID BltImage (
    IN EG_IMAGE *Image,
    IN UINTN XPos,
    IN UINTN YPos
) {
    egDrawImage (Image, XPos, YPos);
    GraphicsScreenDirty = TRUE;
}

VOID BltImageAlpha (
    IN EG_IMAGE *Image,
    IN UINTN XPos,
    IN UINTN YPos,
    IN EG_PIXEL *BackgroundPixel
) {
    EG_IMAGE *CompImage;

    // compose on background
    CompImage = egCreateFilledImage (
        Image->Width,
        Image->Height,
        FALSE,
        BackgroundPixel
    );
    egComposeImage (CompImage, Image, 0, 0);

    // blit to screen and clean up
    egDrawImage (CompImage, XPos, YPos);
    egFreeImage (CompImage);
    GraphicsScreenDirty = TRUE;
}

//VOID BltImageComposite (
//    IN EG_IMAGE *BaseImage,
//    IN EG_IMAGE *TopImage,
//    IN UINTN XPos,
//    IN UINTN YPos
//) {
//    UINTN TotalWidth, TotalHeight, CompWidth, CompHeight, OffsetX, OffsetY;
//    EG_IMAGE *CompImage;
//
//    // initialize buffer with base image
//    CompImage = egCopyImage (BaseImage);
//    TotalWidth  = BaseImage->Width;
//    TotalHeight = BaseImage->Height;
//
//    // Place the top image
//    CompWidth = TopImage->Width;
//    if (CompWidth > TotalWidth) {
//        CompWidth = TotalWidth;
//    }
//    OffsetX = (TotalWidth - CompWidth) >> 1;
//    CompHeight = TopImage->Height;
//    if (CompHeight > TotalHeight) {
//        CompHeight = TotalHeight;
//    }
//    OffsetY = (TotalHeight - CompHeight) >> 1;
//    egComposeImage (CompImage, TopImage, OffsetX, OffsetY);
//
//    // blit to screen and clean up
//    egDrawImage (CompImage, XPos, YPos);
//    egFreeImage (CompImage);
//    GraphicsScreenDirty = TRUE;
//}

VOID BltImageCompositeBadge (
    IN EG_IMAGE *BaseImage,
    IN EG_IMAGE *TopImage,
    IN EG_IMAGE *BadgeImage,
    IN UINTN    XPos,
    IN UINTN    YPos
) {
     UINTN    TotalWidth  = 0;
     UINTN    TotalHeight = 0;
     UINTN    CompWidth   = 0;
     UINTN    CompHeight  = 0;
     UINTN    OffsetX     = 0;
     UINTN    OffsetY     = 0;
     EG_IMAGE *CompImage  = NULL;

     // initialize buffer with base image
     if (BaseImage != NULL) {
         CompImage   = egCopyImage (BaseImage);
         TotalWidth  = BaseImage->Width;
         TotalHeight = BaseImage->Height;
     }

     // place the top image
     if ((TopImage != NULL) && (CompImage != NULL)) {
         CompWidth = TopImage->Width;
         if (CompWidth > TotalWidth) {
             CompWidth = TotalWidth;
         }
         OffsetX = (TotalWidth - CompWidth) >> 1;
         CompHeight = TopImage->Height;
         if (CompHeight > TotalHeight) {
             CompHeight = TotalHeight;
         }
         OffsetY = (TotalHeight - CompHeight) >> 1;
         egComposeImage (CompImage, TopImage, OffsetX, OffsetY);
     }

     // place the badge image
     if (BadgeImage != NULL && CompImage != NULL &&
         (BadgeImage->Width  + 8) < CompWidth &&
         (BadgeImage->Height + 8) < CompHeight
     ) {
         OffsetX += CompWidth  - 8 - BadgeImage->Width;
         OffsetY += CompHeight - 8 - BadgeImage->Height;
         egComposeImage (CompImage, BadgeImage, OffsetX, OffsetY);
     }

     // blit to screen and clean up
     if (CompImage != NULL) {
         if (CompImage->HasAlpha) {
             egDrawImageWithTransparency (
                 CompImage, NULL,
                 XPos, YPos,
                 CompImage->Width, CompImage->Height
             );
         }
         else {
             egDrawImage (CompImage, XPos, YPos);
         }
         egFreeImage (CompImage);
         GraphicsScreenDirty = TRUE;
     }
}
