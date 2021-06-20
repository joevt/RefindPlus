/*
 * BootMaster/main.c
 * Main code for the boot menu
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
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
#include "config.h"
#include "screenmgt.h"
#include "launch_legacy.h"
#include "lib.h"
#include "icns.h"
#include "install.h"
#include "menu.h"
#include "mok.h"
#include "apple.h"
#include "mystrings.h"
#include "security_policy.h"
#include "driver_support.h"
#include "launch_efi.h"
#include "scan.h"
#include "../include/refit_call_wrapper.h"
#include "../libeg/efiConsoleControl.h"
#include "../libeg/efiUgaDraw.h"
#include "../include/version.h"
#include "../libeg/libeg.h"
#include "asm.h"
#include "leaks.h"
#include "gpt.h"
#include "BootLog.h"
#include "MemLogLib.h"

#ifndef __MAKEWITH_GNUEFI
#define LibLocateProtocol EfiLibLocateProtocol
#endif

INT16 NowYear   = 0;
INT16 NowMonth  = 0;
INT16 NowDay    = 0;
INT16 NowHour   = 0;
INT16 NowMinute = 0;
INT16 NowSecond = 0;

extern VOID *MyMemSet(VOID *s, int c, UINTN n);

VOID *memset(VOID *s, int c, UINTN n) {
    return MyMemSet(s,c,n);
}


//
// Some built-in menu definitions....

REFIT_MENU_SCREEN *MainMenu = NULL;
REFIT_MENU_SCREEN MainMenuSrc = {
    {L"Main Menu", TRUE},
    {NULL, FALSE},
    0, NULL,
    0, NULL,
    0, {L"Automatic boot", TRUE},
    {L"Use arrow keys to move cursor; Enter to boot;", TRUE},
    {L"Insert, Tab, or F2 for more options; Esc or Backspace to refresh", TRUE}
};

STATIC REFIT_MENU_SCREEN *AboutMenu = NULL;
STATIC REFIT_MENU_SCREEN AboutMenuSrc = {
    {L"About RefindPlus", TRUE},
    {NULL, FALSE},
    0, NULL,
    0, NULL,
    0, {NULL, FALSE},
    {L"Press 'Enter' to return to main menu", TRUE},
    {L"", TRUE}
};

REFIT_CONFIG GlobalConfig = {
    /* TextOnly = */ FALSE,
    /* ScanAllLinux = */ TRUE,
    /* DeepLegacyScan = */ FALSE,
    /* EnableAndLockVMX = */ FALSE,
    /* FoldLinuxKernels = */ TRUE,
    /* EnableMouse = */ FALSE,
    /* EnableTouch = */ FALSE,
    /* HiddenTags = */ TRUE,
    /* UseNvram = */ FALSE,
    /* IgnorePreviousBoot = */ FALSE,
    /* IgnoreVolumeICNS = */ FALSE,
    /* TextRenderer = */ FALSE,
    /* UgaPassThrough = */ FALSE,
    /* ProvideConsoleGOP = */ FALSE,
    /* ReloadGOP = */ FALSE,
    /* UseDirectGop = */ FALSE,
    /* ContinueOnWarning = */ FALSE,
    /* ForceTRIM = */ FALSE,
    /* DisableCompatCheck = */ FALSE,
    /* DisableAMFI = */ FALSE,
    /* SupplyAPFS = */ FALSE,
    /* SilenceAPFS = */ FALSE,
    /* SyncAPFS = */ FALSE,
    /* ProtectNVRAM = */ FALSE,
    /* ScanOtherESP = */ FALSE,
    /* ShutdownAfterTimeout = */ FALSE,
    /* Install = */ FALSE,
    /* WriteSystemdVars = */ FALSE,
    /* RequestedScreenWidth = */ 0,
    /* RequestedScreenHeight = */ 0,
    /* BannerBottomEdge = */ 0,
    /* RequestedTextMode = */ DONT_CHANGE_TEXT_MODE,
    /* Timeout = */ 0,
    /* HideUIFlags = */ 0,
    /* MaxTags = */ 0,
    /* GraphicsFor = */ GRAPHICS_FOR_OSX,
    /* LegacyType = */ LEGACY_TYPE_MAC,
    /* ScanDelay = */ 0,
    /* ScreensaverTime = */ 0,
    /* MouseSpeed = */ 4,
    /* IconSizes = */ {
        DEFAULT_BIG_ICON_SIZE / 4,
        DEFAULT_SMALL_ICON_SIZE,
        DEFAULT_BIG_ICON_SIZE,
        DEFAULT_MOUSE_SIZE
    },
    /* BannerScale = */ BANNER_NOSCALE,
    /* ScaleUI = */ 0,
    /* ActiveCSR = */ 0,
    /* LogLevel = */ 0,
    /* *DiscoveredRoot = */ NULL,
    /* *SelfDevicePath = */ NULL,
    /* *BannerFileName = */ NULL,
    /* *ScreenBackground = */ NULL,
    /* *ConfigFilename = */ CONFIG_FILE_NAME,
    /* *SelectionSmallFileName = */ NULL,
    /* *SelectionBigFileName = */ NULL,
    /* *DefaultSelection = */ NULL,
    /* *AlsoScan = */ NULL,
    /* *DontScanVolumes = */ NULL,
    /* *DontScanDirs = */ NULL,
    /* *DontScanFiles = */ NULL,
    /* *DontScanTools = */ NULL,
    /* *DontScanFirmware = */ NULL,
    /* *WindowsRecoveryFiles = */ NULL,
    /* *MacOSRecoveryFiles = */ NULL,
    /* *DriverDirs = */ NULL,
    /* *IconsDir = */ NULL,
    /* *SetBootArgs = */ NULL,
    /* *ExtraKernelVersionStrings = */ NULL,
    /* *SpoofOSXVersion = */ NULL,
    /* CsrValues = */ NULL,
    /* ShowTools = */ {
        #define TAGS_DEFAULT_SHOWTOOL
        #include "tags.include"
        #define TAGS_DEFAULT_NOT_SHOWTOOL
        #include "tags.include"
    }
};

#define BOOTKICKER_FILES L"\\EFI\\tools_x64\\x64_BootKicker.efi,\\EFI\\tools_x64\\BootKicker_x64.efi,\
\\EFI\\tools_x64\\BootKicker.efi,\\EFI\\tools\\x64_BootKicker.efi,\\EFI\\tools\\BootKicker_x64.efi,\
\\EFI\\tools\\BootKicker.efi,\\EFI\\x64_BootKicker.efi,\\EFI\\BootKicker_x64.efi,\\EFI\\BootKicker.efi,\
\\x64_BootKicker.efi,\\BootKicker_x64.efi,\\BootKicker.efi"

#define NVRAMCLEAN_FILES L"\\EFI\\tools_x64\\x64_CleanNvram.efi,\\EFI\\tools_x64\\CleanNvram_x64.efi,\
\\EFI\\tools_x64\\CleanNvram.efi,\\EFI\\tools\\x64_CleanNvram.efi,\\EFI\\tools\\CleanNvram_x64.efi,\
\\EFI\\tools\\CleanNvram.efi,\\EFI\\x64_CleanNvram.efi,\\EFI\\CleanNvram_x64.efi,\\EFI\\CleanNvram.efi,\
\\x64_CleanNvram.efi,\\CleanNvram_x64.efi,\\CleanNvram.efi"

CHAR16                *VendorInfo           = NULL;
CHAR16                *gHiddenTools         = NULL;
BOOLEAN                SetSysTab            = FALSE;
BOOLEAN                ConfigWarn           = FALSE;
BOOLEAN                ranCleanNvram        = FALSE;
BOOLEAN                ForceNativeLoggging  = FALSE;
EFI_GUID               RefindPlusGuid       = REFINDPLUS_GUID;
EFI_SET_VARIABLE       AltSetVariable;
EFI_OPEN_PROTOCOL      OrigOpenProtocol;
EFI_HANDLE_PROTOCOL    OrigHandleProtocol;

extern EFI_STATUS RpApfsConnectDevices (VOID);

// Link to Cert GUIDs in mok/guid.c
extern EFI_GUID X509_GUID;
extern EFI_GUID RSA2048_GUID;
extern EFI_GUID PKCS7_GUID;
extern EFI_GUID EFI_CERT_SHA256_GUID;

extern EFI_FILE *gVarsDir;

extern EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPDraw;

//
// misc functions
//

static
EFI_STATUS EFIAPI gRTSetVariableEx (
    IN  CHAR16    *VariableName,
    IN  EFI_GUID  *VendorGuid,
    IN  UINT32     Attributes,
    IN  UINTN      VariableSize,
    IN  VOID      *VariableData
) {
    EFI_STATUS   Status                 = EFI_SECURITY_VIOLATION;
    EFI_GUID     WinGuid                = MICROSOFT_VENDOR_GUID;
    EFI_GUID     X509Guid               = X509_GUID;
    EFI_GUID     PKCS7Guid              = PKCS7_GUID;
    EFI_GUID     Sha001Guid             = EFI_CERT_SHA1_GUID;
    EFI_GUID     Sha224Guid             = EFI_CERT_SHA224_GUID;
    EFI_GUID     Sha256Guid             = EFI_CERT_SHA256_GUID;
    EFI_GUID     Sha384Guid             = EFI_CERT_SHA384_GUID;
    EFI_GUID     Sha512Guid             = EFI_CERT_SHA512_GUID;
    EFI_GUID     RSA2048Guid            = RSA2048_GUID;
    EFI_GUID     RSA2048Sha1Guid        = EFI_CERT_RSA2048_SHA1_GUID;
    EFI_GUID     RSA2048Sha256Guid      = EFI_CERT_RSA2048_SHA256_GUID;
    EFI_GUID     TypeRSA2048Sha256Guid  = EFI_CERT_TYPE_RSA2048_SHA256_GUID;
    UINT32       StorageFlags;

    BOOLEAN BlockCert = (
        (GuidsAreEqual (VendorGuid, &WinGuid) ||
        (GuidsAreEqual (VendorGuid, &X509Guid)) ||
        (GuidsAreEqual (VendorGuid, &PKCS7Guid)) ||
        (GuidsAreEqual (VendorGuid, &Sha001Guid)) ||
        (GuidsAreEqual (VendorGuid, &Sha224Guid)) ||
        (GuidsAreEqual (VendorGuid, &Sha256Guid)) ||
        (GuidsAreEqual (VendorGuid, &Sha384Guid)) ||
        (GuidsAreEqual (VendorGuid, &Sha512Guid)) ||
        (GuidsAreEqual (VendorGuid, &RSA2048Guid)) ||
        (GuidsAreEqual (VendorGuid, &RSA2048Sha1Guid)) ||
        (GuidsAreEqual (VendorGuid, &RSA2048Sha256Guid)) ||
        (GuidsAreEqual (VendorGuid, &TypeRSA2048Sha256Guid))) &&
        (MyStrStr (VendorInfo, L"Apple") != NULL)
    );
    BOOLEAN BlockPRNG = (
        (MyStriCmp (VariableName, L"UnlockID") || MyStriCmp (VariableName, L"UnlockIDCopy")) &&
        MyStrStr (VendorInfo, L"Apple") != NULL
    );

    if (!BlockCert && !BlockPRNG) {
        StorageFlags  = EFI_VARIABLE_BOOTSERVICE_ACCESS;
        StorageFlags |= EFI_VARIABLE_RUNTIME_ACCESS;
        StorageFlags |= EFI_VARIABLE_NON_VOLATILE;
        Status = AltSetVariable (
            VariableName,
            VendorGuid,
            StorageFlags,
            VariableSize,
            (CHAR8 *) &VariableData
        );
    }


    #if REFIT_DEBUG > 0
    LOG2(3, LOG_LINE_NORMAL, L"INFO: ", L"Filtered Write to NVRAM:- '%s' ... %r", VariableName, Status);

    if (BlockCert || BlockPRNG) {
        LOG2(3, LOG_THREE_STAR_MID, L"\n      * ", L"\n", L"Prevented Microsoft Secure Boot NVRAM Write Attempt");
        MsgLog ("         Successful NVRAM Write May Result in BootROM Damage");
    }
    MsgLog ("\n\n");
    #endif

    return Status;
} // VOID gRTSetVariableEx()

static
VOID MapSetVariable (
    IN EFI_SYSTEM_TABLE  *SystemTable
) {
    AltSetVariable                             = gRT->SetVariable;
    RT->SetVariable                            = gRTSetVariableEx;
    gRT->SetVariable                           = gRTSetVariableEx;
    SystemTable->RuntimeServices->SetVariable  = gRTSetVariableEx;
} // MapSetVariable()

static
VOID ActiveCSR (
    VOID
) {
    UINT32  CsrStatus;
    BOOLEAN CsrEnabled;

    // Prime 'Status' for logging
    #if REFIT_DEBUG > 0
    EFI_STATUS Status = EFI_ALREADY_STARTED;
    #endif

    if (GlobalConfig.ActiveCSR == 0) {
        // Early return if not configured to set CSR
        return;
    }
    else {
        // Try to get current CSR status
        if (GetCsrStatus (&CsrStatus) == EFI_SUCCESS) {
            // Record CSR status in the 'gCsrStatus' variable
            RecordgCsrStatus (CsrStatus, FALSE);

            // Check 'gCsrStatus' variable for 'Enabled' term
            if (MyStrStr (GetPoolStr (&gCsrStatus), L"Enabled") != NULL) {
                // 'Enabled' found
                CsrEnabled = TRUE;
            }
            else {
                // 'Enabled' not found
                CsrEnabled = FALSE;
            }

            // If set to always disable
            if (GlobalConfig.ActiveCSR == -1) {
                // Seed the log buffer
                MsgLog ("INFO: Disable SIP/SSV ...");

                if (CsrEnabled) {
                    // Switch SIP/SSV off as currently enabled
                    RotateCsrValue ();

                    // Set 'Status' to 'Success'
                    #if REFIT_DEBUG > 0
                    Status = EFI_SUCCESS;
                    #endif
                }
            }
            else {
                // Seed the log buffer
                MsgLog ("INFO: Enable SIP/SSV ...");

                if (!CsrEnabled) {
                    // Switch SIP/SSV on as currently disbled
                    RotateCsrValue ();

                    // Set 'Status' to 'Success'
                    #if REFIT_DEBUG > 0
                    Status = EFI_SUCCESS;
                    #endif
                }
            }

            // Finalise and flush the log buffer
            #if REFIT_DEBUG > 0
            MsgLog ("%r\n\n", Status);
            #endif
        }
    }
} // VOID ActiveCSR()


static
VOID SetBootArgs (
    VOID
) {
    EFI_STATUS  Status;
    EFI_GUID    AppleGUID   = APPLE_GUID;
    CHAR16      *NameNVRAM  = L"boot-args";
    CHAR16      *BootArg;
    CHAR8       DataNVRAM[255];

    #if REFIT_DEBUG > 0
    BOOLEAN LogDisableAMFI        = FALSE;
    BOOLEAN LogDisableCompatCheck = FALSE;
    #endif

    if (!GlobalConfig.SetBootArgs || GlobalConfig.SetBootArgs[0] == L'\0') {
        #if REFIT_DEBUG > 0
        Status = EFI_INVALID_PARAMETER;
        #endif
    }
    else {
        if (MyStrStr (GlobalConfig.SetBootArgs, L"amfi_get_out_of_my_way=1") != NULL) {
            #if REFIT_DEBUG > 0
            if (GlobalConfig.DisableAMFI) {
                // Ensure Logging
                LogDisableAMFI = TRUE;
            }
            #endif

            // Do not duplicate 'amfi_get_out_of_my_way=1'
            GlobalConfig.DisableAMFI = FALSE;
        }
        if (MyStrStr (GlobalConfig.SetBootArgs, L"-no_compat_check") != NULL) {
            #if REFIT_DEBUG > 0
            if (GlobalConfig.DisableCompatCheck) {
                // Ensure Logging
                LogDisableCompatCheck = TRUE;
            }
            #endif

            // Do not duplicate '-no_compat_check'
            GlobalConfig.DisableCompatCheck = FALSE;
        }

        if (GlobalConfig.DisableAMFI &&
            GlobalConfig.DisableCompatCheck
        ) {
            // Combine Args with DisableAMFI and DisableAMFI
            BootArg = PoolPrint (
                L"%s amfi_get_out_of_my_way=1 -no_compat_check",
                GlobalConfig.SetBootArgs
            );
        }
        else if (GlobalConfig.DisableAMFI) {
            // Combine Args with DisableAMFI
            BootArg = PoolPrint (
                L"%s amfi_get_out_of_my_way=1",
                GlobalConfig.SetBootArgs
            );
        }
        else if (GlobalConfig.DisableCompatCheck) {
            // Combine Args with DisableCompatCheck
            BootArg = PoolPrint (
                L"%s -no_compat_check",
                GlobalConfig.SetBootArgs
            );
        }
        else {
            // Use Args Alone
            BootArg = PoolPrint (L"%s", GlobalConfig.SetBootArgs);
        }

        // Convert BootArg to CHAR8 array in 'ArrCHAR8'
        MyUnicodeStrToAsciiStr  (BootArg, DataNVRAM);
        MyFreePool (&BootArg);

        Status = EfivarSetRaw (
            &AppleGUID,
            NameNVRAM,
            DataNVRAM,
            AsciiStrSize (DataNVRAM),
            TRUE
        );
    }

    #if REFIT_DEBUG > 0
    if (LogDisableAMFI || GlobalConfig.DisableAMFI) {
        LOG2(3, LOG_LINE_NORMAL, L"\n    * ", L"", L"Disable AMFI ... %r", Status);
    }
    LOG2(3, LOG_LINE_NORMAL, L"\n    * ", L"", L"Reset Boot Args ... %r", Status);

    if (LogDisableCompatCheck || GlobalConfig.DisableCompatCheck) {
        LOG2(3, LOG_LINE_NORMAL, L"\n    * ", L"", L"Disable Compat Check ... %r", Status);
    }
    #endif
} // VOID SetBootArgs()


VOID DisableAMFI (
    VOID
) {
    EFI_STATUS  Status;
    EFI_GUID    AppleGUID   = APPLE_GUID;
    CHAR16      *NameNVRAM  = L"boot-args";

    if (GlobalConfig.DisableCompatCheck) {
        // Combine with DisableCompatCheck
        CHAR8 *DataNVRAM = "amfi_get_out_of_my_way=1 -no_compat_check";

        Status = EfivarSetRaw (
            &AppleGUID,
            NameNVRAM,
            DataNVRAM,
            AsciiStrSize (DataNVRAM),
            TRUE
        );
    }
    else {
        CHAR8 *DataNVRAM = "amfi_get_out_of_my_way=1";

        Status = EfivarSetRaw (
            &AppleGUID,
            NameNVRAM,
            DataNVRAM,
            AsciiStrSize (DataNVRAM),
            TRUE
        );
    }

    #if REFIT_DEBUG > 0
    LOG2(3, LOG_LINE_NORMAL, L"\n    * ", L"", L"Disable AMFI ... %r", Status);
    if (GlobalConfig.DisableCompatCheck) {
        LOG(3, LOG_LINE_NORMAL, L"\n    * ", L"" L"Disable Compat Check ... %r", Status);
    }
    #endif
} // VOID DisableAMFI()


VOID DisableCompatCheck (
    VOID
) {
    EFI_STATUS  Status;
    EFI_GUID    AppleGUID    = APPLE_GUID;
    CHAR16      *NameNVRAM   = L"boot-args";
    CHAR8       *DataNVRAM   = "-no_compat_check";

    Status = EfivarSetRaw (
        &AppleGUID,
        NameNVRAM,
        DataNVRAM,
        AsciiStrSize (DataNVRAM),
        TRUE
    );

    LOG2(3, LOG_LINE_NORMAL, L"\n    * ", L"", L"Disable Compat Check ... %r", Status);
} // VOID DisableCompatCheck()


VOID ForceTRIM (
    VOID
) {
    EFI_STATUS  Status;
    EFI_GUID    AppleGUID     = APPLE_GUID;
    CHAR16      *NameNVRAM    = L"EnableTRIM";
    UINT8       DataNVRAM[1]  = {0x01};

    Status = EfivarSetRaw (
        &AppleGUID,
        NameNVRAM,
        DataNVRAM,
        sizeof (DataNVRAM),
        TRUE
    );

    LOG2(3, LOG_LINE_NORMAL, L"\n    * ", L"", L"Forcibly Enable TRIM ... %r", Status);
} // VOID ForceTRIM()


// Extended 'OpenProtocol'
// Ensures GOP Interface for Boot Loading Screen
static
EFI_STATUS EFIAPI OpenProtocolEx (
    IN   EFI_HANDLE  Handle,
    IN   EFI_GUID    *Protocol,
    OUT  VOID        **Interface OPTIONAL,
    IN   EFI_HANDLE  AgentHandle,
    IN   EFI_HANDLE  ControllerHandle,
    IN   UINT32      Attributes
) {
    EFI_STATUS  Status;

    Status = OrigOpenProtocol (
        Handle,
        Protocol,
        Interface,
        AgentHandle,
        ControllerHandle,
        Attributes
    );

    if (Status == EFI_UNSUPPORTED) {
        if (GuidsAreEqual (&gEfiGraphicsOutputProtocolGuid, Protocol)) {
            MsgLog ("[ OpenProtocolEx gEfiGraphicsOutputProtocolGuid\n");
            if (GOPDraw != NULL) {
                Status = EFI_SUCCESS;
                if (Interface) {
                    MsgLog("Using GOPDraw interface for Open GOP Protocol...%r\n", Status);
                }
                else {
                    MsgLog("Using GOPDraw interface for Open GOP Protocol but Interface is NULL ...%r\n", Status);
                }
                *Interface = GOPDraw;
            }
            else {
                MsgLog("Searching for GOPs\n");
                UINTN HandleCount = 0;
                EFI_HANDLE *HandleBuffer = NULL;
                Status = refit_call5_wrapper(
                    gBS->LocateHandleBuffer,
                    ByProtocol,
                    &gEfiGraphicsOutputProtocolGuid,
                    NULL,
                    &HandleCount,
                    &HandleBuffer
                );

                if (!EFI_ERROR (Status)) {
                    UINTN i;
                    for (i = 0; i < HandleCount; i++) {
                        if (HandleBuffer[i] != gST->ConsoleOutHandle) {
                            Status = refit_call6_wrapper(
                                OrigOpenProtocol,
                                HandleBuffer[i],
                                &gEfiGraphicsOutputProtocolGuid,
                                *Interface,
                                AgentHandle,
                                NULL,
                                EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
                            );

                            if (!EFI_ERROR (Status)) {
                                break;
                            }
                        } // if HandleBuffer[i]
                    } // for

                } // if !EFI_ERROR Status

                if (EFI_ERROR (Status) || *Interface == NULL) {
                    if (Interface) {
                        MsgLog("No GOPs found ...%r\n", Status);
                    }
                    else {
                        MsgLog("No GOPs found (NULL Interface) ...%r\n", Status);
                    }
                    Status = EFI_UNSUPPORTED;
                }
                MyFreePool (&HandleBuffer);
            } // If GOPDraw != NULL
            MsgLog ("] OpenProtocolEx gEfiGraphicsOutputProtocolGuid\n");
        } // if GuidsAreEqual

    } // if Status == EFI_UNSUPPORTED

    return Status;
} // EFI_STATUS OpenProtocolEx()


// Extended 'HandleProtocol'
// Routes 'HandleProtocol' to 'OpenProtocol'
static
EFI_STATUS EFIAPI HandleProtocolEx (
    IN   EFI_HANDLE  Handle,
    IN   EFI_GUID    *Protocol,
    OUT  VOID        **Interface
) {
    EFI_STATUS Status;
    LEAKABLEEXTERNALSTART ("HandleProtocolEx OpenProtocol");
    Status = refit_call6_wrapper(
        gBS->OpenProtocol,
        Handle,
        Protocol,
        Interface,
        gImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );
    LEAKABLEEXTERNALSTOP ();
    return Status;
} // EFI_STATUS HandleProtocolEx()

static
VOID ReMapOpenProtocol (
    VOID
) {
    // Amend EFI_BOOT_SERVICES.OpenProtocol
    OrigOpenProtocol    = gBS->OpenProtocol;
    gBS->OpenProtocol   = OpenProtocolEx;
    gBS->Hdr.CRC32      = 0;
    gBS->CalculateCrc32 (gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);
} // ReMapOpenProtocol()


// Checks to see if a specified file seems to be a valid tool.
// Returns TRUE if it passes all tests, FALSE otherwise
static
BOOLEAN IsValidTool (
    IN  REFIT_VOLUME  *BaseVolume,
    IN  CHAR16        *PathName
) {
    UINTN     i            = 0;
    BOOLEAN   retval       = TRUE;
    CHAR16   *TestVolName  = NULL;
    CHAR16   *DontVolName  = NULL;
    CHAR16   *DontPathName = NULL;
    CHAR16   *DontFileName = NULL;
    CHAR16   *TestPathName = NULL;
    CHAR16   *TestFileName = NULL;
    CHAR16   *DontScanThis = NULL;

    if (FileExists (BaseVolume->RootDir, PathName) &&
        IsValidLoader (BaseVolume->RootDir, PathName)
    ) {
        LOGPOOL(TestVolName);
        LOGPOOL(TestPathName);
        LOGPOOL(TestFileName);
        SplitPathName (PathName, &TestVolName, &TestPathName, &TestFileName);

        while (retval && (DontScanThis = FindCommaDelimited (GlobalConfig.DontScanTools, i++))) {
            LOGPOOL(DontVolName);
            LOGPOOL(DontPathName);
            LOGPOOL(DontFileName);
            SplitPathName (DontScanThis, &DontVolName, &DontPathName, &DontFileName);

            if (MyStriCmp (TestFileName, DontFileName) &&
                ((DontPathName == NULL) || (MyStriCmp (TestPathName, DontPathName))) &&
                ((DontVolName == NULL) || (VolumeMatchesDescription (BaseVolume, DontVolName)))
            ) {
                retval = FALSE;
            } // if

            MyFreePool (&DontScanThis);
        } // while

    }
    else {
        retval = FALSE;
    }

    MyFreePool (&TestVolName);
    MyFreePool (&TestPathName);
    MyFreePool (&TestFileName);

    MyFreePool (&DontVolName);
    MyFreePool (&DontPathName);
    MyFreePool (&DontFileName);

    return retval;
} // BOOLEAN IsValidTool()

VOID preBootKicker (
    VOID
) {
    UINTN              MenuExit;
    INTN               DefaultEntry   = 1;
    MENU_STYLE_FUNC    Style          = GraphicsMenuStyle;
    REFIT_MENU_ENTRY  *ChosenEntry;
    static REFIT_MENU_SCREEN *BootKickerMenu = NULL;
    static REFIT_MENU_SCREEN BootKickerMenuSrc = {
        {L"BootKicker" , TRUE},
        {NULL, FALSE},
        0, NULL,
        0, NULL,
        0, {NULL, FALSE},
        {L"Press 'ESC', 'BackSpace' or 'SpaceBar' to Return to Main Menu", TRUE},
        {L"", TRUE}
    };

    if (!BootKickerMenu) {
        BootKickerMenu = CopyMenuScreen (&BootKickerMenuSrc);
        if (!BootKickerMenu) {
            return;
        }
        CopyFromPoolImage_PI_ (&BootKickerMenu->TitleImage_PI_, BuiltinIcon (BUILTIN_ICON_TOOL_BOOTKICKER));
        AssignCachedPoolStr (&BootKickerMenu->Title, L"BootKicker");
        AddMenuInfoLineCached (BootKickerMenu, L"A tool to kick in the Apple Boot Screen");
        AddMenuInfoLineCached (BootKickerMenu, L"Needs GOP Capable Fully Compatible GPUs on Apple Firmware");
        AddMenuInfoLineCached (BootKickerMenu, L"(Fully Compatible GPUs provide native Apple Boot Screen)");
        AddMenuInfoLineCached (BootKickerMenu, L"NB: Hangs and needs physical reboot with other GPUs");
        AddMenuInfoLineCached (BootKickerMenu, L"");
        AddMenuInfoLineCached (BootKickerMenu, L"BootKicker is from OpenCore and Copyright Acidanthera");
        AddMenuInfoLineCached (BootKickerMenu, L"Requires at least one of the files below:");
        AddMenuInfoLineCached (BootKickerMenu, L"\\EFI\\tools_x64\\x64_BootKicker.efi");
        AddMenuInfoLineCached (BootKickerMenu, L"\\EFI\\tools_x64\\BootKicker_x64.efi");
        AddMenuInfoLineCached (BootKickerMenu, L"\\EFI\\tools_x64\\BootKicker.efi");
        AddMenuInfoLineCached (BootKickerMenu, L"\\EFI\\tools\\x64_BootKicker.efi");
        AddMenuInfoLineCached (BootKickerMenu, L"\\EFI\\tools\\BootKicker_x64.efi");
        AddMenuInfoLineCached (BootKickerMenu, L"\\EFI\\tools\\BootKicker.efi");
        AddMenuInfoLineCached (BootKickerMenu, L"\\EFI\\x64_BootKicker.efi");
        AddMenuInfoLineCached (BootKickerMenu, L"\\EFI\\BootKicker_x64.efi");
        AddMenuInfoLineCached (BootKickerMenu, L"\\EFI\\BootKicker.efi");
        AddMenuInfoLineCached (BootKickerMenu, L"");
        AddMenuInfoLineCached (BootKickerMenu, L"The first file found in the order listed will be used");
        AddMenuInfoLineCached (BootKickerMenu, L"You will be returned to the main menu if not found");
        AddMenuInfoLineCached (BootKickerMenu, L"");
        AddMenuInfoLineCached (BootKickerMenu, L"");
        AddMenuInfoLineCached (BootKickerMenu, L"You can get the BootKicker efi file here:");
        AddMenuInfoLineCached (BootKickerMenu, L"https://github.com/acidanthera/OpenCorePkg/releases");
        AddMenuInfoLineCached (BootKickerMenu, L"https://github.com/dakanji/RefindPlus/tree/GOPFix/BootMaster/tools_x64");
        AddMenuInfoLineCached (BootKickerMenu, L"");
        AddMenuInfoLineCached (BootKickerMenu, L"");

        AddMenuEntryCopy (BootKickerMenu, &TagMenuEntry[TAG_LOAD_BOOTKICKER]);
        AddMenuEntryCopy (BootKickerMenu, &TagMenuEntry[TAG_RETURN]);
        
        LEAKABLEROOTMENU (kLeakableMenuBootKicker, BootKickerMenu);
    }

    MenuExit = RunGenericMenu (BootKickerMenu, Style, &DefaultEntry, &ChosenEntry);
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' from RunGenericMenu call on '%s' in 'preBootKicker'",
        MenuExit, GetPoolStr (&ChosenEntry->Title)
    );

    if (ChosenEntry) {
        MsgLog ("User Input Received:\n");

        if (MyStriCmp (GetPoolStr (&ChosenEntry->Title), L"Load BootKicker") &&
            MenuExit == MENU_EXIT_ENTER
        ) {
            UINTN        i = 0;
            UINTN        k = 0;

            CHAR16       *Names          = BOOTKICKER_FILES;
            CHAR16       *FilePath       = NULL;
            BOOLEAN      FoundTool       = FALSE;
            LOADER_ENTRY *ourLoaderEntry = NULL;

            // Log Load BootKicker
            MsgLog ("  - Seek BootKicker\n");

            k = 0;
            while ((FilePath = FindCommaDelimited (Names, k++)) != NULL) {
                MsgLog ("    * Seek %s:\n", FilePath);

                for (i = 0; i < VolumesCount; i++) {
                    if ((Volumes[i]->RootDir != NULL) &&
                        IsValidTool (Volumes[i], FilePath)
                    ) {
                        ourLoaderEntry = AllocateZeroPool (sizeof (LOADER_ENTRY));
                        CopyFromPoolStr (&ourLoaderEntry->me.Title, &ChosenEntry->Title);
                        ourLoaderEntry->me.Tag = TAG_SHOW_BOOTKICKER;
                        ourLoaderEntry->me.Row = 1;
                        ourLoaderEntry->me.ShortcutLetter = 0;
                        CopyFromPoolImage_PI_ (&ourLoaderEntry->me.Image_PI_, BuiltinIcon (BUILTIN_ICON_TOOL_BOOTKICKER));
                        AssignPoolStr (&ourLoaderEntry->LoaderPath, FilePath);
                        AssignVolume (&ourLoaderEntry->Volume, Volumes[i]);
                        ourLoaderEntry->UseGraphicsMode   = TRUE;

                        FoundTool = TRUE;
                        break;
                    } // if
                } // for

                if (FoundTool) {
                    break;
                }
                else {
                    MyFreePool (&FilePath);
                }
            } // while Names

            if (FoundTool) {
                MsgLog ("    ** Success: Found %s\n", FilePath);
                MsgLog ("  - Load BootKicker\n\n");

                // Run BootKicker
                StartTool (ourLoaderEntry);
                MsgLog ("* WARN: BootKicker Error ...Return to Main Menu\n\n");
            }
            else {
                MsgLog ("  * WARN: Could Not Find BootKicker ...Return to Main Menu\n\n");
            }

            MyFreePool (&FilePath);
        }
        else {
            // Log Return to Main Screen
            MsgLog ("  - %s\n\n", GetPoolStr (&ChosenEntry->Title));
        } // if
    }
    else {
        MsgLog ("WARN: Could Not Get User Input  ...Reload Main Menu\n\n");
    } // if
} // VOID preBootKicker()

VOID preCleanNvram (
    VOID
) {
    UINTN              MenuExit;
    INTN               DefaultEntry   = 1;
    MENU_STYLE_FUNC    Style          = GraphicsMenuStyle;
    REFIT_MENU_ENTRY  *ChosenEntry;
    static REFIT_MENU_SCREEN *CleanNvramMenu = NULL;
    static REFIT_MENU_SCREEN CleanNvramMenuSrc = {
        {L"Clean NVRAM", TRUE},
        {NULL, FALSE},
        0, NULL,
        0, NULL,
        0, {NULL, TRUE},
        {L"Press 'ESC', 'BackSpace' or 'SpaceBar' to Return to Main Menu", TRUE},
        {L"", TRUE}
    };

    if (!CleanNvramMenu) {
        CleanNvramMenu = CopyMenuScreen (&CleanNvramMenuSrc);
        if (!CleanNvramMenu) {
            return;
        }

        CopyFromPoolImage_PI_ (&CleanNvramMenu->TitleImage_PI_, BuiltinIcon (BUILTIN_ICON_TOOL_NVRAMCLEAN));
        AddMenuInfoLineCached (CleanNvramMenu, L"A Tool to Clean/Reset Nvram on Macs");
        AddMenuInfoLineCached (CleanNvramMenu, L"Requires Apple Firmware");
        AddMenuInfoLineCached (CleanNvramMenu, L"");
        AddMenuInfoLineCached (CleanNvramMenu, L"CleanNvram is from OpenCore and Copyright Acidanthera");
        AddMenuInfoLineCached (CleanNvramMenu, L"Requires at least one of the files below:");
        AddMenuInfoLineCached (CleanNvramMenu, L"\\EFI\\tools_x64\\x64_CleanNvram.efi");
        AddMenuInfoLineCached (CleanNvramMenu, L"\\EFI\\tools_x64\\CleanNvram_x64.efi");
        AddMenuInfoLineCached (CleanNvramMenu, L"\\EFI\\tools_x64\\CleanNvram.efi");
        AddMenuInfoLineCached (CleanNvramMenu, L"\\EFI\\tools\\x64_CleanNvram.efi");
        AddMenuInfoLineCached (CleanNvramMenu, L"\\EFI\\tools\\CleanNvram_x64.efi");
        AddMenuInfoLineCached (CleanNvramMenu, L"\\EFI\\tools\\CleanNvram.efi");
        AddMenuInfoLineCached (CleanNvramMenu, L"\\EFI\\x64_CleanNvram.efi");
        AddMenuInfoLineCached (CleanNvramMenu, L"\\EFI\\CleanNvram_x64.efi");
        AddMenuInfoLineCached (CleanNvramMenu, L"\\EFI\\CleanNvram.efi");
        AddMenuInfoLineCached (CleanNvramMenu, L"");
        AddMenuInfoLineCached (CleanNvramMenu, L"The first file found in the order listed will be used");
        AddMenuInfoLineCached (CleanNvramMenu, L"You will be returned to the main menu if not found");
        AddMenuInfoLineCached (CleanNvramMenu, L"");
        AddMenuInfoLineCached (CleanNvramMenu, L"");
        AddMenuInfoLineCached (CleanNvramMenu, L"You can get the CleanNvram efi file here:");
        AddMenuInfoLineCached (CleanNvramMenu, L"https://github.com/acidanthera/OpenCorePkg/releases");
        AddMenuInfoLineCached (CleanNvramMenu, L"https://github.com/dakanji/RefindPlus/tree/GOPFix/BootMaster/tools_x64");
        AddMenuInfoLineCached (CleanNvramMenu, L"");
        AddMenuInfoLineCached (CleanNvramMenu, L"");

        AddMenuEntryCopy (CleanNvramMenu, &TagMenuEntry[TAG_LOAD_NVRAMCLEAN]);
        AddMenuEntryCopy (CleanNvramMenu, &TagMenuEntry[TAG_RETURN]);
        
        LEAKABLEROOTMENU (kLeakableMenuCleanNvram, CleanNvramMenu);
    }

    MenuExit = RunGenericMenu (CleanNvramMenu, Style, &DefaultEntry, &ChosenEntry);
    LOG(2, LOG_LINE_NORMAL,
        L"Returned '%d' from RunGenericMenu call on '%s' in 'preCleanNvram'",
        MenuExit, GetPoolStr (&ChosenEntry->Title)
    );

    if (ChosenEntry) {
        MsgLog ("User Input Received:\n");

        if (MyStriCmp (GetPoolStr (&ChosenEntry->Title), L"Load CleanNvram") && (MenuExit == MENU_EXIT_ENTER)) {
            UINTN        i = 0;
            UINTN        k = 0;

            CHAR16        *Names           = NVRAMCLEAN_FILES;
            CHAR16        *FilePath        = NULL;
            BOOLEAN       FoundTool        = FALSE;
            LOADER_ENTRY  *ourLoaderEntry  = NULL;

            // Log Load CleanNvram
            MsgLog ("  - Seek CleanNvram\n");

            k = 0;
            while ((FilePath = FindCommaDelimited (Names, k++)) != NULL) {

                MsgLog ("    * Seek %s:\n", FilePath);

                for (i = 0; i < VolumesCount; i++) {
                    if ((Volumes[i]->RootDir != NULL) && (IsValidTool (Volumes[i], FilePath))) {
                        ourLoaderEntry = AllocateZeroPool (sizeof (LOADER_ENTRY));
                        CopyFromPoolStr (&ourLoaderEntry->me.Title, &ChosenEntry->Title);
                        ourLoaderEntry->me.Tag = TAG_NVRAMCLEAN;
                        ourLoaderEntry->me.Row = 1;
                        ourLoaderEntry->me.ShortcutLetter = 0;
                        CopyFromPoolImage_PI_ (&ourLoaderEntry->me.Image_PI_, BuiltinIcon (BUILTIN_ICON_TOOL_NVRAMCLEAN));
                        CopyPoolStr (&ourLoaderEntry->LoaderPath, FilePath);
                        AssignVolume (&ourLoaderEntry->Volume, Volumes[i]);
                        ourLoaderEntry->UseGraphicsMode   = FALSE;

                        FoundTool = TRUE;
                        break;
                    } // if
                } // for

                if (FoundTool) {
                    break;
                }
                else {
                    MyFreePool (&FilePath);
                }
            } // while Names

            if (FoundTool) {
                MsgLog ("    ** Success: Found %s\n", FilePath);
                MsgLog ("  - Load CleanNvram\n\n");

                ranCleanNvram = TRUE;

                // Run CleanNvram
                StartTool (ourLoaderEntry);

            }
            else {
                MsgLog ("  * WARN: Could Not Find CleanNvram ...Return to Main Menu\n\n");
            }

            MyFreePool (&FilePath);
        }
        else {
            // Log Return to Main Screen
            MsgLog ("  - %s\n\n", GetPoolStr (&ChosenEntry->Title));
        } // if
    }
    else {
        MsgLog ("WARN: Could Not Get User Input  ...Reload Main Menu\n\n");
    } // if
} // VOID preCleanNvram()


VOID AboutRefindPlus (
    VOID
) {
    LOG(1, LOG_LINE_THIN_SEP, L"Displaying About/Info Screen");

    if (!AboutMenu) {
        AboutMenu = CopyMenuScreen (&AboutMenuSrc);
        if (!AboutMenu) {
            return;
        }
        
        UINT32  CsrStatus;
        CHAR16  *TempStr;
        CHAR16  *FirmwareVendor = StrDuplicate (VendorInfo);

        CopyFromPoolImage_PI_ (&AboutMenu->TitleImage_PI_, BuiltinIcon (BUILTIN_ICON_FUNC_ABOUT));
        AddMenuInfoLinePool (AboutMenu, PoolPrint (L"RefindPlus v%s", REFINDPLUS_VERSION));
        AddMenuInfoLineCached (AboutMenu, L"");

        AddMenuInfoLineCached (AboutMenu, L"Copyright (c) 2020-2021 Dayo Akanji");
        AddMenuInfoLineCached (AboutMenu, L"Portions Copyright (c) 2012-2021 Roderick W. Smith");
        AddMenuInfoLineCached (AboutMenu, L"Portions Copyright (c) 2006-2010 Christoph Pfisterer");
        AddMenuInfoLineCached (AboutMenu, L"Portions Copyright (c) The Intel Corporation and others");
        AddMenuInfoLineCached (AboutMenu, L"Distributed under the terms of the GNU GPLv3 license");
        AddMenuInfoLineCached (AboutMenu, L"");

        #if defined (__MAKEWITH_GNUEFI)
        AddMenuInfoLineCached (AboutMenu, L"Built with GNU-EFI");
        #else
        AddMenuInfoLineCached (AboutMenu, L"Built with TianoCore EDK II");
        #endif

        AddMenuInfoLineCached (AboutMenu, L"");

        // More than ~65 causes empty info page on 800x600 display
        LimitStringLength (FirmwareVendor, MAX_LINE_LENGTH);

        AddMenuInfoLinePool (AboutMenu, PoolPrint (
            L"Firmware Vendor: %s %d.%02d",
            FirmwareVendor,
            gST->FirmwareRevision >> 16,
            gST->FirmwareRevision & 0xffff
        ));

        #if defined (EFI32)
        AddMenuInfoLineCached (AboutMenu, L"Platform: x86 (32 bit)");
        #elif defined (EFIX64)
        AddMenuInfoLineCached (AboutMenu, L"Platform: x86_64 (64 bit)");
        #elif defined (EFIAARCH64)
        AddMenuInfoLineCached (AboutMenu, L"Platform: ARM (64 bit)");
        #else
        AddMenuInfoLineCached (AboutMenu, L"Platform: Unknown");
        #endif

        if ((gST->Hdr.Revision >> 16) == 1) {
            TempStr = L"EFI";
        }
        else {
            TempStr = L"UEFI";
        }
        AddMenuInfoLinePool (AboutMenu, PoolPrint (
            L"EFI Revision: %s %d.%02d",
            TempStr,
            gST->Hdr.Revision >> 16,
            gST->Hdr.Revision & 0xffff
        ));

        AddMenuInfoLinePool (AboutMenu, PoolPrint (
            L"Secure Boot: %s",
            secure_mode() ? L"active" : L"inactive"
        ));

        if (GetCsrStatus (&CsrStatus) == EFI_SUCCESS) {
            RecordgCsrStatus (CsrStatus, FALSE);
            AddMenuInfoLinePoolStr (AboutMenu, &gCsrStatus);
        }

        TempStr = egScreenDescription();
        AddMenuInfoLinePool (AboutMenu, PoolPrint(L"Screen Output: %s", TempStr));
        MyFreePool (&TempStr);

        AddMenuInfoLineCached (AboutMenu, L"");
        AddMenuInfoLineCached (AboutMenu, L"RefindPlus is a variant of rEFInd");
        AddMenuInfoLineCached (AboutMenu, L"https://github.com/dakanji/RefindPlus");
        AddMenuInfoLineCached (AboutMenu, L"");
        AddMenuInfoLineCached (AboutMenu, L"For information on rEFInd, visit:");
        AddMenuInfoLineCached (AboutMenu, L"http://www.rodsbooks.com/refind");
        AddMenuEntryCopy (AboutMenu, &TagMenuEntry[TAG_RETURN]);
        MyFreePool (&FirmwareVendor);
        
        LEAKABLEROOTMENU (kLeakableMenuAbout, AboutMenu);
    }

    RunMenu (AboutMenu, NULL);
} // VOID AboutRefindPlus()

// Record the loader's name/description in the "PreviousBoot" EFI variable
// if different from what is already stored there.
VOID StoreLoaderName (
    IN CHAR16 *Name
) {
    // Do not set if configured not to
    if (GlobalConfig.IgnorePreviousBoot) {
        return;
    }

    if (Name) {
        EfivarSetRaw (
            &RefindPlusGuid,
            L"PreviousBoot",
            Name,
            StrSize (Name),
            TRUE
        );
    } // if
} // VOID StoreLoaderName()

VOID ZeroPoolStr_PS_ (
    PoolStr *object
) {
    if (object) {
        object->Str = NULL;
        object->Cached = FALSE;
    }
}

VOID
AssignPoolStr_PS_ (
    PoolStr *object,
    CHAR16 *str
)
{
    if (object) {
        if (str != object->Str) {
            if (!object->Cached) {
                MyFreePool (&object->Str);
            }
            LOGPOOL (str);
            object->Str = str;
            object->Cached = FALSE;
        }
    }
}

VOID
AssignCachedPoolStr_PS_ (
    PoolStr *object,
    CHAR16 *str
)
{
    if (object) {
        if (str != object->Str) {
            if (!object->Cached) {
                MyFreePool (&object->Str);
            }
            object->Str = str;
            object->Cached = TRUE;
        }
    }
}

VOID
CopyPoolStr_PS_ (
    PoolStr *Dst, CHAR16 *Src
) {
    AssignPoolStr_PS_ (Dst, StrDuplicate (Src));
}

VOID
CopyFromPoolStr_PS_ (
    PoolStr *Dst,
    PoolStr *Src
) {
    if (Src && Src->Cached) {
        AssignCachedPoolStr_PS_ (Dst, Src->Str);
    }
    else {
        CopyPoolStr_PS_ (Dst, GetPoolStr_PS_ (Src));
    }
}

CHAR16 *
GetPoolStr_PS_ (
    PoolStr *object
) {
    if (object) {
        if (!object->Cached) {
            LOGPOOL (object->Str);
        }
        return object->Str;
    }
    return NULL;
}

VOID
FreePoolStr_PS_ (
    PoolStr *object
) {
    if (object && object->Str) {
        if (!object->Cached) {
            MyFreePool (&object->Str);
        }
    }
}

VOID ZeroPoolImage_PI_ (
    PoolImage *object
) {
    if (object) {
        object->Image = NULL;
        object->Cached = FALSE;
    }
}

VOID
AssignPoolImage_PI_ (
    PoolImage *object,
    EG_IMAGE *image
)
{
    if (object) {
        if (image != object->Image) {
            if (!object->Cached) {
                egFreeImage (object->Image);
            }
            LOGPOOL (image);
            object->Image = image;
            object->Cached = FALSE;
        }
    }
}

VOID
AssignCachedPoolImage_PI_ (
    PoolImage *object,
    EG_IMAGE *image
)
{
    if (object) {
        if (image != object->Image) {
            if (!object->Cached) {
                egFreeImage (object->Image);
            }
            object->Image = image;
            object->Cached = TRUE;
        }
    }
}

VOID
CopyPoolImage_PI_ (
    PoolImage *Dst, EG_IMAGE *Src
) {
    AssignPoolImage_PI_ (Dst, egCopyImage (Src));
}

VOID
CopyFromPoolImage_PI_ (
    PoolImage *Dst,
    PoolImage *Src
) {
    if (Src && Src->Cached) {
        AssignCachedPoolImage_PI_ (Dst, Src->Image);
    }
    else {
        CopyPoolImage_PI_ (Dst, GetPoolImage_PI_ (Src));
    }
}

EG_IMAGE *
GetPoolImage_PI_ (
    PoolImage *object
) {
    if (object) {
        if (!object->Cached) {
            LOGPOOL (object->Image);
        }
        return object->Image;
    }
    return NULL;
}

VOID
FreePoolImage_PI_ (
    PoolImage *object
) {
    if (object && object->Image) {
        if (!object->Cached) {
            egFreeImage (object->Image);
        }
    }
}

// Rescan for boot loaders
VOID RescanAll (
    BOOLEAN DisplayMessage,
    BOOLEAN Reconnect
) {
    MsgLog ("[ RescanAll\n");
    LOG2(1, LOG_LINE_NORMAL, L"INFO: ", L"\n\n", L"Re-scanning all boot loaders");

    FreeMenuScreen (&MainMenu);
    MainMenu = CopyMenuScreen (&MainMenuSrc);
    MainMenu->TimeoutSeconds = GlobalConfig.Timeout;

    // ConnectAllDriversToAllControllers() can cause system hangs with some
    // buggy filesystem drivers, so do it only if necessary....
    if (Reconnect) {
        ConnectAllDriversToAllControllers(FALSE);
        ScanVolumes();
    }

    ReadConfig (GlobalConfig.ConfigFilename);
    SetVolumeIcons();
    ScanForBootloaders (DisplayMessage);
    ScanForTools();
    LEAKABLEVOLUMES();
    LEAKABLEPARTITIONS();
    LEAKABLEROOTMENU (kLeakableMenuMain, MainMenu);
    MsgLog ("] RescanAll\n");
} // VOID RescanAll()

#ifdef __MAKEWITH_TIANO

// Minimal initialisation function
static VOID InitializeLib (
    IN EFI_HANDLE        ImageHandle,
    IN EFI_SYSTEM_TABLE  *SystemTable
) {
    gImageHandle  = ImageHandle;
    gST           = SystemTable;
    gBS           = SystemTable->BootServices;
    gRT           = SystemTable->RuntimeServices;

    EfiGetSystemConfigurationTable (
        &gEfiDxeServicesTableGuid,
        (VOID **) &gDS
    );

    // Upgrade EFI_BOOT_SERVICES.HandleProtocol
    OrigHandleProtocol   = gBS->HandleProtocol;
    gBS->HandleProtocol  = HandleProtocolEx;
    gBS->Hdr.CRC32       = 0;
    gBS->CalculateCrc32 (gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);
} // VOID InitializeLib()

#endif

// Set up our own Secure Boot extensions....
// Returns TRUE on success, FALSE otherwise
static
BOOLEAN SecureBootSetup (
    VOID
) {
    EFI_STATUS  Status;
    BOOLEAN     Success         = FALSE;
    CHAR16     *MsgStr  = NULL;

    LOG(1, LOG_LINE_NORMAL, L"Setting up Secure Boot (if applicable)");

    if (secure_mode() && ShimLoaded()) {
        LOG(2, LOG_LINE_NORMAL, L"Secure boot mode detected with loaded Shim; adding MOK extensions");

        Status = security_policy_install();
        if (Status == EFI_SUCCESS) {
            Success = TRUE;
        }
        else {
            MsgStr = L"Secure boot disabled ... doing nothing";

            LOG2(2, LOG_LINE_NORMAL, L"** WARN: ", L"\n-----------------\n\n", L"%s", MsgStr);

            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText (MsgStr, NEXTLINE);
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

            PauseForKey();
        }
    }

    return Success;
} // VOID SecureBootSetup()

// Remove our own Secure Boot extensions.
// Returns TRUE on success, FALSE otherwise
static
BOOLEAN SecureBootUninstall (VOID) {
    EFI_STATUS  Status;
    BOOLEAN     Success = TRUE;
    CHAR16     *MsgStr;

    if (secure_mode()) {
        Status = security_policy_uninstall();
        if (Status != EFI_SUCCESS) {
            Success = FALSE;
            BeginTextScreen (L"Secure Boot Policy Failure");

            MsgStr = L"Failed to Uninstall MOK Secure Boot Extensions ... Forcing Shutdown in 9 Seconds";

            MsgLog ("%s\n-----------------\n\n", MsgStr);

            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText (MsgStr, NEXTLINE);
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

            PauseSeconds(9);

            refit_call4_wrapper(
                gRT->ResetSystem,
                EfiResetShutdown,
                EFI_SUCCESS,
                0,
                NULL
            );
        }
    }

    return Success;
} // BOOLEAN SecureBootUninstall()

// Sets the global configuration filename; will be CONFIG_FILE_NAME unless the
// "-c" command-line option is set, in which case that takes precedence.
// If an error is encountered, leaves the value alone (it should be set to
// CONFIG_FILE_NAME when GlobalConfig is initialized).
static
VOID SetConfigFilename (EFI_HANDLE ImageHandle) {
    EFI_STATUS        Status;
    CHAR16            *Options;
    CHAR16            *FileName;
    CHAR16            *SubString;
    CHAR16            *MsgStr;
    EFI_LOADED_IMAGE  *Info;

    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        ImageHandle,
        &LoadedImageProtocol,
        (VOID **) &Info
    );
    if ((Status == EFI_SUCCESS) && (Info->LoadOptionsSize > 0)) {
        MsgLog ("Set Config Filename from Command Line Option:\n");

        Options = (CHAR16 *) Info->LoadOptions;
        SubString = MyStrStr (Options, L" -c ");
        if (SubString) {
            FileName = StrDuplicate (&SubString[4]);
            if (FileName) {
                LimitStringLength (FileName, 256);
            }

            if (FileExists (SelfDir, FileName)) {
                GlobalConfig.ConfigFilename = FileName;

                MsgLog ("  - Config File:- '%s'\n\n", FileName);
            }
            else {
                MsgStr = L"Specified Config File Not Found";
                MsgLog ("** WARN: %s\n", MsgStr);
                PrintUglyText (MsgStr, NEXTLINE);

                MsgStr = L"Try Default:- 'config.conf / refind.conf'";
                MsgLog ("         %s\n\n", MsgStr);
                PrintUglyText (MsgStr, NEXTLINE);

                HaltForKey();
            } // if/else

            MyFreePool (&FileName);
        } // if
        else {
            MsgStr = L"Invalid Load Option";

            MsgLog ("** WARN: %s\n", MsgStr);
            PrintUglyText (MsgStr, NEXTLINE);

            HaltForKey();
        }
    } // if
} // VOID SetConfigFilename()

// Adjust the GlobalConfig.DefaultSelection variable: Replace all "+" elements with the
//  PreviousBoot variable, if it's available. If it's not available, delete that element.
static
VOID AdjustDefaultSelection() {
    EFI_STATUS  Status;
    UINTN i = 0;
    CHAR16     *Element           = NULL;
    CHAR16     *NewCommaDelimited = NULL;
    CHAR16     *PreviousBoot      = NULL;

    MsgLog ("Adjust Default Selection...\n\n");

    while ((Element = FindCommaDelimited (GlobalConfig.DefaultSelection, i++)) != NULL) {
        if (MyStriCmp (Element, L"+")) {
            MyFreePool (&Element);

            Status = EfivarGetRaw (
                &RefindPlusGuid,
                L"PreviousBoot",
                (VOID **) &PreviousBoot,
                NULL
            );
            if (Status == EFI_SUCCESS) {
                Element = PreviousBoot;
            }
        }

        if (Element && StrLen (Element)) {
            MergeStrings (&NewCommaDelimited, Element, L',');
        }

        MyFreePool (&Element);
    } // while
    MyFreePool (&GlobalConfig.DefaultSelection);
    GlobalConfig.DefaultSelection = NewCommaDelimited;
    LEAKABLE (GlobalConfig.DefaultSelection, "DefaultSelection");
} // AdjustDefaultSelection()

#if REFIT_DEBUG > 0
STATIC
VOID
LogRevisionInfo (
    EFI_TABLE_HEADER *Hdr,
    CHAR16 *Name,
    UINT16 ExpectedSize,
    BOOLEAN DoEFICheck
) {
    MsgLog (
        "%s:- '%-4s %d.%02d'",
        Name,
        DoEFICheck ? ((Hdr->Revision >> 16 == 1) ? L"EFI" : L"UEFI") : L"",
        Hdr->Revision >> 16,
        Hdr->Revision & 0xffff
    );
    if (Hdr->HeaderSize == ExpectedSize) {
        MsgLog (" (size:%d)\n", Hdr->HeaderSize);
    }
    else {
        MsgLog (" (size:%d  expected:%d)\n", Hdr->HeaderSize, ExpectedSize);
    }
}


EFI_GUID gAppleMysteryGuid = { 0x5751DA6E, 0x1376, 0x4E02, { 0xBA, 0x92, 0xD2, 0x94, 0xFD, 0xD3, 0x09, 0x01 } };

STATIC
VOID
LogTableInfo (
    EFI_CONFIGURATION_TABLE *t
) {
    CHAR16 *guidName = L"";
         if (CompareGuid (&gEfiAcpi10TableGuid              , &t->VendorGuid)) guidName = L"ACPI 1.0 Table"           ;
    else if (CompareGuid (&gEfiAcpi20TableGuid              , &t->VendorGuid)) guidName = L"ACPI 2.0 Table"           ;
    else if (CompareGuid (&gEfiSalSystemTableGuid           , &t->VendorGuid)) guidName = L"SAL System Table"         ;
    else if (CompareGuid (&gEfiSmbiosTableGuid              , &t->VendorGuid)) guidName = L"SMBIOS Table"             ;
    else if (CompareGuid (&gEfiSmbios3TableGuid             , &t->VendorGuid)) guidName = L"SMBIOS 3 Table"           ;
    else if (CompareGuid (&gEfiMpsTableGuid                 , &t->VendorGuid)) guidName = L"MPS Table"                ;
    else if (CompareGuid (&gEfiDxeServicesTableGuid         , &t->VendorGuid)) guidName = L"DXE Services Table"       ;
    else if (CompareGuid (&gLzmaCustomDecompressGuid        , &t->VendorGuid)) guidName = L"LZMA Compress"            ;
    else if (CompareGuid (&gEfiHobListGuid                  , &t->VendorGuid)) guidName = L"HOB List"                 ;
    else if (CompareGuid (&gEfiMemoryTypeInformationGuid    , &t->VendorGuid)) guidName = L"Memory Type Information"  ;
    else if (CompareGuid (&gEfiMemoryAttributesTableGuid    , &t->VendorGuid)) guidName = L"Memory Attributes Table"  ;
    else if (CompareGuid (&gEfiHiiDatabaseProtocolGuid      , &t->VendorGuid)) guidName = L"Hii Database"             ;
    else if (CompareGuid (&gEfiHiiConfigRoutingProtocolGuid , &t->VendorGuid)) guidName = L"Hii Config Routing"       ;
    else if (CompareGuid (&gEfiImageSecurityDatabaseGuid    , &t->VendorGuid)) guidName = L"Image Security Database"  ;
    else if (CompareGuid (&gEfiDebugImageInfoTableGuid      , &t->VendorGuid)) guidName = L"Debug Image Info Table"   ;
    else if (CompareGuid (&gAppleDiagVaultProtocolGuid      , &t->VendorGuid)) guidName = L"Apple Diagnostic Vault"   ;
    else if (CompareGuid (&gAppleMysteryGuid                , &t->VendorGuid)) guidName = L"Apple ???"                ;
    MsgLog ("%g :- %s\n", t->VendorGuid, guidName);
}

// Log basic information (RefindPlus version, EFI version, etc.) to the log file.
static
VOID LogBasicInfo (
    VOID
) {
    EFI_STATUS Status;
    CHAR16     *TempStr;
    UINT64     MaximumVariableSize;
    UINT64     MaximumVariableStorageSize;
    UINT64     RemainingVariableStorageSize;
    UINTN      HandleCount                        = 0;
    EFI_GUID   ConsoleControlProtocolGuid         = EFI_CONSOLE_CONTROL_PROTOCOL_GUID;
    EFI_GUID   AppleFramebufferInfoProtocolGuid   = APPLE_FRAMEBUFFER_INFO_PROTOCOL_GUID;
    EFI_HANDLE *HandleBuffer                      = NULL;
    APPLE_FRAMEBUFFER_INFO_PROTOCOL  *FramebufferInfo;

    MsgLog ("System Summary...\n");
    LogRevisionInfo (&gST->Hdr, L"    System Table", sizeof(*gST), TRUE);
    LogRevisionInfo (&gBS->Hdr, L"   Boot Services", sizeof(*gBS), TRUE);
    LogRevisionInfo (&gRT->Hdr, L"Runtime Services", sizeof(*gRT), TRUE);
    LogRevisionInfo (&gDS->Hdr, L"    DXE Services", sizeof(*gDS), FALSE);

    UINTN Index;
    for (Index = 0; Index < gST->NumberOfTableEntries; Index++) {
        LogTableInfo (&gST->ConfigurationTable[Index]);
    }

    VOID *TestPool = PoolPrint(L"");
    switch (PoolVersion(TestPool)) {
        case  1: TempStr = L"EFI"    ; break;
        case  2: TempStr = L"UEFI"   ; break;
        default: TempStr = L"Unknown";
    }
    MyFreePool (&TestPool);
    MsgLog ("Pool type:- '%s'\n", TempStr);
    
    MsgLog ("Architecture:- ");
    #if defined(EFI32)
        MsgLog ("'x86 (32 bit)'");
    #elif defined(EFIX64)
        MsgLog ("'x86 (64 bit)'");
    #elif defined(EFIAARCH64)
        MsgLog ("'ARM (64 bit)'");
    #else
        MsgLog ("'Unknown'");
    #endif
    MsgLog ("\n");

    switch (GlobalConfig.LegacyType) {
        case LEGACY_TYPE_MAC:
            TempStr = L"Mac";
            break;
        case LEGACY_TYPE_UEFI:
            TempStr = L"UEFI";
            break;
        case LEGACY_TYPE_NONE:
            TempStr = L"Unavailable";
            break;
        default:
            // just in case ... should never happen
            TempStr = L"Unknown";
            break;
    }
    MsgLog ("CSM:- '%s'\n", TempStr);

    MsgLog ("Shim:- '%s'\n", ShimLoaded()         ? L"Present" : L"Absent");
    MsgLog ("Secure Boot:- '%s'\n", secure_mode() ? L"Active"  : L"Inactive");

    if (MyStrStr (VendorInfo, L"Apple") != NULL) {
        Status = LibLocateProtocol (&AppleFramebufferInfoProtocolGuid, (VOID *) &FramebufferInfo);
        if (EFI_ERROR (Status)) {
            HandleCount = 0;
        }
        else {
            Status = gBS->LocateHandleBuffer (
                ByProtocol,
                &AppleFramebufferInfoProtocolGuid,
                NULL,
                &HandleCount,
                &HandleBuffer
            );
            if (EFI_ERROR (Status)) {
                HandleCount = 0;
            }

        }
        MsgLog ("Apple Framebuffers:- '%d'\n", HandleCount);
        MyFreePool (&HandleBuffer);
    }

    if (
        (gRT->Hdr.Revision >> 16 > 1) &&
        (gRT->Hdr.HeaderSize >= MY_OFFSET_OF(EFI_RUNTIME_SERVICES, QueryVariableInfo) + sizeof(gRT->QueryVariableInfo))
    ) {
        // NB: QueryVariableInfo() is not supported by EFI 1.x
        MsgLog ("EFI Non-Volatile Storage Info:\n");

        Status = refit_call4_wrapper(
            gRT->QueryVariableInfo,
            EFI_VARIABLE_NON_VOLATILE,
            &MaximumVariableStorageSize,
            &RemainingVariableStorageSize,
            &MaximumVariableSize
        );
        if (EFI_ERROR(Status)) {
            MsgLog ("** WARN: Could not Retrieve Info!!\n");
        }
        else {
            MsgLog ("  - Total Storage         : %ld\n", MaximumVariableStorageSize);
            MsgLog ("  - Remaining Available   : %ld\n", RemainingVariableStorageSize);
            MsgLog ("  - Maximum Variable Size : %ld\n", MaximumVariableSize);
        }
    }

    // Report which video output devices are natively available. We do not actually
    // use them, so just use TempStr as a throwaway pointer to the protocol.
    MsgLog ("Screen Modes:\n");

    Status = LibLocateProtocol (&ConsoleControlProtocolGuid, (VOID **) &TempStr);
    MsgLog ("  - Native Text Mode           : %s", EFI_ERROR (Status) ? L" NO" : L"YES");
    MsgLog ("\n");

    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        gST->ConsoleOutHandle,
        &gEfiUgaDrawProtocolGuid,
        (VOID **) &TempStr
    );
    MsgLog ("  - Native Graphics Mode (UGA) : %s", EFI_ERROR (Status) ? L" NO" : L"YES");
    MsgLog ("\n");

    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        gST->ConsoleOutHandle,
        &gEfiGraphicsOutputProtocolGuid,
        (VOID **) &TempStr
    );
    MsgLog ("  - Native Graphics Mode (GOP) : %s", EFI_ERROR (Status) ? L" NO" : L"YES");
    MsgLog ("\n\n");
} // VOID LogBasicInfo()
#endif

#define MyPrint(x, ...) AsciiPrint(x, ##__VA_ARGS__)
//#define MyPrint(x) DebugPrint(DEBUG_INFO, x)

UINTN
EFIAPI
MyPrintMemLogCallback (
    IN INTN DebugMode,
    IN CHAR8 *LastMessage
) {
  LEAKABLEEXTERNALSTART ("MyPrintMemLogCallback AsciiPrint");
  UINTN BytesWritten = AsciiPrint ("%a", LastMessage);
  LEAKABLEEXTERNALSTOP ();
  return BytesWritten;
}

//
// main entry point
//
EFI_STATUS EFIAPI efi_main (
    EFI_HANDLE        ImageHandle,
    EFI_SYSTEM_TABLE  *SystemTable
) {
    EFI_STATUS  Status;

    BOOLEAN  MainLoopRunning = TRUE;
    BOOLEAN  MokProtocol     = FALSE;

    REFIT_MENU_ENTRY  *ChosenEntry    = NULL;
    LOADER_ENTRY      *ourLoaderEntry = NULL;
    LEGACY_ENTRY      *ourLegacyEntry = NULL;

    UINTN  i        = 0;
    UINTN  MenuExit = 0;

    EG_PIXEL  BGColor        = COLOR_LIGHTBLUE;
    CHAR16    *MsgStr;
    CHAR16    *SelectionName = NULL;

    // Force Native Logging
    ForceNativeLoggging = TRUE;

    // bootstrap
    InitializeLib (ImageHandle, SystemTable);

    // disable EFI watchdog timer
    refit_call4_wrapper(
        gBS->SetWatchdogTimer,
        0x0000, 0x0000, 0x0000,
        NULL
    );

    #if 0
    // Pause startup so we can start the debugger here.
    DebugLoop ();
    #endif

    //MyPrint("efi_main\n");
    #if 0
    SetMemLogCallback (MyPrintMemLogCallback);
    #endif

    #if REFIT_DEBUG > 0
    //MyPrint("ReMapPoolFunctions\n");
    ReMapPoolFunctions ();
    AdjustStackMax ();
    #endif

    //MyPrint("InitRefitLib\n");
    Status = InitRefitLib (ImageHandle);

    if (EFI_ERROR (Status)) {
        return Status;
    }

    //MyPrint("GetTime\n");
    EFI_TIME Now;
    gRT->GetTime (&Now, NULL);
    NowYear   = Now.Year;
    NowMonth  = Now.Month;
    NowDay    = Now.Day;
    NowHour   = Now.Hour;
    NowMinute = Now.Minute;
    NowSecond = Now.Second;

    //MyPrint("VendorInfo\n");
    if (MyStrStr (gST->FirmwareVendor, L"Apple") != NULL) {
        VendorInfo = StrDuplicate (L"Apple");
    }
    else {
        VendorInfo = PoolPrint (
            L"%s %d.%02d",
            gST->FirmwareVendor,
            gST->FirmwareRevision >> 16,
            gST->FirmwareRevision & 0xffff
        );
    }
    LEAKABLE (VendorInfo, "VendorInfo");

    //MyPrint("InitBooterLog\n");
    #if REFIT_DEBUG > 0
    InitBooterLog();

    //MyPrint("ConstDateStr\n");
    CONST CHAR16 *ConstDateStr = PoolPrint (
        L"%d-%02d-%02d %02d:%02d:%02d",
        NowYear, NowMonth,
        NowDay, NowHour,
        NowMinute, NowSecond
    );

    //MyPrint("MsgLog Loading RefindPlus\n");
    MsgLog (
        "Loading RefindPlus v%s on %s Firmware\n",
        REFINDPLUS_VERSION, VendorInfo
    );
    
    MsgLog (" Compiled:- '%a %a'\n", __DATE__, __TIME__);

#if defined(__MAKEWITH_GNUEFI)
    MsgLog ("Made With:- 'GNU-EFI'\n");
#else
    MsgLog ("Made With:- 'TianoCore EDK II'\n");
#endif
    MsgLog ("Timestamp:- '%s (GMT)'\n\n", ConstDateStr);
    MyFreePool (&ConstDateStr);
    
    // Log System Details
    LogBasicInfo ();
    #endif

    //MyPrint("DumpCallStack\n");
    #if 0
    DumpCallStack (NULL, TRUE);
    #endif

    // read configuration
    CopyMem (GlobalConfig.ScanFor, "ieom       ", NUM_SCAN_OPTIONS);
    FindLegacyBootType();
    if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC) {
        CopyMem (GlobalConfig.ScanFor, "ihebocm    ", NUM_SCAN_OPTIONS);
    }
    SetConfigFilename (ImageHandle);

    // Set Secure Boot Up
    MokProtocol = SecureBootSetup();

    // Scan volumes first to find SelfVolume, which is required by LoadDrivers() and ReadConfig();
    // however, if drivers are loaded, a second call to ScanVolumes() is needed
    // to register the new filesystem (s) accessed by the drivers.
    // Also, ScanVolumes() must be done before ReadConfig(), which needs
    // SelfVolume->VolName.
    ScanVolumes();

    // Read Config first to get tokens that may be required by LoadDrivers();
    if (!FileExists (SelfDir, GlobalConfig.ConfigFilename)) {
        ConfigWarn = TRUE;

        MsgLog ("** WARN: Could Not Find RefindPlus Configuration File:- 'config.conf'\n"
                "         Trying rEFInd's Configuration File:- 'refind.conf'\n"
                "         Provide 'config.conf' file to silence this warning\n"
                "         You can rename 'refind.conf' file as 'config.conf'\n"
                "         NB: Will not contain all RefindPlus config tokens\n\n");

        GlobalConfig.ConfigFilename = L"refind.conf";
    }
    ReadConfig (GlobalConfig.ConfigFilename);
    AdjustDefaultSelection();

    #if REFIT_DEBUG > 0
    MsgLog ("INFO: Log Level:- '%d'", GlobalConfig.LogLevel);

    // Show REFIT_DEBUG Setting
    MsgLog ("\n");
    MsgLog ("      Debug Level:- '%d'", REFIT_DEBUG);

    // Show ScanDelay Setting
    MsgLog ("\n");
    MsgLog ("      Scan Delay:- '%d'", GlobalConfig.ScanDelay);

    // Show ReloadGOP Status
    MsgLog ("\n");
    MsgLog ("      ReloadGOP:- ");
    if (GlobalConfig.ReloadGOP) {
        MsgLog ("'YES'");
    }
    else {
        MsgLog ("'NO'");
    }

    // Show SyncAPFS Status
    MsgLog ("\n");
    MsgLog ("      SyncAPFS:- ");
    if (GlobalConfig.SyncAPFS) {
        MsgLog ("'Active'");
    }
    else {
        MsgLog ("'Inactive'");
    }


    // Show TextOnly Status
    MsgLog ("\n");
    MsgLog ("      TextOnly:- ");
    if (GlobalConfig.TextOnly) {
        MsgLog ("'Active'");
    }
    else {
        MsgLog ("'Inactive'");
    }

    // Show ProtectNVRAM Status
    MsgLog ("\n");
    if (MyStrStr (VendorInfo, L"Apple") == NULL) {
        MsgLog ("      ProtectNVRAM:- 'Disabled'");
    }
    else {
    MsgLog ("      ProtectNVRAM:- ");
    if (GlobalConfig.ProtectNVRAM) {
        MsgLog ("'Active'");
    }
    else {
        MsgLog ("'Inactive'");
    }
    }

    // Show ScanOtherESP Status
    MsgLog ("\n");
    MsgLog ("      ScanOtherESP:- ");
    if (GlobalConfig.ScanOtherESP) {
        MsgLog ("'Active'");
    }
    else {
        MsgLog ("'Inactive'");
    }

    // Show IgnorePreviousBoot Status
    MsgLog ("\n");
    MsgLog ("      IgnorePreviousBoot:- ");
    if (GlobalConfig.IgnorePreviousBoot) {
        MsgLog ("'Active'");
    }
    else {
        MsgLog ("'Inactive'");
    }

    #endif

    #ifdef __MAKEWITH_TIANO
    // DA-TAG: Limit to TianoCore
    if (GlobalConfig.SupplyAPFS) {
        Status = RpApfsConnectDevices();

        #if REFIT_DEBUG > 0
        MsgLog ("\n\n");
        MsgLog ("INFO: Supply APFS ... %r", Status);
        #endif
    }
    #endif

    #if REFIT_DEBUG > 0
    // Clear Lines
    if (GlobalConfig.LogLevel > 0) {
        MsgLog ("\n");
        }
        else {
        MsgLog ("\n\n");
    }
    #endif

    // Disable Forced Native Logging
    //ForceNativeLoggging = FALSE;

    LoadDrivers();

    MsgLog ("Scan Volumes...\n");
    ScanVolumes();

    if (GlobalConfig.SpoofOSXVersion && GlobalConfig.SpoofOSXVersion[0] != L'\0') {
        Status = SetAppleOSInfo();

        MsgLog ("INFO: Spoof Mac OS Version ...%r\n\n", Status);
    }

    // Restore SystemTable if previously amended
    if (SetSysTab) {
        // Reinitialise
        InitializeLib (ImageHandle, SystemTable);

        LOG2(1, LOG_STAR_SEPARATOR, L"INFO: ", L"\n\n", L"Restore System Table ...%r", EFI_SUCCESS);
    }

    LOG2(1, LOG_LINE_SEPARATOR, L"", L"...\n", L"Initialise Screen");

    InitScreen();

    WarnIfLegacyProblems();

    FreeMenuScreen (&MainMenu);
    MainMenu = CopyMenuScreen (&MainMenuSrc);
    MainMenu->TimeoutSeconds = GlobalConfig.Timeout;

    // further bootstrap (now with config available)
    SetupScreen();
    SetVolumeIcons();
    ScanForBootloaders (FALSE);
    ScanForTools();
    LEAKABLEVOLUMES();
    LEAKABLEPARTITIONS();
    LEAKABLEROOTMENU (kLeakableMenuMain, MainMenu);

    // SetupScreen() clears the screen; but ScanForBootloaders() may display a
    // message that must be deleted, so do so
    BltClearScreen (TRUE);
    pdInitialize();

    if (GlobalConfig.ScanDelay > 0) {
       if (GlobalConfig.ScanDelay > 1) {
           LOG(1, LOG_LINE_NORMAL, L"Pausing before re-scan");

            egDisplayMessage (
                L"Pausing before disc scan. Please wait....",
                &BGColor, CENTER
            );
       }

       MsgLog ("Pause for Scan Delay:\n");

       for (i = -1; i < GlobalConfig.ScanDelay; ++i) {
            refit_call1_wrapper(gBS->Stall, 1000000);
       }
       if (i == 1) {
           MsgLog ("  - Waited %d Second\n", i);
       }
       else {
           MsgLog ("  - Waited %d Seconds\n", i);
       }
       RescanAll (GlobalConfig.ScanDelay > 1, TRUE);
       BltClearScreen (TRUE);
    } // if

    if (GlobalConfig.DefaultSelection) {
        SelectionName = StrDuplicate (GlobalConfig.DefaultSelection);
        LEAKABLE(SelectionName, "efi_main SelectionName DefaultSelection");
    }
    if (GlobalConfig.ShutdownAfterTimeout) {
        AssignCachedPoolStr (&MainMenu->TimeoutText, L"Shutdown");
    }

    // show config mismatch warning
    if (ConfigWarn) {
        MsgLog ("INFO: Displaying User Warning\n\n");

        SwitchToText (FALSE);

        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
        if (ConfigWarn) {
            PrintUglyText (L"                                                                          ", NEXTLINE);
            PrintUglyText (L" WARN: Could Not Find RefindPlus Configuration File                       ", NEXTLINE);
            PrintUglyText (L"                                                                          ", NEXTLINE);
            PrintUglyText (L"       Trying rEFInd's Configuration File:- 'refind.conf'                 ", NEXTLINE);
            PrintUglyText (L"       Provide 'config.conf' file to silence this warning                 ", NEXTLINE);
            PrintUglyText (L"       You can rename 'refind.conf' file as 'config.conf'                 ", NEXTLINE);
            PrintUglyText (L"       NB: Will not contain all RefindPlus config tokens                  ", NEXTLINE);
            PrintUglyText (L"                                                                          ", NEXTLINE);
        }
        refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

        PauseSeconds(6);

        #if REFIT_DEBUG > 0
        MsgLog ("INFO: User Warning");

        if (GlobalConfig.ContinueOnWarning) {
            MsgLog (" Acknowledged or Timed Out ...");
        }
        else {
            MsgLog (" Acknowledged ...");
        }
        #endif

        if (egIsGraphicsModeEnabled()) {
            MsgLog ("Restore Graphics Mode\n\n");

            SwitchToGraphicsAndClear (TRUE);
        }
        else {
            MsgLog ("Proceeding\n\n");
        }
    }

    // Set CSR if required
    ActiveCSR();

    LOG2(1, LOG_STAR_SEPARATOR, L"INFO: ", L"\n\n", L"Loaded RefindPlus v%s on %s Firmware", REFINDPLUS_VERSION, VendorInfo);

    MsgLog ("[ Main Loop\n");

    while (MainLoopRunning) {
        // Set to false as may not be booting
        MsgLog ("IsBoot = FALSE\n");
        IsBoot = FALSE;

        MenuExit = RunMainMenu (MainMenu, &SelectionName, &ChosenEntry);

        // The Escape key triggers a re-scan operation....
        if (MenuExit == MENU_EXIT_ESCAPE) {
            MsgLog ("User Input Received:\n");
            MsgLog ("  - Escape Key Pressed ...Rescan All\n\n");

            RescanAll (TRUE, TRUE);
            continue;
        }
        
        if (!ChosenEntry) {
            MsgLog ("No chosen entry %a:%d\n", __FILE__, __LINE__);
            continue;
        }

        ChosenEntry = CopyMenuEntryShallow (ChosenEntry);

        if (!ChosenEntry) {
            MsgLog ("No chosen entry copy %a:%d\n", __FILE__, __LINE__);
            continue;
        }
        
        if ((MenuExit == MENU_EXIT_TIMEOUT) && GlobalConfig.ShutdownAfterTimeout) {
            ChosenEntry->Tag = TAG_SHUTDOWN;
        }
        
        CHAR8 *TheTagName = "NULL";
        switch (ChosenEntry->Tag) {
            #define TAGS_TAG_NAME
            #include "tags.include"
        }
        
        MsgLog ("[ %a\n", TheTagName);
        
        switch (ChosenEntry->Tag) {

            case TAG_NVRAMCLEAN:    // Clean NVRAM
                MsgLog ("User Input Received:\n");
                LOG2(1, LOG_LINE_NORMAL, L"  - ", egIsGraphicsModeEnabled() ? L"\n---------------\n\n" : L"\n\n", L"Cleaning NVRAM");
                StartTool ((LOADER_ENTRY *) ChosenEntry);
                break;

            case TAG_PRE_NVRAMCLEAN:    // Clean NVRAM Info
                MsgLog ("User Input Received:\n");
                LOG2(1, LOG_LINE_THIN_SEP, L"  - ", L"\n\n", L"Showing Clean NVRAM Info");
                preCleanNvram();

                // Reboot if CleanNvram was triggered
                if (ranCleanNvram) {
                    LOG2(1, LOG_LINE_NORMAL, L"INFO: ", L"\n\n", L"Cleaned NVRAM");
                    MsgLog ("Terminating Screen:\n");
                    MsgLog ("System Restart...\n");

                    TerminateScreen();

                    LOG2(1, LOG_LINE_NORMAL, L"", L"\n---------------\n\n", L"System Restart");

                    refit_call4_wrapper(
                        gRT->ResetSystem,
                        EfiResetCold,
                        EFI_SUCCESS,
                        0, NULL
                    );

                    MsgStr = L"System Restart FAILED!!";
                    PrintUglyText (MsgStr, NEXTLINE);
                    LOG2(1, LOG_THREE_STAR_SEP, L"INFO: ", L"\n\n", L"%s", MsgStr);

                    PauseForKey();

                    MainLoopRunning = FALSE;   // just in case we get this far
                }
                break;

            case TAG_SHOW_BOOTKICKER:    // Apple Boot Screen
                MsgLog ("User Input Received:\n");
                LOG2(1, LOG_LINE_NORMAL, L"  - ", egIsGraphicsModeEnabled() ? L"\n---------------\n\n" : L"\n\n", L"Loading Apple Boot Screen");
                ourLoaderEntry = (LOADER_ENTRY *) ChosenEntry;
                ourLoaderEntry->UseGraphicsMode = TRUE;
                StartTool (ourLoaderEntry);
                break;

            case TAG_PRE_BOOTKICKER:    // Apple Boot Screen Info
                MsgLog ("User Input Received:\n");
                LOG2(1, LOG_LINE_THIN_SEP, L"  - ", L"\n\n", L"Showing BootKicker Info");
                preBootKicker();
                break;

            case TAG_REBOOT:    // Reboot
                MsgLog ("User Input Received:\n");
                LOG2(1, LOG_LINE_SEPARATOR, L"  - ", egIsGraphicsModeEnabled() ? L"\n---------------\n\n" : L"\n\n", L"System Restart");

                TerminateScreen();

                LOG(1, LOG_LINE_SEPARATOR, L"Restarting System");

                refit_call4_wrapper(
                    gRT->ResetSystem,
                    EfiResetCold,
                    EFI_SUCCESS,
                    0, NULL
                );

                LOG(1, LOG_THREE_STAR_SEP, L"Restart FAILED!!");

                MainLoopRunning = FALSE;   // just in case we get this far
                break;

            case TAG_SHUTDOWN: // Shut Down
                MsgLog ("User Input Received:\n");
                LOG2(1, LOG_LINE_SEPARATOR, L"  - ", egIsGraphicsModeEnabled() ? L"\n---------------\n\n" : L"\n\n", L"Shutting System Down");

                TerminateScreen();

                refit_call4_wrapper(
                    gRT->ResetSystem,
                    EfiResetShutdown,
                    EFI_SUCCESS,
                    0, NULL
                );

                LOG(1, LOG_THREE_STAR_SEP, L"Shutdown FAILED!!");

                MainLoopRunning = FALSE;   // just in case we get this far
                break;

            case TAG_ABOUT:    // About RefindPlus
                MsgLog ("User Input Received:\n");
                MsgLog ("  - Show 'About RefindPlus' Page\n\n");

                AboutRefindPlus();

                MsgLog ("User Input Received:\n");
                MsgLog ("  - Exit 'About RefindPlus' Page\n\n");

                break;

            case TAG_LOADER:   // Boot OS via .EFI Loader
                ourLoaderEntry = (LOADER_ENTRY *) ChosenEntry;

                // Fix undetected Mac OS
                if (MyStrStr (GetPoolStr (&ourLoaderEntry->Title), L"Mac OS") == NULL &&
                    MyStrStr (GetPoolStr (&ourLoaderEntry->LoaderPath), L"System\\Library\\CoreServices") != NULL
                ) {
                    if (MyStriCmp (GetPoolStr (&ourLoaderEntry->Volume->VolName), L"PreBoot")) {
                        AssignCachedPoolStr (&ourLoaderEntry->Title, L"Mac OS");
                    }
                    else {
                        AssignCachedPoolStr (&ourLoaderEntry->Title, L"RefindPlus");
                    }
                }

                // Fix undetected Windows
                if (MyStrStr (GetPoolStr (&ourLoaderEntry->Title), L"Windows") == NULL &&
                    MyStrStr (GetPoolStr (&ourLoaderEntry->LoaderPath), L"EFI\\Microsoft\\Boot") != NULL
                ) {
                    AssignCachedPoolStr (&ourLoaderEntry->Title, L"Windows (UEFI)");
                }

                // Use multiple instaces of "User Input Received:"

                if (MyStrStr (GetPoolStr (&ourLoaderEntry->Title), L"OpenCore") != NULL) {
                    if (!ourLoaderEntry->UseGraphicsMode) {
                        ourLoaderEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OPENCORE;
                    }

                    MsgLog ("User Input Received:\n");
                    LOG2(1, LOG_LINE_THIN_SEP, L"  - ", L"",
                        L"Loading OpenCore Instance:- '%s%s'",
                        GetPoolStr (&ourLoaderEntry->Volume->VolName),
                        GetPoolStr (&ourLoaderEntry->LoaderPath)
                    );
                }
                else if (MyStrStr (GetPoolStr (&ourLoaderEntry->Title), L"Clover") != NULL) {
                    if (!ourLoaderEntry->UseGraphicsMode) {
                        ourLoaderEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_CLOVER;
                    }

                    MsgLog ("User Input Received:\n");
                    LOG2(1, LOG_LINE_THIN_SEP, L"  - ", L"",
                        L"Loading Clover Instance:- '%s%s'",
                        GetPoolStr (&ourLoaderEntry->Volume->VolName),
                        GetPoolStr (&ourLoaderEntry->LoaderPath)
                    );
                }
                else if (MyStrStr (GetPoolStr (&ourLoaderEntry->Title), L"Mac OS") != NULL) {
                    #if REFIT_DEBUG > 0
                    MsgLog ("User Input Received:\n");
                    if (GetPoolStr (&ourLoaderEntry->Volume->VolName)) {
                        LOG2(1, LOG_LINE_THIN_SEP, L"  - ", L"",
                            L"Booting Mac OS from '%s'",
                            GetPoolStr (&ourLoaderEntry->Volume->VolName)
                        );
                    }
                    else {
                        LOG2(1, LOG_LINE_THIN_SEP, L"  - ", L"",
                            L"Booting Mac OS:- '%s'",
                            GetPoolStr (&ourLoaderEntry->LoaderPath)
                        );
                    }
                    #endif

                    // Enable TRIM on non-Apple SSDs if configured to
                    if (GlobalConfig.ForceTRIM) {
                        ForceTRIM();
                    }

                    // Set Mac boot args if configured to
                    // Also disables AMFI if this is configured
                    // Also disables Mac OS compatibility check if this is configured
                    if (GlobalConfig.SetBootArgs && GlobalConfig.SetBootArgs[0] != L'\0') {
                        SetBootArgs();
                    }
                    else {
                        if (GlobalConfig.DisableAMFI) {
                            // Disable AMFI if configured to
                            // Also disables Mac OS compatibility check if this is configured
                            DisableAMFI();
                        }
                        else if (GlobalConfig.DisableCompatCheck) {
                            // Disable Mac OS compatibility check if configured to
                            DisableCompatCheck();
                        }
                    }

                    // Re-Map OpenProtocol
                    ReMapOpenProtocol();
                }
                else if (MyStrStr (GetPoolStr (&ourLoaderEntry->Title), L"Windows") != NULL) {
                    if (GlobalConfig.ProtectNVRAM &&
                        MyStrStr (VendorInfo, L"Apple") != NULL
                    ) {
                        // Protect Mac NVRAM from UEFI Windows
                        MapSetVariable (SystemTable);
                    }

                    #if REFIT_DEBUG > 0
                    CHAR16 *WinType;
                    MsgLog ("User Input Received:\n");
                    if (MyStrStr (GetPoolStr (&ourLoaderEntry->Title), L"UEFI") != NULL) {
                        WinType = L"UEFI";
                    }
                    else {
                        WinType = L"Legacy";
                    }
                    if (GetPoolStr (&ourLoaderEntry->Volume->VolName)) {
                        LOG2(1, LOG_LINE_THIN_SEP, L"  - ", L"",
                            L"Boot %s Windows from '%s'",
                            WinType, GetPoolStr (&ourLoaderEntry->Volume->VolName)
                        );
                    }
                    else {
                        LOG2(1, LOG_LINE_THIN_SEP, L"  - ", L"",
                            L"Boot %s Windows:- '%s'",
                            WinType, GetPoolStr (&ourLoaderEntry->LoaderPath)
                        );
                    }
                    #endif
                }
                else {
                    MsgLog ("User Input Received:\n");
                    LOG2(1, LOG_LINE_THIN_SEP, L"  - ", L"", 
                        L"Boot OS via EFI Loader:- '%s%s'",
                        GetPoolStr (&ourLoaderEntry->Volume->VolName),
                        GetPoolStr (&ourLoaderEntry->LoaderPath)
                    );
                }

                StartLoader (ourLoaderEntry, SelectionName);
                break;

            case TAG_LEGACY:   // Boot legacy OS
                ourLegacyEntry = (LEGACY_ENTRY *) ChosenEntry;

                MsgLog ("User Input Received:\n");

                if (MyStrStr (GetPoolStr (&ourLegacyEntry->Volume->OSName), L"Windows") != NULL) {
                    if (GlobalConfig.ProtectNVRAM &&
                        MyStrStr (VendorInfo, L"Apple") != NULL
                    ) {
                        // Protect Mac NVRAM from UEFI Windows
                        MapSetVariable (SystemTable);
                    }

                    LOG2(1, LOG_LINE_THIN_SEP, L"  - ", egIsGraphicsModeEnabled() ? L"\n---------------\n\n" : L"\n\n",
                        L"Boot %s from '%s'",
                        GetPoolStr (&ourLegacyEntry->Volume->OSName),
                        GetPoolStr (&ourLegacyEntry->Volume->VolName)
                    );
                }
                else {
                    LOG2(1, LOG_LINE_THIN_SEP, L"  - ", egIsGraphicsModeEnabled() ? L"\n---------------\n\n" : L"\n\n",
                        L"Boot Legacy OS:- '%s'",
                        GetPoolStr (&ourLegacyEntry->Volume->OSName)
                    );
                }

                StartLegacy (ourLegacyEntry, SelectionName);
                break;

            case TAG_LEGACY_UEFI: // Boot a legacy OS on a non-Mac
                ourLegacyEntry = (LEGACY_ENTRY *) ChosenEntry;

                MsgLog ("User Input Received:\n");
                LOG2(1, LOG_LINE_THIN_SEP, L"  - ", egIsGraphicsModeEnabled() ? L"\n---------------\n\n" : L"\n\n",
                    L"Boot Legacy UEFI:- '%s'",
                    GetPoolStr (&ourLegacyEntry->Volume->OSName)
                );

                StartLegacyUEFI (ourLegacyEntry, SelectionName);
                break;

            case TAG_TOOL:     // Start a EFI tool
                ourLoaderEntry = (LOADER_ENTRY *) ChosenEntry;

                MsgLog ("User Input Received:\n");
                LOG2(1, LOG_LINE_THIN_SEP, L"  - ", egIsGraphicsModeEnabled() ? L"\n---------------\n\n" : L"\n\n",
                    L"Start EFI Tool:- '%s'",
                    GetPoolStr (&ourLoaderEntry->LoaderPath)
                );

                if (MyStrStr (GetPoolStr (&ourLoaderEntry->Title), L"Boot Screen") != NULL) {
                    ourLoaderEntry->UseGraphicsMode = TRUE;
                }

                StartTool (ourLoaderEntry);
                break;

            case TAG_FIRMWARE_LOADER:  // Reboot to a loader defined in the EFI UseNVRAM
                MsgLog ("User Input Received:\n");
                MsgLog ("  - Reboot into Loader\n-----------------\n\n");

                RebootIntoLoader ((LOADER_ENTRY *) ChosenEntry);
                break;

            case TAG_HIDDEN:  // Manage hidden tag entries
                MsgLog ("User Input Received:\n");
                MsgLog ("  - Manage Hidden Tag Entries\n\n");

                ManageHiddenTags();

                MsgLog ("User Input Received:\n");
                MsgLog ("  - Exit Hidden Tags Page\n\n");

                break;

            case TAG_EXIT:    // Terminate RefindPlus
                MsgLog ("User Input Received:\n");
                MsgLog ("  - Terminate RefindPlus%s", egIsGraphicsModeEnabled() ? L"\n-----------------\n\n" : L"\n\n");

                if ((MokProtocol) && !SecureBootUninstall()) {
                   MainLoopRunning = FALSE;   // just in case we get this far
                }
                else {
                   BeginTextScreen (L" ");
                   MsgLog ("] %a (leaving main)\n", TheTagName);
                   return EFI_SUCCESS;
                }
                break;

            case TAG_FIRMWARE: // Reboot into firmware's user interface
                MsgLog ("User Input Received:\n");
                MsgLog ("  - Reboot into Firmware%s", egIsGraphicsModeEnabled() ? L"\n-----------------\n\n" : L"\n\n");

                RebootIntoFirmware();
                break;

            case TAG_CSR_ROTATE:
                MsgLog ("User Input Received:\n");
                MsgLog ("  - Toggle Mac CSR\n");

                RotateCsrValue();
                break;

            case TAG_INSTALL:
                MsgLog ("User Input Received:\n");
                MsgLog ("  - Install RefindPlus%s", egIsGraphicsModeEnabled() ? L"\n-----------------\n\n" : L"\n\n");

                InstallRefindPlus();
                break;

            case TAG_BOOTORDER:
                MsgLog ("User Input Received:\n");
                MsgLog ("  - Manage Boot Order\n\n");

                ManageBootorder();

                MsgLog ("User Input Received:\n");
                MsgLog ("  - Exit Manage Boot Order Page\n\n");
                break;

        } // switch()

        MsgLog ("] %a\n", TheTagName);

        MyFreePool (&ChosenEntry); // free the shallow copy
    } // while()

    MsgLog ("] Main Loop\n");

    // MyFreePool (&SelectionName); // this should never happen - don't do it since it may be from a menu entry

    // If we end up here, things have gone wrong. Try to reboot, and if that
    // fails, go into an endless loop.
    LOG(1, LOG_LINE_SEPARATOR, L"Main loop has exited, but it should not have!!");

    MsgLog ("Fallback: System Restart...\n");
    MsgLog ("Screen Termination:\n");

    TerminateScreen();

    MsgLog ("System Reset:\n\n");

    refit_call4_wrapper(
        gRT->ResetSystem,
        EfiResetCold,
        EFI_SUCCESS,
        0, NULL
    );

    LOG(1, LOG_THREE_STAR_SEP, L"Shutdown after main loop exit has FAILED!!");

    SwitchToText (FALSE);

    MsgStr = L"INFO: Reboot Failed ... Entering Endless Idle Loop";

    refit_call2_wrapper(
        gST->ConOut->SetAttribute,
        gST->ConOut,
        ATTR_ERROR
    );
    PrintUglyText (MsgStr, NEXTLINE);
    refit_call2_wrapper(
        gST->ConOut->SetAttribute,
        gST->ConOut,
        ATTR_BASIC
    );

    MsgLog ("%s\n-----------------\n\n", MsgStr);

    PauseForKey();
    EndlessIdleLoop();

    return EFI_SUCCESS;
} // EFI_STATUS EFIAPI efi_main()
