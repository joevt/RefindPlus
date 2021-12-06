/*
 * BootMaster/lib.c
 * General library functions
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
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
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "lib.h"
#include "icns.h"
#include "screenmgt.h"
#include "../include/refit_call_wrapper.h"
#include "../include/RemovableMedia.h"
#include "gpt.h"
#include "config.h"
#include "apple.h"
#include "mystrings.h"
#include "leaks.h"

#ifdef __MAKEWITH_GNUEFI
#define EfiReallocatePool ReallocatePool
#else
#define LibLocateHandle gBS->LocateHandleBuffer
#define DevicePathProtocol gEfiDevicePathProtocolGuid
#define BlockIoProtocol gEfiBlockIoProtocolGuid
#define LibFileSystemInfo EfiLibFileSystemInfo
#define LibOpenRoot EfiLibOpenRoot
EFI_DEVICE_PATH EndDevicePath[] = {
   {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {END_DEVICE_PATH_LENGTH, 0}}
};
#endif

// "Magic" signatures for various filesystems
#define FAT_MAGIC                        0xAA55
#define EXT2_SUPER_MAGIC                 0xEF53
#define HFSPLUS_MAGIC1                   0x2B48
#define HFSPLUS_MAGIC2                   0x5848
#define REISERFS_SUPER_MAGIC_STRING      "ReIsErFs"
#define REISER2FS_SUPER_MAGIC_STRING     "ReIsEr2Fs"
#define REISER2FS_JR_SUPER_MAGIC_STRING  "ReIsEr3Fs"
#define BTRFS_SIGNATURE                  "_BHRfS_M"
#define XFS_SIGNATURE                    "XFSB"
#define JFS_SIGNATURE                    "JFS1"
#define NTFS_SIGNATURE                   "NTFS    "
#define FAT12_SIGNATURE                  "FAT12   "
#define FAT16_SIGNATURE                  "FAT16   "
#define FAT32_SIGNATURE                  "FAT32   "

#if defined (EFIX64)
    EFI_GUID gFreedesktopRootGuid = {0x4f68bce3, 0xe8cd, 0x4db1, {0x96, 0xe7, 0xfb, 0xca, 0xf9, 0x84, 0xb7, 0x09}};
#elif defined (EFI32)
    EFI_GUID gFreedesktopRootGuid = {0x44479540, 0xf297, 0x41b2, {0x9a, 0xf7, 0xd1, 0x31, 0xd5, 0xf0, 0x45, 0x8a}};
#elif defined (EFIAARCH64)
    EFI_GUID gFreedesktopRootGuid = {0xb921b045, 0x1df0, 0x41c3, {0xaf, 0x44, 0x4c, 0x6f, 0x28, 0x0d, 0x3f, 0xae}};
#else // Below is GUID for ARM32
    EFI_GUID gFreedesktopRootGuid = {0x69dad710, 0x2ce4, 0x4e3c, {0xb1, 0x6c, 0x21, 0xa1, 0xd4, 0x9a, 0xbe, 0xd3}};
#endif

// variables

EFI_HANDLE         SelfImageHandle;

EFI_LOADED_IMAGE  *SelfLoadedImage;

EFI_FILE          *SelfRootDir;
EFI_FILE          *SelfDir;
EFI_FILE          *gVarsDir            = NULL;

CHAR16            *SelfDirPath         = NULL;

REFIT_VOLUME      *SelfVolume          = NULL;
REFIT_VOLUME     **Volumes             = NULL;
REFIT_VOLUME     **PreBootVolumes      = NULL;
REFIT_VOLUME     **SystemVolumes       = NULL;
REFIT_VOLUME     **DataVolumes         = NULL;

UINTN              PreBootVolumesCount = 0;
UINTN              SystemVolumesCount  = 0;
UINTN              DataVolumesCount    = 0;
UINTN              VolumesCount        = 0;

BOOLEAN            MediaCheck          = FALSE;
BOOLEAN            ScannedOnce         = FALSE;
BOOLEAN            SelfVolSet          = FALSE;
BOOLEAN            SelfVolRun          = FALSE;
BOOLEAN            DoneHeadings        = FALSE;
BOOLEAN            ScanMBR             = FALSE;
BOOLEAN            SkipSpacing         = FALSE;

#if REFIT_DEBUG > 0
BOOLEAN            FirstVolume         = TRUE;
BOOLEAN            FoundMBR            = FALSE;
#endif

EFI_GUID           GuidNull            = NULL_GUID_VALUE;
EFI_GUID           GuidESP             = ESP_GUID_VALUE;
EFI_GUID           GuidHFS             = HFS_GUID_VALUE;
EFI_GUID           GuidAPFS            = APFS_GUID_VALUE;
EFI_GUID           GuidMacRaidOn       = MAC_RAID_ON_GUID_VALUE;
EFI_GUID           GuidMacRaidOff      = MAC_RAID_OFF_GUID_VALUE;
EFI_GUID           GuidRecoveryHD      = MAC_RECOVERYHD_GUID_VALUE;
EFI_GUID           GuidCoreStorage     = CORE_STORAGE_GUID_VALUE;
EFI_GUID           GuidAppleTvRec      = APPLE_TV_RECOVERY_GUID;


extern EFI_GUID RefindPlusGuid;

extern BOOLEAN  ScanningLoaders;


// Maximum size for disk sectors
#define SECTOR_SIZE 4096

// Number of bytes to read from a partition to determine its filesystem type
// and identify its boot loader, and hence probable BIOS-mode OS installation
#define SAMPLE_SIZE 69632 /* 68 KiB -- ReiserFS superblock begins at 64 KiB */

BOOLEAN egIsGraphicsModeEnabled (VOID);

//
// Pathname manipulations
//

// Converts forward slashes to backslashes, removes duplicate slashes, and
// removes slashes from both the start and end of the pathname.
// Necessary because some (buggy?) EFI implementations produce "\/" strings
// in pathnames, because some user inputs can produce duplicate directory
// separators, and because we want consistent start and end slashes for
// directory comparisons. A special case: If the PathName refers to root,
// but is non-empty, return "\", since some firmware implementations flake
// out if this is not present.
VOID CleanUpPathNameSlashes (
    IN OUT CHAR16 *PathName
) {
    UINTN Dest   = 0;
    UINTN Source = 0;

    if (PathName == NULL ||
        PathName[0] == '\0'
    ) {
        return;
    }

    while (PathName[Source] != '\0') {
        if ((PathName[Source] == L'/') || (PathName[Source] == L'\\')) {
            if (Dest == 0) {
                // Skip slash if to first position
                Source++;
            }
            else {
                PathName[Dest] = L'\\';
                do {
                    // Skip subsequent slashes
                    Source++;
                } while ((PathName[Source] == L'/') || (PathName[Source] == L'\\'));

                Dest++;
            }
        }
        else {
            // Regular character; copy it straight.
            PathName[Dest] = PathName[Source];
            Source++;
            Dest++;
        }
    } // while

    if ((Dest > 0) && (PathName[Dest - 1] == L'\\')) {
        Dest--;
    }

    PathName[Dest]   = L'\0';

    if (PathName[0] == L'\0') {
        PathName[0]  = L'\\';
        PathName[1]  = L'\0';
    }
} // CleanUpPathNameSlashes()

// Splits an EFI device path into device and filename components. For instance, if InString is
// PciRoot (0x0)/Pci (0x1f,0x2)/Ata (Secondary,Master,0x0)/HD (2,GPT,8314ae90-ada3-48e9-9c3b-09a88f80d921,0x96028,0xfa000)/\bzImage-3.5.1.efi, this function will truncate that input to
// PciRoot (0x0)/Pci (0x1f,0x2)/Ata (Secondary,Master,0x0)/HD (2,GPT,8314ae90-ada3-48e9-9c3b-09a88f80d921,0x96028,0xfa000)
// and return bzImage-3.5.1.efi as its return value.
// It does this by searching for the last ")" character in InString, copying everything
// after that string (after some cleanup) as the return value, and truncating the original
// input value.
// If InString contains no ")" character, this function leaves the original input string
// unmodified and also returns that string. If InString is NULL, this function returns NULL.
CHAR16 * SplitDeviceString (
    IN OUT CHAR16 *InString
) {
    INTN     i;
    CHAR16  *FileName = NULL;
    BOOLEAN  Found    = FALSE;

    if (InString != NULL) {
        i = StrLen (InString) - 1;
        while ((i >= 0) && (!Found)) {
            if (InString[i] == L')') {
                Found    = TRUE;
                FileName = StrDuplicate (&InString[i + 1]);
                CleanUpPathNameSlashes (FileName);
                InString[i + 1] = '\0';
            }
            i--;
        } // while

        if (FileName == NULL) {
            FileName = StrDuplicate (InString);
        }
    } // if

    return FileName;
} // static CHAR16* SplitDeviceString()

//
// Library initialization and de-initialization
//

static
EFI_STATUS FinishInitRefitLib (VOID) {
    LOGPROCENTRY();
    EFI_STATUS Status = EFI_SUCCESS;

    if (SelfVolume && SelfVolume->DeviceHandle != SelfLoadedImage->DeviceHandle) {
        MsgLog ("Self device handle changed %p -> %p\n", SelfLoadedImage->DeviceHandle, SelfVolume->DeviceHandle);
        SelfLoadedImage->DeviceHandle = SelfVolume->DeviceHandle;
    }

    if (SelfRootDir == NULL) {
        SelfRootDir = LibOpenRoot (SelfLoadedImage->DeviceHandle);
        if (SelfRootDir == NULL) {
            Status = EFI_LOAD_ERROR;
            CheckError (Status, L"While (Re)Opening Installation Volume");
        }
    }

    if (!EFI_ERROR(Status)) {
        LEAKABLEEXTERNALSTART ("FinishInitRefitLib Open");
        Status = REFIT_CALL_5_WRAPPER(
            SelfRootDir->Open, SelfRootDir,
            &SelfDir, SelfDirPath,
            EFI_FILE_MODE_READ, 0
        );
        LEAKABLEEXTERNALSTOP ();
        if (CheckFatalError (Status, L"While Opening Installation Directory")) {
            Status = EFI_LOAD_ERROR;
        }
    }

    LOGPROCEXIT("%r", Status);
    return Status;
} // static EFI_STATUS FinishInitRefitLib()

EFI_STATUS InitRefitLib (
    IN EFI_HANDLE ImageHandle
) {
    //LOGPROCENTRY();
    EFI_STATUS   Status;
    CHAR16      *Temp = NULL;
    CHAR16      *DevicePathAsString;

    SelfImageHandle = ImageHandle;
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol, SelfImageHandle,
        &LoadedImageProtocol, (VOID **) &SelfLoadedImage
    );

    if (CheckFatalError (Status, L"While Getting a LoadedImageProtocol Handle")) {
        return EFI_LOAD_ERROR;
    }

    // find the current directory
    DevicePathAsString = DevicePathToStr (SelfLoadedImage->FilePath);
    GlobalConfig.SelfDevicePath = FileDevicePath (
        SelfLoadedImage->DeviceHandle,
        DevicePathAsString
    );
    LEAKABLE (GlobalConfig.SelfDevicePath, "SelfDevicePath");

    CleanUpPathNameSlashes (DevicePathAsString);
    MY_FREE_POOL(SelfDirPath);
    Temp        = FindPath (DevicePathAsString);
    SelfDirPath = SplitDeviceString (Temp);
    LEAKABLE (SelfDirPath, "SelfDirPath");

    MY_FREE_POOL(DevicePathAsString);
    MY_FREE_POOL(Temp);

    Status = FinishInitRefitLib();

    //LOGPROCEXIT("%r", Status);
    return Status;
} // InitRefitLib

static
VOID UninitVolume (
    IN OUT REFIT_VOLUME  **Volume
) {
    if (Volume && *Volume) {
        if ((*Volume)->RootDir != NULL) {
            REFIT_CALL_1_WRAPPER((*Volume)->RootDir->Close, (*Volume)->RootDir);
            (*Volume)->RootDir = NULL;
        }

        (*Volume)->OldDeviceHandle = (*Volume)->DeviceHandle;
        (*Volume)->DeviceHandle = NULL;
        (*Volume)->BlockIO = NULL;
        (*Volume)->WholeDiskBlockIO = NULL;
    }
} // static VOID UninitVolume()

static
VOID UninitVolumes (VOID) {
    LOGPROCENTRY();
    REFIT_VOLUME *Volume = NULL;
    UINTN VolumeIndex;

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        AssignVolume (&Volume, Volumes[VolumeIndex]);
        UninitVolume (&Volume);
    }
    // since Volume started as NULL, free it if it is not NULL
    FreeVolume (&Volume);
    LOGPROCEXIT();
} // static VOID UninitVolumes()

static
VOID ReinitVolume (
    IN OUT REFIT_VOLUME *Volume
) {
    EFI_STATUS        StatusA, StatusB;
    EFI_DEVICE_PATH  *RemainingDevicePath;
    EFI_HANDLE        DeviceHandle;
    EFI_HANDLE        WholeDiskHandle;

    if (Volume) {
        if (Volume->DevicePath != NULL) {
            // get the handle for that path
            RemainingDevicePath = Volume->DevicePath;
            StatusA = REFIT_CALL_3_WRAPPER(
                gBS->LocateDevicePath, &BlockIoProtocol,
                &RemainingDevicePath, &DeviceHandle
            );

            if (!EFI_ERROR(StatusA)) {
                if (DeviceHandle != Volume->OldDeviceHandle) {
                    MsgLog ("Volume '%s' device handle changed %p -> %p\n", GetPoolStr (&Volume->VolName), Volume->OldDeviceHandle, DeviceHandle);
                }
                Volume->DeviceHandle = DeviceHandle;

                // get the root directory
                Volume->RootDir = LibOpenRoot (Volume->DeviceHandle);

            }
            else {
                CheckError (StatusA, L"from LocateDevicePath");
            }
        }

        if (Volume->WholeDiskDevicePath != NULL) {
            // get the handle for that path
            RemainingDevicePath = Volume->WholeDiskDevicePath;
            StatusB = REFIT_CALL_3_WRAPPER(
                gBS->LocateDevicePath, &BlockIoProtocol,
                &RemainingDevicePath, &WholeDiskHandle
            );

            if (!EFI_ERROR(StatusB)) {
                // get the BlockIO protocol
                StatusB = REFIT_CALL_3_WRAPPER(
                    gBS->HandleProtocol, WholeDiskHandle,
                    &BlockIoProtocol, (VOID **) &Volume->WholeDiskBlockIO
                );

                if (EFI_ERROR(StatusB)) {
                    Volume->WholeDiskBlockIO = NULL;
                    CheckError (StatusB, L"from HandleProtocol");
                }
            }
            else {
                CheckError (StatusB, L"from LocateDevicePath");
            }
        }
    } // if Volume
} // static VOID ReinitVolume()

VOID ReinitVolumes (VOID) {
    LOGPROCENTRY();
    REFIT_VOLUME *Volume = NULL;
    UINTN VolumeIndex;

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        AssignVolume (&Volume, Volumes[VolumeIndex]);
        ReinitVolume (Volume);
    }
    // since Volume started as NULL, free it if it is not NULL
    FreeVolume (&Volume);
    LOGPROCEXIT();
} // VOID ReinitVolumes()

VOID CloseDir (EFI_FILE **theDir) {
    if (theDir && *theDir) {
        REFIT_CALL_1_WRAPPER((*theDir)->Close, *theDir);
        *theDir = NULL;
    }
}

// called before running external programs to close open file handles
VOID UninitRefitLib (VOID) {
    LOGPROCENTRY();
    // This piece of code was made to correspond to weirdness in ReinitRefitLib().
    // See the comment on it there.
    if (SelfRootDir == SelfVolume->RootDir) {
        MsgLog ("SelfRootDir == SelfVolume->RootDir\n");
        SelfRootDir = NULL;
    }

    UninitVolumes();

    CloseDir (&SelfDir);
    CloseDir (&SelfRootDir);
    CloseDir (&gVarsDir);

    LOGPROCEXIT();
} // VOID UninitRefitLib()

// called after running external programs to re-open file handles
EFI_STATUS ReinitRefitLib (VOID) {
    LOGPROCENTRY();
    EFI_STATUS Status;

    ReinitVolumes();
    Status = FinishInitRefitLib();

    LOGPROCEXIT("%r", Status);
    return Status;
}

//
// UEFI variable read and write functions
//

// Create a directory for holding RefindPlus variables, if they are stored on
// disk. This will be in the RefindPlus install directory if possible, or on the
// first ESP that RefindPlus can identify if not (this typically means RefindPlus
// is on a read-only filesystem, such as HFS+ on a Mac). If neither location can
// be used, the variable is not stored. Sets the pointer to the directory in the
// file-global gVarsDir variable and returns the status of the operation.
EFI_STATUS FindVarsDir (VOID) {
    EFI_STATUS       Status = EFI_SUCCESS;
    EFI_FILE_HANDLE  EspRootDir;

    if (gVarsDir == NULL) {
        LEAKABLEEXTERNALSTART ("FindVarsDir Open");
        Status = REFIT_CALL_5_WRAPPER(
            SelfDir->Open, SelfDir,
            &gVarsDir, L"vars",
            EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
            EFI_FILE_DIRECTORY
        );
        LEAKABLEEXTERNALSTOP ();

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Locate/Create Emulated NVRAM for RefindPlus-Specific Items ... In Installation Folder:- '%r'",
            Status
        );
        #endif

        if (EFI_ERROR(Status)) {
            Status = egFindESP (&EspRootDir);
            if (!EFI_ERROR(Status)) {
                Status = REFIT_CALL_5_WRAPPER(
                    EspRootDir->Open, EspRootDir,
                    &gVarsDir, L"refind-vars",
                    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                    EFI_FILE_DIRECTORY
                );
            }

            #if REFIT_DEBUG > 0
            LOG(3, LOG_LINE_NORMAL,
                L"Locate/Create Emulated NVRAM for RefindPlus-Specific Items ... In First Available ESP:- '%r'",
                Status
            );

            if (EFI_ERROR(Status)) {
                LOG(4, LOG_THREE_STAR_MID,
                    L"Activate the 'use_nvram' Config Token to Use Hardware NVRAM Instead"
                );
            }
            #endif
        }
    }

    return Status;
} // EFI_STATUS FindVarsDir()

// Retrieve a raw UEFI variable, either from NVRAM or from a disk file under
// RefindPlus' "vars" subdirectory, depending on GlobalConfig.UseNvram.
// Returns EFI status
EFI_STATUS EfivarGetRaw (
    IN  EFI_GUID  *VendorGUID,
    IN  CHAR16    *VariableName,
    OUT VOID     **VariableData,
    OUT UINTN     *VariableSize  OPTIONAL
) {
    BOOLEAN UseEmulatedNVRAM = !GlobalConfig.UseNvram && GuidsAreEqual (VendorGUID, &RefindPlusGuid);
    LOGPROCENTRY("'%s' %s", VariableName, UseEmulatedNVRAM ? L"from Emulated NVRAM" : L"from Hardware NVRAM");

    VOID        *TmpBuffer     = NULL;
    UINTN        BufferSize    = 0;
    EFI_STATUS   Status        = EFI_LOAD_ERROR;

    BOOLEAN HybridLogger = FALSE;
    if (NativeLogger) {
        HybridLogger = TRUE;
        MsgLog("NativeLogger = FALSE\n");
        NativeLogger = FALSE;
    }

    if (UseEmulatedNVRAM) {
        Status = FindVarsDir();
        if (Status == EFI_SUCCESS) {
            Status = egLoadFile (
                gVarsDir,
                VariableName,
                (UINT8 **) &TmpBuffer,
                &BufferSize
            );
        }
        else if (Status != EFI_NOT_FOUND) {
            #if REFIT_DEBUG > 0
            LOG2(4, LOG_THREE_STAR_MID, L"** WARN: ", L"\n",   L"In Emulated NVRAM ... Could Not Read UEFI Variable:- '%s'", VariableName);
            LOG2(4, LOG_THREE_STAR_MID, L"         ", L"\n\n", L"Activate the 'use_nvram' Option to Silence this Warning");
            #endif
        }

        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID,
            L"In Emulated NVRAM ... %r When Fetching Variable:- '%s'",
            Status, VariableName
        );
        #endif

        if (!EFI_ERROR(Status)) {
            if (MyStriCmp (VariableName, L"PreviousBoot")) {
                #if REFIT_DEBUG > 0
                LOG(3, LOG_LINE_NORMAL, L"Previous Boot:- '%s'", TmpBuffer);
                #endif
            }
        }
        else {
            MY_FREE_POOL(TmpBuffer);
            BufferSize = 0;
        }

        if (MyStriCmp (VariableName, L"PreviousBoot")) {
            #if REFIT_DEBUG > 0
            LOG(3, LOG_BLANK_LINE_SEP, L"X");
            #endif
        }
    }
    else {
        // Pass in a zero-size buffer to find the required buffer size.
        Status = REFIT_CALL_5_WRAPPER(
            gRT->GetVariable, VariableName,
            VendorGUID, NULL,
            &BufferSize, TmpBuffer
        );

        // If the variable exists, the status should be EFI_BUFFER_TOO_SMALL and BufferSize updated.
        // Any other status means the variable does not exist.
        if (Status != EFI_BUFFER_TOO_SMALL) {
            Status = EFI_NOT_FOUND;
        }
        else {
            TmpBuffer = AllocateZeroPool (BufferSize);
            if (!TmpBuffer) {
                Status = EFI_OUT_OF_RESOURCES;
            }
            else {
                // Retry with the correct buffer size.
                Status = REFIT_CALL_5_WRAPPER(
                    gRT->GetVariable, VariableName,
                    VendorGUID, NULL,
                    &BufferSize, TmpBuffer
                );
            }
        }

        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID,
            L"In Hardware NVRAM ... %r When Fetching Variable:- '%s'",
            Status, VariableName
        );
        #endif

        if (!EFI_ERROR(Status)) {
            if (MyStriCmp (VariableName, L"PreviousBoot")) {
                #if REFIT_DEBUG > 0
                LOG(3, LOG_LINE_NORMAL, L"Previous Boot:- '%s'", TmpBuffer);
                LOG(3, LOG_BLANK_LINE_SEP, L"X");
                #endif
            }
        }
        else {
            MY_FREE_POOL(TmpBuffer);
            BufferSize = 0;
        }
    }

    *VariableData = TmpBuffer;
    *VariableSize = BufferSize;

    if (HybridLogger) {
        NativeLogger = TRUE;
        MsgLog("NativeLogger = TRUE\n");
    }

    LOGPROCEXIT("'%s' ...'%r'", VariableName, Status);
    return Status;
} // EFI_STATUS EfivarGetRaw ()

// Set an UEFI variable. This is normally done to NVRAM; however, RefindPlus'
// variables (as determined by the *VendorGUID code) will be saved to a disk file IF
// GlobalConfig.UseNvram == FALSE.
// Returns EFI status
EFI_STATUS EfivarSetRaw (
    IN  EFI_GUID  *VendorGUID,
    IN  CHAR16    *VariableName,
    IN  VOID      *VariableData,
    IN  UINTN      VariableSize,
    IN  BOOLEAN    Persistent
) {
    LOGPROCENTRY("'%s' %p(%d)", VariableName, VariableData, VariableSize);
    EFI_STATUS   Status;
    UINT32       StorageFlags;
    VOID        *OldBuf;
    UINTN        OldSize;
    BOOLEAN      SettingMatch;

    BOOLEAN HybridLogger = FALSE;
    if (NativeLogger) {
        HybridLogger = TRUE;
        MsgLog("NativeLogger = FALSE\n");
        NativeLogger = FALSE;
    }

    if (VariableSize > 0 &&
        VariableData != NULL &&
        !MyStriCmp (VariableName, L"HiddenTags") &&
        !MyStriCmp (VariableName, L"HiddenTools") &&
        !MyStriCmp (VariableName, L"HiddenLegacy") &&
        !MyStriCmp (VariableName, L"HiddenFirmware")
    ) {
        MuteLogger = TRUE;
        Status     = EfivarGetRaw (VendorGUID, VariableName, &OldBuf, &OldSize);
        MuteLogger = FALSE;

        if (!EFI_ERROR(Status)) {
            // Check for settings match
            SettingMatch = (
                VariableSize == OldSize &&
                CompareMem (VariableData, OldBuf, VariableSize) == 0
            );

            if (SettingMatch) {
                // Return if settings match
                if (HybridLogger) {
                    NativeLogger = TRUE;
                    MsgLog("NativeLogger = TRUE\n");
                }

                MY_FREE_POOL(OldBuf);

                LOGPROCEXIT("Already Set");
                // State to be logged by caller if required
                return EFI_ALREADY_STARTED;
            }
        }
        MY_FREE_POOL(OldBuf);
    }

    // Proceed ... settings do not match
    if (!GlobalConfig.UseNvram &&
        GuidsAreEqual (VendorGUID, &RefindPlusGuid)
    ) {
        Status = FindVarsDir();
        if (Status == EFI_SUCCESS) {
            // Clear the current value
            egSaveFile (
                gVarsDir, VariableName,
                NULL, 0
            );

            // Store the new value
            Status = egSaveFile (
                gVarsDir, VariableName,
                (UINT8 *) VariableData, VariableSize
            );
        }

        #if REFIT_DEBUG > 0
        CHAR16 *MsgStr = L"Activate the 'use_nvram' Option to Silence this Warning";
        LOG(4, LOG_THREE_STAR_MID,
            L"In Emulated NVRAM ... %r When Saving Variable:- '%s'",
            Status, VariableName
        );

        if (EFI_ERROR(Status)) {
            LOG(4, LOG_THREE_STAR_MID, L"%s", MsgStr);

            MsgLog ("** WARN: Could Not Save to Emulated NVRAM:- '%s'\n", VariableName);
            MsgLog ("         %s\n\n", MsgStr);
        }
        #endif
    }
    else {
        StorageFlags  = EFI_VARIABLE_BOOTSERVICE_ACCESS;
        StorageFlags |= EFI_VARIABLE_RUNTIME_ACCESS;
        if (Persistent) {
            StorageFlags |= EFI_VARIABLE_NON_VOLATILE;
        }

        // Clear the current value
        REFIT_CALL_5_WRAPPER(
            gRT->SetVariable, VariableName,
            VendorGUID, StorageFlags,
            0, NULL
        );

        // Store the new value
        Status = REFIT_CALL_5_WRAPPER(
            gRT->SetVariable, VariableName,
            VendorGUID, StorageFlags,
            VariableSize, VariableData
        );

        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID,
            L"In Hardware NVRAM ... %r When Saving Variable:- '%s'",
            Status, VariableName
        );
        #endif
    }

    if (HybridLogger) {
        NativeLogger = TRUE;
        MsgLog("NativeLogger = TRUE\n");
    }

    LOGPROCEXIT("%r", Status);
    return Status;
} // EFI_STATUS EfivarSetRaw ()

//
// list functions
//

VOID AddListElementSized (
    IN OUT VOID   **ListPtr,
    IN OUT UINTN   *ElementCount,
    IN     VOID    *NewElement,
    IN     UINTN    ElementSize
) {
    #if REFIT_DEBUG > 0
    if ((!*ElementCount) ^ (!*ListPtr)) {
        LOG(4, LOG_THREE_STAR_MID, L"Empty list is not correct count:%d list:%p->%p", *ElementCount, ListPtr, *ListPtr);
        DumpCallStack(NULL, FALSE);
    }
    #endif
    CONST UINTN AllocatePointer = ElementSize * (*ElementCount + 16);
    if (*ListPtr == NULL) {
        *ListPtr = AllocatePool (AllocatePointer);
    }
    else if ((*ElementCount & 15) == 0) {
        *ListPtr = EfiReallocatePool (*ListPtr, ElementSize * (*ElementCount), AllocatePointer);
    }
    if (*ListPtr) {
        LOGPOOL(*ListPtr);
        CopyMem (*ListPtr + ElementSize * (*ElementCount), NewElement, ElementSize);
        (*ElementCount)++;
    }
    else {
        LOG(4, LOG_THREE_STAR_MID, L"Could not allocate memory for list elements");
    }
} // VOID AddListElementSized()

VOID AddListElement (
    IN OUT VOID  ***ListPtr,
    IN OUT UINTN   *ElementCount,
    IN     VOID    *NewElement
) {
    AddListElementSized ((VOID**)ListPtr, ElementCount, &NewElement, sizeof (NewElement));
} // VOID AddListElement()

VOID FreeList (
    IN OUT VOID  ***ListPtr,
    IN OUT UINTN   *ElementCount
) {
    if (ListPtr && *ListPtr) {
        if (ElementCount && *ElementCount) {
            UINTN i;
            for (i = 0; i < *ElementCount; i++) {
                // TODO: call a user-provided routine for each element here
                MY_FREE_POOL((*ListPtr)[i]);
            }
            *ElementCount = 0;
        }
        MY_FREE_POOL(*ListPtr);
    }
} // VOID FreeList()

//
// volume functions
//

#define DEBUG_VOLUMES 0

#if DEBUG_VOLUMES == 1
#define VolumeLog(x) x
#else
#define VolumeLog(x)
#endif

VOID SanitiseVolumeName (
    REFIT_VOLUME *Volume
) {
    LOGPROCENTRY("Volume:%p VolName:%s", Volume, GetPoolStr (&Volume->VolName));
    CHAR16 *VolumeName = NULL;

    #define SANITIZE(v) if (MyStrStrIns (GetPoolStr (&Volume->VolName), v)) VolumeName = v;
    if (Volume && GetPoolStr (&Volume->VolName)) {
        if (0) ;
        else SANITIZE(L"EFI System Partition"        )
        else SANITIZE(L"Whole Disk Volume"           )
        else SANITIZE(L"Unknown Volume"              )
        else SANITIZE(L"NTFS Volume"                 )
        else SANITIZE(L"HFS+ Volume"                 )
        else SANITIZE(L"FAT Volume"                  )
        else SANITIZE(L"XFS Volume"                  )
        else SANITIZE(L"PreBoot"                     )
        else SANITIZE(L"Ext4 Volume"                 )
        else SANITIZE(L"Ext3 Volume"                 )
        else SANITIZE(L"Ext2 Volume"                 )
        else SANITIZE(L"Btrfs Volume"                )
        else SANITIZE(L"ReiserFS Volume"             )
        else SANITIZE(L"ISO-9660 Volume"             )
        else SANITIZE(L"Basic Data Partition"        )
        else SANITIZE(L"Microsoft Reserved Partition")
    }

    if (VolumeName != NULL) {
        AssignCachedPoolStr (&Volume->VolName, VolumeName);
        LOGPROCEXIT("VolName:%s", GetPoolStr (&Volume->VolName));
    }
    else {
        LOGPROCEXIT("unchanged");
    }
} // VOID SanitiseVolumeName()

VOID FreeVolumes (
    IN OUT REFIT_VOLUME  ***ListVolumes,
    IN OUT UINTN           *ListCount
) {
    if (ListVolumes && *ListVolumes) {
        LOGPROCENTRY("%p->%p[%d]", ListVolumes, *ListVolumes, ListCount ? *ListCount : -1);
        UINTN i;

        if (ListCount && *ListCount) {
            for (i = 0; i < *ListCount; i++) {
                FreeVolume (&(*ListVolumes)[i]);
            }
            *ListCount = 0;
        }
        MY_FREE_POOL(*ListVolumes);
        LOGPROCEXIT();
    }
} // VOID FreeVolumes()

REFIT_VOLUME * CopyVolume (
    IN REFIT_VOLUME *VolumeToCopy
) {
    LOGPROCENTRY("%p", VolumeToCopy);
    REFIT_VOLUME *Volume = NULL;

    if (VolumeToCopy) {
        // UnInit VolumeToCopy
        UninitVolume (&VolumeToCopy);

        // Create New Volume based on VolumeToCopy (in 'UnInit' state)
        Volume = AllocateCopyPool (sizeof (REFIT_VOLUME), VolumeToCopy);
        if (Volume) {
            Volume->ReferenceCount = 1;
            ZeroPoolStr(&Volume->FsName)         ; CopyFromPoolStr(&Volume->FsName, &VolumeToCopy->FsName);
            ZeroPoolStr(&Volume->VolName)        ; CopyFromPoolStr(&Volume->VolName, &VolumeToCopy->VolName);
            ZeroPoolStr(&Volume->PartName)       ; CopyFromPoolStr(&Volume->PartName, &VolumeToCopy->PartName);
            ZeroPoolStr(&Volume->OSIconName)     ; CopyFromPoolStr(&Volume->OSIconName, &VolumeToCopy->OSIconName);
            ZeroPoolStr(&Volume->OSName)         ; CopyFromPoolStr(&Volume->OSName, &VolumeToCopy->OSName);
            ZeroPoolImage(&Volume->VolIconImage) ; CopyFromPoolImage(&Volume->VolIconImage, &VolumeToCopy->VolIconImage);
            ZeroPoolImage(&Volume->VolBadgeImage); CopyFromPoolImage(&Volume->VolBadgeImage, &VolumeToCopy->VolBadgeImage);

            if (VolumeToCopy->DevicePath != NULL) {
                Volume->DevicePath = DuplicateDevicePath (VolumeToCopy->DevicePath);
            }

            if (VolumeToCopy->WholeDiskDevicePath != NULL) {
                Volume->WholeDiskDevicePath = DuplicateDevicePath (VolumeToCopy->WholeDiskDevicePath);
            }

            if (VolumeToCopy->MbrPartitionTable) {
                Volume->MbrPartitionTable = AllocateCopyPool (4 * 16, VolumeToCopy->MbrPartitionTable);
            }

            // ReInit Volume
            ReinitVolume (Volume);
        }

        // ReInit VolumeToCopy
        ReinitVolume (VolumeToCopy);
    } // if VolumeToCopy

    LOGPROCEXIT("%p", Volume);

    return Volume;
} // REFIT_VOLUME * CopyVolume()

/*
    Call FreeVolume after using CopyVolume or AllocateVolume or RetainVolume, but not when using AssignVolume.
    Don't call this in a loop unless CopyVolume or AllocateVolume or RetainVolume is called in a loop.
*/
VOID FreeVolume (
    REFIT_VOLUME **Volume
) {
    if (Volume && *Volume) {
        VolumeLog (LOGPROCENTRY("%p->%p(#%d)", Volume, *Volume, (*Volume)->ReferenceCount));
        if ((*Volume)->ReferenceCount > 0) {
            (*Volume)->ReferenceCount--;
            if ((*Volume)->ReferenceCount == 0) {
                MsgLog ("FreeVolume %p->%p(#%d)\n", Volume, *Volume, (*Volume)->ReferenceCount);

                // UnInit Volume
                UninitVolume (&(*Volume));

                // Free pool elements
                FreePoolStr (&(*Volume)->FsName);
                FreePoolStr (&(*Volume)->VolName);
                FreePoolStr (&(*Volume)->PartName);
                FreePoolStr (&(*Volume)->OSIconName);
                FreePoolStr (&(*Volume)->OSName);
                MY_FREE_POOL((*Volume)->DevicePath);
                MY_FREE_POOL((*Volume)->MbrPartitionTable);
                MY_FREE_POOL((*Volume)->WholeDiskDevicePath);

                // Free image elements
                FreePoolImage (&(*Volume)->VolIconImage);
                FreePoolImage (&(*Volume)->VolBadgeImage);

                // Free whole volume
                MY_FREE_POOL(*Volume);
            }
        }
        else {
            MsgLog ("Allocation Error: Volume reference count is already zero.\n");
            DumpCallStack(NULL, FALSE);
        }
        VolumeLog (LOGPROCEXIT("%p->%p(#%d)", Volume, *Volume, *Volume ? (*Volume)->ReferenceCount : -1));
        *Volume = NULL;
    }
} // VOID FreeVolume()

// Return a pointer to a string containing a filesystem type name. If the
// filesystem type is unknown, a blank (but non-null) string is returned.
// The returned variable is a constant that should NOT be freed.
static
CHAR16 * FSTypeName (
    IN UINT32 TypeCode
) {
    CHAR16 *retval = NULL;

    switch (TypeCode) {
        case FS_TYPE_WHOLEDISK: retval = L"Whole Disk";  break;
        case FS_TYPE_HFSPLUS:   retval = L"HFS+";        break;
        case FS_TYPE_APFS:      retval = L"APFS";        break;
        case FS_TYPE_NTFS:      retval = L"NTFS";        break;
        case FS_TYPE_EXT4:      retval = L"Ext4";        break;
        case FS_TYPE_EXT3:      retval = L"Ext3";        break;
        case FS_TYPE_EXT2:      retval = L"Ext2";        break;
        case FS_TYPE_FAT:       retval = L"FAT";         break;
        case FS_TYPE_XFS:       retval = L"XFS";         break;
        case FS_TYPE_JFS:       retval = L"JFS";         break;
        case FS_TYPE_BTRFS:     retval = L"Btrfs";       break;
        case FS_TYPE_ISO9660:   retval = L"ISO-9660";    break;
        case FS_TYPE_REISERFS:  retval = L"ReiserFS";    break;
        default:                retval = L"Unknown";     break;
    } // switch

    return retval;
} // CHAR16 *FSTypeName()

// Sets the FsName field of Volume, based on data recorded in the partition's
// filesystem. This field may remain unchanged if there is no known filesystem
// or if the name field is empty.
static
VOID SetFilesystemName (
    IN OUT REFIT_VOLUME *Volume
) {
    EFI_FILE_SYSTEM_INFO *FileSystemInfoPtr = NULL;

    if ((Volume) && (Volume->RootDir != NULL)) {
        FileSystemInfoPtr = LibFileSystemInfo (Volume->RootDir);
    }

    if ((FileSystemInfoPtr != NULL) &&
        (StrLen (FileSystemInfoPtr->VolumeLabel) > 0)
    ) {
        CopyPoolStr (&Volume->FsName, FileSystemInfoPtr->VolumeLabel);
    }

    MY_FREE_POOL(FileSystemInfoPtr);
} // VOID *SetFilesystemName()

// Identify the filesystem type and record the filesystem's UUID/serial number,
// if possible. Expects a Buffer containing the first few (normally at least
// 4096) bytes of the filesystem. Sets the filesystem type code in Volume->FSType
// and the UUID/serial number in Volume->VolUuid. Note that the UUID value is
// recognized differently for each filesystem, and is currently supported only
// for NTFS, FAT, ext2/3/4fs, and ReiserFS (and for NTFS and FAT it is really a
// 64-bit or 32-bit serial number not a UUID or GUID). If the UUID can't be
// determined, it is set to 0. Also, the UUID is just read directly into memory;
// it is *NOT* valid when displayed by GuidAsString() or used in other
// GUID/UUID-manipulating functions. (As I write, it is being used merely to
// detect partitions that are part of a RAID 1 array.)
static
VOID SetFilesystemData (
    IN     UINT8        *Buffer,
    IN     UINTN         BufferSize,
    IN OUT REFIT_VOLUME *Volume
) {
    UINT32  *Ext2Compat;
    UINT32  *Ext2Incompat;
    UINT16  *Magic16;
    char    *MagicString;

    if ((Buffer != NULL) && (Volume != NULL)) {
        SetMem(&(Volume->VolUuid), sizeof(EFI_GUID), 0);
        Volume->FSType = FS_TYPE_UNKNOWN;

        if (BufferSize >= (1024 + 100)) {
            Magic16 = (UINT16*) (Buffer + 1024 + 56);
            if (*Magic16 == EXT2_SUPER_MAGIC) {
                // ext2/3/4
                Ext2Compat   = (UINT32*) (Buffer + 1024 + 92);
                Ext2Incompat = (UINT32*) (Buffer + 1024 + 96);

                if ((*Ext2Incompat & 0x0040) || (*Ext2Incompat & 0x0200)) {
                    // check for extents or flex_bg
                    Volume->FSType = FS_TYPE_EXT4;
                }
                else if (*Ext2Compat & 0x0004) {
                    // check for journal
                    Volume->FSType = FS_TYPE_EXT3;
                }
                else {
                    // none of these features. Assume it is ext2
                    Volume->FSType = FS_TYPE_EXT2;
                }
                CopyMem(&(Volume->VolUuid), Buffer + 1024 + 104, sizeof(EFI_GUID));

                return;
            }
        } // search for ext2/3/4 magic

        if (BufferSize >= (65536 + 100)) {
            MagicString = (char*) (Buffer + 65536 + 52);
            if ((CompareMem(MagicString, REISERFS_SUPER_MAGIC_STRING,     8) == 0) ||
                (CompareMem(MagicString, REISER2FS_SUPER_MAGIC_STRING,    9) == 0) ||
                (CompareMem(MagicString, REISER2FS_JR_SUPER_MAGIC_STRING, 9) == 0)
            ) {
                Volume->FSType = FS_TYPE_REISERFS;
                CopyMem(&(Volume->VolUuid), Buffer + 65536 + 84, sizeof(EFI_GUID));

                return;
            }
        } // search for ReiserFS magic

        if (BufferSize >= (65536 + 64 + 8)) {
            MagicString = (char*) (Buffer + 65536 + 64);
            if (CompareMem(MagicString, BTRFS_SIGNATURE, 8) == 0) {
                Volume->FSType = FS_TYPE_BTRFS;

                return;
            }
        } // search for Btrfs magic

        if (BufferSize >= 512) {
            MagicString = (char*) Buffer;
            if (CompareMem(MagicString, XFS_SIGNATURE, 4) == 0) {
                Volume->FSType = FS_TYPE_XFS;

                return;
            }
        } // search for XFS magic

        if (BufferSize >= (32768 + 4)) {
            MagicString = (char*) (Buffer + 32768);
            if (CompareMem (MagicString, JFS_SIGNATURE, 4) == 0) {
                Volume->FSType = FS_TYPE_JFS;

                return;
            }
        } // search for JFS magic

        if (BufferSize >= (1024 + 2)) {
            Magic16 = (UINT16*) (Buffer + 1024);
            if ((*Magic16 == HFSPLUS_MAGIC1) ||
                (*Magic16 == HFSPLUS_MAGIC2)
            ) {
                Volume->FSType = FS_TYPE_HFSPLUS;

                return;
            }
        } // search for HFS+ magic

        if (BufferSize >= 512) {
            // Search for NTFS, FAT, and MBR/EBR.
            // These all have 0xAA55 at the end of the first sector, so we must
            // also search for NTFS, FAT12, FAT16, and FAT32 signatures to
            // figure out where to look for the filesystem serial numbers.
            Magic16 = (UINT16*) (Buffer + 510);
            if (*Magic16 == FAT_MAGIC) {
                MagicString = (char*) Buffer;
                if (CompareMem(MagicString + 3, NTFS_SIGNATURE, 8) == 0) {
                    Volume->FSType = FS_TYPE_NTFS;
                    CopyMem(&(Volume->VolUuid), Buffer + 0x48, sizeof(UINT64));
                }
                else if ((CompareMem(MagicString + 0x36, FAT12_SIGNATURE, 8) == 0) ||
                    (CompareMem(MagicString + 0x36,      FAT16_SIGNATURE, 8) == 0)
                ) {
                    Volume->FSType = FS_TYPE_FAT;
                    CopyMem(&(Volume->VolUuid), Buffer + 0x27, sizeof(UINT32));
                }
                else if (CompareMem(MagicString + 0x52, FAT32_SIGNATURE, 8) == 0) {
                    Volume->FSType = FS_TYPE_FAT;
                    CopyMem(&(Volume->VolUuid), Buffer + 0x43, sizeof(UINT32));
                }
                else if (!Volume->BlockIO->Media->LogicalPartition) {
                    Volume->FSType = FS_TYPE_WHOLEDISK;
                }

                return;
            }
        } // search for FAT and NTFS magic

        // If no other filesystem is identified and block size is right, assume ISO-9660
        if (Volume->BlockIO->Media->BlockSize == 2048) {
            Volume->FSType = FS_TYPE_ISO9660;
            return;
        }

        // If no other filesystem is identified, assume APFS if on partition with APFS GUID
        if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidAPFS)) {
            Volume->FSType = FS_TYPE_APFS;
            return;
        }

        // If no other filesystem is identified, assume HFS+ if on partition with HFS+ GUID
        if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidHFS)) {
            Volume->FSType = FS_TYPE_HFSPLUS;
            return;
        }
    } // if ((Buffer != NULL) && (Volume != NULL))
} // UINT32 SetFilesystemData()

static
VOID ScanVolumeBootcode (
    IN OUT REFIT_VOLUME  *Volume,
    IN OUT BOOLEAN       *Bootable
) {
    EFI_STATUS           Status;
    UINTN                i;
    UINT8                Buffer[SAMPLE_SIZE];
    BOOLEAN              MbrTableFound = FALSE;
    MBR_PARTITION_INFO  *MbrTable;

    #if REFIT_DEBUG > 0
    UINTN   LogLineType;
    CHAR16 *StrSpacer = NULL;
    #endif

    *Bootable           = FALSE;
    MediaCheck          = FALSE;
    Volume->HasBootCode = FALSE;
    FreePoolStr (&Volume->OSIconName);
    FreePoolStr (&Volume->OSName);

    if (Volume->BlockIO == NULL) {
        return;
    }
    if (Volume->BlockIO->Media->BlockSize > SAMPLE_SIZE) {
        // our buffer is too small
        return;
    }

    LEAKABLEEXTERNALSTART ("ScanVolumeBootcode ReadBlocks");
    // look at the boot sector (this is used for both hard disks and El Torito images!)
    Status = REFIT_CALL_5_WRAPPER(
        Volume->BlockIO->ReadBlocks,
        Volume->BlockIO,
        Volume->BlockIO->Media->MediaId,
        Volume->BlockIOOffset,
        SAMPLE_SIZE,
        Buffer
    );
    LEAKABLEEXTERNALSTOP ();

    if (!EFI_ERROR(Status)) {
        SetFilesystemData (Buffer, SAMPLE_SIZE, Volume);

        if (GlobalConfig.LegacyType != LEGACY_TYPE_MAC) {
            #if REFIT_DEBUG > 0
            if (SelfVolRun && ScanMBR) {
                if (Status == EFI_NO_MEDIA) {
                    MediaCheck = TRUE;
                }
                ScannedOnce = FALSE;
                CHAR16 *MsgStr = L"Error While Reading Boot Sector";
                MsgLog ("\n\n");
                MsgLog ("** WARN: '%r' %s", Status, MsgStr);
                MsgLog ("\n");
                CheckError (Status, MsgStr);
            }
            #endif

            return;
        }

        if ((*((UINT16 *)(Buffer + 510)) == 0xaa55 && Buffer[0] != 0) &&
            (FindMem (Buffer, 512, "EXFAT", 5) == -1)
        ) {
            *Bootable            = TRUE;
             Volume->HasBootCode = TRUE;
        }

        // detect specific boot codes
        if (CompareMem (Buffer + 2, "LILO",           4) == 0 ||
            CompareMem (Buffer + 6, "LILO",           4) == 0 ||
            CompareMem (Buffer + 3, "SYSLINUX",       8) == 0 ||
            FindMem (Buffer, SECTOR_SIZE, "ISOLINUX", 8) >= 0
        ) {
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"linux");
            AssignCachedPoolStr (&Volume->OSName, L"Linux (Legacy)");
        }
        else if (FindMem (Buffer, 512, "Geom\0Hard Disk\0Read\0 Error", 26) >= 0) {
            // GRUB
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"grub,linux");
            AssignCachedPoolStr (&Volume->OSName, L"Linux (Legacy)");
        }
        else if ((*((UINT32 *)(Buffer + 502)) == 0 &&
            *((UINT32 *)(Buffer + 506)) == 50000 &&
            *((UINT16 *)(Buffer + 510)) == 0xaa55) ||
            FindMem (Buffer, SECTOR_SIZE, "Starting the BTX loader", 23) >= 0
        ) {
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"freebsd");
            AssignCachedPoolStr (&Volume->OSName, L"FreeBSD (Legacy)");
        }
        else if ((*((UINT16 *)(Buffer + 510)) == 0xaa55) &&
            (FindMem (Buffer, SECTOR_SIZE, "Boot loader too large",         21) >= 0) &&
            (FindMem (Buffer, SECTOR_SIZE, "I/O error loading boot loader", 29) >= 0)
        ) {
            // If more differentiation needed, also search for
            // "Invalid partition table" &/or "Missing boot loader".
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"freebsd");
            AssignCachedPoolStr (&Volume->OSName, L"FreeBSD (Legacy)");
        }
        else if (
            FindMem (Buffer, 512,    "!Loading",            8) >= 0 ||
            FindMem (Buffer, SECTOR_SIZE, "/cdboot\0/CDBOOT\0", 16) >= 0
        ) {
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"openbsd");
            AssignCachedPoolStr (&Volume->OSName, L"OpenBSD (Legacy)");
        }
        else if (FindMem (Buffer, 512, "Not a bootxx image", 18) >= 0 ||
            *((UINT32 *)(Buffer + 1028)) == 0x7886b6d1
        ) {
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"netbsd");
            AssignCachedPoolStr (&Volume->OSName, L"NetBSD (Legacy)");
        }
        else if (FindMem (Buffer, SECTOR_SIZE, "NTLDR", 5) >= 0) {
            // Windows NT/200x/XP
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"win");
            AssignCachedPoolStr (&Volume->OSName, L"Windows (NT/XP)");
        }
        else if (FindMem (Buffer, SECTOR_SIZE, "BOOTMGR", 7) >= 0) {
            // Windows Vista/7/8/10
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"win8,win");
            AssignCachedPoolStr (&Volume->OSName, L"Windows (Legacy)");
        }
        else if (
            FindMem (Buffer, 512, "CPUBOOT SYS", 11) >= 0 ||
            FindMem (Buffer, 512, "KERNEL  SYS", 11) >= 0
        ) {
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"freedos");
            AssignCachedPoolStr (&Volume->OSName, L"FreeDOS (Legacy)");
        }
        else if (
            FindMem (Buffer, 512, "OS2LDR",  6) >= 0 ||
            FindMem (Buffer, 512, "OS2BOOT", 7) >= 0
        ) {
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"ecomstation");
            AssignCachedPoolStr (&Volume->OSName, L"eComStation (Legacy)");
        }
        else if (FindMem (Buffer, 512, "Be Boot Loader", 14) >= 0) {
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"beos");
            AssignCachedPoolStr (&Volume->OSName, L"BeOS (Legacy)");
        }
        else if (FindMem (Buffer, 512, "yT Boot Loader", 14) >= 0) {
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"zeta,beos");
            AssignCachedPoolStr (&Volume->OSName, L"ZETA (Legacy)");
        }
        else if (
            FindMem (Buffer, 512, "\x04" "beos\x06" "system\x05" "zbeos", 18) >= 0 ||
            FindMem (Buffer, 512, "\x06" "system\x0c" "haiku_loader",     20) >= 0
        ) {
            Volume->HasBootCode  = TRUE;
            AssignCachedPoolStr (&Volume->OSIconName, L"haiku,beos");
            AssignCachedPoolStr (&Volume->OSName, L"Haiku (Legacy)");
        } // CompareMem

        /**
         * NOTE: If you add an operating system with a name that starts with 'W' or 'L',
         *       you need to fix AddLegacyEntry in BootMaster/launch_legacy.c.
         *       DA-TAGGED
        **/

        if (Volume->HasBootCode) {
            // verify Windows boot sector on Macs
            if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC &&
                Volume->FSType == FS_TYPE_NTFS
            ) {
                Volume->HasBootCode = HasWindowsBiosBootFiles (Volume);
            }
            else if (FindMem (Buffer, 512, "Non-system disk", 15) >= 0) {
                // dummy FAT boot sector (created by OS X's newfs_msdos)
                Volume->HasBootCode = FALSE;
            }
            else if (FindMem (Buffer, 512, "This is not a bootable disk", 27) >= 0) {
                // dummy FAT boot sector (created by Linux's mkdosfs)
                Volume->HasBootCode = FALSE;
            }
            else if (FindMem (Buffer, 512, "Press any key to restart", 24) >= 0) {
                // dummy FAT boot sector (created by Windows)
                Volume->HasBootCode = FALSE;
            }
        }

        #if REFIT_DEBUG > 0
        if (Volume->HasBootCode) {
            if (DoneHeadings) {
                if (SkipSpacing) {
                    StrSpacer   = L" ... ";
                    LogLineType = LOG_LINE_SAME;
                }
                else {
                    StrSpacer   = L"";
                    LogLineType = LOG_LINE_SPECIAL;
                }
                LOG(4, LogLineType,  L"%sFound Legacy Boot Code", StrSpacer);
                SkipSpacing = (GlobalConfig.LogLevel > 3) ? TRUE : FALSE;
            }
        }
        #endif

        // check for MBR partition table
        if (*((UINT16 *)(Buffer + 510)) == 0xaa55) {
            MbrTable = (MBR_PARTITION_INFO *)(Buffer + 446);
            for (i = 0; i < 4; i++) {
                if (MbrTable[i].StartLBA && MbrTable[i].Size) {
                    MbrTableFound = TRUE;
                }
            }

            for (i = 0; i < 4; i++) {
                if (MbrTable[i].Flags != 0x00 && MbrTable[i].Flags != 0x80) {
                    MbrTableFound = FALSE;
                }
            }

            if (MbrTableFound) {
                UINTN SizeMBR = 4 * 16;
                Volume->MbrPartitionTable = AllocatePool (SizeMBR);
                if (Volume->MbrPartitionTable) {
                    CopyMem (Volume->MbrPartitionTable, MbrTable, SizeMBR);

                    #if REFIT_DEBUG > 0
                    FoundMBR = TRUE;

                    if (DoneHeadings) {
                        if (SkipSpacing) {
                            StrSpacer   = L" ... ";
                            LogLineType = LOG_LINE_SAME;
                        }
                        else {
                            StrSpacer   = L"";
                            LogLineType = LOG_LINE_SPECIAL;
                        }
                        LOG(4, LogLineType, L"%sFound MBR Partition Table", StrSpacer);
                        SkipSpacing = (GlobalConfig.LogLevel > 3) ? TRUE : FALSE;
                    }
                    #endif
                }
            }
        }
    } // if !EFI_ERROR Status
} // VOID ScanVolumeBootcode()

// Set default volume badge icon based on /.VolumeBadge.{icns|png} file or disk kind
VOID SetVolumeBadgeIcon (
    IN OUT REFIT_VOLUME *Volume
) {
    LOGPROCENTRY("'%s'", GetPoolStr (&Volume->VolName));
    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BADGES) {
        LOGPROCEXIT("Hidden");
        return;
    }

    if (Volume == NULL) {
        LOGPROCEXIT("NULL Volume!!");
        return;
    }

    if (!GetPoolImage (&Volume->VolBadgeImage)) {
        AssignPoolImage (&Volume->VolBadgeImage, egLoadIconAnyType (
            Volume->RootDir, L"", L".VolumeBadge",
            GlobalConfig.IconSizes[ICON_SIZE_BADGE]
        ));
    }

    if (!GetPoolImage (&Volume->VolBadgeImage)) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"Trying BuiltinIcon");
        #endif

        switch (Volume->DiskKind) {
            case DISK_KIND_INTERNAL: CopyFromPoolImage_PI_ (&Volume->VolBadgeImage_PI_, BuiltinIcon (BUILTIN_ICON_VOL_INTERNAL)); break;
            case DISK_KIND_EXTERNAL: CopyFromPoolImage_PI_ (&Volume->VolBadgeImage_PI_, BuiltinIcon (BUILTIN_ICON_VOL_EXTERNAL)); break;
            case DISK_KIND_OPTICAL : CopyFromPoolImage_PI_ (&Volume->VolBadgeImage_PI_, BuiltinIcon (BUILTIN_ICON_VOL_OPTICAL )); break;
            case DISK_KIND_NET     : CopyFromPoolImage_PI_ (&Volume->VolBadgeImage_PI_, BuiltinIcon (BUILTIN_ICON_VOL_NET     )); break;
        } // switch
    }
    LOGPROCEXIT("%sFound", GetPoolImage (&Volume->VolBadgeImage) ? L"" : L"Not ");
} // VOID SetVolumeBadgeIcon()

// Return a string representing the input size in IEEE-1541 units.
// The calling function is responsible for freeing the allocated memory.
static
CHAR16 * SizeInIEEEUnits (
    IN UINT64 SizeInBytes
) {
    UINTN   NumPrefixes;
    UINTN   Index    = 0;
    CHAR16  Units[]  = L"  iB";
    CHAR16 *Prefixes = L" KMGTPEZ";
    CHAR16 *TheValue;
    UINT64  SizeInIeee;

    NumPrefixes = StrLen (Prefixes);
    SizeInIeee = SizeInBytes;
    while ((SizeInIeee > 1024) && (Index < (NumPrefixes - 1))) {
        Index++;
        SizeInIeee /= 1024;
    } // while
    Units[1] = Prefixes[Index];
    TheValue = PoolPrint (L"%ld%s", SizeInIeee, Index ? Units : L"-byte");
    return TheValue;
} // CHAR16 *SizeInIEEEUnits()

// Return a name for the volume. Ideally this should be the label for the
// filesystem or partition, but this function falls back to describing the
// filesystem by size (200 MiB, etc.) and/or type (ext2, HFS+, etc.), if
// this information can be extracted.
// The calling function is responsible for freeing the memory allocated
// for the name string.
CHAR16 * GetVolumeName (
    IN REFIT_VOLUME *Volume
) {
    CHAR16                *SISize;
    CHAR16                *TypeName;
    CHAR16                *FoundName         = NULL;
    EFI_FILE_SYSTEM_INFO  *FileSystemInfoPtr = NULL;


    if ((GetPoolStr (&Volume->FsName) != NULL) &&
        (GetPoolStr (&Volume->FsName)[0] != L'\0') &&
        (StrLen (GetPoolStr (&Volume->FsName)) > 0)
    ) {
        FoundName = StrDuplicate (GetPoolStr (&Volume->FsName));

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Set Name to Filesystem Name:- '%s'",
            FoundName
        );
        #endif
    }

    // Try to use the partition name if no filesystem name
    if ((FoundName == NULL) &&
        (GetPoolStr (&Volume->PartName)) &&
        (StrLen (GetPoolStr (&Volume->PartName)) > 0) &&
        !IsIn (GetPoolStr (&Volume->PartName), IGNORE_PARTITION_NAMES)
    ) {
        FoundName = StrDuplicate (GetPoolStr (&Volume->PartName));

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Set Name to Partition Name:- '%s'",
            FoundName
        );
        #endif
    }

    // NOTE: Do not free TypeName as FSTypeName returns a constant
    TypeName = FSTypeName (Volume->FSType);

    // No filesystem or acceptable partition name ... use fs type and size
    if (FoundName == NULL) {
        if (Volume->RootDir != NULL) {
            FileSystemInfoPtr = LibFileSystemInfo (Volume->RootDir);
        }

        if (FileSystemInfoPtr != NULL) {
            SISize    = SizeInIEEEUnits (FileSystemInfoPtr->VolumeSize);
            FoundName = PoolPrint (L"%s %s Volume", SISize, TypeName);

            #if REFIT_DEBUG > 0
            LOG(3, LOG_LINE_NORMAL,
                L"Set Name to Filesystem Description:- '%s'",
                FoundName
            );
            #endif

            MY_FREE_POOL(SISize);
            MY_FREE_POOL(FileSystemInfoPtr);
        }
    }

    if (FoundName == NULL) {
        if (Volume->DiskKind == DISK_KIND_OPTICAL) {
            FoundName = StrDuplicate (L"Optical Disc Drive");
        }
        else if (MediaCheck) {
            FoundName = StrDuplicate (L"Network Volume (Assumed)");
        }
        else {
            if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidMacRaidOn)) {
                FoundName = StrDuplicate (L"Apple Raid Partition (Online)");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidMacRaidOff)) {
                FoundName = StrDuplicate (L"Apple Raid Partition (Offline)");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidRecoveryHD)) {
                FoundName = StrDuplicate (L"Recovery HD");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidAppleTvRec)) {
                FoundName = StrDuplicate (L"AppleTV Recovery Partition");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidCoreStorage)) {
                FoundName = StrDuplicate (L"Fusion/FileVault Container");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidAPFS)) {
                FoundName = StrDuplicate (L"APFS/FileVault Container");
            }
            else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidHFS)) {
                FoundName = StrDuplicate (L"Unidentified HFS+ Volume");
            }
            else {
                if (MyStriCmp (TypeName, L"APFS")) {
                    FoundName = StrDuplicate (L"APFS Volume (Assumed)");
                }
                else {
                    FoundName = PoolPrint (L"%s Volume", TypeName);
                }
            }
        }

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Set Name to Generic Description:- '%s'",
            FoundName
        );
        #endif
    } // if FoundName == NULL

    // TODO: Above could be improved/extended, in case filesystem name is not found,
    //       such as:
    //         - use or add disk/partition number (e.g., "(hd0,2)")

    return FoundName;
} // static CHAR16 *GetVolumeName()

// Determine the unique GUID, type code GUID, and name of the volume and store them.
static
VOID SetPartGuidAndName (
    IN OUT REFIT_VOLUME             *Volume,
    IN OUT EFI_DEVICE_PATH_PROTOCOL *DevicePath
) {
    //LOGPROCENTRY("%p(#%d)", Volume, Volume ? Volume->ReferenceCount : -1);
    HARDDRIVE_DEVICE_PATH    *HdDevicePath;
    GPT_ENTRY                *PartInfo;

    if (Volume && DevicePath &&
        (DevicePath->Type == MEDIA_DEVICE_PATH) &&
        (DevicePath->SubType == MEDIA_HARDDRIVE_DP)
    ) {
        HdDevicePath = (HARDDRIVE_DEVICE_PATH*) DevicePath;
        if (HdDevicePath->SignatureType == SIGNATURE_TYPE_GUID) {
            Volume->PartGuid = *((EFI_GUID*) HdDevicePath->Signature);
            PartInfo = FindPartWithGuid (&(Volume->PartGuid));

            if (PartInfo) {
                CopyPoolStr (&Volume->PartName, PartInfo->name);
                CopyMem (&(Volume->PartTypeGuid), PartInfo->type_guid, sizeof (EFI_GUID));

                if (GuidsAreEqual (&(Volume->PartTypeGuid), &gFreedesktopRootGuid) &&
                    ((PartInfo->attributes & GPT_NO_AUTOMOUNT) == 0)
                ) {
                    AssignVolume (&GlobalConfig.DiscoveredRoot, Volume);
                }

                Volume->IsMarkedReadOnly = ((PartInfo->attributes & GPT_READ_ONLY) > 0);

                MY_FREE_POOL(PartInfo);
            }
        }
        else {
            // TODO: Better to assign a random GUID to MBR partitions, could not
            // find an EFI function to do this. The GUID below is just one that I
            // generated in Linux.
            Volume->PartGuid = StringAsGuid (L"92a6c61f-7130-49b9-b05c-8d7e7b039127");
        }
    } // if DevicePath->Type
    //LOGPROCEXIT("%p(#%d)", Volume, Volume ? Volume->ReferenceCount : -1);
} // VOID SetPartGuidAndName()

// Return TRUE if NTFS boot files are found or if Volume is unreadable,
// FALSE otherwise. The idea is to weed out non-boot NTFS volumes from
// BIOS/legacy boot list on Macs. We cannot assume NTFS will be readable,
// so return TRUE if it is unreadable; but if it *IS* readable, return
// TRUE only if Windows boot files are found.
BOOLEAN HasWindowsBiosBootFiles (
    IN REFIT_VOLUME *Volume
) {
    BOOLEAN FilesFound = TRUE;

    if (Volume->RootDir != NULL) {
        // NTLDR   = Windows boot file: NT/200x/XP
        // bootmgr = Windows boot file: Vista/7/8/10
        FilesFound = (
            FileExists (Volume->RootDir, L"NTLDR") ||
            FileExists (Volume->RootDir, L"bootmgr")
        );
    }

    return FilesFound;
} // static VOID HasWindowsBiosBootFiles()

VOID ScanVolume (
    IN OUT REFIT_VOLUME *Volume
) {
    LOGPROCENTRY();
    EFI_STATUS        Status;
    EFI_DEVICE_PATH  *DevicePath;
    EFI_DEVICE_PATH  *NextDevicePath;
    EFI_DEVICE_PATH  *DiskDevicePath;
    EFI_DEVICE_PATH  *RemainingDevicePath;
    EFI_HANDLE        WholeDiskHandle;
    UINTN             PartialLength;
    BOOLEAN           Bootable;

    #if REFIT_DEBUG > 0
    UINTN   LogLineType;
    CHAR16 *StrSpacer = NULL;
    #endif

    BOOLEAN HybridLogger = FALSE;
    if (NativeLogger) {
        HybridLogger = TRUE;
        MsgLog("NativeLogger = FALSE\n");
        NativeLogger = FALSE;
    }

    //MsgLog ("DuplicateDevicePath\n");
    // get device path
    Volume->DevicePath = DuplicateDevicePath (
        DevicePathFromHandle (Volume->DeviceHandle)
    );

    //MsgLog ("DumpHex\n");
    if (Volume->DevicePath != NULL) {
        #if REFIT_DEBUG >= 2
        DumpHex (1, 0, DevicePathSize (Volume->DevicePath), Volume->DevicePath);
        #endif
    }

    Volume->DiskKind = DISK_KIND_INTERNAL;  // default

    //MsgLog ("HandleProtocol DeviceHandle BlockIoProtocol BlockIO\n");
    // get block i/o
    Status = REFIT_CALL_3_WRAPPER(
        gBS->HandleProtocol,
        Volume->DeviceHandle,
        &BlockIoProtocol,
        (VOID **) &(Volume->BlockIO)
    );

    if (EFI_ERROR(Status)) {
        Volume->BlockIO = NULL;

        #if REFIT_DEBUG > 0
        if (HybridLogger) {
            LogLineType = LOG_LINE_SPECIAL;
        }
        else {
            LogLineType = LOG_LINE_NORMAL;
        }
        LOG(3, LogLineType, L"Cannot Get BlockIO Protocol in ScanVolume!!");
        #endif
    }
    else if (Volume->BlockIO->Media->BlockSize == 2048) {
        Volume->DiskKind = DISK_KIND_OPTICAL;
    }

    // detect device type
    DevicePath = Volume->DevicePath;
    //LOGBLOCKENTRY("DevicePaths");
    while (DevicePath != NULL && !IsDevicePathEndType (DevicePath)) {
        //LOGBLOCKENTRY("DevicePath");
        NextDevicePath = NextDevicePathNode (DevicePath);

        if (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH) {
            //MsgLog ("SetPartGuidAndName\n");
            SetPartGuidAndName (Volume, DevicePath);
        }

        if (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH &&
            (
                DevicePathSubType (DevicePath) == MSG_USB_DP       ||
                DevicePathSubType (DevicePath) == MSG_1394_DP      ||
                DevicePathSubType (DevicePath) == MSG_USB_CLASS_DP ||
                DevicePathSubType (DevicePath) == MSG_FIBRECHANNEL_DP
            )
        ) {
            // USB/FireWire/FC device -> external
            Volume->DiskKind = DISK_KIND_EXTERNAL;
        }

        if (DevicePathType (DevicePath) == MEDIA_DEVICE_PATH &&
            DevicePathSubType (DevicePath) == MEDIA_CDROM_DP
        ) {
            // El Torito entry -> optical disk
            Volume->DiskKind = DISK_KIND_OPTICAL;
            Bootable = TRUE;
        }

        if (DevicePathType (DevicePath) == MESSAGING_DEVICE_PATH) {
            // make a device path for the whole device
            PartialLength  = (UINT8 *) NextDevicePath - (UINT8 *) (Volume->DevicePath);
            DiskDevicePath = (EFI_DEVICE_PATH *) AllocatePool (
                PartialLength + sizeof (EFI_DEVICE_PATH)
            );

            //MsgLog ("CopyMem1\n");
            CopyMem (
                DiskDevicePath,
                Volume->DevicePath,
                PartialLength
            );

            //MsgLog ("CopyMem2\n");
            CopyMem (
                (UINT8 *) DiskDevicePath + PartialLength,
                EndDevicePath,
                sizeof (EFI_DEVICE_PATH)
            );

            //MsgLog ("LocateDevicePath\n");
            // get the handle for that path
            RemainingDevicePath = DiskDevicePath;
            Status = REFIT_CALL_3_WRAPPER(
                gBS->LocateDevicePath,
                &BlockIoProtocol,
                &RemainingDevicePath,
                &WholeDiskHandle
            );

            //MsgLog ("MY_FREE_POOL DiskDevicePath\n");
            MY_FREE_POOL(DiskDevicePath);

            if (EFI_ERROR(Status)) {
                #if REFIT_DEBUG > 0
                if (DoneHeadings) {
                    if (HybridLogger) {
                        if (SkipSpacing) {
                            StrSpacer   = L" ... ";
                            LogLineType = LOG_LINE_SAME;
                        }
                        else {
                            StrSpacer   = L"";
                            LogLineType = LOG_LINE_SPECIAL;
                        }
                    }
                    else {
                        StrSpacer   = L"";
                        LogLineType = LOG_LINE_NORMAL;
                    }
                    LOG(4, LogLineType, L"%sCould Not Locate Device Path", StrSpacer);
                    SkipSpacing = (GlobalConfig.LogLevel > 3) ? TRUE : FALSE;
                }
                #endif
            }
            else {
                //MsgLog ("HandleProtocol WholeDiskHandle DevicePathProtocol DiskDevicePath\n");
                // get the device path for later
                Status = REFIT_CALL_3_WRAPPER(
                    gBS->HandleProtocol,
                    WholeDiskHandle,
                    &DevicePathProtocol,
                    (VOID **) &DiskDevicePath
                );

                if (EFI_ERROR(Status)) {
                    #if REFIT_DEBUG > 0
                    if (DoneHeadings) {
                        if (HybridLogger) {
                            if (SkipSpacing) {
                                StrSpacer   = L" ... ";
                                LogLineType = LOG_LINE_SAME;
                            }
                            else {
                                StrSpacer   = L"";
                                LogLineType = LOG_LINE_SPECIAL;
                            }
                        }
                        else {
                            StrSpacer   = L"";
                            LogLineType = LOG_LINE_NORMAL;
                        }
                        LOG(4, LogLineType, L"%sCould Not Get DiskDevicePath", StrSpacer);
                        SkipSpacing = (GlobalConfig.LogLevel > 3) ? TRUE : FALSE;
                    }
                    #endif
                }
                else {
                    //MsgLog ("WholeDiskDevicePath = DuplicateDevicePath\n");
                    Volume->WholeDiskDevicePath = DuplicateDevicePath (DiskDevicePath);
                }

                //MsgLog ("HandleProtocol WholeDiskHandle BlockIoProtocol WholeDiskBlockIO\n");
                // look at the BlockIO protocol
                Status = REFIT_CALL_3_WRAPPER(
                    gBS->HandleProtocol,
                    WholeDiskHandle,
                    &BlockIoProtocol,
                    (VOID **) &Volume->WholeDiskBlockIO
                );
                if (EFI_ERROR(Status)) {
                    Volume->WholeDiskBlockIO = NULL;

                    #if REFIT_DEBUG > 0
                    if (DoneHeadings) {
                        if (HybridLogger) {
                            if (SkipSpacing) {
                                StrSpacer   = L" ... ";
                                LogLineType = LOG_LINE_SAME;
                            }
                            else {
                                StrSpacer   = L"";
                                LogLineType = LOG_LINE_SPECIAL;
                            }
                        }
                        else {
                            StrSpacer   = L"";
                            LogLineType = LOG_LINE_NORMAL;
                        }
                        LOG(4, LogLineType, L"%sCould Not Get WholeDiskBlockIO", StrSpacer);
                        SkipSpacing = (GlobalConfig.LogLevel > 3) ? TRUE : FALSE;
                    }
                    #endif
                }
                else {
                    // check the media block size
                    if (Volume->WholeDiskBlockIO->Media->BlockSize == 2048) {
                        Volume->DiskKind = DISK_KIND_OPTICAL;
                    }
                }
            } // if/else EFI_ERROR(Status)
        } // if DevicePathType
        DevicePath = NextDevicePath;
        //LOGBLOCKEXIT("DevicePath");
    } // while
    //LOGBLOCKEXIT("DevicePaths");

    // scan for bootcode and MBR table
    Bootable = FALSE;
    ScanVolumeBootcode (Volume, &Bootable);
    if (Volume->DiskKind == DISK_KIND_OPTICAL) {
        Bootable = TRUE;
    }

    if (!Bootable) {
        if (Volume->HasBootCode) {
            #if REFIT_DEBUG > 0
            CHAR16 *MsgStr = L"Considered Non-Bootable but Boot Code is Present";
            if (DoneHeadings) {
                if (HybridLogger) {
                    if (SkipSpacing) {
                        StrSpacer   = L" ... ";
                        LogLineType = LOG_LINE_SAME;
                    }
                    else {
                        StrSpacer   = L"";
                        LogLineType = LOG_LINE_SPECIAL;
                    }
                }
                else {
                    StrSpacer   = L"";
                    LogLineType = LOG_LINE_NORMAL;
                }
                LOG(4, LogLineType, L"%s%s!!", StrSpacer, MsgStr);
            }
            MsgLog ("\n");
            MsgLog ("** WARN: %s", MsgStr);
            SkipSpacing = (GlobalConfig.LogLevel > 3) ? TRUE : FALSE;
            #endif

            Volume->HasBootCode = FALSE;
        }
    }

    if (HybridLogger) {
        NativeLogger = TRUE;
        MsgLog("NativeLogger = TRUE\n");
    }

    //MsgLog ("Volume->RootDir = LibOpenRoot Volume->DeviceHandle:%p\n", Volume->DeviceHandle);
    // open the root directory of the volume
    Volume->RootDir = LibOpenRoot (Volume->DeviceHandle);

    //MsgLog ("SetFilesystemName\n");
    SetFilesystemName (Volume);
    //MsgLog ("GetVolumeName\n");
    AssignPoolStr (&Volume->VolName, GetVolumeName (Volume));
    SanitiseVolumeName (Volume);

    if (Volume->RootDir == NULL) {
        Volume->IsReadable = FALSE;
    }
    else {
        Volume->IsReadable = TRUE;
        if (GlobalConfig.LegacyType == LEGACY_TYPE_MAC
            && Volume->FSType == FS_TYPE_NTFS
            && Volume->HasBootCode
        ) {
            // VBR boot code found on NTFS, but volume is not actually bootable
            // on Mac unless there are actual boot file, so check for them.
            //MsgLog ("HasWindowsBiosBootFiles");
            Volume->HasBootCode = HasWindowsBiosBootFiles (Volume);
        }
    }
    LOGPROCEXIT();
} // ScanVolume()

static
VOID ScanExtendedPartition (
    IN OUT REFIT_VOLUME       *WholeDiskVolume,
    IN     MBR_PARTITION_INFO *MbrEntry
) {
    EFI_STATUS          Status;
    UINT32              ExtBase;
    UINT32              ExtCurrent;
    UINT32              NextExtCurrent;
    UINTN               i, LogicalPartitionIndex = 4;
    UINT8               SectorBuffer[512];
    BOOLEAN             Bootable;
    REFIT_VOLUME       *Volume = NULL;
    MBR_PARTITION_INFO *EMbrTable;

    ExtBase = MbrEntry->StartLBA;

    for (ExtCurrent = ExtBase; ExtCurrent; ExtCurrent = NextExtCurrent) {
        // read current EMBR
        Status = REFIT_CALL_5_WRAPPER(
            WholeDiskVolume->BlockIO->ReadBlocks,
            WholeDiskVolume->BlockIO,
            WholeDiskVolume->BlockIO->Media->MediaId,
            ExtCurrent,
            512,
            SectorBuffer
        );

        if (EFI_ERROR(Status)) {
            #if REFIT_DEBUG > 0
            LOG(3, LOG_BLANK_LINE_SEP, L"X");
            LOG(3, LOG_LINE_NORMAL, L"Error %d Reading Blocks from Disk", Status);
            #endif

            break;
        }

        if (*((UINT16 *) (SectorBuffer + 510)) != 0xaa55) {
            break;
        }

        EMbrTable = (MBR_PARTITION_INFO *) (SectorBuffer + 446);

        // scan logical partitions in this EMBR
        NextExtCurrent = 0;
        for (i = 0; i < 4; i++) {
            if ((EMbrTable[i].Flags != 0x00 && EMbrTable[i].Flags != 0x80) ||
                EMbrTable[i].StartLBA == 0 || EMbrTable[i].Size == 0
            ) {
                break;
            }

            if (IS_EXTENDED_PART_TYPE(EMbrTable[i].Type)) {
                // set next ExtCurrent
                NextExtCurrent = ExtBase + EMbrTable[i].StartLBA;
                break;
            }
            else if (AllocateVolume (&Volume)) {
                // found a logical partition
                Volume->DiskKind          = WholeDiskVolume->DiskKind;
                Volume->IsMbrPartition    = TRUE;
                Volume->MbrPartitionIndex = LogicalPartitionIndex++;

                AssignPoolStr (&Volume->VolName, PoolPrint (
                    L"Partition %d",
                    Volume->MbrPartitionIndex + 1
                ));

                Volume->BlockIO          = WholeDiskVolume->BlockIO;
                Volume->BlockIOOffset    = ExtCurrent + EMbrTable[i].StartLBA;
                Volume->WholeDiskBlockIO = WholeDiskVolume->BlockIO;

                ScanMBR  = TRUE;
                Bootable = FALSE;
                ScanVolumeBootcode (Volume, &Bootable);
                if (!Bootable) {
                    Volume->HasBootCode = FALSE;
                }
                ScanMBR = FALSE;

                SetVolumeBadgeIcon (Volume);
                AddToVolumeList (&Volumes, &VolumesCount, Volume);

                // since Volume started as NULL, free it if it is not NULL
                FreeVolume (&Volume);
            }
            else {
                MsgLog ("** ERROR: Volume is NULL!!!");
            }
        } // for i
    } // for ExtCurrent = ExtBase
} // VOID ScanExtendedPartition()

// Ensure SyncAPFS can be used.
static
VOID VetSyncAPFS (VOID) {
    #if REFIT_DEBUG > 0
    CHAR16 *MsgStr = NULL;
    #endif

    if (PreBootVolumesCount == 0) {
        // Disable SyncAPFS if we do not have, or could not identify, any PreBoot volume
        GlobalConfig.SyncAPFS = FALSE;

        #if REFIT_DEBUG > 0
        LOG(3, LOG_BLANK_LINE_SEP, L"X");
        LOG2(1, LOG_STAR_SEPARATOR, L"INFO: ", L"\n\n", L"Could Not Positively Identify APFS Partition Types ... Disabling SyncAFPS");
        #endif
    }
    else {
        UINTN   i, j;
        CHAR16 *CheckName = NULL;
        CHAR16 *TweakName = NULL;

        #if REFIT_DEBUG > 0
        LOG2(1, LOG_LINE_THIN_SEP, L"\n", L":\n", L"ReMap APFS Volumes");
        for (i = 0; i < SystemVolumesCount; i++) {
            LOG2(3, LOG_LINE_NORMAL, L"  - ", L"\n", L"System Volume:- '%s'", GetPoolStr (&SystemVolumes[i]->VolName));
        } // for
        #endif

        // Filter '- Data' string tag out of Volume Group name if present
        for (i = 0; i < DataVolumesCount; i++) {
            if (MyStrStr (GetPoolStr (&DataVolumes[i]->VolName), L"- Data") != NULL) {
                for (j = 0; j < SystemVolumesCount; j++) {
                    MY_FREE_POOL(TweakName);
                    TweakName = SanitiseString (GetPoolStr (&SystemVolumes[j]->VolName));

                    MY_FREE_POOL(CheckName);
                    CheckName = PoolPrint (L"%s - Data", TweakName);

                    if (MyStriCmp (GetPoolStr (&DataVolumes[i]->VolName), CheckName)) {
                        CopyFromPoolStr (&DataVolumes[i]->VolName, &SystemVolumes[j]->VolName);

                        break;
                    }

                    // Check against raw name string if apporpriate
                    if (!MyStriCmp (GetPoolStr (&SystemVolumes[j]->VolName), TweakName)) {
                        MY_FREE_POOL(CheckName);
                        CheckName = PoolPrint (L"%s - Data", GetPoolStr (&SystemVolumes[j]->VolName));

                        if (MyStriCmp (GetPoolStr (&DataVolumes[i]->VolName), CheckName)) {
                            CopyFromPoolStr (&DataVolumes[i]->VolName, &SystemVolumes[j]->VolName);

                            break;
                        }
                    }
                } // for j = 0

                MY_FREE_POOL(TweakName);
                MY_FREE_POOL(CheckName);
            }
        } // for i = 0

        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"ReMapped %d APFS Volume%s",
            SystemVolumesCount, (SystemVolumesCount == 1) ? L"" : L"s"
        );
        LOG(3, LOG_LINE_NORMAL, L"%s", MsgStr);

        if (SystemVolumesCount == 0) {
            MsgLog ("\n...................") ;
        }
        else {
            MsgLog ("\n\n");
            MsgLog ("INFO: ");
        }
        MsgLog ("%s", MsgStr);
        MsgLog ("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif
    } // if/else PreBootVolumesCount == 0
} // VOID VetSyncAPFS()

static
CHAR16 * GetApfsRoleString (
    IN APPLE_APFS_VOLUME_ROLE VolumeRole
) {
    CHAR16 *retval = NULL;

    switch (VolumeRole) {
        case APPLE_APFS_VOLUME_ROLE_UNDEFINED: retval = L"0x00 - Undefined";  break;
        case APPLE_APFS_VOLUME_ROLE_SYSTEM:    retval = L"0x01 - System";     break;
        case APPLE_APFS_VOLUME_ROLE_RECOVERY:  retval = L"0x04 - Recovery";   break;
        case APPLE_APFS_VOLUME_ROLE_VM:        retval = L"0x08 - VM";         break;
        case APPLE_APFS_VOLUME_ROLE_PREBOOT:   retval = L"0x10 - PreBoot";    break;
        case APPLE_APFS_VOLUME_ROLE_DATA:      retval = L"0x40 - Data";       break;
        case APPLE_APFS_VOLUME_ROLE_UPDATE:    retval = L"0xC0 - Snapshot";   break;
        default:                               retval = L"0xFF - Unknown";    break;
    } // switch

    return retval;
} // static CHAR16 * GetApfsRoleString()

VOID ScanVolumes (VOID) {
    EFI_STATUS          Status;
    EFI_HANDLE         *Handles = NULL;
    REFIT_VOLUME       *Volume = NULL;
    REFIT_VOLUME       *WholeDiskVolume = NULL;
    MBR_PARTITION_INFO *MbrTable;
    UINTN               HandleCount = 0;
    UINTN               HandleIndex;
    UINTN               VolumeIndex;
    UINTN               VolumeIndex2;
    UINTN               PartitionIndex;
    UINTN               SectorSum, i;
    UINT8              *SectorBuffer1;
    UINT8              *SectorBuffer2;
    CHAR16             *RoleStr      = NULL;
    CHAR16             *PartType     = NULL;
    BOOLEAN             DupFlag;
    EFI_GUID           *UuidList;
    EFI_GUID            VolumeGuid;
    EFI_GUID            ContainerGuid;
    APPLE_APFS_VOLUME_ROLE VolumeRole;

    LOGPROCENTRY();

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr       = NULL;
    CHAR16  *PartName     = NULL;
    CHAR16  *PartGUID     = NULL;
    CHAR16  *PartTypeGUID = NULL;
    CHAR16  *VolumeUUID   = NULL;

    const CHAR16 *ITEMVOLA = L"PARTITION TYPE GUID";
    const CHAR16 *ITEMVOLB = L"PARTITION GUID";
    const CHAR16 *ITEMVOLC = L"PARTITION TYPE";
    const CHAR16 *ITEMVOLD = L"VOLUME UUID";
    const CHAR16 *ITEMVOLE = L"VOLUME ROLE";
    const CHAR16 *ITEMVOLF = L"VOLUME NAME";

    LOG(1, LOG_LINE_SEPARATOR, L"Scan Readable Volumes");
    #endif

    if (SelfVolRun) {
        // Clear Volume Lists if not Scanning for Self Volume
        FreeVolumes (
            &Volumes,
            &VolumesCount
        );

        FreeVolumes (
            &PreBootVolumes,
            &PreBootVolumesCount
        );

        FreeVolumes (
            &SystemVolumes,
            &SystemVolumesCount
        );

        FreeVolumes (
            &DataVolumes,
            &DataVolumesCount
        );

        ForgetPartitionTables();
    }

    // get all filesystem handles
    Status = LibLocateHandle (
        ByProtocol,
        &BlockIoProtocol,
        NULL,
        &HandleCount,
        &Handles
    );
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        LOG3(2, LOG_THREE_STAR_SEP, L"\n\n** ", L"\n\n", L"!!", L"In ScanVolumes ... '%r' While Listing File Systems (Fatal Error)", Status);
        #endif

        goto Done;
    }

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL,
        L"Found Handles for %d Volumes",
        HandleCount
    );
    #endif

    UuidList = AllocateZeroPool (sizeof (EFI_GUID) * HandleCount);
    if (UuidList == NULL) {
        #if REFIT_DEBUG > 0
        LOG(2, LOG_BLANK_LINE_SEP, L"X");
        LOG3(2, LOG_THREE_STAR_SEP, L"\n\n** WARN: ", L"\n\n", L"!!", L"In ScanVolumes ... '%r' While Allocating 'UuidList'", EFI_BUFFER_TOO_SMALL);
        #endif

        goto Done;
    }

    // first pass: collect information about all handles
    ScannedOnce = FALSE;

    LOGBLOCKENTRY("ScanVolumes first pass [%d]", HandleCount);
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        LOGBLOCKENTRY("Volumes[%d]", HandleIndex);
        #if REFIT_DEBUG > 0
        /* Exception for LOG_THREE_STAR_SEP */
        LOG(3, LOG_THREE_STAR_SEP, L"NEXT VOLUME");
        #endif

        AllocateVolume (&Volume);
        if (Volume == NULL) {
            #if REFIT_DEBUG > 0
            LOG(2, LOG_BLANK_LINE_SEP, L"X");
            LOG3(1, LOG_THREE_STAR_SEP, L"\n\n** WARN: ", L"\n\n", L"!!", L"In ScanVolumes ... '%r' While Allocating 'Volumes'", EFI_BUFFER_TOO_SMALL);
            #endif

            goto Done;
        }

        Volume->DeviceHandle = Handles[HandleIndex];
        AddPartitionTable (Volume);
        ScanVolume (Volume);

        UuidList[HandleIndex] = Volume->VolUuid;
        // Deduplicate filesystem UUID so that we do not add duplicate entries for file systems
        // that are part of RAID mirrors. Do not deduplicate ESP partitions though, since unlike
        // normal file systems they are likely to all share the same volume UUID, and it is also
        // unlikely that they are part of software RAID mirrors.
        for (i = 0; i < HandleIndex; i++) {
            if (GlobalConfig.ScanAllESP) {
                DupFlag = (
                    (!GuidsAreEqual (&(Volume->PartTypeGuid), &GuidESP)) &&
                    (CompareMem (&(Volume->VolUuid), &(UuidList[i]), sizeof (EFI_GUID)) == 0) &&
                    (CompareMem (&(Volume->VolUuid), &GuidNull,      sizeof (EFI_GUID)) != 0)
                );
            }
            else {
                DupFlag = (
                    (CompareMem (&(Volume->VolUuid), &(UuidList[i]), sizeof (EFI_GUID)) == 0) &&
                    (CompareMem (&(Volume->VolUuid), &GuidNull,      sizeof (EFI_GUID)) != 0)
                );
            }

            if (DupFlag) {
                // This is a duplicate filesystem item
                Volume->IsReadable = FALSE;
            }
        } // for

        if (SelfVolRun) {
            // Update/Create Volumes List if not Scanning for Self Volume
            AddToVolumeList (&Volumes, &VolumesCount, Volume);
        }

        if (Volume->DeviceHandle == SelfLoadedImage->DeviceHandle) {
            SelfVolSet = TRUE;
            LOGBLOCKENTRY("SelfVolume at %d of %d", HandleIndex, HandleCount);
            AssignVolume (&SelfVolume, Volume);
            LOGBLOCKEXIT("SelfVolume");
        }

        if (SelfVolRun) {
            #if REFIT_DEBUG > 0
            if (SkipSpacing) {
                MsgLog (" on Volume Below");
            }

            else if (ScannedOnce) {
                if (!SkipSpacing && (HandleIndex % 4) == 0 && (HandleCount - HandleIndex) > 2) {
                    if (!SkipSpacing && (HandleIndex % 28) == 0 && (HandleCount - HandleIndex) > 14) {
                        DoneHeadings = FALSE;
                    }
                    else {
                        MsgLog ("\n\n");
                    }
                }
                else {
                    MsgLog ("\n");
                }
            }
            SkipSpacing = FALSE;

            // 'FSTypeName' returns a constant ... Do not free 'PartType'!
            PartType = FSTypeName (Volume->FSType);

            // Improve Volume Id
            if (MyStrStrIns (PartType, L"Unknown")) {
                     if (MyStriCmp   (GetPoolStr (&Volume->VolName), L"Microsoft Reserved Partition")) PartType = L"NTFS (Assumed)";
                else if (MyStriCmp   (GetPoolStr (&Volume->VolName), L"Basic Data Partition"        )) PartType = L"NTFS (Assumed)";
                else if (MyStrStrIns (GetPoolStr (&Volume->VolName), L"/FileVault Container"        )) PartType = L"APFS (Assumed)";

                // Split checks as '/FileVault' may be Core Storage
                     if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidAPFS)       ) PartType = L"APFS";
                else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidHFS)        ) PartType = L"HFS+";
                else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidMacRaidOn)  ) PartType = L"Mac Raid (ON)";
                else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidMacRaidOff) ) PartType = L"Mac Raid (OFF)";
                else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidAppleTvRec) ) PartType = L"AppleTV Recov";
                else if (GuidsAreEqual (&(Volume->PartTypeGuid), &GuidCoreStorage)) PartType = L"Core Storage";
            }

            if (!DoneHeadings) {
                MsgLog (
                    "%-41s%-41s%-20s%-41s%-22s%s\n",
                    ITEMVOLA, ITEMVOLB, ITEMVOLC, ITEMVOLD, ITEMVOLE, ITEMVOLF
                );
                DoneHeadings = TRUE;

                if (FirstVolume) {
                    if (Volume->HasBootCode) {
                        MsgLog ("Found Legacy Boot Code");
                    }
                    if (FoundMBR) {
                        if (Volume->HasBootCode) {
                            MsgLog (" ... ");
                        }
                        MsgLog ("Found MBR Partition Table");
                    }
                    if (FoundMBR || Volume->HasBootCode) {
                        MsgLog (" on Volume Below\n");
                    }
                }
            }
            #endif

            RoleStr = L"";
            VolumeRole = 0;
                 if (MyStriCmp   (GetPoolStr (&Volume->VolName), L"EFI")                         ) RoleStr = L" * ESP";
            else if (MyStriCmp   (GetPoolStr (&Volume->VolName), L"Recovery HD")                 ) RoleStr = L" * Mac Recovery";
            else if (MyStriCmp   (GetPoolStr (&Volume->VolName), L"Basic Data Partition")        ) RoleStr = L" * Windows Data";
            else if (MyStriCmp   (GetPoolStr (&Volume->VolName), L"Microsoft Reserved Partition")) RoleStr = L" * Windows System";
            else if (MyStrStrIns (GetPoolStr (&Volume->VolName), L"System Reserved")             ) RoleStr = L" * Windows System";
            else if (MyStrStrIns (GetPoolStr (&Volume->VolName), L"/FileVault Container")        ) RoleStr = L"0xEE - Not Set";
            else {
                #ifdef __MAKEWITH_GNUEFI
                Status = EFI_NOT_FOUND;
                #else
                // DA-TAG: Limit to TianoCore
                Status = RP_GetApfsVolumeInfo (
                    Volume->DeviceHandle,
                    &ContainerGuid,
                    &VolumeGuid,
                    &VolumeRole
                );
                #endif

                if (!EFI_ERROR(Status)) {
                    PartType        = L"APFS";
                    Volume->FSType  = FS_TYPE_APFS;
                    Volume->VolUuid = VolumeGuid;
                    RoleStr         = GetApfsRoleString (VolumeRole);

                    if (VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT) {
                        // Create or add to a list of APFS PreBoot Volumes
                        AddToVolumeList (&PreBootVolumes, &PreBootVolumesCount, Volume);
                    }
                    else if (VolumeRole == APPLE_APFS_VOLUME_ROLE_DATA) {
                        // Create or add to a list representing APFS VolumeGroups
                        AddToVolumeList (&DataVolumes, &DataVolumesCount, Volume);
                    }
                    else if (VolumeRole == APPLE_APFS_VOLUME_ROLE_SYSTEM
                        || VolumeRole == APPLE_APFS_VOLUME_ROLE_UNDEFINED
                    ) {
                        // Create or add to a list of APFS System Volumes
                        AddToVolumeList (&SystemVolumes, &SystemVolumesCount, Volume);
                    }
                }
            }

            #if REFIT_DEBUG > 0
            // Allocate Pools for Log Details
            PartName     = StrDuplicate (PartType);
            PartGUID     = GuidAsString (&(Volume->PartGuid));
            PartTypeGUID = GuidAsString (&(Volume->PartTypeGuid));
            VolumeUUID   = GuidAsString (&(Volume->VolUuid));

            // Control PartName Length
            LimitStringLength (PartName, 15);

            MsgStr = PoolPrint (
                L"%-36s  :  %-36s  :  %-15s  :  %-36s  :  %-17s  :  %s",
                PartTypeGUID, PartGUID, PartType,
                VolumeUUID, RoleStr, GetPoolStr (&Volume->VolName)
            );

            LOG2(3, LOG_LINE_NORMAL, L"", L"\n", L"%s", MsgStr);
            MY_FREE_POOL(MsgStr);
            
            
            MY_FREE_POOL(PartName);
            MY_FREE_POOL(PartGUID);
            MY_FREE_POOL(PartTypeGUID);
            MY_FREE_POOL(VolumeUUID);
            #endif
        }

        #if REFIT_DEBUG > 0
        FoundMBR      = FALSE;
        FirstVolume   = FALSE;
        #endif

        ScannedOnce = TRUE;

        // since Volume started as NULL, free it if it is not NULL
        FreeVolume (&Volume);

        LOGBLOCKEXIT("Volumes[%d]", HandleIndex);
    } // for: first pass
    LOGBLOCKEXIT("ScanVolumes first pass");

    MY_FREE_POOL(UuidList);
    MY_FREE_POOL(Handles);

    if (!SelfVolSet || !SelfVolRun) {
        #if REFIT_DEBUG > 0
        if (!SelfVolSet) {
            LOG2(2, LOG_STAR_HEAD_SEP, L"** WARN: ", L"\n\n", L"Could Not Set Self Volume!!");
        }
        else {
            CHAR16 *SelfUUID = GuidAsString (&SelfVolume->VolUuid);
            CHAR16 *SelfGUID = GuidAsString (&SelfVolume->PartGuid);
            MsgLog ("INFO: Self Volume:- '%s ::: %s ::: %s'\n", GetPoolStr (&SelfVolume->VolName), SelfGUID, SelfUUID);
            MsgLog ("      Install Dir:- '%s'\n\n", SelfDirPath ? SelfDirPath : L"Not Set");
            MY_FREE_POOL(SelfGUID);
            MY_FREE_POOL(SelfUUID);
        }
        #endif

        SelfVolRun = TRUE;

        goto Done;
    }
    else {
        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"%-41s%-41s%-20s%-41s%-22s%s",
            ITEMVOLA, ITEMVOLB, ITEMVOLC, ITEMVOLD, ITEMVOLE, ITEMVOLF
        );
        LOG(3, LOG_LINE_NORMAL, L"%s", MsgStr);
        MsgLog ("\n...................");
        MsgLog ("%s", MsgStr);
        MsgLog ("\n\n");
        MY_FREE_POOL(MsgStr);
        #endif
    }

    LOGBLOCKENTRY("ScanVolumes second pass");
    // second pass: relate partitions and whole disk devices
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        LOGBLOCKENTRY("Volumes[%d]", VolumeIndex);
        AssignVolume (&Volume, Volumes[VolumeIndex]);
        // check MBR partition table for extended partitions
        if (Volume->BlockIO != NULL && Volume->WholeDiskBlockIO != NULL &&
            Volume->BlockIO == Volume->WholeDiskBlockIO && Volume->BlockIOOffset == 0 &&
            Volume->MbrPartitionTable != NULL
        ) {
            //MsgLog ("check MBR partition table for extended partitions\n");
            MbrTable = Volume->MbrPartitionTable;
            for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
                if (IS_EXTENDED_PART_TYPE(MbrTable[PartitionIndex].Type)) {
                    ScanExtendedPartition (Volume, MbrTable + PartitionIndex);
                }
            }
        }

        // search for corresponding whole disk volume entry
        FreeVolume (&WholeDiskVolume);
        if (Volume->BlockIO != NULL && Volume->WholeDiskBlockIO != NULL &&
            Volume->BlockIO != Volume->WholeDiskBlockIO
        ) {
            //MsgLog ("search for corresponding whole disk volume entry\n");
            for (VolumeIndex2 = 0; VolumeIndex2 < VolumesCount; VolumeIndex2++) {
                if (Volumes[VolumeIndex2]->BlockIO == Volume->WholeDiskBlockIO &&
                    Volumes[VolumeIndex2]->BlockIOOffset == 0
                ) {
                    //LOGBLOCKENTRY("WholeDiskVolume");
                    AssignVolume (&WholeDiskVolume, Volumes[VolumeIndex2]);
                    //LOGBLOCKEXIT("WholeDiskVolume");
                }
            }
        }

        if (WholeDiskVolume != NULL &&
            WholeDiskVolume->MbrPartitionTable != NULL
        ) {
            //MsgLog ("check if this volume is one of the partitions in the table\n");
            // check if this volume is one of the partitions in the table
            MbrTable = WholeDiskVolume->MbrPartitionTable;
            SectorBuffer1 = AllocatePool (512);
            SectorBuffer2 = AllocatePool (512);
            for (PartitionIndex = 0; PartitionIndex < 4; PartitionIndex++) {
                // check size
                if ((UINT64)(MbrTable[PartitionIndex].Size) !=
                    Volume->BlockIO->Media->LastBlock + 1
                ) {
                    continue;
                }

                // compare boot sector read through offset vs. directly
                Status = REFIT_CALL_5_WRAPPER(
                    Volume->BlockIO->ReadBlocks,
                    Volume->BlockIO,
                    Volume->BlockIO->Media->MediaId,
                    Volume->BlockIOOffset,
                    512,
                    SectorBuffer1
                );
                if (EFI_ERROR(Status)) {
                    break;
                }
                Status = REFIT_CALL_5_WRAPPER(
                    Volume->WholeDiskBlockIO->ReadBlocks,
                    Volume->WholeDiskBlockIO,
                    Volume->WholeDiskBlockIO->Media->MediaId,
                    MbrTable[PartitionIndex].StartLBA,
                    512,
                    SectorBuffer2
                );
                if (EFI_ERROR(Status)) {
                    break;
                }
                if (CompareMem (SectorBuffer1, SectorBuffer2, 512) != 0) {
                    continue;
                }

                SectorSum = 0;
                for (i = 0; i < 512; i++) {
                    SectorSum += SectorBuffer1[i];
                }
                if (SectorSum < 1000) {
                    continue;
                }

                // TODO: mark entry as non-bootable if it is an extended partition

                // We are now reasonably sure the association is correct...
                Volume->IsMbrPartition = TRUE;
                Volume->MbrPartitionIndex = PartitionIndex;
                if (GetPoolStr (&Volume->VolName) == NULL) {
                    AssignPoolStr (&Volume->VolName, PoolPrint (
                        L"Partition %d",
                        PartitionIndex + 1
                    ));
                }
                break;
            }

            MY_FREE_POOL(SectorBuffer1);
            MY_FREE_POOL(SectorBuffer2);
        }
        LOGBLOCKEXIT("Volumes[%d]", VolumeIndex);
    } // for
    LOGBLOCKEXIT("ScanVolumes second pass");

    #if REFIT_DEBUG > 0
    LOG2(2, LOG_THREE_STAR_SEP, L"INFO: ", L"", L"Processed %d Volume%s", VolumesCount, (VolumesCount == 1) ? L"" : L"s");
    #endif

    if (SelfVolRun && GlobalConfig.SyncAPFS) {
        VetSyncAPFS();
    }

Done:
    // since Volume started as NULL, free it if it is not NULL
    FreeVolume (&Volume);
    // since WholeDiskVolume started as NULL, free it if it is not NULL
    FreeVolume (&WholeDiskVolume);

#if REFIT_DEBUG > 0
    LEAKABLEVOLUMES();
    LEAKABLEPARTITIONS();
#endif

    LOGPROCEXIT();
} // VOID ScanVolumes()

static
VOID GetVolumeBadgeIcons (VOID) {
    LOGPROCENTRY();
    UINTN         i, VolumeIndex;
    BOOLEAN       FoundSysVol = FALSE;
    REFIT_VOLUME *Volume;

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr   = NULL;
    BOOLEAN  LoopOnce = FALSE;

    LOG(1, LOG_LINE_THIN_SEP, L"Set Volume Badges for Internal Volumes");
    #endif

    if (!AllowGraphicsMode) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"Skipped Loading Volume Badges ... Screen is in Text Mode");
        #endif

        LOGPROCEXIT();
        return;
    }

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BADGES) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"Skipped Loading Volume Badges ... Config Setting is Active:- 'HideUI Badges'");
        #endif

        LOGPROCEXIT();
        return;
    }

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];

        if (GlobalConfig.SyncAPFS && Volume->FSType == FS_TYPE_APFS) {
            FoundSysVol = FALSE;
            for (i = 0; i < SystemVolumesCount; i++) {
                if (GuidsAreEqual (&(SystemVolumes[i]->VolUuid), &(Volume->VolUuid))) {
                    FoundSysVol = TRUE;
                    break;
                }
            }

            // Skip APFS system volumes when SyncAPFS is active
            if (FoundSysVol) {
                continue;
            }
        }

        if (Volume->IsReadable) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"Trying to Set VolumeBadge for '%s'",
                GetPoolStr (&Volume->VolName)
            );
            if (LoopOnce) {
                LOG(2, LOG_STAR_HEAD_SEP, L"%s", MsgStr);
            }
            else {
                LOG(4, LOG_THREE_STAR_MID, L"%s", MsgStr);
            }
            MY_FREE_POOL(MsgStr);
            #endif

            // Set volume badge icon
            SetVolumeBadgeIcon (Volume);

            #if REFIT_DEBUG > 0
            if (GetPoolImage (&Volume->VolBadgeImage) == NULL) {
                MsgStr = L"VolumeBadge Not Found";
            }
            else {
                MsgStr = L"VolumeBadge Found";
            }
            LOG(3, LOG_LINE_NORMAL, L"%s", MsgStr);

            LoopOnce = TRUE;
            #endif
        } // if Volume->IsReadable
    } // for
    LOGPROCEXIT();
} // VOID GetVolumeBadgeIcons()

VOID SetVolumeIcons (VOID) {
    LOGPROCENTRY();
    UINTN         i, VolumeIndex;
    BOOLEAN       FoundSysVol = FALSE;
    REFIT_VOLUME *Volume;

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr   = NULL;
    BOOLEAN  LoopOnce = FALSE;
    #endif

    // Set volume badge icon
    GetVolumeBadgeIcons();

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_THIN_SEP, L"Set '.VolumeIcon' Icons for %s Volumes", GlobalConfig.AllHiddenIcons ? L"All" : L"Internal" );
    #endif

    if (!AllowGraphicsMode) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"Skipped Loading .VolumeIcons ... Screen is in Text Mode");
        #endif

        LOGPROCEXIT();
        return;
    }

    if (GlobalConfig.IgnoreHiddenIcons) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"Skipped Loading .VolumeIcons ... Config Setting is Active:- 'ignore_hidden_icons'");
        #endif

        LOGPROCEXIT();
        return;
    }

    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        Volume = Volumes[VolumeIndex];

        if (GlobalConfig.SyncAPFS && Volume->FSType == FS_TYPE_APFS) {
            FoundSysVol = FALSE;
            for (i = 0; i < SystemVolumesCount; i++) {
                if (GuidsAreEqual (&(SystemVolumes[i]->VolUuid), &(Volume->VolUuid))) {
                FoundSysVol = TRUE;
                break;
            }
            }

            // Skip APFS system volumes when SyncAPFS is active
            if (FoundSysVol) {
                continue;
            }
        }

        if (Volume->IsReadable) {
            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"Trying to Set VolumeIcon for '%s'",
                GetPoolStr (&Volume->VolName)
            );

            if (LoopOnce) {
                LOG(2, LOG_STAR_HEAD_SEP, L"%s", MsgStr);
            }
            else {
                LOG(4, LOG_THREE_STAR_MID, L"%s", MsgStr);
            }
            MY_FREE_POOL(MsgStr);
            #endif

            // load custom volume icon for internal disks if present
            if (!GetPoolImage (&Volume->VolIconImage)) {
                if (GlobalConfig.AllHiddenIcons || Volume->DiskKind == DISK_KIND_INTERNAL) {
                    AssignPoolImage (&Volume->VolIconImage, egLoadIconAnyType (
                        Volume->RootDir,
                        L"",
                        L".VolumeIcon",
                        GlobalConfig.IconSizes[ICON_SIZE_BIG]
                    ));
                }
                else {
                    #if REFIT_DEBUG > 0
                    LOG(3, LOG_LINE_NORMAL,
                        L"Skipped '%s' ... Not Internal Volume ... Config Setting is Not Active:- 'all_hidden_icons'",
                        GetPoolStr (&Volume->VolName)
                    );
                    #endif
                }
            }
            else {
                #if REFIT_DEBUG > 0
                LOG(3, LOG_LINE_NORMAL,
                    L"Skipped '%s' ... Icon Already Set",
                    GetPoolStr (&Volume->VolName)
                );
                #endif
            }

            #if REFIT_DEBUG > 0
            LoopOnce = TRUE;
            #endif
        } // if Volume->IsReadable
    } // for
    LOGPROCEXIT();
} // VOID SetVolumeIcons()

//
// file and dir functions
//

BOOLEAN FileExists (
    IN EFI_FILE *BaseDir,
    IN CHAR16   *RelativePath
) {
    EFI_STATUS      Status;
    EFI_FILE_HANDLE TestFile;

    if (BaseDir != NULL) {
        LEAKABLEEXTERNALSTART (kLeakableWhatFileExistsOpen);
        Status = REFIT_CALL_5_WRAPPER(
            BaseDir->Open, BaseDir,
            &TestFile,
            RelativePath,
            EFI_FILE_MODE_READ,
            0
        );
        LEAKABLEEXTERNALSTOP ();
        if (Status == EFI_SUCCESS) {
            REFIT_CALL_1_WRAPPER(TestFile->Close, TestFile);

            return TRUE;
        }
    }

    return FALSE;
}

static
EFI_STATUS DirNextEntry (
    IN  EFI_FILE       *Directory,
    OUT EFI_FILE_INFO **DirEntry,
    IN  UINTN           FilterMode
) {
    VOID       *Buffer;
    UINTN       LastBufferSize;
    UINTN       BufferSize;
    INTN        IterCount;

    EFI_STATUS Status = EFI_BAD_BUFFER_SIZE;
    *DirEntry = NULL;
    for (;;) {
        // read next directory entry
        LastBufferSize = BufferSize = 256;
        Buffer         = AllocatePool (BufferSize);
        if (Buffer == NULL) {
            return EFI_BAD_BUFFER_SIZE;
        }

        for (IterCount = 0; ; IterCount++) {
            LEAKABLEEXTERNALSTART ("DirNextEntry Read");
            Status = REFIT_CALL_3_WRAPPER(
                Directory->Read, Directory,
                &BufferSize,
                Buffer
            );
            LEAKABLEEXTERNALSTOP ();

            if (Status != EFI_BUFFER_TOO_SMALL || IterCount >= 4) {
                break;
            }

            if (BufferSize <= LastBufferSize) {
                #if REFIT_DEBUG > 0
                LOG2(3, LOG_LINE_NORMAL, L"\n", L"", L"FS Driver Requests Bad Buffer Size %d (was %d) ... Using %d Instead",
                    BufferSize,
                    LastBufferSize,
                    LastBufferSize * 2
                );
                #endif

                BufferSize = LastBufferSize * 2;
            }
            else {
                #if REFIT_DEBUG > 0
                if (IterCount > 0) {
                    LOG2(4, LOG_LINE_NORMAL, L"\n", L"\n", L"Reallocating Buffer from %d to %d",
                        LastBufferSize, BufferSize
                    );
                }
                #endif
            }
            Buffer = EfiReallocatePool (
                Buffer, LastBufferSize, BufferSize
            );
            LastBufferSize = BufferSize;
        }

        if (EFI_ERROR(Status)) {
            MY_FREE_POOL(Buffer);
            break;
        }

        // check for end of listing

        if (BufferSize == 0) {
            // end of directory listing
            MY_FREE_POOL(Buffer);
            break;
        }

        // entry is ready to be returned
        *DirEntry = (EFI_FILE_INFO *)Buffer;

        // filter results
        if (FilterMode == 1) {
            // only return directories
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY)) {
                break;
            }
        }
        else if (FilterMode == 2) {
            // only return files
            if (((*DirEntry)->Attribute & EFI_FILE_DIRECTORY) == 0) {
                break;
            }
        }
        else {
            // no filter or unknown filter -> return everything
            break;
        }
        MY_FREE_POOL(*DirEntry);
    } // for ;;

    return Status;
}

VOID DirIterOpen (
    IN  EFI_FILE        *BaseDir,
    IN  CHAR16          *RelativePath OPTIONAL,
    OUT REFIT_DIR_ITER  *DirIter
) {
    if (RelativePath == NULL) {
        DirIter->LastStatus     = EFI_SUCCESS;
        DirIter->DirHandle      = BaseDir;
        DirIter->CloseDirHandle = FALSE;
    }
    else {
        LEAKABLEEXTERNALSTART ("DirIterOpen Open");
        DirIter->LastStatus = REFIT_CALL_5_WRAPPER(
            BaseDir->Open, BaseDir,
            &(DirIter->DirHandle), RelativePath,
            EFI_FILE_MODE_READ, 0
        );
        LEAKABLEEXTERNALSTOP ();
        DirIter->CloseDirHandle = EFI_ERROR(DirIter->LastStatus) ? FALSE : TRUE;
    }
}

#ifndef __MAKEWITH_GNUEFI
EFI_UNICODE_COLLATION_PROTOCOL *mUnicodeCollation = NULL;

static
EFI_STATUS InitializeUnicodeCollationProtocol (VOID) {
   EFI_STATUS  Status;

   if (mUnicodeCollation != NULL) {
      return EFI_SUCCESS;
   }

   //
   // BUGBUG: Proper impelmentation is to locate all Unicode Collation Protocol
   // instances first and then select one which support English language.
   // Current implementation just pick the first instance.
   //
   Status = REFIT_CALL_3_WRAPPER(
       gBS->LocateProtocol,
       &gEfiUnicodeCollation2ProtocolGuid,
       NULL,
       (VOID **) &mUnicodeCollation
   );
   if (EFI_ERROR(Status)) {
       Status = REFIT_CALL_3_WRAPPER(
           gBS->LocateProtocol,
           &gEfiUnicodeCollationProtocolGuid,
           NULL,
           (VOID **) &mUnicodeCollation
       );
   }

   return Status;
}

static
BOOLEAN MetaiMatch (
    IN CHAR16 *String,
    IN CHAR16 *Pattern
) {
   if (!mUnicodeCollation) {
      InitializeUnicodeCollationProtocol();
   }
   if (mUnicodeCollation) {
       return mUnicodeCollation->MetaiMatch (mUnicodeCollation, String, Pattern);
   }

   // Should not happen
   return FALSE;
}

#endif

BOOLEAN DirIterNext (
    IN  OUT REFIT_DIR_ITER  *DirIter,
    IN      UINTN            FilterMode,
    IN      CHAR16          *FilePattern OPTIONAL,
        OUT EFI_FILE_INFO  **DirEntry
) {
    CHAR16 *OnePattern;
    EFI_FILE_INFO *LastFileInfo;

    if (EFI_ERROR(DirIter->LastStatus)) {
        // stop iteration
        return FALSE;
    }

    for (;;) {
        DirIter->LastStatus = DirNextEntry (
            DirIter->DirHandle,
            &LastFileInfo,
            FilterMode
        );
        if (EFI_ERROR(DirIter->LastStatus) || LastFileInfo == NULL) {
            return FALSE;
        }
        if (FilePattern == NULL || LastFileInfo->Attribute & EFI_FILE_DIRECTORY) {
            break;
        }
        BOOLEAN Found = FALSE;
        UINTN i = 0;
        while (!Found && (OnePattern = FindCommaDelimited (FilePattern, i++)) != NULL) {
           if (MetaiMatch (LastFileInfo->FileName, OnePattern)) {
               Found = TRUE;
           }
               MY_FREE_POOL(OnePattern);
        } // while
        if (Found) {
            break;
        }
        MY_FREE_POOL(LastFileInfo);
   } // for

    *DirEntry = LastFileInfo;
    return TRUE;
}

EFI_STATUS DirIterClose (
    IN OUT REFIT_DIR_ITER *DirIter
) {
    if ((DirIter->CloseDirHandle) && (DirIter->DirHandle->Close)) {
        REFIT_CALL_1_WRAPPER(DirIter->DirHandle->Close, DirIter->DirHandle);
    }

    return DirIter->LastStatus;
}

//
// file name manipulation
//

// Returns the filename portion (minus path name) of the
// specified file
CHAR16 * Basename (
    IN CHAR16 *Path
) {
    CHAR16  *FileName;
    UINTN    i;

    FileName = Path;

    if (Path != NULL) {
        for (i = StrLen (Path); i > 0; i--) {
            if (Path[i-1] == '\\' || Path[i-1] == '/') {
                FileName = Path + i;
                break;
            }
        }
    }

    return StrDuplicate (FileName);
}

// Remove the .efi extension from FileName -- for instance, if FileName is
// "fred.efi", returns "fred". If the filename contains no .efi extension,
// returns a copy of the original input.
CHAR16 * StripEfiExtension (
    IN CHAR16 *FileName
) {
    UINTN   Length;
    CHAR16 *Copy = NULL;

    if ((FileName != NULL) && ((Copy = StrDuplicate (FileName)) != NULL)) {
        Length = StrLen (Copy);
        if ((Length >= 4) && MyStriCmp (&Copy[Length - 4], L".efi")) {
            Copy[Length - 4] = 0;
        }
    }

    return Copy;
} // CHAR16 *StripExtension()

//
// memory string search
//

INTN FindMem (
    IN VOID  *Buffer,
    IN UINTN  BufferLength,
    IN VOID  *SearchString,
    IN UINTN  SearchStringLength
) {
    UINT8 *BufferPtr;
    UINTN  Offset;

    BufferPtr = Buffer;
    BufferLength -= SearchStringLength;
    for (Offset = 0; Offset < BufferLength; Offset++, BufferPtr++) {
        if (CompareMem (BufferPtr, SearchString, SearchStringLength) == 0) {
            return (INTN) Offset;
        }
    }

    return -1;
}

// Takes an input pathname (*Path) and returns the part of the filename from
// the final dot onwards, converted to lowercase. If the filename includes
// no dots, or if the input is NULL, returns an empty (but allocated) string.
// The calling function is responsible for freeing the memory associated with
// the return value.
CHAR16 * FindExtension (
    IN CHAR16 *Path
) {
    CHAR16     *Extension;
    BOOLEAN    Found = FALSE, FoundSlash = FALSE;
    INTN       i;

    Extension = AllocateZeroPool (sizeof (CHAR16));
    if (Path) {
        i = StrLen (Path);
        while ((!Found) && (!FoundSlash) && (i >= 0)) {
            if (Path[i] == L'.') {
                Found = TRUE;
            }
            else if ((Path[i] == L'/') || (Path[i] == L'\\')) {
                FoundSlash = TRUE;
            }

            if (!Found) {
                i--;
            }
        } // while

        if (Found) {
            MergeStrings (&Extension, &Path[i], 0);
            ToLower (Extension);
        }
    }

    return (Extension);
} // CHAR16 *FindExtension()

// Takes an input pathname (*Path) and locates the final directory component
// of that name. For instance, if the input path is 'EFI\foo\bar.efi', this
// function returns the string 'foo'.
// Assumes the pathname is separated with backslashes.
CHAR16 * FindLastDirName (
    IN CHAR16 *Path
) {
    UINTN   i;
    UINTN   PathLength;
    UINTN   CopyLength;
    UINTN   EndOfElement   = 0;
    UINTN   StartOfElement = 0;
    CHAR16 *Found          = NULL;

    if (Path == NULL) {
        return NULL;
    }

    PathLength = StrLen (Path);
    // Find start & end of target element
    for (i = 0; i < PathLength; i++) {
        if (Path[i] == '\\') {
            StartOfElement = EndOfElement;
            EndOfElement   = i;
        }
    } // for

    // Extract the target element
    if (EndOfElement > 0) {
        while ((StartOfElement < PathLength) && (Path[StartOfElement] == '\\')) {
            StartOfElement++;
        } // while

        EndOfElement--;
        if (EndOfElement >= StartOfElement) {
            CopyLength = EndOfElement - StartOfElement + 1;
            Found = StrDuplicate (&Path[StartOfElement]);
            if (Found != NULL) {
                Found[CopyLength] = 0;
            }
        }
    }

    return (Found);
} // CHAR16 *FindLastDirName()

// Returns the directory portion of a pathname. For instance,
// if FullPath is 'EFI\foo\bar.efi', this function returns the
// string 'EFI\foo'. The calling function is responsible for
// freeing the returned string's memory.
CHAR16 * FindPath (
    IN CHAR16 *FullPath
) {
   UINTN   i;
   UINTN   LastBackslash = 0;
   CHAR16 *PathOnly      = NULL;

   if (FullPath != NULL) {
      for (i = 0; i < StrLen (FullPath); i++) {
         if (FullPath[i] == '\\') {
             LastBackslash = i;
         }
      }

      PathOnly = StrDuplicate (FullPath);
      if (PathOnly != NULL) {
          PathOnly[LastBackslash] = 0;
      }
   }

   return (PathOnly);
}

// Takes an input loadpath, splits it into disk and filename components, finds a matching
// DeviceVolume, and returns that and the filename (*loader).
VOID FindVolumeAndFilename (
    IN  EFI_DEVICE_PATH  *loadpath,
    OUT REFIT_VOLUME    **DeviceVolume,
    OUT CHAR16          **loader
) {
    CHAR16 *DeviceString, *VolumeDeviceString, *Temp;
    UINTN i = 0;
    REFIT_VOLUME *FoundVolume = NULL;

    if (!loadpath || !DeviceVolume || !loader) {
        return;
    }

    MY_FREE_POOL(*loader);
    DeviceString  = DevicePathToStr (loadpath);
    *loader       = SplitDeviceString (DeviceString);

    while ((i < VolumesCount) && (!FoundVolume)) {
        if (Volumes[i]->DevicePath == NULL) {
            i++;
            continue;
        }

        VolumeDeviceString = DevicePathToStr (Volumes[i]->DevicePath);
        Temp               = SplitDeviceString (VolumeDeviceString);

        if (MyStrStr (VolumeDeviceString, DeviceString)) {
            FoundVolume = Volumes[i];
        }
        i++;

        MY_FREE_POOL(Temp);
        MY_FREE_POOL(VolumeDeviceString);
    } // while

    AssignVolume (DeviceVolume, FoundVolume);

    MY_FREE_POOL(DeviceString);
} // VOID FindVolumeAndFilename()

// Splits a volume/filename string (e.g., "fs0:\EFI\BOOT") into separate
// volume and filename components (e.g., "fs0" and "\EFI\BOOT"), returning
// the filename component in the original *Path variable and the split-off
// volume component in the *VolName variable.
// Returns TRUE if both components are found, FALSE otherwise.
BOOLEAN SplitVolumeAndFilename (
    IN  OUT CHAR16 **Path,
        OUT CHAR16 **VolName
) {
    UINTN   Length, i = 0;
    CHAR16 *Filename;

    *VolName = NULL;
    if (*Path == NULL) {
        return FALSE;
    }

    Length = StrLen (*Path);
    while ((i < Length) && ((*Path)[i] != L':')) {
        i++;
    }

    if (i < Length) {
        Filename   =  StrDuplicate ((*Path) + i + 1);
        (*Path)[i] =  0;
        *VolName   = *Path;
        *Path      =  Filename;

        return TRUE;
    }
    else {
        return FALSE;
    }
} // BOOLEAN SplitVolumeAndFilename()

// Take an input path name, which may include a volume specification and/or
// a path, and return separate volume, path, and file names. For instance,
// "BIGVOL:\EFI\ubuntu\grubx64.efi" will return a VolName of "BIGVOL", a Path
// of "EFI\ubuntu", and a Filename of "grubx64.efi". If an element is missing,
// the returned pointer is NULL. The calling function is responsible for
// freeing the allocated memory.
VOID SplitPathName (
    IN     CHAR16  *InPath,
    IN OUT CHAR16 **VolName,
    IN OUT CHAR16 **Path,
    IN OUT CHAR16 **Filename
) {
    CHAR16 *Temp = NULL;

    MY_FREE_POOL(*VolName);
    MY_FREE_POOL(*Path);
    MY_FREE_POOL(*Filename);

    Temp = StrDuplicate (InPath);
    SplitVolumeAndFilename (&Temp, VolName); // VolName is NULL or has volume; Temp has rest of path
    CleanUpPathNameSlashes (Temp);

    *Path     = FindPath (Temp); // *Path has path (may be 0-length); Temp unchanged.
    *Filename = StrDuplicate (Temp + StrLen (*Path));

    CleanUpPathNameSlashes (*Filename);

    if (StrLen (*Path) == 0) {
        MY_FREE_POOL(*Path);
    }

    if (StrLen (*Filename) == 0) {
        MY_FREE_POOL(*Filename);
    }

    MY_FREE_POOL(Temp);
} // VOID SplitPathName()

// Finds a volume with the specified Identifier (a filesystem label, a
// partition name, or a partition GUID). If found, sets *Volume to point
// to that volume. If not, leaves it unchanged.
// Returns TRUE if a match was found, FALSE if not.
BOOLEAN FindVolume (
    IN REFIT_VOLUME **Volume,
    IN CHAR16        *Identifier
) {
    UINTN     i = 0;
    BOOLEAN   Found = FALSE;

    while ((i < VolumesCount) && (!Found)) {
        if (VolumeMatchesDescription (Volumes[i], Identifier)) {
            AssignVolume (Volume, Volumes[i]);
            Found   = TRUE;
        }
        i++;
    }

    return (Found);
} // static VOID FindVolume()

// Returns TRUE if Description matches Volume's VolName, FsName, or (once
// transformed) PartGuid fields, FALSE otherwise (or if either pointer is NULL)
BOOLEAN VolumeMatchesDescription (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *Description
) {
    EFI_GUID TargetVolGuid = NULL_GUID_VALUE;

    if ((Volume == NULL) || (Description == NULL)) {
        return FALSE;
    }

    if (IsGuid (Description)) {
        TargetVolGuid = StringAsGuid (Description);
        return GuidsAreEqual (&TargetVolGuid, &(Volume->PartGuid));
    }
    else {
        return (
            MyStriCmp (Description, GetPoolStr (&Volume->VolName))  ||
            MyStriCmp (Description, GetPoolStr (&Volume->PartName)) ||
            MyStriCmp (Description, GetPoolStr (&Volume->FsName))
        );
    }
} // BOOLEAN VolumeMatchesDescription()

// Returns TRUE if specified Volume, Directory, and Filename correspond to an
// element in the comma-delimited List, FALSE otherwise. Note that Directory and
// Filename must *NOT* include a volume or path specification (that is part of
// the Volume variable), but the List elements may. Performs comparison
// case-insensitively.
BOOLEAN FilenameIn (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *Directory,
    IN CHAR16       *Filename,
    IN CHAR16       *List
) {

    CHAR16    *OneElement;
    CHAR16    *TargetVolName  = NULL;
    CHAR16    *TargetPath     = NULL;
    CHAR16    *TargetFilename = NULL;
    UINTN      i              = 0;
    BOOLEAN    Found          = FALSE;

    if (Filename && List) {
        while (!Found && (OneElement = FindCommaDelimited (List, i++))) {
            Found = TRUE;
            SplitPathName (OneElement, &TargetVolName, &TargetPath, &TargetFilename);
            if ((TargetVolName  != NULL && !VolumeMatchesDescription (Volume, TargetVolName)) ||
                (TargetPath     != NULL && !MyStriCmp (TargetPath, Directory)) ||
                (TargetFilename != NULL && !MyStriCmp (TargetFilename, Filename))
            ) {
                Found = FALSE;
            }
            MY_FREE_POOL(OneElement);
        }

        MY_FREE_POOL(TargetVolName);
        MY_FREE_POOL(TargetPath);
        MY_FREE_POOL(TargetFilename);
    }

    return Found;
} // BOOLEAN FilenameIn()

// Eject all removable media.
// Returns TRUE if any media were ejected, FALSE otherwise.
BOOLEAN EjectMedia (VOID) {
    EFI_STATUS                      Status;
    UINTN                           HandleIndex;
    UINTN                           HandleCount             = 0;
    UINTN                           Ejected                 = 0;
    EFI_GUID                        AppleRemovableMediaGuid = APPLE_REMOVABLE_MEDIA_PROTOCOL_GUID;
    EFI_HANDLE                     *Handles;
    EFI_HANDLE                      Handle;
    APPLE_REMOVABLE_MEDIA_PROTOCOL *Ejectable;

    Status = LibLocateHandle (
        ByProtocol,
        &AppleRemovableMediaGuid,
        NULL,
        &HandleCount,
        &Handles
    );
    if (EFI_ERROR(Status) || HandleCount == 0) {
        // probably not an Apple system
        return (FALSE);
    }

    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
        Handle = Handles[HandleIndex];
        Status = REFIT_CALL_3_WRAPPER(
            gBS->HandleProtocol,
            Handle,
            &AppleRemovableMediaGuid,
            (VOID **) &Ejectable
        );

        if (EFI_ERROR(Status)) {
            continue;
        }

        Status = REFIT_CALL_1_WRAPPER(Ejectable->Eject, Ejectable);
        if (!EFI_ERROR(Status)) {
            Ejected++;
        }
    }
    MY_FREE_POOL(Handles);

    return (Ejected > 0);
} // VOID EjectMedia()

// Returns TRUE if the two GUIDs are equal, FALSE otherwise
BOOLEAN GuidsAreEqual (
    IN EFI_GUID *Guid1,
    IN EFI_GUID *Guid2
) {
    return (CompareMem (Guid1, Guid2, 16) == 0);
} // BOOLEAN GuidsAreEqual()

// Erase linked-list of UINT32 values
VOID EraseUint32List (
    IN UINT32_LIST **TheList
) {
    UINT32_LIST *NextItem;

    while (*TheList) {
        NextItem = (*TheList)->Next;
        MY_FREE_POOL(*TheList);
        *TheList = NextItem;
    }
} // EraseUin32List()

//
// volume functions for maintaining reference count
//

/*
    If Volume is an input argument to a function, then you probably want to retain it during the function.
*/
VOID
RetainVolume (
    REFIT_VOLUME *Volume
) {
    if (Volume) {
        Volume->ReferenceCount++;
    }
}

REFIT_VOLUME *
AllocateVolume (
    REFIT_VOLUME **Volume
) {
    VolumeLog (LOGPROCENTRY());
    if (Volume) {
        *Volume = AllocateZeroPool (sizeof (REFIT_VOLUME));
        RetainVolume (*Volume);
    }
    VolumeLog (LOGPROCEXIT("%p(#%d)", Volume ? *Volume : NULL, (Volume && *Volume) ? (*Volume)->ReferenceCount : -1));
    return Volume ? *Volume : NULL;
}

/*
    This frees the original destination Volume after retaining the source Volume so there's usually no need for a corresponding free.
*/
VOID
AssignVolume (
    REFIT_VOLUME **Volume,
    REFIT_VOLUME *NewVolume
) {
    if (Volume) {
        VolumeLog (LOGPROCENTRY("Old:%p->%p(#%d) New:%p(#%d)", Volume, *Volume, *Volume ? (*Volume)->ReferenceCount : -1, NewVolume, NewVolume ? NewVolume->ReferenceCount : -1));
        if (NewVolume && (NewVolume->ReferenceCount & 0xFFFF0000)) {
            MsgLog ("Allocation Error: weird volume reference count 0x%X\n", NewVolume->ReferenceCount);
            DumpCallStack(NULL, FALSE);
        }
        LOGPOOL(*Volume);
        LOGPOOL(NewVolume);
        REFIT_VOLUME *OldVolume = *Volume;
        RetainVolume (NewVolume);
        *Volume = NewVolume;
        FreeVolume (&OldVolume);
        VolumeLog (LOGPROCEXIT("Current:%p->%p(#%d) Old:%p(#%d)", Volume, *Volume, *Volume ? (*Volume)->ReferenceCount : -1, OldVolume, OldVolume ? OldVolume->ReferenceCount : -1));
    }
}

VOID
AddToVolumeList (
    REFIT_VOLUME ***Volumes,
    UINTN *VolumesCount,
    REFIT_VOLUME *Volume
) {
    if (*Volumes) {
        LOGPOOL(*Volumes);
    }
    LOGPOOL(Volume);
    AddListElement ((VOID***)Volumes, VolumesCount, Volume);
    if (Volume) {
        RetainVolume (Volume);
        VolumeLog (LOGPROC("%p->%p[%d] = %p(#%d)\n", Volumes, *Volumes ? *Volumes : 0, VolumesCount ? *VolumesCount - 1 : -1, Volume, Volume ? Volume->ReferenceCount : -1));
    }
    else {
        MsgLog ("Allocation Error: Assigning null volume to list\n");
    }
}

#if REFIT_DEBUG > 0
VOID
LEAKABLEPOOLSTR (
    PoolStr *Str,
    CHAR8 *Description
) {
    LEAKABLEWITHPATH ((Str && !Str->Cached) ? Str->Str : NULL, Description);
}

VOID
LEAKABLEONEPOOLSTRProc (
    PoolStr *Str,
    CHAR8 *Description
) {
    if (Str && Str->Str && !Str->Cached) {
        LEAKABLE (Str->Str, Description);
    }
}

VOID
LEAKABLEPOOLIMAGE (
    PoolImage *Image
) {
    LEAKABLEIMAGE ((Image && !Image->Cached) ? Image->Image : NULL);
}

VOID
LEAKABLEVOLUME (
    REFIT_VOLUME *Volume
) {
    if (Volume) {
        LEAKABLEPATHINC (); // space for Volume info
            LEAKABLEWITHPATH  (Volume->DevicePath, "Volume DevicePath");
            LEAKABLEPOOLSTR   (&Volume->PartName_PS_, "Volume PartName");
            LEAKABLEPOOLSTR   (&Volume->FsName_PS_, "Volume FsName");
            LEAKABLEPOOLSTR   (&Volume->VolName_PS_, "Volume VolName");
            LEAKABLEPOOLIMAGE (&Volume->VolIconImage_PI_);
            LEAKABLEPOOLIMAGE (&Volume->VolBadgeImage_PI_);
            LEAKABLEPOOLSTR   (&Volume->OSIconName_PS_, "Volume OSIconName");
            LEAKABLEPOOLSTR   (&Volume->OSName_PS_, "Volume OSName");
            LEAKABLEWITHPATH  (Volume->WholeDiskDevicePath, "Volume WholeDiskDevicePath");
            LEAKABLEWITHPATH  (Volume->MbrPartitionTable, "Volume MbrPartitionTable");
        LEAKABLEPATHDEC ();
    }
    LEAKABLEWITHPATH (Volume, "Volume");
}

VOID
LEAKABLEVOLUMES (
) {
    LOGPROCENTRY();
    if (Volumes) {
        LEAKABLEPATHINIT (kLeakableVolumes);
            UINTN VolumeIndex;
            LEAKABLEPATHINC (); // space for VolumeIndex
            for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                LEAKABLEVOLUME (Volumes[VolumeIndex]);
            }
            LEAKABLEPATHDEC ();
            LEAKABLEWITHPATH (Volumes, "Volumes");
        LEAKABLEPATHDONE ();
    }

    // The volumes in these lists should already exist in Volumes
    if (PreBootVolumes) {
        LEAKABLEPATHINIT (kLeakablePreBootVolumes);
            LEAKABLEWITHPATH (PreBootVolumes, "PreBootVolumes");
        LEAKABLEPATHDONE ();
    }
    if (DataVolumes) {
        LEAKABLEPATHINIT (kLeakableDataVolumes);
            LEAKABLEWITHPATH (DataVolumes, "DataVolumes");
        LEAKABLEPATHDONE ();
    }
    if (SystemVolumes) {
        LEAKABLEPATHINIT (kLeakableSystemVolumes);
            LEAKABLEWITHPATH (SystemVolumes, "SystemVolumes");
        LEAKABLEPATHDONE ();
    }

    LOGPROCEXIT();
}
#endif
