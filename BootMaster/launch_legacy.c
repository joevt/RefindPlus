/*
 * BootMaster/launch_legacy.c
 * Functions related to BIOS/CSM/Legacy Booting
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
 * Modifications copyright (c) 2012-2015 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 *
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "icns.h"
#include "launch_legacy.h"
#include "lib.h"
#include "leaks.h"
#include "menu.h"
#include "../include/refit_call_wrapper.h"
#include "screenmgt.h"
#include "../include/syslinux_mbr.h"
#include "mystrings.h"
#include "../EfiLib/BdsHelper.h"
#include "../EfiLib/legacy.h"
#include "../include/Handle.h"

extern REFIT_MENU_ENTRY MenuEntryReturn;
extern REFIT_MENU_SCREEN *MainMenu;

#ifndef __MAKEWITH_GNUEFI
#define LibLocateHandle gBS->LocateHandleBuffer
#define DevicePathProtocol gEfiDevicePathProtocolGuid
#endif

EFI_GUID EfiGlobalVariableGuid = { 0x8BE4DF61, 0x93CA, 0x11D2, \
    { 0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C }};

BOOLEAN FirstLegacyScan = TRUE;

static
EFI_STATUS ActivateMbrPartition (
    IN EFI_BLOCK_IO *BlockIO,
    IN UINTN PartitionIndex
) {
    EFI_STATUS          Status;
    UINT8               SectorBuffer[512];
    MBR_PARTITION_INFO  *MbrTable, *EMbrTable;
    UINT32              ExtBase, ExtCurrent, NextExtCurrent;
    UINTN               LogicalPartitionIndex = 4;
    UINTN               i;
    BOOLEAN             HaveBootCode;

    // read MBR
    Status = refit_call5_wrapper(BlockIO->ReadBlocks, BlockIO, BlockIO->Media->MediaId, 0, 512, SectorBuffer);
    if (EFI_ERROR (Status))
        return Status;
    if (*((UINT16 *)(SectorBuffer + 510)) != 0xaa55)
        return EFI_NOT_FOUND;  // safety measure #1

    // add boot code if necessary
    HaveBootCode = FALSE;
    for (i = 0; i < MBR_BOOTCODE_SIZE; i++) {
        if (SectorBuffer[i] != 0) {
            HaveBootCode = TRUE;
            break;
        }
    }
    if (!HaveBootCode) {
        // no boot code found in the MBR, add the syslinux MBR code
        SetMem (SectorBuffer, MBR_BOOTCODE_SIZE, 0);
        CopyMem (SectorBuffer, syslinux_mbr, SYSLINUX_MBR_SIZE);
    }

    // set the partition active
    MbrTable = (MBR_PARTITION_INFO *)(SectorBuffer + 446);
    ExtBase = 0;
    for (i = 0; i < 4; i++) {
        if (MbrTable[i].Flags != 0x00 && MbrTable[i].Flags != 0x80) {
            return EFI_NOT_FOUND;   // safety measure #2
        }
        if (i == PartitionIndex) {
            MbrTable[i].Flags = 0x80;
        }
        else if (PartitionIndex >= 4 && IS_EXTENDED_PART_TYPE (MbrTable[i].Type)) {
            MbrTable[i].Flags = 0x80;
            ExtBase = MbrTable[i].StartLBA;
        }
        else {
            MbrTable[i].Flags = 0x00;
        }
    }

    // write MBR
    Status = refit_call5_wrapper(
        BlockIO->WriteBlocks,
        BlockIO,
        BlockIO->Media->MediaId,
        0,
        512,
        SectorBuffer
    );
    if (EFI_ERROR (Status)) {
        return Status;
    }

    if (PartitionIndex >= 4) {
        // we have to activate a logical partition, so walk the EMBR chain

        // NOTE: ExtBase was set above while looking at the MBR table
        for (ExtCurrent = ExtBase; ExtCurrent; ExtCurrent = NextExtCurrent) {
            // read current EMBR
            Status = refit_call5_wrapper(
                BlockIO->ReadBlocks,
                BlockIO,
                BlockIO->Media->MediaId,
                ExtCurrent,
                512,
                SectorBuffer
            );
            if (EFI_ERROR (Status)) {
                return Status;
            }
            if (*((UINT16 *)(SectorBuffer + 510)) != 0xaa55) {
                return EFI_NOT_FOUND;  // safety measure #3
            }

            // scan EMBR, set appropriate partition active
            EMbrTable = (MBR_PARTITION_INFO *)(SectorBuffer + 446);
            NextExtCurrent = 0;
            for (i = 0; i < 4; i++) {
                if (EMbrTable[i].Flags != 0x00 && EMbrTable[i].Flags != 0x80) {
                    return EFI_NOT_FOUND;   // safety measure #4
                }
                if (EMbrTable[i].StartLBA == 0 || EMbrTable[i].Size == 0) {
                    break;
                }
                if (IS_EXTENDED_PART_TYPE (EMbrTable[i].Type)) {
                    // link to next EMBR
                    NextExtCurrent = ExtBase + EMbrTable[i].StartLBA;
                    EMbrTable[i].Flags = (PartitionIndex >= LogicalPartitionIndex) ? 0x80 : 0x00;
                    break;
                }
                else {
                    // logical partition
                    EMbrTable[i].Flags = (PartitionIndex == LogicalPartitionIndex) ? 0x80 : 0x00;
                    LogicalPartitionIndex++;
                }
            }

            // write current EMBR
            Status = refit_call5_wrapper(
                BlockIO->WriteBlocks,
                BlockIO,
                BlockIO->Media->MediaId,
                ExtCurrent,
                512,
                SectorBuffer
            );
            if (EFI_ERROR (Status)) {
                return Status;
            }

            if (PartitionIndex < LogicalPartitionIndex) {
                break;  // stop the loop, no need to touch further EMBRs
            }
        }

    }

    return EFI_SUCCESS;
} /* static EFI_STATUS ActivateMbrPartition() */

static
EFI_GUID AppleVariableVendorID = { 0x7C436110, 0xAB2A, 0x4BBB, { 0xA8, 0x80, 0xFE, 0x41, 0x99, 0x5C, 0x9F, 0x82 } };

static
EFI_STATUS WriteBootDiskHint (
    IN EFI_DEVICE_PATH *WholeDiskDevicePath
){
   EFI_STATUS Status;

   Status = EfivarSetRaw (
       &AppleVariableVendorID,
       L"BootCampHD",
       WholeDiskDevicePath,
       GetDevicePathSize (WholeDiskDevicePath),
       TRUE
   );

   if (EFI_ERROR (Status)) {
       return Status;
   }

   return EFI_SUCCESS;
} // EFI_STATUS WriteBootDiskHint

//
// firmware device path discovery
//

static
UINT8 LegacyLoaderMediaPathData[] = {
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
static
EFI_DEVICE_PATH *LegacyLoaderMediaPath = (EFI_DEVICE_PATH *)LegacyLoaderMediaPathData;

static
VOID ExtractLegacyLoaderPaths (
    EFI_DEVICE_PATH **PathList,
    UINTN             MaxPaths,
    EFI_DEVICE_PATH **HardcodedPathList
) {
    EFI_STATUS          Status;
    UINTN               PathIndex;
    UINTN               PathCount   = 0;
    UINTN               HandleCount = 0;
    UINTN               HandleIndex;
    UINTN               HardcodedIndex;
    BOOLEAN             Seen;
    EFI_HANDLE         *Handles;
    EFI_HANDLE          Handle;
    EFI_DEVICE_PATH    *DevicePath;
    EFI_LOADED_IMAGE   *LoadedImage;

    MaxPaths--;  // leave space for the terminating NULL pointer

    // get all LoadedImage handles
    Status = LibLocateHandle (
        ByProtocol,
        &LoadedImageProtocol, NULL,
        &HandleCount, &Handles
    );
    if (CheckError (Status, L"while listing LoadedImage handles")) {
        if (HardcodedPathList) {
            for (HardcodedIndex = 0; HardcodedPathList[HardcodedIndex] && PathCount < MaxPaths; HardcodedIndex++) {
                PathList[PathCount++] = HardcodedPathList[HardcodedIndex];
            }
        }
        PathList[PathCount] = NULL;
        return;
    }
    for (HandleIndex = 0; HandleIndex < HandleCount && PathCount < MaxPaths; HandleIndex++) {
        Handle = Handles[HandleIndex];

        Status = refit_call3_wrapper(
            gBS->HandleProtocol,
            Handle,
            &LoadedImageProtocol,
            (VOID **) &LoadedImage
        );
        if (EFI_ERROR (Status)) {
            // This can only happen if the firmware scewed up ... ignore it.
            continue;
        }

        Status = refit_call3_wrapper(
            gBS->HandleProtocol,
            LoadedImage->DeviceHandle,
            &DevicePathProtocol,
            (VOID **) &DevicePath
        );
        if (EFI_ERROR (Status)) {
            // This happens ... ignore it.
            continue;
        }

        // Only grab memory range nodes
        if (DevicePathType (DevicePath) != HARDWARE_DEVICE_PATH ||
            DevicePathSubType (DevicePath) != HW_MEMMAP_DP
        ) {
            continue;
        }

        // Check if we have this device path in the list already
        // WARNING: This assumes the first node in the device path is unique!
        Seen = FALSE;
        for (PathIndex = 0; PathIndex < PathCount; PathIndex++) {
            if (DevicePathNodeLength (DevicePath) != DevicePathNodeLength (PathList[PathIndex])) {
                continue;
            }
            if (CompareMem (
                    DevicePath, PathList[PathIndex],
                    DevicePathNodeLength (DevicePath)
                ) == 0
            ) {
                Seen = TRUE;
                break;
            }
        }
        if (Seen) {
            continue;
        }

        PathList[PathCount++] = AppendDevicePath (DevicePath, LegacyLoaderMediaPath);
    }
    MyFreePool (&Handles);

    if (HardcodedPathList) {
        for (HardcodedIndex = 0; HardcodedPathList[HardcodedIndex] && PathCount < MaxPaths; HardcodedIndex++) {
            PathList[PathCount++] = HardcodedPathList[HardcodedIndex];
        }
    }
    PathList[PathCount] = NULL;
} /* VOID ExtractLegacyLoaderPaths() */

// early 2006 Core Duo / Core Solo models
static UINT8 LegacyLoaderDevicePath1Data[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xE0, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xF9, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
// mid-2006 Mac Pro (and probably other Core 2 models)
static UINT8 LegacyLoaderDevicePath2Data[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xE0, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xF7, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
// mid-2007 MBP ("Santa Rosa" based models)
static UINT8 LegacyLoaderDevicePath3Data[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xE0, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xF8, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
// early-2008 MBA
static UINT8 LegacyLoaderDevicePath4Data[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xC0, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xF8, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};
// late-2008 MB/MBP (NVidia chipset)
static UINT8 LegacyLoaderDevicePath5Data[] = {
    0x01, 0x03, 0x18, 0x00, 0x0B, 0x00, 0x00, 0x00,
    0x00, 0x40, 0xCB, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0xBF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x04, 0x06, 0x14, 0x00, 0xEB, 0x85, 0x05, 0x2B,
    0xB8, 0xD8, 0xA9, 0x49, 0x8B, 0x8C, 0xE2, 0x1B,
    0x01, 0xAE, 0xF2, 0xB7, 0x7F, 0xFF, 0x04, 0x00,
};

static EFI_DEVICE_PATH *LegacyLoaderList[] = {
    (EFI_DEVICE_PATH *)LegacyLoaderDevicePath1Data,
    (EFI_DEVICE_PATH *)LegacyLoaderDevicePath2Data,
    (EFI_DEVICE_PATH *)LegacyLoaderDevicePath3Data,
    (EFI_DEVICE_PATH *)LegacyLoaderDevicePath4Data,
    (EFI_DEVICE_PATH *)LegacyLoaderDevicePath5Data,
    NULL
};

#define MAX_DISCOVERED_PATHS (16)

// Launch a BIOS boot loader (Mac mode)
static
EFI_STATUS StartLegacyImageList (
    IN EFI_DEVICE_PATH **DevicePaths,
    IN CHAR16 *LoadOptions,
    OUT UINTN *ErrorInStep
) {
    EFI_STATUS              Status, ReturnStatus;
    EFI_HANDLE              ChildImageHandle;
    EFI_LOADED_IMAGE        *ChildLoadedImage = NULL;
    UINTN                   DevicePathIndex;
    CHAR16                  *FullLoadOptions = NULL;

    if (ErrorInStep != NULL) {
        *ErrorInStep = 0;
    }

    // set load options
    if (LoadOptions != NULL) {
        FullLoadOptions = StrDuplicate (LoadOptions);
    } // if (LoadOptions != NULL)
    Print (
        L"Starting legacy loader\nUsing load options '%s'\n",
        FullLoadOptions ? FullLoadOptions : L""
    );

    // load the image into memory
    ReturnStatus = Status = EFI_LOAD_ERROR;  // in case the list is empty
    for (DevicePathIndex = 0; DevicePaths[DevicePathIndex] != NULL; DevicePathIndex++) {
        Status = refit_call6_wrapper(
            gBS->LoadImage,
            FALSE,
            SelfImageHandle,
            DevicePaths[DevicePathIndex],
            NULL,
            0,
            &ChildImageHandle
        );
        ReturnStatus = Status;
        if (ReturnStatus != EFI_NOT_FOUND) {
            break;
        }
    } // for

    if (CheckError (Status, L"while loading legacy loader")) {
        if (ErrorInStep != NULL) {
            *ErrorInStep = 1;
        }
        goto bailout;
    }

    Status = refit_call3_wrapper(
        gBS->HandleProtocol,
        ChildImageHandle,
        &LoadedImageProtocol,
        (VOID **) &ChildLoadedImage
    );

    ReturnStatus = Status;
    if (CheckError (Status, L"while getting a LoadedImageProtocol handle")) {
        if (ErrorInStep != NULL) {
            *ErrorInStep = 2;
        }
        goto bailout_unload;
    }

    ChildLoadedImage->LoadOptions = (VOID *)FullLoadOptions;
    ChildLoadedImage->LoadOptionsSize = FullLoadOptions
        ? ((UINT32)StrLen (FullLoadOptions) + 1) * sizeof (CHAR16)
        : 0;
    // turn control over to the image
    // TODO: (optionally) re-enable the EFI watchdog timer!

    // close open file handles
    LOG(1, LOG_LINE_NORMAL, L"Launching Mac-style BIOS/CSM/Legacy Loader");

    UninitRefitLib();

    Status = refit_call3_wrapper(
        gBS->StartImage,
        ChildImageHandle,
        NULL, NULL
    );
    ReturnStatus = Status;

    // control returns here when the child image calls Exit()
    if (CheckError (Status, L"returned from legacy loader")) {
        LOG(1, LOG_LINE_NORMAL, L"returned from legacy loader");

        if (ErrorInStep != NULL) {
            *ErrorInStep = 3;
        }
    }

    // re-open file handles
    ReinitRefitLib();

bailout_unload:
    // unload the image, we don't care if it works or not...
    refit_call1_wrapper(gBS->UnloadImage, ChildImageHandle);

bailout:
    MyFreePool (&FullLoadOptions);

    return ReturnStatus;
} // EFI_STATUS StartLegacyImageList()

VOID StartLegacy (
    IN LEGACY_ENTRY *Entry,
    IN CHAR16 *SelectionName
) {
    EFI_STATUS      Status;
    EG_IMAGE        *BootLogoImage;
    UINTN           ErrorInStep = 0;
    EFI_DEVICE_PATH *DiscoveredPathList[MAX_DISCOVERED_PATHS];

    CHAR16 *MsgStrA = NULL;
    CHAR16 *MsgStrB = NULL;

    MsgLog ("IsBoot = TRUE\n");
    IsBoot = TRUE;

    LOG(1, LOG_LINE_NORMAL,
        L"Starting Mac-style BIOS/CSM/Legacy Loader:- '%s'",
        SelectionName
    );

    BeginExternalScreen (TRUE, L"Booting Legacy OS (Mac mode)");

    BootLogoImage = LoadOSIcon (GetPoolStr (&Entry->Volume->OSIconName), L"legacy", TRUE);
    if (BootLogoImage != NULL) {
        BltImageAlpha (
            BootLogoImage,
            (ScreenW - BootLogoImage->Width ) >> 1,
            (ScreenH - BootLogoImage->Height) >> 1,
            &StdBackgroundPixel
        );
    }

    if (Entry->Volume->IsMbrPartition) {
        ActivateMbrPartition (
            Entry->Volume->WholeDiskBlockIO,
            Entry->Volume->MbrPartitionIndex
        );
    }

    if (Entry->Volume->DiskKind != DISK_KIND_OPTICAL &&
        Entry->Volume->WholeDiskDevicePath != NULL
    ) {
        WriteBootDiskHint (Entry->Volume->WholeDiskDevicePath);
    }

    ExtractLegacyLoaderPaths (
        DiscoveredPathList,
        MAX_DISCOVERED_PATHS,
        LegacyLoaderList
    );

    StoreLoaderName (SelectionName);
    Status = StartLegacyImageList (
        DiscoveredPathList,
        GetPoolStr (&Entry->LoadOptions),
        &ErrorInStep
    );
    if (Status == EFI_NOT_FOUND) {
        if (ErrorInStep == 1) {
            SwitchToText (FALSE);

            MsgStrA = L"Please make sure you have the latest firmware update installed";
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText (MsgStrA, NEXTLINE);
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

            MsgLog ("** WARN: %s\n\n", MsgStrA);

            PauseForKey();
            SwitchToGraphics();
        }
        else if (ErrorInStep == 3) {
            SwitchToText (FALSE);

            MsgStrA = L"The firmware refused to boot from the selected volume";
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText (MsgStrA, NEXTLINE);
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

            MsgLog ("** WARN: %s\n", MsgStrA);

            MsgStrB = L"NB: External drives are not well-supported by Apple firmware for legacy booting";
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText (MsgStrB, NEXTLINE);
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

            MsgLog ("         %s\n\n", MsgStrB);

            PauseForKey();
            SwitchToGraphics();
        }
    }

    FinishExternalScreen();
} // static VOID StartLegacy()

// Start a device on a non-Mac using the EFI_LEGACY_BIOS_PROTOCOL
VOID StartLegacyUEFI (
    LEGACY_ENTRY *Entry,
    CHAR16 *SelectionName
) {
    LOG(1, LOG_LINE_NORMAL,
        L"Launching UEFI-style BIOS/CSM/Legacy OS '%s'",
        SelectionName
    );

    MsgLog ("IsBoot = TRUE\n");
    IsBoot = TRUE;

    BeginExternalScreen (TRUE, L"Booting Legacy OS (UEFI mode)");
    StoreLoaderName (SelectionName);

    UninitRefitLib();
    BdsLibConnectDevicePath (Entry->BdsOption->DevicePath);
    BdsLibDoLegacyBoot (Entry->BdsOption);

    // If we get here, it means that there was a failure....
    ReinitRefitLib();

    LOG(1, LOG_LINE_NORMAL, L"Failure booting legacy (BIOS) OS.");

    Print(L"Failure booting legacy (BIOS) OS.");
    PauseForKey();
    FinishExternalScreen();
} // static VOID StartLegacyUEFI()

static
LEGACY_ENTRY * AddLegacyEntry (
    IN CHAR16 *LoaderTitle,
    IN REFIT_VOLUME *Volume
) {
    LEGACY_ENTRY      *Entry;
    LEGACY_ENTRY      *SubEntry;
    REFIT_MENU_SCREEN *SubScreen;
    CHAR16            *VolDesc;
    CHAR16            *LegacyTitle;
    CHAR16            ShortcutLetter = 0;

    if (LoaderTitle == NULL) {
        if (GetPoolStr (&Volume->OSName) != NULL) {
            LoaderTitle = GetPoolStr (&Volume->OSName);
            if (LoaderTitle[0] == 'W' || LoaderTitle[0] == 'L') {
                ShortcutLetter = LoaderTitle[0];
            }
        }
        else {
            LoaderTitle = L"Legacy OS";
        }
    }
    if (GetPoolStr (&Volume->VolName) != NULL) {
        VolDesc = StrDuplicate (GetPoolStr (&Volume->VolName));
    }
    else {
        VolDesc = (Volume->DiskKind == DISK_KIND_OPTICAL) ? L"CD" : L"HD";
    }

    if (MyStrStr (VolDesc, L"NTFS volume") != NULL) {
        VolDesc = L"NTFS Volume";
    }

    LegacyTitle = PoolPrint (L"Boot %s from %s", LoaderTitle, VolDesc);

    if (IsInSubstring (LegacyTitle, GlobalConfig.DontScanVolumes)) {
       MyFreePool (&LegacyTitle);

       return NULL;
    } // if

    LOG(1, FirstLegacyScan ? LOG_THREE_STAR_MID : LOG_THREE_STAR_SEP,
        L"Adding BIOS/CSM/Legacy Entry for '%s'",
        LegacyTitle
    );
    FirstLegacyScan = FALSE;

    // prepare the menu entry
    Entry = AllocateZeroPool (sizeof (LEGACY_ENTRY));
    Entry->Enabled = TRUE;
    AssignPoolStr (&Entry->me.Title, LegacyTitle);
    Entry->me.Tag = TAG_LEGACY;
    Entry->me.Row = 0;
    Entry->me.ShortcutLetter = ShortcutLetter;
    AssignPoolImage (&Entry->me.Image, LoadOSIcon (GetPoolStr (&Volume->OSIconName), L"legacy", FALSE));
    CopyFromPoolImage (&Entry->me.BadgeImage, &Volume->VolBadgeImage);
    AssignVolume (&Entry->Volume, Volume);
    AssignPoolStr (&Entry->LoadOptions, (Volume->DiskKind == DISK_KIND_OPTICAL)
       ? L"CD"
       : ((Volume->DiskKind == DISK_KIND_EXTERNAL) ? L"USB" : L"HD")
    );

    MsgLog ("\n");
    MsgLog ("  - Found '%s' on '%s'", LoaderTitle, VolDesc);

    // create the submenu
    SubScreen = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    AssignPoolStr (&SubScreen->Title, PoolPrint (
        L"Boot Options for %s on %s",
        LoaderTitle, VolDesc
     ));

    CopyFromPoolImage (&SubScreen->TitleImage, &Entry->me.Image);
    AssignCachedPoolStr (&SubScreen->Hint1, SUBSCREEN_HINT1);

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
       AssignCachedPoolStr (&SubScreen->Hint2, SUBSCREEN_HINT2_NO_EDITOR);
    }
    else {
       AssignCachedPoolStr (&SubScreen->Hint2, SUBSCREEN_HINT2);
    } // if/else
    LOG(2, LOG_LINE_NORMAL, L"Hint2: '%s'", GetPoolStr (&SubScreen->Hint2));

    // default entry
    SubEntry = AllocateZeroPool (sizeof (LEGACY_ENTRY));
    AssignPoolStr (&SubEntry->me.Title, PoolPrint (L"Boot %s", LoaderTitle));

    SubEntry->me.Tag = TAG_LEGACY;
    AssignVolume (&SubEntry->Volume, Entry->Volume);
    CopyFromPoolStr (&SubEntry->LoadOptions, &Entry->LoadOptions);

    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
    AddMenuEntryCopy (SubScreen, &MenuEntryReturn);

    Entry->me.SubScreen = SubScreen;
    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);

    return Entry;
} // static LEGACY_ENTRY * AddLegacyEntry()


/**
    Create a RefindPlus boot option from a Legacy BIOS protocol option.
*/
static
LEGACY_ENTRY * AddLegacyEntryUEFI (
    BDS_COMMON_OPTION *BdsOption,
    IN UINT16 DiskType
) {
    LEGACY_ENTRY      *Entry;
    LEGACY_ENTRY      *SubEntry;
    REFIT_MENU_SCREEN *SubScreen;
    CHAR16            ShortcutLetter = 0;
    CHAR16            *LegacyDescription = StrDuplicate (BdsOption->Description);

    if (IsInSubstring (LegacyDescription, GlobalConfig.DontScanVolumes)) {
        return NULL;
    }

    // Remove stray spaces, since many EFIs produce descriptions with lots of
    // extra spaces, especially at the end; this throws off centering of the
    // description on the screen....
    LimitStringLength (LegacyDescription, 100);

    // prepare the menu entry
    Entry = AllocateZeroPool (sizeof (LEGACY_ENTRY));
    AssignPoolStr (&Entry->me.Title, PoolPrint (L"Boot legacy OS from %s", LegacyDescription));

    LOG(1, LOG_LINE_NORMAL,
        L"Adding UEFI-style BIOS/CSM/Legacy Entry for '%s'",
        GetPoolStr (&Entry->me.Title)
    );

    Entry->me.Tag = TAG_LEGACY_UEFI;
    Entry->me.Row = 0;
    Entry->me.ShortcutLetter = ShortcutLetter;
    AssignPoolImage (&Entry->me.Image, LoadOSIcon (L"legacy", L"legacy", TRUE));
    AssignPoolStr (&Entry->LoadOptions, (DiskType == BBS_CDROM)
        ? L"CD"
        : ((DiskType == BBS_USB)
            ? L"USB" : L"HD")
    );
    CopyFromPoolImage_PI_ (&Entry->me.BadgeImage_PI_, GetDiskBadge (DiskType));
    Entry->BdsOption = BdsOption;
    Entry->Enabled = TRUE;

    // create the submenu
    SubScreen = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
    AssignCachedPoolStr (&SubScreen->Title, L"No boot options for legacy target");

    CopyFromPoolImage (&SubScreen->TitleImage, &Entry->me.Image);
    AssignCachedPoolStr (&SubScreen->Hint1, SUBSCREEN_HINT1);

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
       AssignCachedPoolStr (&SubScreen->Hint2, SUBSCREEN_HINT2_NO_EDITOR);
    }
    else {
       AssignCachedPoolStr (&SubScreen->Hint2, SUBSCREEN_HINT2);
    } // if/else

    // default entry
    SubEntry = AllocateZeroPool (sizeof (LEGACY_ENTRY));
    AssignPoolStr (&SubEntry->me.Title, PoolPrint (L"Boot %s", LegacyDescription));

    SubEntry->me.Tag = TAG_LEGACY_UEFI;
    Entry->BdsOption = BdsOption;

    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);

    AddMenuEntryCopy (SubScreen, &MenuEntryReturn);
    Entry->me.SubScreen = SubScreen;
    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *)Entry);

    return Entry;
} // static LEGACY_ENTRY * AddLegacyEntryUEFI()

/**
    Scan for legacy BIOS targets on machines that implement EFI_LEGACY_BIOS_PROTOCOL.
    In testing, protocol has not been implemented on Macs but has been
    implemented on most UEFI PCs.
    Restricts output to disks of the specified DiskType.
*/
static
VOID ScanLegacyUEFI (
    IN UINTN DiskType
) {
    EFI_STATUS                Status;
    UINT16                    *BootOrder = NULL;
    UINTN                     Index = 0;
    UINTN                     BootOrderSize = 0;
    CHAR16                    Buffer[20];
    CHAR16                     BootOption[10];
    BOOLEAN                   SearchingForUsb = FALSE;
    LIST_ENTRY                 TempList;
    BDS_COMMON_OPTION         *BdsOption;
    BBS_BBS_DEVICE_PATH       *BbsDevicePath   = NULL;
    EFI_LEGACY_BIOS_PROTOCOL  *LegacyBios;

    LOG(1, FirstLegacyScan ? LOG_THREE_STAR_MID : LOG_THREE_STAR_SEP, L"Scanning for a UEFI-style BIOS/CSM/Legacy OS");
    FirstLegacyScan = FALSE;

    InitializeListHead (&TempList);
    ZeroMem (Buffer, sizeof (Buffer));

    // If LegacyBios protocol is not implemented on this platform, then
    //we do not support this type of legacy boot on this machine.
    Status = refit_call3_wrapper(
        gBS->LocateProtocol,
        &gEfiLegacyBootProtocolGuid,
        NULL,
        (VOID **) &LegacyBios
    );
    if (EFI_ERROR (Status)) {
        return;
    }

    // EFI calls USB drives BBS_HARDDRIVE, but we want to distinguish them,
    // so we set DiskType inappropriately elsewhere in the program and
    // "translate" it here.
    if (DiskType == BBS_USB) {
       DiskType = BBS_HARDDISK;
       SearchingForUsb = TRUE;
    } // if

    // Grab the boot order
    BootOrder = BdsLibGetVariableAndSize (
        L"BootOrder",
        &EfiGlobalVariableGuid,
        &BootOrderSize
    );
    if (BootOrder == NULL) {
        BootOrderSize = 0;
    }

    Index = 0;

    while (Index < BootOrderSize / sizeof (UINT16)) {
        // Grab each boot option variable from the boot order, and convert
        // the variable into a BDS boot option
        SPrint (BootOption, sizeof (BootOption), L"Boot%04x", BootOrder[Index]);
        BdsOption = BdsLibVariableToOption (&TempList, BootOption);

        if (BdsOption != NULL) {
           BbsDevicePath = (BBS_BBS_DEVICE_PATH *)BdsOption->DevicePath;
           // Only add the entry if it is of a requested type (e.g. USB, HD)
           // Two checks necessary because some systems return EFI boot loaders
           // with a DeviceType value that would inappropriately include them
            // as legacy loaders.
            if ((BbsDevicePath->DeviceType == DiskType) &&
                (BdsOption->DevicePath->Type == DEVICE_TYPE_BIOS)
            ) {
              // USB flash drives appear as hard disks with certain media flags set.
              // Look for this, and if present, pass it on with the (technically
              // incorrect, but internally useful) BBS_TYPE_USB flag set.
              if (DiskType == BBS_HARDDISK) {
                 if (SearchingForUsb && (BbsDevicePath->StatusFlag &
                     (BBS_MEDIA_PRESENT | BBS_MEDIA_MAYBE_PRESENT))
                 ) {
                    AddLegacyEntryUEFI (BdsOption, BBS_USB);
                 }
                 else if (!SearchingForUsb && !(BbsDevicePath->StatusFlag &
                     (BBS_MEDIA_PRESENT | BBS_MEDIA_MAYBE_PRESENT))
                 ) {
                    AddLegacyEntryUEFI (BdsOption, DiskType);
                 }
              }
              else {
                 AddLegacyEntryUEFI (BdsOption, DiskType);
              } // if/else
           } // if
        } // if (BdsOption != NULL)
        Index++;
    } // while
} // static VOID ScanLegacyUEFI()

static
VOID ScanLegacyVolume (
    REFIT_VOLUME *Volume,
    UINTN VolumeIndex
) {
    UINTN VolumeIndex2;
    BOOLEAN ShowVolume, HideIfOthersFound;

    ShowVolume = FALSE;
    HideIfOthersFound = FALSE;
    if (Volume->HasBootCode) {
        ShowVolume = TRUE;
        if (Volume->BlockIO == Volume->WholeDiskBlockIO &&
            Volume->BlockIOOffset == 0 &&
            GetPoolStr (&Volume->OSName) == NULL
        ) {
            // this is a whole disk (MBR) entry; hide if we have entries for partitions
            HideIfOthersFound = TRUE;
        }
    }
    if (HideIfOthersFound) {
        // check for other bootable entries on the same disk
        for (VolumeIndex2 = 0; VolumeIndex2 < VolumesCount; VolumeIndex2++) {
            if (VolumeIndex2 != VolumeIndex && Volumes[VolumeIndex2]->HasBootCode &&
                Volumes[VolumeIndex2]->WholeDiskBlockIO == Volume->WholeDiskBlockIO
            ) {
                ShowVolume = FALSE;
            }
        }
    }

    if (ShowVolume) {
        if ((GetPoolStr (&Volume->VolName) == NULL) ||
            (StrLen (GetPoolStr (&Volume->VolName)) == 0)
        ) {
            AssignPoolStr (&Volume->VolName, GetVolumeName (Volume));
        }

        AddLegacyEntry (NULL, Volume);
    }
} // static VOID ScanLegacyVolume()


// Scan attached optical discs for legacy (BIOS) boot code
// and add anything found to the list....
VOID ScanLegacyDisc (
    VOID
) {
    UINTN VolumeIndex;
    REFIT_VOLUME *Volume = NULL;

    LOG(1, LOG_LINE_THIN_SEP,
        L"Scanning Optical Discs with Mode:- 'BIOS/CSM/Legacy'"
    );

    FirstLegacyScan = TRUE;
    if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC) {
        for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
            AssignVolume (&Volume, Volumes[VolumeIndex]);
            if (Volume->DiskKind == DISK_KIND_OPTICAL) {
                ScanLegacyVolume (Volume, VolumeIndex);
            }
        } // for
        // since Volume started as NULL, free it if it is not NULL
        FreeVolume (&Volume);
    }
    else if (GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) {
        ScanLegacyUEFI (BBS_CDROM);
    }
    FirstLegacyScan = FALSE;
} // VOID ScanLegacyDisc()

// Scan internal hard disks for legacy (BIOS) boot code
// and add anything found to the list....
VOID ScanLegacyInternal (
    VOID
) {
    UINTN VolumeIndex;
    REFIT_VOLUME *Volume = NULL;

    LOG(1, LOG_LINE_THIN_SEP,
        L"Scanning Internal Disk Volumes with Mode:- 'BIOS/CSM/Legacy'"
    );

    FirstLegacyScan = TRUE;
    if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC) {
       for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
           AssignVolume (&Volume, Volumes[VolumeIndex]);
           if (Volume->DiskKind == DISK_KIND_INTERNAL) {
               ScanLegacyVolume (Volume, VolumeIndex);
           }
       } // for
       // since Volume started as NULL, free it if it is not NULL
       FreeVolume (&Volume);
    }
    else if (GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) {
       // TODO: This actually picks up USB flash drives, too; try to find
       // a way to differentiate the two....
       ScanLegacyUEFI (BBS_HARDDISK);
    }
    FirstLegacyScan = FALSE;
} // VOID ScanLegacyInternal()

// Scan external disks for legacy (BIOS) boot code
// and add anything found to the list....
VOID ScanLegacyExternal (
    VOID
) {
   UINTN VolumeIndex;
   REFIT_VOLUME *Volume = NULL;

   LOG(1, LOG_LINE_THIN_SEP,
       L"Scanning External Disk Volumes with Mode:- 'BIOS/CSM/Legacy'"
   );

   FirstLegacyScan = TRUE;
   if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC) {
      for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
         AssignVolume (&Volume, Volumes[VolumeIndex]);
         if (Volume->DiskKind == DISK_KIND_EXTERNAL) {
             ScanLegacyVolume (Volume, VolumeIndex);
         }
      } // for
      // since Volume started as NULL, free it if it is not NULL
      FreeVolume (&Volume);
   }
   else if (GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) {
      // TODO: This actually doesn't do anything useful; leaving in hopes of
      // fixing it later....
      ScanLegacyUEFI (BBS_USB);
   }
   FirstLegacyScan = FALSE;
} // VOID ScanLegacyExternal()

// Determine what (if any) type of legacy (BIOS) boot support is available
VOID FindLegacyBootType (VOID) {
   EFI_STATUS                Status;
   EFI_LEGACY_BIOS_PROTOCOL  *LegacyBios;

   GlobalConfig.LegacyType = LEGACY_TYPE_NONE;

    // UEFI-style legacy BIOS support is only available with some EFI implementations
   Status = refit_call3_wrapper(
       gBS->LocateProtocol,
       &gEfiLegacyBootProtocolGuid,
       NULL,
       (VOID **) &LegacyBios
   );
   if (!EFI_ERROR (Status)) {
       GlobalConfig.LegacyType = LEGACY_TYPE_UEFI;
   }

   // Macs have their own system. If the firmware vendor code contains the
    // string "Apple", assume it is available. Note that this overrides the
   // UEFI type, and might yield false positives if the vendor string
    // contains "Apple" as part of something bigger, so this is not perfect.
   if (StriSubCmp (L"Apple", gST->FirmwareVendor)) {
       GlobalConfig.LegacyType = LEGACY_TYPE_MAC;
   }
} // VOID FindLegacyBootType()

// Warn the user if legacy OS scans are enabled but the firmware can't support them
VOID WarnIfLegacyProblems (
    VOID
) {
    UINTN    i           = 0;
    BOOLEAN  found       = FALSE;


    if (GlobalConfig.LegacyType == LEGACY_TYPE_NONE) {
        do {
            if (GlobalConfig.ScanFor[i] == 'H' || GlobalConfig.ScanFor[i] == 'h' ||
                GlobalConfig.ScanFor[i] == 'C' || GlobalConfig.ScanFor[i] == 'c' ||
                GlobalConfig.ScanFor[i] == 'B' || GlobalConfig.ScanFor[i] == 'b'
            ) {
                found = TRUE;
            }
            i++;
        } while ((i < NUM_SCAN_OPTIONS) && (!found));

        if (found) {
            LOG(1, LOG_LINE_NORMAL,
                L"BIOS/CSM/Legacy support enabled in RefindPlus but unavailable in EFI!!"
            );

            SwitchToText (FALSE);

            CHAR16 *MsgStr =
                L"** WARN: Your 'scanfor' config line specifies scanning for one or more legacy\n"
                L"         (BIOS) boot options; however, this is not possible because your computer lacks\n"
                L"         the necessary Compatibility Support Module (CSM) support or that support is\n"
                L"         disabled in your firmware.";

            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_ERROR);
            PrintUglyText (MsgStr, NEXTLINE);
            refit_call2_wrapper(gST->ConOut->SetAttribute, gST->ConOut, ATTR_BASIC);

            MsgLog ("%s\n\n", MsgStr);

            PauseForKey();
        } // if (found)
    } // if no legacy support
} // VOID WarnIfLegacyProblems()
