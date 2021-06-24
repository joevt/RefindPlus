/*
 * BootMaster/lib.h
 * General header file
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
 * Modifications copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), a copy of which must be distributed
 * with this source code or binaries made from it.
 *
 */

#ifndef __LIB_H_
#define __LIB_H_

#ifdef __MAKEWITH_GNUEFI
#include "efi.h"
#include "efilib.h"
#define EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH
#else
#include "../include/tiano_includes.h"
#endif

#include "global.h"

#include "libeg.h"

//
// lib module
//

// types

typedef struct {
    EFI_STATUS          LastStatus;
    EFI_FILE_HANDLE     DirHandle;
    BOOLEAN             CloseDirHandle;
    EFI_FILE_INFO       *LastFileInfo;
} REFIT_DIR_ITER;

#define DISK_KIND_INTERNAL  (0)
#define DISK_KIND_EXTERNAL  (1)
#define DISK_KIND_OPTICAL   (2)
#define DISK_KIND_NET       (3)

#define VOL_UNREADABLE 999

#define IS_EXTENDED_PART_TYPE(type) ((type) == 0x05 || (type) == 0x0f || (type) == 0x85)

// GPT attributes of interest to us for Freedesktop.org Discoverable
// Partitions Specification....
#define GPT_READ_ONLY     0x1000000000000000
#define GPT_NO_AUTOMOUNT  0x8000000000000000

// Partition names to be ignored when setting volume name
#define IGNORE_PARTITION_NAMES L"Microsoft basic data,Linux filesystem,Apple HFS/HFS+"

extern EFI_GUID gFreedesktopRootGuid;

INTN FindMem (
    IN VOID *Buffer,
    IN UINTN BufferLength,
    IN VOID *SearchString,
    IN UINTN SearchStringLength
);

EFI_STATUS FindVarsDir (VOID);
EFI_STATUS ReinitRefitLib (VOID);
EFI_STATUS InitRefitLib (IN EFI_HANDLE ImageHandle);
EFI_STATUS DirIterClose (IN OUT REFIT_DIR_ITER *DirIter);
EFI_STATUS EfivarGetRaw (
    IN  EFI_GUID  *VendorGUID,
    IN  CHAR16    *VariableName,
    OUT VOID     **VariableData,
    OUT UINTN     *VariableSize     OPTIONAL
);
EFI_STATUS EfivarSetRaw (
    IN  EFI_GUID  *VendorGUID,
    IN  CHAR16    *VariableName,
    IN  VOID      *VariableData,
    IN  UINTN      VariableSize,
    IN  BOOLEAN    Persistent
);
EFI_STATUS DirNextEntry (
    IN EFI_FILE *Directory,
    IN OUT EFI_FILE_INFO **DirEntry,
    IN UINTN FilterMode
);

VOID ScanVolumes (VOID);
VOID ReinitVolumes (VOID);
VOID UninitRefitLib (VOID);
VOID SetVolumeIcons (VOID);

#if REFIT_DEBUG > 0
VOID DebugOneVolume (REFIT_VOLUME *Volume, CHAR16 *TitleEntry, CHAR16 *EntryLoaderPath);
#else
#define DebugOneVolume(...)
#endif

VOID MyFreePoolProc (IN OUT VOID **Pointer);
#define MyFreePool(a) do { MyFreePoolProc((VOID **)(a)); if (sizeof((a)) != sizeof(VOID *) || sizeof(*(a)) != sizeof(VOID *) || sizeof(**(a)) == 0) MsgLog ("What!!??\n"); } while (0)

VOID RetainVolume (REFIT_VOLUME *Volume);
VOID AllocateVolume (REFIT_VOLUME **Volume);
REFIT_VOLUME * CopyVolume (REFIT_VOLUME *VolumeToCopy);
VOID FreeVolume (REFIT_VOLUME **Volume);
VOID AssignVolume (REFIT_VOLUME **Volume, REFIT_VOLUME *NewVolume);
VOID AddToVolumeList (REFIT_VOLUME ***Volumes, UINTN *VolumesCount, REFIT_VOLUME *Volume);
VOID FreeVolumes (REFIT_VOLUME ***Volumes, UINTN *VolumesCount);


#if REFIT_DEBUG > 0
VOID LEAKABLEPOOLSTR (PoolStr *Str, CHAR8 *Description);
VOID LEAKABLEONEPOOLSTRProc(PoolStr *Str, CHAR8 *Description);
#define LEAKABLEONEPOOLSTR(Str, Description) LEAKABLEONEPOOLSTRProc(Str ## _PS_, Description)
VOID LEAKABLEPOOLIMAGE (PoolImage *Image);
VOID LEAKABLEVOLUME (REFIT_VOLUME *Volume);
VOID LEAKABLEVOLUMES ();
#else
#define LEAKABLEPOOLIMAGE(...)
#define LEAKABLEPOOLSTR(...)
#define LEAKABLEONEPOOLSTR(...)
#define LEAKABLEVOLUME(...)
#define LEAKABLEVOLUMES(...)
#endif


VOID EraseUint32List (UINT32_LIST **TheList);
VOID SetVolumeBadgeIcon (REFIT_VOLUME *Volume);
VOID CleanUpPathNameSlashes (IN OUT CHAR16 *PathName);
VOID FreeList (IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount);
VOID SplitPathName (CHAR16 *InPath, CHAR16 **VolName, CHAR16 **Path, CHAR16 **Filename);
VOID CreateList (OUT VOID ***ListPtr, OUT UINTN *ElementCount, IN UINTN InitialElementCount);
VOID AddListElementSized (IN OUT VOID **ListPtr, IN OUT UINTN *ElementCount, IN VOID *NewElement, IN UINTN ElementSize);
VOID AddListElement (IN OUT VOID ***ListPtr, IN OUT UINTN *ElementCount, IN VOID *NewElement);
VOID DirIterOpen (
    IN EFI_FILE *BaseDir,
    IN CHAR16 *RelativePath OPTIONAL,
    OUT REFIT_DIR_ITER *DirIter
);
VOID FindVolumeAndFilename (
    IN EFI_DEVICE_PATH *loadpath,
    OUT REFIT_VOLUME **DeviceVolume,
    OUT CHAR16 **loader
);

CHAR16 *Basename (IN CHAR16 *Path);
CHAR16 *FindPath (IN CHAR16* FullPath);
CHAR16 *FindExtension (IN CHAR16 *Path);
CHAR16 *FindLastDirName (IN CHAR16 *Path);
CHAR16 *StripEfiExtension (CHAR16 *FileName);
CHAR16 *GetVolumeName (IN REFIT_VOLUME *Volume);
CHAR16 *SplitDeviceString (IN OUT CHAR16 *InString);

BOOLEAN EjectMedia (VOID);
BOOLEAN HasWindowsBiosBootFiles (REFIT_VOLUME *Volume);
VOID DumpHexString (IN UINTN DataSize, IN VOID *UserData);
BOOLEAN GuidsAreEqual (EFI_GUID *Guid1, EFI_GUID *Guid2);
BOOLEAN FindVolume (REFIT_VOLUME **Volume, CHAR16 *Identifier);
BOOLEAN FileExists (IN EFI_FILE *BaseDir, IN CHAR16 *RelativePath);
BOOLEAN SplitVolumeAndFilename (IN OUT CHAR16 **Path, OUT CHAR16 **VolName);
BOOLEAN VolumeMatchesDescription (REFIT_VOLUME *Volume, CHAR16 *Description);
BOOLEAN FilenameIn (
    IN REFIT_VOLUME *Volume,
    IN CHAR16 *Directory,
    IN CHAR16 *Filename,
    IN CHAR16 *List
);
BOOLEAN DirIterNext (
    IN OUT REFIT_DIR_ITER *DirIter,
    IN UINTN FilterMode,
    IN CHAR16 *FilePattern OPTIONAL,
    OUT EFI_FILE_INFO **DirEntry
);
#endif
