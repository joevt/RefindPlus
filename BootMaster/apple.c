/*
 * BootMaster/apple.c
 * Functions specific to Apple computers
 *
 * Copyright (c) 2015 Roderick W. Smith
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "config.h"
#include "lib.h"
#include "screenmgt.h"
#include "apple.h"
#include "mystrings.h"
#include "../include/refit_call_wrapper.h"

PoolStr    gCsrStatus_PS_ = NULLPS;
BOOLEAN    MuteLogger     = FALSE;
BOOLEAN    MsgNormalised  = FALSE;

// Get CSR (Apple's Configurable Security Restrictions; aka System Integrity
// Protection [SIP], or "rootless") status information. If the variable is not
// present and the firmware is Apple, fake it and claim it is enabled, since
// that is how OS X 10.11 treats a system with the variable absent.
EFI_STATUS GetCsrStatus (
    IN OUT UINT32 *CsrStatus
) {
    EFI_STATUS  Status;
    UINTN       CsrLength;
    UINT32     *ReturnValue  = NULL;
    EFI_GUID    CsrGuid      = APPLE_GUID;

    Status = EfivarGetRaw (
        &CsrGuid,
        L"csr-active-config",
        (VOID **) &ReturnValue,
        &CsrLength
    );

    if (Status == EFI_SUCCESS) {
        if (CsrLength == 4) {
            *CsrStatus = *ReturnValue;
        }
        else {
            Status     = EFI_BAD_BUFFER_SIZE;
            AssignCachedPoolStr (&gCsrStatus, L"Unknown SIP/SSV Status");
            LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");
        }

        MyFreePool (&ReturnValue);
    }
    else if (Status == EFI_NOT_FOUND) {
        *CsrStatus = SIP_ENABLED_EX;
        AssignCachedPoolStr (&gCsrStatus, L"SIP/SSV Enabled (Cleared/Empty)");
        LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");

        // Treat as Success
        Status = EFI_SUCCESS;
    }
    else {
        AssignCachedPoolStr (&gCsrStatus, L"Error While Getting SIP/SSV Status");
        LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");
    }

    return Status;
} // EFI_STATUS GetCsrStatus()

// Store string describing CSR status value in gCsrStatus variable, which appears
// on the Info page. If DisplayMessage is TRUE, displays the new value of
// gCsrStatus on the screen for four seconds.
VOID RecordgCsrStatus (
    UINT32  CsrStatus,
    BOOLEAN DisplayMessage
) {
    UINTN     WaitSeconds = 3;
    CHAR16   *MsgStr      = NULL;
    EG_PIXEL  BGColor     = COLOR_LIGHTBLUE;

    switch (CsrStatus) {
        // SIP "Cleared" Setting
        case SIP_ENABLED_EX:
            AssignCachedPoolStr (&gCsrStatus, L"SIP Enabled (Cleared/Empty)");
            LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");
            break;

        // SIP "Enabled" Setting
        case SIP_ENABLED:
            AssignPoolStr (&gCsrStatus, PoolPrint (
                L"SIP Enabled (0x%04x)",
                CsrStatus
            ));
            LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");
            break;

        // SIP "Disabled" Settings
        case SIP_DISABLED:
        case SIP_DISABLED_B:
        case SIP_DISABLED_EX:
        case SIP_DISABLED_DBG:
        case SIP_DISABLED_KEXT:
        case SIP_DISABLED_EXTRA:
            AssignPoolStr (&gCsrStatus, PoolPrint (
                L"SIP Disabled (0x%04x)",
                CsrStatus
            ));
            LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");
            break;

        // SSV "Disabled" Settings
        case SSV_DISABLED:
        case SSV_DISABLED_B:
        case SSV_DISABLED_EX:
            AssignPoolStr (&gCsrStatus, PoolPrint (
                L"SIP/SSV Disabled (0x%04x)",
                CsrStatus
            ));
            LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");
            break;

        // Recognised Custom SIP "Disabled" Settings
        case SSV_DISABLED_ANY:
        case SSV_DISABLED_KEXT:
        case SSV_DISABLED_ANY_EX:
            AssignPoolStr (&gCsrStatus, PoolPrint (
                L"SIP/SSV Disabled (0x%04x - Custom Setting)",
                CsrStatus
            ));
            LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");
            break;

        // Wide Open and Max Legal CSR "Disabled" Settings
        case SSV_DISABLED_WIDE_OPEN:
        case CSR_MAX_LEGAL_VALUE:
            AssignPoolStr (&gCsrStatus, PoolPrint (
                L"SIP/SSV Removed (0x%04x - Caution!)",
                CsrStatus
            ));
            LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");
            break;

        // Unknown Custom Setting
        default:
            AssignPoolStr (&gCsrStatus, PoolPrint (
                L"SIP/SSV Disabled: 0x%04x - Caution: Unknown Custom Setting",
                CsrStatus
            ));
            LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");
    } // switch

    if (DisplayMessage) {
        if (MsgNormalised) {
            WaitSeconds = 4;
            MsgStr = PoolPrint (L"Normalised CSR:- '%s'", GetPoolStr (&gCsrStatus));
        }
        else {
            MsgStr = PoolPrint (L"%s", GetPoolStr (&gCsrStatus));
        }

        egDisplayMessage (MsgStr, &BGColor, CENTER);
        PauseSeconds (WaitSeconds);

        #if REFIT_DEBUG > 0
        MsgLog ("    * %s\n\n", MsgStr);
        #endif

        MyFreePool (&MsgStr);
    }
} // VOID RecordgCsrStatus()

// Find the current CSR status and reset it to the next one in the
// GlobalConfig.CsrValues list, or to the first value if the current
// value is not on the list.
VOID RotateCsrValue (VOID) {
    EFI_STATUS    Status;
    UINT32        CurrentValue, TargetCsr;
    UINT32        StorageFlags = APPLE_FLAGS;
    EFI_GUID      CsrGuid      = APPLE_GUID;
    UINT32_LIST  *ListItem;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_SEPARATOR, L"Rotating CSR Value");
    #endif

    Status = GetCsrStatus (&CurrentValue);
    if ((Status == EFI_SUCCESS) && GlobalConfig.CsrValues) {
        ListItem = GlobalConfig.CsrValues;

        while ((ListItem != NULL) && (ListItem->Value != CurrentValue)) {
            ListItem = ListItem->Next;
        } // while

        TargetCsr = (ListItem == NULL || ListItem->Next == NULL)
            ? GlobalConfig.CsrValues->Value
            : ListItem->Next->Value;

        #if REFIT_DEBUG > 0
        if (TargetCsr == 0) {
            // Set target CSR value to NULL
            LOG(3, LOG_LINE_NORMAL,
                L"Clearing CSR to 'NULL' from '0x%04x'",
                CurrentValue
            );
        }
        else if (CurrentValue == 0) {
            LOG(3, LOG_LINE_NORMAL,
                L"Setting CSR to '0x%04x' from 'NULL'",
                TargetCsr
            );
        }
        else {
            LOG(3, LOG_LINE_NORMAL,
                L"Setting CSR to '0x%04x' from '0x%04x'",
                CurrentValue, TargetCsr
            );
        }
        #endif

        if (TargetCsr != 0) {
            Status = EfivarSetRaw (
                &CsrGuid, L"csr-active-config",
                &TargetCsr, 4, TRUE
            );
        }
        else {
            Status = REFIT_CALL_5_WRAPPER(
                gRT->SetVariable, L"csr-active-config",
                &CsrGuid, StorageFlags, 0, NULL
            );
        }

        if (Status == EFI_SUCCESS) {
            RecordgCsrStatus (TargetCsr, TRUE);

            #if REFIT_DEBUG > 0
            LOG(3, LOG_LINE_NORMAL,
                L"Successfully Set SIP/SSV:- '0x%04x'",
                TargetCsr
            );
            #endif
        }
        else {
            AssignCachedPoolStr (&gCsrStatus, L"Error While Setting SIP/SSV");
            LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");

            #if REFIT_DEBUG > 0
            LOG(3, LOG_LINE_NORMAL, L"%s", GetPoolStr (&gCsrStatus));
            #endif

            EG_PIXEL BGColor = COLOR_LIGHTBLUE;
            egDisplayMessage (
                GetPoolStr (&gCsrStatus),
                &BGColor,
                CENTER
            );
            PauseSeconds (4);
        }
    }
    else {
        AssignCachedPoolStr (&gCsrStatus, L"Could Not Retrieve SIP/SSV Status");
        LEAKABLEONEPOOLSTR (&gCsrStatus, "gCsrStatus");

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"%s", GetPoolStr (&gCsrStatus));
        #endif

        EG_PIXEL BGColor = COLOR_LIGHTBLUE;
        egDisplayMessage (
            GetPoolStr (&gCsrStatus),
            &BGColor,
            CENTER
        );
        PauseSeconds (4);
    } // if/else
} // VOID RotateCsrValue()


EFI_STATUS NormaliseCSR (VOID) {
    EFI_STATUS  Status;
    UINT32      OurCSR;

    // Mute logging if active
    MuteLogger = TRUE;

    // Get csr-active-config value
    Status = GetCsrStatus (&OurCSR);

    if (Status == EFI_NOT_FOUND) {
        // csr-active-config not found ... Proceed as 'OK'
        Status = EFI_ALREADY_STARTED;
    }
    else if (Status == EFI_SUCCESS) {
        if ((OurCSR & CSR_ALLOW_APPLE_INTERNAL) == 0) {
            // 'CSR_ALLOW_APPLE_INTERNAL' bit not present ... Proceed as 'OK'
            Status = EFI_ALREADY_STARTED;
        }
        else {
            // 'CSR_ALLOW_APPLE_INTERNAL' bit present ... Clear and Reset
            OurCSR &= ~CSR_ALLOW_APPLE_INTERNAL;

            MsgNormalised = TRUE;
            RecordgCsrStatus (OurCSR, TRUE);
            MsgNormalised = FALSE;
        }
    }

    // Restore logging if previously active
    MuteLogger = FALSE;

    return Status;
} // EFI_STATUS NormaliseCSR()


/*
 * The definitions below and the SetAppleOSInfo() function are based on a GRUB patch by Andreas Heider:
 * https://lists.gnu.org/archive/html/grub-devel/2013-12/msg00442.html
 */

#define EFI_APPLE_SET_OS_PROTOCOL_GUID  { 0xc5c5da95, 0x7d5c, 0x45e6, \
    { 0xb2, 0xf1, 0x3f, 0xd5, 0x2b, 0xb1, 0x00, 0x77 } }

typedef struct EfiAppleSetOsInterface {
    UINT64 Version;
    EFI_STATUS EFIAPI (*SetOsVersion) (IN CHAR8 *Version);
    EFI_STATUS EFIAPI (*SetOsVendor) (IN CHAR8 *Vendor);
} EfiAppleSetOsInterface;

// Function to tell the firmware that Mac OS X is being launched. This is
// required to work around problems on some Macs that do not fully
// initialize some hardware (especially video displays) when third-party
// OSes are launched in EFI mode.
EFI_STATUS SetAppleOSInfo (
    VOID
) {
    EFI_STATUS               Status;
    EFI_GUID                 apple_set_os_guid  = EFI_APPLE_SET_OS_PROTOCOL_GUID;
    CHAR16                  *AppleVersionOS     = NULL;
    CHAR8                   *MacVersionStr      = NULL;
    EfiAppleSetOsInterface  *SetOs              = NULL;

    Status = REFIT_CALL_3_WRAPPER(
        gBS->LocateProtocol,
        &apple_set_os_guid,
        NULL,
        (VOID **) &SetOs
    );

    // If not a Mac, ignore the call.
    if ((Status != EFI_SUCCESS) || (!SetOs)) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Not a Mac ... Do Not Set Mac OS Information"
        );
        #endif

        Status = EFI_SUCCESS;
    }
    else if (SetOs->Version != 0 && GlobalConfig.SpoofOSXVersion) {
        Status         = EFI_OUT_OF_RESOURCES;
        AppleVersionOS = StrDuplicate (L"Mac OS");
        MergeStrings (&AppleVersionOS, GlobalConfig.SpoofOSXVersion, ' ');

        if (AppleVersionOS) {
            MacVersionStr = AllocateZeroPool (
                (StrLen (AppleVersionOS) + 1) * sizeof (CHAR8)
            );

            if (MacVersionStr) {
                #if REFIT_DEBUG > 0
                LOG(3, LOG_LINE_NORMAL,
                    L"Setting Mac OS Information to '%s'",
                    AppleVersionOS
                );
                #endif

                UnicodeStrToAsciiStr (AppleVersionOS, MacVersionStr);
                Status = REFIT_CALL_1_WRAPPER(
                    SetOs->SetOsVersion, MacVersionStr
                );

                if (!EFI_ERROR(Status)) {
                    Status = EFI_SUCCESS;
                }

                MyFreePool (&MacVersionStr);
            }

            if (Status == EFI_SUCCESS && SetOs->Version >= 2) {
                Status = REFIT_CALL_1_WRAPPER(
                    SetOs->SetOsVendor, (CHAR8 *) "Apple Inc."
                );
            }

            MyFreePool (&AppleVersionOS);
        } // if (AppleVersionOS)
    } // if/else

    return Status;
} // EFI_STATUS SetAppleOSInfo()



//
// APFS Volume Role Identification
//

/**
 * Copyright (C) 2019, vit9696
 *
 * All rights reserved.
 *
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
/**
 * Modified for RefindPlus
 * Copyright (c) 2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
**/

extern
BOOLEAN OcOverflowAddUN (
    UINTN   A,
    UINTN   B,
    UINTN  *Result
);
extern
VOID * OcReadFile (
    IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem,
    IN  CONST CHAR16                     *FilePath,
    OUT UINT32                           *FileSize   OPTIONAL,
    IN  UINT32                           MaxFileSize OPTIONAL
);
extern
UINTN OcFileDevicePathNameSize (
    IN CONST FILEPATH_DEVICE_PATH  *FilePath
);


static
CHAR16 * RP_GetAppleDiskLabelEx (
    IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem,
    IN  CHAR16                           *BootDirectoryName,
    IN  CONST CHAR16                     *LabelFilename
) {
    UINTN     DiskLabelPathSize;
    UINTN     MaxVolumelabelSize = 64;
    CHAR8    *AsciiDiskLabel;
    CHAR16   *DiskLabelPath;
    CHAR16   *UnicodeDiskLabel = NULL;
    UINT32    DiskLabelLength;

    DiskLabelPathSize = StrSize (BootDirectoryName) + StrSize (LabelFilename) - sizeof (CHAR16);

    DiskLabelPath = AllocatePool (DiskLabelPathSize);
    if (DiskLabelPath == NULL) {
        return NULL;
    }

    DiskLabelPath = PoolPrint (L"%s%s", BootDirectoryName, LabelFilename);

    AsciiDiskLabel = (CHAR8 *) OcReadFile (
        FileSystem,
        DiskLabelPath,
        &DiskLabelLength,
        MaxVolumelabelSize
    );

    MyFreePool (&DiskLabelPath);

    if (AsciiDiskLabel != NULL) {
        return NULL;
    }

    UnicodeDiskLabel = MyAsciiStrCopyToUnicode (AsciiDiskLabel, DiskLabelLength);

    if (UnicodeDiskLabel != NULL) {
        MyUnicodeFilterString (UnicodeDiskLabel, TRUE);
    }
    MyFreePool (&AsciiDiskLabel);

    return UnicodeDiskLabel;
} // static CHAR16 * RP_GetAppleDiskLabelEx()

static
VOID * RP_GetFileInfo (
    IN  EFI_FILE_PROTOCOL  *File,
    IN  EFI_GUID           *InformationType,
    IN  UINTN               MinFileInfoSize,
    OUT UINTN              *RealFileInfoSize  OPTIONAL
) {
    EFI_STATUS  Status;
    UINTN       FileInfoSize;
    VOID       *FileInfoBuffer;

    FileInfoSize   = 0;
    FileInfoBuffer = NULL;

    Status = File->GetInfo (
        File,
        InformationType,
        &FileInfoSize,
        NULL
    );

    if (Status == EFI_BUFFER_TOO_SMALL &&
        FileInfoSize >= MinFileInfoSize
    ) {
        // Some drivers (i.e. built-in 32-bit Apple HFS driver) may possibly
        // omit null terminators from file info data.
        if (CompareGuid (InformationType, &gEfiFileInfoGuid) &&
            OcOverflowAddUN (FileInfoSize, sizeof (CHAR16), &FileInfoSize)
        ) {
            return NULL;
        }

        FileInfoBuffer = AllocateZeroPool (FileInfoSize);
        if (FileInfoBuffer != NULL) {
            Status = File->GetInfo (
                File,
                InformationType,
                &FileInfoSize,
                FileInfoBuffer
            );

            if (EFI_ERROR(Status)) {
                MyFreePool (&FileInfoBuffer);
            }
            else if (RealFileInfoSize != NULL) {
                *RealFileInfoSize = FileInfoSize;
            }
        }
    }

  return FileInfoBuffer;
} // static VOID * RP_GetFileInfo()

static
EFI_STATUS RP_GetApfsSpecialFileInfo (
    IN     EFI_FILE_PROTOCOL           *Root,
    IN OUT APPLE_APFS_VOLUME_INFO     **VolumeInfo OPTIONAL,
    IN OUT APPLE_APFS_CONTAINER_INFO  **ContainerInfo OPTIONAL
) {
    EFI_GUID AppleApfsVolumeInfoGuid    = APPLE_APFS_VOLUME_INFO_GUID;
    EFI_GUID AppleApfsContainerInfoGuid = APPLE_APFS_CONTAINER_INFO_GUID;

    if (ContainerInfo == NULL && VolumeInfo == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    if (VolumeInfo != NULL) {
        *VolumeInfo = RP_GetFileInfo (
            Root,
            &AppleApfsVolumeInfoGuid,
            sizeof (**VolumeInfo),
            NULL
        );

        if (*VolumeInfo == NULL) {
            return EFI_NOT_FOUND;
        }
    }

    if (ContainerInfo != NULL) {
        *ContainerInfo = RP_GetFileInfo (
            Root,
            &AppleApfsContainerInfoGuid,
            sizeof (**ContainerInfo),
            NULL
        );

        if (*ContainerInfo == NULL) {
            MyFreePool (VolumeInfo);

            return EFI_NOT_FOUND;
        }
    }

    return EFI_SUCCESS;
} // static EFI_STATUS RP_GetApfsSpecialFileInfo()

static
CHAR16 * RP_GetBootPathName (
    IN  EFI_DEVICE_PATH_PROTOCOL  *DevicePath
) {
    UINTN                            Len;
    UINTN                            Size;
    UINTN                            PathNameSize;
    CHAR16                          *PathName;
    CHAR16                          *FilePathName;
    FILEPATH_DEVICE_PATH            *FilePath;

    if ((DevicePathType    (DevicePath) == MEDIA_DEVICE_PATH) &&
        (DevicePathSubType (DevicePath) == MEDIA_FILEPATH_DP)
    ) {
        FilePath     = (FILEPATH_DEVICE_PATH *) DevicePath;
        Size         = OcFileDevicePathNameSize (FilePath);
        PathNameSize = Size + sizeof (CHAR16);
        PathName     = AllocateZeroPool (PathNameSize);

        if (PathName == NULL) {
            return NULL;
        }

        CopyMem (PathName, FilePath->PathName, Size);

        if (!MyStrStr (PathName, L"\\")) {
            StrCpyS (PathName, PathNameSize, L"\\");
        }
        else {
            Len          = StrLen (PathName);
            FilePathName = &PathName[Len - 1];

            while (*FilePathName != L'\\') {
                *FilePathName = L'\0';
                --FilePathName;
            } // while
        }
    }
    else {
        PathName = AllocateZeroPool (sizeof (L"\\"));
        if (PathName != NULL) {
            StrCpyS (PathName, sizeof (L"\\"), L"\\");
        }
    }

    return PathName;
} // static EFI_STATUS RP_GetBootPathName

EFI_STATUS RP_GetApfsVolumeInfo (
    IN  EFI_HANDLE               Device,
    OUT EFI_GUID                *ContainerGuid,
    OUT EFI_GUID                *VolumeGuid,
    OUT APPLE_APFS_VOLUME_ROLE  *VolumeRole
) {
    EFI_STATUS                       Status;
    EFI_FILE_PROTOCOL               *Root;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    APPLE_APFS_CONTAINER_INFO       *ApfsContainerInfo;
    APPLE_APFS_VOLUME_INFO          *ApfsVolumeInfo;

    Root = NULL;

    Status = gBS->HandleProtocol (
        Device,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **) &FileSystem
    );

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = FileSystem->OpenVolume (FileSystem, &Root);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = RP_GetApfsSpecialFileInfo (Root, &ApfsVolumeInfo, &ApfsContainerInfo);

    Root->Close (Root);

    if (EFI_ERROR(Status)) {
        return EFI_NOT_FOUND;
    }

    CopyGuid (
        VolumeGuid,
        &ApfsVolumeInfo->Uuid
    );

    *VolumeRole = ApfsVolumeInfo->Role;

    CopyGuid (
        ContainerGuid,
        &ApfsContainerInfo->Uuid
    );

    MyFreePool (&ApfsVolumeInfo);
    MyFreePool (&ApfsContainerInfo);

    return EFI_SUCCESS;
} // EFI_STATUS RP_GetApfsVolumeInfo()

CHAR16 * RP_GetAppleDiskLabel (
    IN  REFIT_VOLUME *Volume
) {
    EFI_STATUS                        Status;
    CHAR16                           *BootDirectoryName;
    CHAR16                           *AppleDiskLabel = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *FileSystem;

    BootDirectoryName = RP_GetBootPathName (Volume->DevicePath);
    if (!BootDirectoryName) {
        return NULL;
    }

    Status = gBS->HandleProtocol (
        Volume->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **) &FileSystem
    );

    if (EFI_ERROR (Status)) {
        MyFreePool (&BootDirectoryName);
        return NULL;
    }

    AppleDiskLabel = RP_GetAppleDiskLabelEx (
        FileSystem,
        BootDirectoryName,
        L".contentDetails"
    );

    if (AppleDiskLabel == NULL) {
        AppleDiskLabel = RP_GetAppleDiskLabelEx (
            FileSystem,
            BootDirectoryName,
            L".disk_label.contentDetails"
        );
    }
    MyFreePool (&BootDirectoryName);

    return AppleDiskLabel;
} // CHAR16 * RP_GetAppleDiskLabel()
