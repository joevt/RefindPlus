/*
 * MainLoader/leaks.h
 * UEFI Pool allocation debugging functions
 *
 * Copyright (c) 2021 Joe van Tunen
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
 *  * Neither the name of the author nor the names of the
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

#ifndef __LEAKS_H_
#define __LEAKS_H_

#include "globalExtra.h"
#include <Protocol/LoadedImage.h>

extern const CHAR8 * kLeakableWhatGetDebugLogFileCreate;
extern const CHAR8 * kLeakableWhatGetDebugLogFileOpen;
extern const CHAR8 * kLeakableWhatLibOpenRootOpenVolume;
extern const CHAR8 * kLeakableWhatSaveMessageToDebugLogFile;
extern const CHAR8 * kLeakableWhatdaConnectControllerConnectController;
extern const CHAR8 * kLeakableWhatConnectControllerExOrigConnectController;
extern const CHAR8 * kLeakableWhategLoadFileStart;
extern const CHAR8 * kLeakableWhatFileExistsOpen;
extern const CHAR8 * kLeakableWhatStartEFIImageStartImage;


typedef struct {
    UINTN StackAddress;
    UINTN IPAddress;
    UINTN FramePointerAddress;
} StackFrame;

enum {
    kLeakableVolumes = 1000,
    kLeakableRecoveryVolumes,
    kLeakablePreBootVolumes,
    kLeakableDataVolumes,
    kLeakableSystemVolumes,
    kLeakablePartitions = 2000,
    kLeakableHandles = 3000,
    kLeakableBuiltinIcons = 4000,
    kLeakableMenuMain = 5000,
    kLeakableMenuAbout,
    kLeakableMenuCleanNvram,
    kLeakableMenuBootKicker,
    kLeakableCsrValues = 6000,
    kLeakableApfsPrivateData = 7000,
};

typedef enum {
    ImageFlag_ProtocolLoaded = 1,
    ImageFlag_Include = 2,
    ImageFlag_NameLoaded = 4
} LoadedImageFlags;

typedef struct {
    EFI_HANDLE Handle;
    EFI_LOADED_IMAGE_PROTOCOL *Protocol;
    CHAR16 *Name;
    UINTN ImageFlags;
} LoadedImageRec;


VOID DebugLoop ();

#if REFIT_DEBUG > 0

INTN LogPoolProc (
    IN VOID *Pointer, IN VOID **AtPointer, IN CHAR8 *StrPointer,
    IN OUT UINTN *type,
    IN OUT VOID **head,
    IN OUT VOID **tail,
    IN CHAR8 *f, UINTN l,
    BOOLEAN always,
    BOOLEAN DoDumpCallStack
);


VOID LeakableProc (VOID *Buffer, CHAR8 *What, BOOLEAN IncludePath);
VOID LEAKABLEPATHINIT (UINT16 LeakableObjectID);
VOID LEAKABLEPATHDONE ();
VOID LEAKABLEPATHINC ();
VOID LEAKABLEPATHDEC ();
VOID LEAKABLEPATHSETID (UINT16 ID);
VOID LEAKABLEPATHSET (VOID *Buffer);
VOID LEAKABLEPATHUNSET ();
VOID LEAKABLEWITHPATH (VOID *object, CHAR8 *description);
#define LEAKABLE(object,discription) LeakableProc(object,discription, FALSE)
VOID LEAKABLEEXTERNALSTART (const CHAR8 *description);
VOID LEAKABLEEXTERNALSTOP ();

INTN PoolVersion (VOID *Pointer);

UINTN
GetNextAllocationNum ();

VOID
DumpAllocations (
    UINTN MinAllocation,
    BOOLEAN ExcludeLeakable,
    UINTN MinStackLength
);

VOID
ReMapPoolFunctions ();

StackFrame *
GetCallStack (
    IN UINTN StackAddress,
    IN UINTN IpAddress,
    IN UINTN FramePointerAddress,
    BOOLEAN DoFullScan,
    LoadedImageRec *LoadedImages
);

VOID
FreeCallStack (
    StackFrame *Stack
);

VOID
DumpCallStack (
    StackFrame *Stack,
    BOOLEAN DoDumpLoadedImages
);

VOID
AdjustStackMax ();

VOID
SetStackScanType (BOOLEAN DoFullScan);

#else

#define LogPoolProc(...) (0)
#define LEAKABLEPATHINIT(...)
#define LEAKABLEPATHDONE(...)
#define LEAKABLEPATHINC(...)
#define LEAKABLEPATHDEC(...)
#define LEAKABLEPATHSETID(...)
#define LEAKABLEPATHSET(...)
#define LEAKABLEPATHUNSET(...)
#define LEAKABLEWITHPATH(...)
#define LEAKABLE(...)
#define LEAKABLEEXTERNALSTART(...)
#define LEAKABLEEXTERNALSTOP(...)


#define PoolVersion(...) (0)

#define GetNextAllocationNum(...) (0)
#define DumpAllocations(...)
#define ReMapPoolFunctions(...)
#define DumpCallStack(...)
#define AdjustStackMax(...)
#define SetStackScanType(...)

#endif

#endif // __LEAKS_H
