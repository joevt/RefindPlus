/*
 * MainLoader/leaks.c
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

#include "leaks.h"
#include "lib.h"
#include "asm.h"
#include "MemLogLib.h"
#include "BootLog.h"


UINTN DebugLoopVar = 1;

VOID DebugLoop () {
    while (DebugLoopVar++) {
        AsciiPrint ("%d\n", DebugLoopVar);
    }
}


#if REFIT_DEBUG > 0


#define EFI_SIGNATURE_32(a, b, c, d) SIGNATURE_32(a, b, c, d)

#define EFI_FIELD_OFFSET(TYPE,Field) ((UINTN)(&(((TYPE *) 0)->Field)))

#define POOL_HEAD_SIGNATURE   EFI_SIGNATURE_32('p','h','d','0')

typedef struct {
  UINT32          Signature;
  UINT32          Size;
  EFI_MEMORY_TYPE Type;
  UINTN           Reserved;
  CHAR8           Data[1];
} POOL_HEAD1;

typedef struct {
  UINT32          Signature;
  UINT32          Reserved;
  EFI_MEMORY_TYPE Type;
  UINTN           Size;
  CHAR8           Data[1];
} POOL_HEAD2;

#define SIZE_OF_POOL_HEAD1 EFI_FIELD_OFFSET(POOL_HEAD1,Data)
#define SIZE_OF_POOL_HEAD2 EFI_FIELD_OFFSET(POOL_HEAD2,Data)

#define POOL_TAIL_SIGNATURE   EFI_SIGNATURE_32('p','t','a','l')
typedef struct {
  UINT32      Signature;
  UINT32      Size;
} POOL_TAIL1;

typedef struct {
  UINT32      Signature;
  UINT32      Reserved;
  UINTN       Size;
} POOL_TAIL2;

#define HEAD_TO_TAIL1(a) ((POOL_TAIL1 *) (((CHAR8 *) (a)) + (a)->Size - sizeof(POOL_TAIL1)))
#define HEAD_TO_TAIL2(a) ((POOL_TAIL2 *) (((CHAR8 *) (a)) + (a)->Size - sizeof(POOL_TAIL2)))
#define DATA_TO_HEAD1(a) ((POOL_HEAD1 *) (((CHAR8 *) (a)) - SIZE_OF_POOL_HEAD1))
#define DATA_TO_HEAD2(a) ((POOL_HEAD2 *) (((CHAR8 *) (a)) - SIZE_OF_POOL_HEAD2))


INTN PoolVersion(VOID *Pointer) {
    POOL_HEAD1 *head1 = DATA_TO_HEAD1(Pointer);
    POOL_HEAD2 *head2 = DATA_TO_HEAD2(Pointer);
    // head1 = head2

    if (head1->Signature == POOL_HEAD_SIGNATURE) {
        // head is valid
        POOL_TAIL1 *tail1 = HEAD_TO_TAIL1(head1);
        POOL_TAIL2 *tail2 = HEAD_TO_TAIL2(head2);

        if ( (VOID*)tail1 >= (VOID*)&head1->Data ) {
            // tail1 is valid
            if ( (VOID*)tail2 >= (VOID*)&head2->Data ) {
                // tail2 is valid
                if ((VOID*)tail1 == (VOID*)tail2) {
                    // the tails are the same location
                    if (tail1->Signature == POOL_TAIL_SIGNATURE) {
                        if (tail1->Size == head1->Size) return 1;
                        if (tail2->Size == head2->Size) return 2;
                        return -1;
                    }
                    return -2;
                }
                if ((VOID*)tail1 < (VOID*)tail2) {
                    // tail1 is closer
                    if (tail1->Signature == POOL_TAIL_SIGNATURE && tail1->Size == head1->Size) return 1;
                    if (tail2->Signature == POOL_TAIL_SIGNATURE && tail2->Size == head2->Size) return 2;
                    if (tail1->Signature == POOL_TAIL_SIGNATURE) return -3;
                    if (tail2->Signature == POOL_TAIL_SIGNATURE) return -4;
                    return -5;
                }
                // tail2 is closer
                if (tail2->Signature == POOL_TAIL_SIGNATURE && tail2->Size == head2->Size) return 2;
                if (tail1->Signature == POOL_TAIL_SIGNATURE && tail1->Size == head1->Size) return 1;
                if (tail2->Signature == POOL_TAIL_SIGNATURE) return -6;
                if (tail1->Signature == POOL_TAIL_SIGNATURE) return -7;
                return -8;
            }
            // only tail1 is valid
            if (tail1->Signature == POOL_TAIL_SIGNATURE && tail1->Size == head1->Size) return 1;
            if (tail1->Signature == POOL_TAIL_SIGNATURE) return -9;
            return -10;
        }
        if ( (VOID*)tail2 >= (VOID*)&head2->Data ) {
            // only tail2 is valid
            if (tail2->Signature == POOL_TAIL_SIGNATURE && tail2->Size == head2->Size) return 2;
            if (tail2->Signature == POOL_TAIL_SIGNATURE) return -11;
            return -12;
        }
        // neither tail is valid
        return -13;
    }
    // head is not valid
    return -14;
}


INTN
LogPoolProc (
    IN VOID *Pointer, IN VOID **AtPointer, IN CHAR8 *StrPointer,
    IN OUT UINTN *type,
    IN OUT VOID **head,
    IN OUT VOID **tail,
    IN CHAR8 *f, UINTN l,
    BOOLEAN always,
    BOOLEAN DoDumpCStack
) {
    INTN result;

    static INTN curLogProc = 0;

    do {
        if (!Pointer) {
            DoDumpCStack = FALSE;
            result = -100; break;
        }

        {
            // recursion protection
            static INTN maxLogProc = 0;
            static INTN maxReported = 0;

            curLogProc++;
            if (curLogProc > maxLogProc) maxLogProc = curLogProc;
            if (curLogProc > 1) {
                result = 1; break;
            }
            if (maxLogProc > maxReported) {
                maxReported = maxLogProc;
                if (maxReported > 1) {
                    MsgLog ("LogPoolProc maxReported:%d\n", maxReported);
                }
            }
        }
    
        POOL_HEAD1 *head1 = DATA_TO_HEAD1(Pointer);
        POOL_HEAD2 *head2 = DATA_TO_HEAD2(Pointer);
        POOL_TAIL1 *tail1 = HEAD_TO_TAIL1(head1);
        POOL_TAIL2 *tail2 = HEAD_TO_TAIL2(head2);
        UINTN head1size = head1->Size - sizeof(POOL_TAIL1) - SIZE_OF_POOL_HEAD1;
        UINTN head2size = head2->Size - sizeof(POOL_TAIL2) - SIZE_OF_POOL_HEAD2;
        result = PoolVersion(Pointer);
        switch (result) {
            case 1:
                if (always) {
                    MsgLog ("%a:%d &%a:%p->%p size:%d type:%d\n", f, l, StrPointer, AtPointer, Pointer, head1size, head1->Type);
                }
                else {
                    DoDumpCStack = FALSE;
                }
                if (head) *head = head1;
                if (tail) *tail = tail1;
                if (type) *type = 1;
                result = head1size;
                break;
            case 2:
                if (always) {
                    MsgLog ("%a:%d &%a:%p->%p size:%d type:%d\n", f, l, StrPointer, AtPointer, Pointer, head2size, head2->Type);
                }
                else {
                    DoDumpCStack = FALSE;
                }
                if (head) *head = head2;
                if (tail) *tail = tail2;
                if (type) *type = 2;
                result = head2size;
                break;
            case -1: case -2: case -3: case -5: case -7: case -9: case -10:
                MsgLog ("%a:%d &%a:%p->%p problem:%d head:%016lX%016lX%016lX tail:%016lX%016lX\n", f, l, StrPointer, AtPointer, Pointer,
                    result, ((UINTN*)head1)[0], ((UINTN*)head1)[1], ((UINTN*)head1)[2], ((UINTN*)tail1)[0], ((UINTN*)tail1)[1]
                );
                break;
            case -4: case -6: case -8: case -11: case -12:
                MsgLog ("%a:%d &%a:%p->%p problem:%d head:%016lX%016lX%016lX tail:%016lX%016lX\n", f, l, StrPointer, AtPointer, Pointer,
                    result, ((UINTN*)head2)[0], ((UINTN*)head2)[1], ((UINTN*)head2)[2], ((UINTN*)tail2)[0], ((UINTN*)tail2)[1]
                );
                break;
            case -13: case -14: default:
                MsgLog ("%a:%d &%a:%p->%p problem:%d head:%016lX%016lX%016lX\n", f, l, StrPointer, AtPointer, Pointer,
                    result, ((UINTN*)head1)[0], ((UINTN*)head1)[1], ((UINTN*)head1)[2]
                );
                break;
        }
    } while (0);
    
    if (DoDumpCStack) {
        DumpCallStack (NULL, FALSE);
    }
    
    curLogProc--;

    return result;
}


/*
INTN PoolSize (IN VOID *Pointer) {
    return LogPoolProc (Pointer, __FILE__, __LINE__, FALSE, TRUE);
}

BOOLEAN IsPool (IN VOID *Pointer) {
    return PoolSize (Pointer) >= 0;
}
*/


// Since we don't patch every loaded image's version of AllocatePool, we may get some allocations that are
// not created by our AllocatePoolEx routine. We override some other functions that might make such an
// allocation.
typedef enum {
    kAllocationTypeAllocatePool,
    kAllocationTypeFindLoadedImageFileName,
    kAllocationTypeOpenProtocolInformation,
    kAllocationTypeProtocolsPerHandle,
    kAllocationTypeLocateHandleBuffer
} AllocationTypeEnum;

// Some functions we do not override at all, but we can surround calls to them with LEAKABLEEXTERNALSTART and LEAKABLEEXTERNALSTOP to set a flag.
#define LEAKABLEEXTERNALMAX 20
UINTN LEAKABLEEXTERNALNEXT = 0;
static CHAR8* LEAKABLEEXTERNALSTACK[LEAKABLEEXTERNALMAX];

typedef enum {
    kAllocFlagExternal = 1
} AllocFlags;

typedef struct Allocation Allocation;
struct Allocation {
    Allocation* Next;            // Needs to be first element, points to the next allocation.
    VOID *Where;                 // Address in pool.
    UINTN Size;                  // Size of allocation (should be less than pool size).
    UINTN Num;                   // When it was allocated - it is a number that increases with each allocation, not a time stamp (maybe we should use a timestamp)
    AllocationTypeEnum Type;     // What function did the allocation (usually kAllocationTypeAllocatePool).
    UINTN AllocFlags;            // AllocFlags
    CHAR8 *What;                 // A constant string used for allocations that are allowed to leak once (must be a literal - or we could use another enumeration or a string compare (strings need a destructor if pool allocated))
    UINT16 *Path;                // A path describing multiple objects in a list or tree that is allowed to leak once (for the entire list or tree). See LEAKABLEPATH.
    StackFrame *Stack;           // The callstack when the allocation was created, used to help find the source of leaks.
};

UINTN StackMin = 0;
UINTN StackMax = 0;

EFI_ALLOCATE_POOL OrigAllocatePool = NULL;
EFI_FREE_POOL OrigFreePool = NULL;
EFI_OPEN_PROTOCOL_INFORMATION OrigOpenProtocolInformation = NULL;
EFI_PROTOCOLS_PER_HANDLE OrigProtocolsPerHandle = NULL;
EFI_LOCATE_HANDLE_BUFFER OrigLocateHandleBuffer = NULL;
EFI_CONNECT_CONTROLLER OrigConnectController = NULL;

#define LEAKS_FULL_SCAN 0
#define LEAKS_FILL_FREED 0
#define LEAKS_FILL_STACK 1
#define LEAKS_INVALIDATE_TAIL 1
#define LEAKS_DUMP_STACK 1
#define LEAKS_SHOW_PATHS 0
#define LEAKS_DO_CLEANUP 1


VOID
CheckStackPointer () {
    if (StackMin && AsmGetStackPointerAddress() < (StackMin + 8000)) {
        DebugLoop ();
    }
}


VOID *
LeaksAllocatePool (
    UINTN Size
) {
    CheckStackPointer ();
    VOID * Result = NULL;
    if (OrigAllocatePool) {
        OrigAllocatePool (EfiBootServicesData, Size, &Result);
    }
    else {
        STATIC BOOLEAN DidDumpMessage = FALSE;
        if (!DidDumpMessage) {
            DidDumpMessage = TRUE;
            MsgLog ("Allocation Error: OrigAllocatePool is not set yet\n");
            //DumpCallStack (NULL, FALSE);
        }
        Result = AllocatePool (Size);
    }
    return Result;
}


// Only use this on objects created by LeaksAllocatePool
VOID
LeaksFreePool (
    VOID **Buffer
) {
    CheckStackPointer ();
    if (Buffer) {
        if (OrigFreePool) {
            OrigFreePool (*Buffer);
            *Buffer = NULL;
        }
        else {
            STATIC BOOLEAN DidDumpMessage = FALSE;
            if (!DidDumpMessage) {
                DidDumpMessage = TRUE;
                MsgLog ("Allocation Error: OrigFreePool is not set yet\n");
                //DumpCallStack (NULL, FALSE);
            }
            MyFreePool (Buffer);
        }
    }
}


STATIC Allocation* AllocationsList = NULL;
STATIC Allocation* FreeAllocations = NULL;
STATIC INTN DoingAlloc = 0;
STATIC UINTN NextAllocationNum = 0;


STATIC
CHAR16 *
AllocationTypeString (
        AllocationTypeEnum AllocationType
) {
    CheckStackPointer ();
    switch (AllocationType) {
        case kAllocationTypeAllocatePool            : return L"AllocatePoolEx";
        case kAllocationTypeFindLoadedImageFileName : return L"FindLoadedImageFileName";
        case kAllocationTypeOpenProtocolInformation : return L"OpenProtocolInformationEx";
        case kAllocationTypeProtocolsPerHandle      : return L"ProtocolsPerHandleEx";
        case kAllocationTypeLocateHandleBuffer      : return L"LocateHandleBufferEx";
        default                                     : return L"Unknown";
    }
}


STATIC
VOID
RemoveAllocation (
    Allocation *prev,
    Allocation *a
) {
    CheckStackPointer ();
    prev->Next = a->Next;
    if (a->Stack) {
        #if LEAKS_DO_CLEANUP
        FreeCallStack (a->Stack);
        #endif
        a->Stack = NULL;
    }
    if (a->Path) {
        #if LEAKS_DO_CLEANUP
        LeaksFreePool ((VOID **) &a->Path);
        #endif
        a->Path = NULL;
    }

    a->Next = FreeAllocations;
    FreeAllocations = a;
}


STATIC
VOID
GetStackLimits (
    IN UINTN FramePointerAddress
) {
    CheckStackPointer ();
    if (FramePointerAddress < StackMin || FramePointerAddress >= StackMax) {
        MsgLog ("[ GetStackLimits\n");
        UINTN MemoryMapSize = 0;
        EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
        UINTN MapKey;
        UINTN DescriptorSize;
        UINT32 DescriptorVersion;
        EFI_STATUS Status;

        Status = gBS->GetMemoryMap (
            &MemoryMapSize,
            NULL,
            &MapKey,
            &DescriptorSize,
            &DescriptorVersion
        );
        
        do {
            MemoryMap = LeaksAllocatePool (MemoryMapSize);
            if (MemoryMap) {
                Status = gBS->GetMemoryMap (
                    &MemoryMapSize,
                    MemoryMap,
                    &MapKey,
                    &DescriptorSize,
                    &DescriptorVersion
                );
            }
            else {
                Status = EFI_OUT_OF_RESOURCES;
            }
        } while (Status == EFI_BUFFER_TOO_SMALL);

        if (!EFI_ERROR(Status)) {
            EFI_MEMORY_DESCRIPTOR *MemoryMapEntry;
            EFI_MEMORY_DESCRIPTOR *MemoryMapEnd;
            
            MemoryMapEntry = MemoryMap;
            MemoryMapEnd = (EFI_MEMORY_DESCRIPTOR *) ((UINT8 *) MemoryMap + MemoryMapSize);

            while (MemoryMapEntry < MemoryMapEnd) {
                UINTN Min = MemoryMapEntry->PhysicalStart;
                UINTN Max = Min + MultU64x32 (MemoryMapEntry->NumberOfPages, EFI_PAGE_SIZE);
                if (FramePointerAddress >= Min && FramePointerAddress < Max) {
                    StackMin = Min;
                    StackMax = Max;
                    MsgLog ("Stack Memory: %p..%p Type:%d\n", Min, Max, MemoryMapEntry->Type);
                    break;
                }
                MemoryMapEntry = NEXT_MEMORY_DESCRIPTOR (MemoryMapEntry, DescriptorSize);
            }
            LeaksFreePool ((VOID **) &MemoryMap);
        }
        MsgLog ("] GetStackLimits\n");
    }
}


STATIC
VOID
AddAllocation(
    VOID *Buffer,
    UINTN Size,
    AllocationTypeEnum Type
);


/**
  Function to find the file name associated with a LoadedImageProtocol.

  @param[in] LoadedImage     An instance of LoadedImageProtocol.

  @retval                    A string representation of the file name associated
                             with LoadedImage, or NULL if no name can be found.
**/
STATIC
CHAR16*
FindLoadedImageFileName (
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage
  )
{
    EFI_GUID                       *NameGuid;
    EFI_STATUS                     Status;
    EFI_FIRMWARE_VOLUME2_PROTOCOL  *Fv2;
    EFI_FIRMWARE_VOLUME_PROTOCOL   *Fv1;
    VOID                           *Buffer = NULL;
    UINTN                          BufferSize = 0;
    UINT32                         AuthenticationStatus;
  
    if ((LoadedImage == NULL) || (LoadedImage->FilePath == NULL)) {
        return NULL;
    }
  
    NameGuid = EfiGetNameGuidFromFwVolDevicePathNode((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)LoadedImage->FilePath);
    if (NameGuid) {
        /*        */ {
            //
            // Get the FirmwareVolume2Protocol of the device handle that this image was loaded from.
            //
            Status = gBS->HandleProtocol (LoadedImage->DeviceHandle, &gEfiFirmwareVolume2ProtocolGuid, (VOID**) &Fv2);
            //MsgLog ("gBS->HandleProtocol gEfiFirmwareVolume2ProtocolGuid Status:%r Fv2:%p\n", Status, Fv2);
  
            //
            // FirmwareVolume2Protocol is PI, and is not required to be available.
            //
            if (!EFI_ERROR (Status)) {
                //
                // Read the user interface section of the image.
                //
                BufferSize = 0;
                Status = Fv2->ReadSection(Fv2, NameGuid, EFI_SECTION_USER_INTERFACE, 0, &Buffer, &BufferSize, &AuthenticationStatus);
                //MsgLog ("Fv2->ReadSection Status:%r Name:%s\n", Status, Buffer ? Buffer : L"NULL");
            }
        }
    
        if (!Buffer) {
            //
            // Get the FirmwareVolume1Protocol of the device handle that this image was loaded from.
            //
            Status = gBS->HandleProtocol (LoadedImage->DeviceHandle, &gEfiFirmwareVolumeProtocolGuid, (VOID**) &Fv1);
            //MsgLog ("gBS->HandleProtocol gEfiFirmwareVolume1ProtocolGuid Status:%r Fv1:%p\n", Status, Fv1);
        
            if (!EFI_ERROR (Status)) {
                //
                // Read the user interface section of the image.
                //
                BufferSize = 0;
                Status = Fv1->ReadSection(Fv1, NameGuid, EFI_SECTION_USER_INTERFACE, 0, &Buffer, &BufferSize, &AuthenticationStatus);
                //MsgLog ("Fv1->ReadSection Status:'%r' Guid:%g Name:%s\n", Status, NameGuid, Buffer ? Buffer : L"NULL");
            }
        }

        if (!Buffer) {
            Buffer = PoolPrint (L"%g", NameGuid);
        }
    }
    else {
        Buffer = ConvertDevicePathToText (LoadedImage->FilePath, TRUE, TRUE);
        if (!Buffer) {
            Buffer = PoolPrint (L"Handle%p", LoadedImage->DeviceHandle);
        }
    }

    if (Buffer) {
        // need to keep track of allocations made by Fv->ReadSection so we don't get an error when we free the allocation
        AddAllocation (Buffer, BufferSize, kAllocationTypeFindLoadedImageFileName);
    }

    //
    // ReadSection returns just the section data, without any section header. For
    // a user interface section, the only data is the file name.
    //
    return Buffer;
}


STATIC
CHAR16 *
GetLoadedImageName (
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage
) {
    CheckStackPointer ();
    CHAR16 *FileName = NULL;
    if (LoadedImage) {
        //DumpHexString (64 /* GetDevicePathSize (LoadedImage->FilePath) */, LoadedImage->FilePath); // GetDevicePathSize may crash for paths in EFI 1.1 that use type 0xFF
        FileName = FindLoadedImageFileName (LoadedImage);
    }
    return FileName;
}


STATIC
VOID
LoadedImageGetProtocol (
    LoadedImageRec *LoadedImage
) {
    CheckStackPointer ();
    if (LoadedImage) {
        if (!(LoadedImage->ImageFlags & ImageFlag_ProtocolLoaded)) {
            //MsgLog ("[ gBS->HandleProtocol Handle:%p\n", LoadedImage->Handle);
            gBS->HandleProtocol (
                LoadedImage->Handle,
                &gEfiLoadedImageProtocolGuid,
                (VOID**)&LoadedImage->Protocol
            );
            //MsgLog ("] gBS->HandleProtocol Protocol:%p\n", LoadedImage->Protocol);
            LoadedImage->ImageFlags |= ImageFlag_ProtocolLoaded;
        }
    }
}


STATIC
VOID
LoadedImageGetName (
    LoadedImageRec *LoadedImage
) {
    CheckStackPointer ();
    if (LoadedImage) {
        if (!(LoadedImage->ImageFlags & ImageFlag_NameLoaded)) {
            LoadedImageGetProtocol (LoadedImage);
            LoadedImage->Name = GetLoadedImageName (LoadedImage->Protocol);
            LoadedImage->ImageFlags |= ImageFlag_NameLoaded;
        }
    }
}


STATIC
BOOLEAN
GetLoadedImageInfoForAddress (
    VOID *p,
    LoadedImageRec *LoadedImages,
    CHAR16 **Name,
    UINTN *Offset
) {
    CheckStackPointer ();
    BOOLEAN result = FALSE;
    if (Name) *Name = NULL;
    if (Offset) *Offset = 0;
    if (LoadedImages) {
        LoadedImageRec *LoadedImage;
        for (LoadedImage = LoadedImages; LoadedImage->Handle; LoadedImage++) {
            LoadedImageGetProtocol (LoadedImage);
            if (LoadedImage->Protocol) {
                if (p >= LoadedImage->Protocol->ImageBase && p < LoadedImage->Protocol->ImageBase + LoadedImage->Protocol->ImageSize) {
                    if (Name) {
                        LoadedImageGetName (LoadedImage);
                        *Name = LoadedImage->Name;
                    }
                    if (Offset) {
                        *Offset = p - LoadedImage->Protocol->ImageBase;
                    }
                    LoadedImage->ImageFlags |= ImageFlag_Include;
                    result = TRUE;
                    break;
                }
            } // if LoadedImage
        } // for LoadedImage
    } // if LoadedImages
    return result;
} // GetLoadedImageInfoForAddress


StackFrame *
GetCallStack (
    IN UINTN StackAddress,
    IN UINTN IpAddress,
    IN UINTN FramePointerAddress,
    BOOLEAN DoFullScan,
    LoadedImageRec *LoadedImages
) {
    CheckStackPointer ();
    DoingAlloc++;

    StackFrame *CallStack = NULL;

    INTN FrameCount;
    INTN AllocFrameCount;

    UINTN CurrentStackAddress;
    UINTN CurrentIpAddress;
    UINTN CurrentFramePointerAddress;
    UINTN NextFramePointerAddress;
    BOOLEAN dumpit = FALSE;
    
    GetStackLimits (FramePointerAddress);
    
    INTN Pass;
    for (Pass = 0; Pass < 2; Pass++) {
        if (Pass) {
            AllocFrameCount = FrameCount;
            CallStack = LeaksAllocatePool (FrameCount * sizeof(*CallStack));
            if (!CallStack)
                break;
        }
        FrameCount = 0;
        CurrentStackAddress = StackAddress;
        CurrentIpAddress = IpAddress;
        CurrentFramePointerAddress = FramePointerAddress;
        NextFramePointerAddress = CurrentFramePointerAddress - 1;
        
        for (;;) {
            if (!Pass) {
                // if (DoFullScan) MsgLog ("%d.%d: %8p %8p %8p %8p\n", Pass, FrameCount, CurrentStackAddress, CurrentFramePointerAddress, *(VOID**)CurrentFramePointerAddress, CurrentIpAddress);
            }
            if (Pass) {
                if (FrameCount >= AllocFrameCount) {
                    MsgLog ("What's wrong with the call stack!!!!!!!!!!!!!!!?????????\n");
                    break;
                }
                CallStack[FrameCount].StackAddress = CurrentStackAddress;
                CallStack[FrameCount].IPAddress = CurrentIpAddress;
                CallStack[FrameCount].FramePointerAddress = CurrentFramePointerAddress;
            }
            if (DoFullScan && FrameCount) {
                CurrentStackAddress += sizeof(VOID*);
            }
            
            FrameCount++;
            if (!CurrentIpAddress) {
                break;
            }

            if (
                (DoFullScan && CurrentStackAddress >= StackMax)
                || (
                    (!DoFullScan) && (
                        CurrentFramePointerAddress <= NextFramePointerAddress ||
                        CurrentFramePointerAddress < StackMin ||
                        CurrentFramePointerAddress >= StackMax 
                    )
                )
                || (BootLogIsPaused () && FrameCount > 200)
            ) {
                if (BootLogIsPaused () && FrameCount > 200) {
                    dumpit = TRUE;
                }
                CurrentIpAddress = 0;
            }
            else {
                if (DoFullScan) {
                    CurrentIpAddress = 0;
                    for (; CurrentStackAddress < StackMax; CurrentStackAddress += sizeof(VOID*)) {
                        UINTN PossibleReturnAddress = *(UINTN*)CurrentStackAddress;
                        if (GetLoadedImageInfoForAddress ((VOID*)PossibleReturnAddress, LoadedImages, NULL, NULL)) {
                            /* TODO: check if the return address points to after a call instruction */
                            CurrentIpAddress = PossibleReturnAddress;
                            CurrentFramePointerAddress = CurrentStackAddress - sizeof(VOID*);
                            break;
                        }
                    }
                }
                else {
                    CurrentStackAddress = CurrentFramePointerAddress + sizeof(VOID*);
                    CurrentIpAddress = AsmGetReturnAddress (CurrentFramePointerAddress);
                    NextFramePointerAddress = CurrentFramePointerAddress;
                    CurrentFramePointerAddress = AsmGetLastFramePointerAddress (CurrentFramePointerAddress);
                } // else not full scan
            } // else valid frame
        } // for
        
    } // For pass
    
    if (DoFullScan) {
    
/*
    Stack grows from StackMax (high address) to StackMin (low address)
    rbp is the current frame pointer address.
    rip is the execution instruction pointer.
    rsp is the stack pointer.

    Note that a full scan may find return addresses in the local variables if the
    variables were not cleared or the variables include function pointers.

    Currently, the full scan does not verify that an address points to after a call
    instruction so some results might be data.


    StackMin:
    
        Deepest/latest/newest data:

                   rsp: ...
                        frame0 variables        local variables
                        frame1 registers        save frame1's registers before using them
           rbp, frame0: frame1 frame pointer    points to frame1's frame pointer
    /\                  frame1 return address   points to after the call statement in frame1
    ||                  frame0 argument0        arguments are pushed in reverse order so that the
    ||                  frame0 argument1..      first argument always has the same offset from rbp
    ||          
    ||                  frame1 variables
    ||                  frame2 registers
    stack       frame1: frame2 frame pointer            
    grows               frame2 return address           
    toward              frame1 argument0
    min                 frame1 argument1..

                        frame2 variables
                        frame3 registers
                frame2: frame3 frame pointer
                        frame3 return address
                        frame2 argument0
                        frame2 argument1..
                        
        Shallowest/earliest/oldest data:
                    
    StackMax:
*/
        
        // Fix up the frame pointer address for each stack frame.
        UINTN frameIndex;
        for (frameIndex = 1; frameIndex < FrameCount; frameIndex++) {
            // The frame pointer address was initialized as the address to a possible frame pointer address (NextFramePointerAddress)
            NextFramePointerAddress = *(UINTN*)CallStack[frameIndex].FramePointerAddress;
            // Clear the frame pointer address
            CallStack[frameIndex].FramePointerAddress = 0;
            // See if the frame pointer exists in a later frame
            if (NextFramePointerAddress >= StackMin && NextFramePointerAddress < StackMax) {
                UINTN frameIndex2;
                for (frameIndex2 = frameIndex + 1; frameIndex2 < FrameCount; frameIndex2++) {
                    if (CallStack[frameIndex2].FramePointerAddress == NextFramePointerAddress) {
                        // if so then it is probably the frame pointer for this frame
                        CallStack[frameIndex].FramePointerAddress = NextFramePointerAddress;
                        break;
                    }
                }
            }
        }
    } // if DoFullScan

    if (dumpit) {
        MsgLog ("[ dumpit\n");
        DumpCallStack (CallStack, FALSE);
        MsgLog ("] dumpit\n");
    }

    DoingAlloc--;
    return CallStack;
} // GetCallStack


VOID
FreeCallStack (
    StackFrame *Stack
) {
    CheckStackPointer ();
    if (Stack) {
        LeaksFreePool ((VOID **) &Stack);
    }
}


STATIC BOOLEAN STACK_SCAN_TYPE = FALSE;
VOID
SetStackScanType (
    BOOLEAN DoFullScan
) {
    CheckStackPointer ();
    STACK_SCAN_TYPE = DoFullScan;
}


STATIC
LoadedImageRec *
GetLoadedImages(
);


STATIC
VOID
FreeLoadedImages (
    LoadedImageRec *LoadedImages
);


STATIC
VOID
AddAllocation(
    VOID *Buffer,
    UINTN Size,
    AllocationTypeEnum Type
) {
    CheckStackPointer ();
    if (DoingAlloc) {
        return;
    }

    Allocation *prev = (Allocation *)&AllocationsList;
    Allocation *a = AllocationsList;
    while (a) {
        if (a->Where == Buffer) {
            if (
                (
                    a->Size == Size
                    // Ignore size for kAllocationTypeFindLoadedImageFileName because we
                    // don't know the allocation size for that type in all cases.
                    || Type == kAllocationTypeFindLoadedImageFileName
                )
                && Type != kAllocationTypeAllocatePool
                && a->Type == kAllocationTypeAllocatePool
            ) {
                // If the size is also the same and the previous type was kAllocationTypeAllocatePool
                // and the new type is not, then we just want to change the type.
                a->Type = Type;
                return;
            }
            else {
                // if the previous type was not kAllocationTypeAllocatePool but the new type is,
                // then it probably means we missed a free so don't report it.
                BOOLEAN reportit = !(Type == kAllocationTypeAllocatePool && a->Type != kAllocationTypeAllocatePool);
                if (reportit) {
                    MsgLog ("Allocation Error: already allocated %p (%d) (was %d) Type:%s (was %s)\n",
                        Buffer, Size, a->Size, AllocationTypeString (Type), AllocationTypeString (a->Type)
                    );
                }
                StackFrame *PrevousStack = a->Stack;
                a->Stack = NULL;
                RemoveAllocation (prev, a);
                if (reportit) {
                    DumpCallStack (NULL, FALSE);
                    MsgLog ("Previous:\n");
                    if (PrevousStack) DumpCallStack (PrevousStack, FALSE);
                }
                #if LEAKS_DO_CLEANUP
                FreeCallStack (PrevousStack);
                #endif
            }
            break;
        }
        prev = a;
        a = a->Next;
    }

    if (FreeAllocations) {
        a = FreeAllocations;
        FreeAllocations = a->Next;
    }
    else {
        a = LeaksAllocatePool (sizeof(Allocation));
    }
    if (a) {
        a->Where = Buffer;
        a->Num = NextAllocationNum++;
        a->Size = Size;
        a->Type = Type;
        a->What = LEAKABLEEXTERNALNEXT ? LEAKABLEEXTERNALSTACK[LEAKABLEEXTERNALNEXT - 1] : NULL;
        a->AllocFlags = 0;
        if (a->What) {
            a->AllocFlags |= kAllocFlagExternal;
        }
        a->Path = NULL;
        a->Stack = NULL;
        if (!(a->AllocFlags & kAllocFlagExternal)) {
            if (!DoingAlloc) {
                DoingAlloc++;
                LoadedImageRec *LoadedImages = NULL;
                if (STACK_SCAN_TYPE) {
                    LoadedImages = GetLoadedImages ();
                }
                a->Stack = GetCallStack (AsmGetStackPointerAddress (), AsmGetCurrentIpAddress (), AsmGetFramePointerAddress (), STACK_SCAN_TYPE, LoadedImages);
                if (STACK_SCAN_TYPE) {
                    FreeLoadedImages (LoadedImages);
                }
                DoingAlloc--;
            }
        }

        a->Next = AllocationsList;
        AllocationsList = a;
    }
}


extern VOID *MyMemSet(VOID *s, int c, UINTN n);


STATIC
LoadedImageRec *
GetLoadedImages(
) {
    CheckStackPointer ();
    LoadedImageRec *LoadedImages = NULL;

    BootLogPause ();
    UINTN OldInAlloc = DoingAlloc++;
    
    if (!OldInAlloc) MsgLog ("[ GetLoadedImages\n");

    UINTN LoadedImagesCount = 0;
    EFI_HANDLE *LoadedImagesHandles = NULL;

    //if (!OldInAlloc) MsgLog ("[ LocateHandleBuffer\n");
    EFI_STATUS Status = gBS->LocateHandleBuffer (
        ByProtocol,
        &gEfiLoadedImageProtocolGuid,
        NULL,
        &LoadedImagesCount,
        &LoadedImagesHandles
    );
    //if (!OldInAlloc) MsgLog ("] LocateHandleBuffer\n");

    if (!EFI_ERROR(Status) && LoadedImagesHandles) {
        UINTN LoadedImagesSize = (LoadedImagesCount + 1) * sizeof(LoadedImageRec);
        LoadedImages = LeaksAllocatePool (LoadedImagesSize);
        if (LoadedImages) {
            MyMemSet(LoadedImages, 0, LoadedImagesSize);
            UINTN LoadedImageIndex;
            for (LoadedImageIndex = 0; LoadedImageIndex < LoadedImagesCount; LoadedImageIndex++) {
                LoadedImages[LoadedImageIndex].Handle = LoadedImagesHandles[LoadedImageIndex];
            }
        }
        MyFreePool (&LoadedImagesHandles);
    }

    if (!OldInAlloc) MsgLog ("] GetLoadedImages %p\n", LoadedImages);

    DoingAlloc--;
    BootLogResume ();

    return LoadedImages;
} // GetLoadedImages


STATIC
VOID
DumpLoadedImages (
    LoadedImageRec *LoadedImages
) {
    CheckStackPointer ();
    if (LoadedImages) {
        LoadedImageRec *LoadedImage;
        BOOLEAN NeedTitle = TRUE;
        for (LoadedImage = LoadedImages; LoadedImage->Handle; LoadedImage++) {
            if (LoadedImage->ImageFlags & ImageFlag_Include) {
                if (NeedTitle) {
                    MsgLog ("Loaded Images:\n");
                    NeedTitle = FALSE;
                }
                LoadedImageGetName (LoadedImage);
                if (LoadedImage->Protocol) {
                    MsgLog ("  %p..%p %s\n",
                        LoadedImage->Protocol->ImageBase,
                        LoadedImage->Protocol->ImageBase + LoadedImage->Protocol->ImageSize - 1,
                        LoadedImage->Name ? LoadedImage->Name : L"<noname>"
                    );
                } else {
                    MsgLog ("  Handle:%p <noimage>\n", LoadedImage->Handle);
                }
            }
        } // for LoadedImage
    } // if LoadedImagesHandles
} // DumpLoadedImages


STATIC
VOID
FreeLoadedImages (
    LoadedImageRec *LoadedImages
) {
    CheckStackPointer ();
    if (LoadedImages) {
        LoadedImageRec *LoadedImage;
        for (LoadedImage = LoadedImages; LoadedImage->Handle; LoadedImage++) {
            if (LoadedImage->Name) {
                MyFreePool (&LoadedImage->Name);
            }
        } // for LoadedImage
        LeaksFreePool ((VOID **) &LoadedImages);
    } // if LoadedImagesHandles
}


STATIC UINTN DumpingStack = 0;


STATIC
VOID
DumpOneCallStack (
    StackFrame *Stack,
    LoadedImageRec *LoadedImages
) {
    CheckStackPointer ();
    if (!Stack) {
        return;
    }
        
    if (DumpingStack >= 4) {
        return;
    }

    DumpingStack++;
    UINTN FrameCount;
    VOID *p;
    MsgLog ("Call Stack:\n");
    for (FrameCount = 0; (p = (VOID*)Stack[FrameCount].IPAddress); FrameCount++) {
        CHAR16 *Name;
        UINTN Offset;
        GetLoadedImageInfoForAddress (p, LoadedImages, &Name, &Offset);
        /*
        Subtract one from the return address so that it points into the call 
        instruction. This gives better results for `dwarfdump --lookup`.
        */
        #if 1
        if (Name) {
            MsgLog ("  %2d: %8p %8p %p = %s+0x%X\n", FrameCount, Stack[FrameCount].StackAddress, Stack[FrameCount].FramePointerAddress, p - 1, Name, Offset - 1);
        }
        else {
            MsgLog ("  %2d: %8p %8p %p\n", FrameCount, Stack[FrameCount].StackAddress, Stack[FrameCount].FramePointerAddress, p - 1);
        }
        #else
        if (Name) {
            MsgLog ("  %2d: %p = %s+0x%X\n", FrameCount, p - 1, Name, Offset - 1);
        }
        else {
            MsgLog ("  %2d: %p\n", FrameCount, p - 1);
        }
        #endif
    }
    DumpingStack--;
}


VOID
DumpCallStack (
    StackFrame *Stack,
    BOOLEAN DoDumpLoadedImages
) {
    CheckStackPointer ();
    #if LEAKS_DUMP_STACK
    if (DumpingStack >= 4) {
        return;
    }

    DumpingStack++;

    if (Stack) {
        MsgLog ("[ DumpCallStack %p\n", Stack);
    }
    else {
        MsgLog ("[ DumpCallStack CurrentStack\n");
    }
    LoadedImageRec *LoadedImages = GetLoadedImages ();
    
    BOOLEAN DoFreeStack = FALSE;
    if (!Stack) {
        Stack = GetCallStack (AsmGetStackPointerAddress (), AsmGetCurrentIpAddress (), AsmGetFramePointerAddress (), STACK_SCAN_TYPE, LoadedImages);
        DoFreeStack = TRUE;
    }
    DumpOneCallStack (Stack, LoadedImages);
    if (DoDumpLoadedImages) {
        DumpLoadedImages (LoadedImages);
    }
    FreeLoadedImages (LoadedImages);
    if (DoFreeStack) {
        FreeCallStack (Stack);
    }
    MsgLog ("] DumpCallStack\n");

    DumpingStack--;
    #endif
}


STATIC
UINTN
GetCallStackLength (
    StackFrame *Stack
) {
    CheckStackPointer ();
    UINTN FrameCount = 0;
    if (Stack) {
        VOID *p;
        for (; (p = (VOID*)Stack[FrameCount].IPAddress); FrameCount++) {
        }
    }
    return FrameCount;
}


STATIC
VOID
SetStackMax (
    UINTN NewStackMax
) {
    CheckStackPointer ();
    MsgLog ("StackMax changed from %p to %p before allocation %d\n", StackMax, NewStackMax, NextAllocationNum);
    StackMax = NewStackMax;
}


VOID
AdjustStackMax (
) {
    CheckStackPointer ();
    BootLogPause ();
    MsgLog ("[ AdjustStackMax\n");
    LoadedImageRec *LoadedImages = GetLoadedImages ();
    StackFrame * Stack = GetCallStack (AsmGetStackPointerAddress (), AsmGetCurrentIpAddress (), AsmGetFramePointerAddress (), FALSE, NULL);
    if (Stack) {
        DumpOneCallStack (Stack, LoadedImages);
        UINTN StackLength = GetCallStackLength (Stack);
        if (StackLength) {
            SetStackMax (Stack[StackLength - 1].StackAddress + sizeof(VOID*));
        }
        FreeCallStack (Stack);
    }
    FreeLoadedImages (LoadedImages);
    
    #if LEAKS_FILL_STACK
        VOID *e = (VOID *)(AsmGetStackPointerAddress () & ~7);
        UINTN *p = (UINTN *)StackMin;
        MsgLog ("Filling %p to %p\n", p, e);
        while ((VOID *)p < e) *p++ = 0x4041424344454660; // 0x0000006b006a0069; // "ijk"
        UINT8 *q = (VOID *)p;
        while ((VOID *)q < e) *q++ = 0x69;
    #endif
    
    MsgLog ("] AdjustStackMax\n");
    BootLogResume ();
    SetStackScanType(LEAKS_FULL_SCAN);
}


VOID
DumpLeakablePath (
    UINT16 *Path
) {
    CheckStackPointer ();
    if (Path) {
        UINTN PathIndex;
        for (PathIndex = 1; PathIndex <= Path[0]; PathIndex++) {
            MsgLog ("%d ", Path[PathIndex]);
        }
    }
}


BOOLEAN LeakablePathsAreEqual (
    CHAR8 *aWhat,
    CHAR8 *bWhat,
    UINT16 *aPath,
    UINT16 *bPath,
    BOOLEAN noPathIsAnyPath
) {
    CheckStackPointer ();
    return (
        (bWhat == aWhat)
        && (
            (noPathIsAnyPath && (!aPath || !bPath))
            || (!aPath && !bPath)
            || (aPath && bPath && (aPath[0] == bPath[0]) && !CompareMem(&aPath[1], &bPath[1], aPath[0] * sizeof (aPath[0])))
        )
    );
}

VOID
DumpAllocations (
    UINTN MinAllocationNum,
    BOOLEAN ExcludeLeakable,
    UINTN MinStackLength
) {
    CheckStackPointer ();
    UINTN TypeIndex;
    UINTN MaxAllocationNum = NextAllocationNum;

    MsgLog ("[ DumpAllocations\n");

    enum {
        atExcludedRange     = (1 << 0), // exclude
        atExcludedStackSize = (1 << 1), // exclude
        atExcludedExternal  = (1 << 2), // exclude
        atExcludedLeakable  = (1 << 3), // exclude
        atTypeLeakable      = (1 << 4), // descriptor
        atTotal             = (1 << 5),
    };

    UINTN TotalAllocs = 0;
    UINTN StackLength;
    UINTN AllocTypesSize = atTotal * sizeof(UINTN);
    UINTN *AllocTypes = LeaksAllocatePool (AllocTypesSize);
    if (AllocTypes) {
        MyMemSet (AllocTypes, 0, AllocTypesSize);
    }
    
    LoadedImageRec *LoadedImages = GetLoadedImages ();
    Allocation *a = AllocationsList;
    while (a) {
        TypeIndex = 0;
        StackLength = GetCallStackLength (a->Stack);
        
        if (a->Num < MinAllocationNum || a->Num >= MaxAllocationNum) {
            TypeIndex |= atExcludedRange;
        }
        if (a->Stack && StackLength < MinStackLength) {
            TypeIndex |= atExcludedStackSize;
        }
        if (a->AllocFlags & kAllocFlagExternal) {
            TypeIndex |= atExcludedExternal;
        }
        else if (a->What) { // use else here since externals cannot be leakables
            TypeIndex |= atTypeLeakable;
            TypeIndex |= atExcludedLeakable;
            // Report leakable allocation only if there is another occurrence.
            Allocation *b = AllocationsList;
            while (b) {
                // Report if the items are different but have the same What.
                if (b != a && LeakablePathsAreEqual (a->What, b->What, a->Path, b->Path, TRUE)) {
                    // Report if neither doesn't have a path or the paths are the same.
                    // Basically, only one item is allowed to have a What and Path combination, otherwise it's a leak.
                    TypeIndex &= ~atExcludedLeakable;
                    break;
                }
                b = b->Next;
            }
        }
        
        if (
               (!(TypeIndex & atExcludedRange))
            && (!(TypeIndex & atExcludedStackSize))
            && (!(TypeIndex & atExcludedExternal))
            && (!(TypeIndex & atExcludedLeakable) || !ExcludeLeakable)
        ) {
            MsgLog ("Allocation:%p (%d) #%d Type:%s flags:%s%s%s%s%s%s %a%a",
                a->Where, a->Size, a->Num, AllocationTypeString (a->Type),
                (!TypeIndex)                      ? L", Normal"              : L"",
                (TypeIndex & atExcludedRange    ) ? L", Excluded Range"      : L"",
                (TypeIndex & atExcludedStackSize) ? L", Excluded Stack Size" : L"",
                (TypeIndex & atExcludedExternal ) ? L", Excluded External"   : L"",
                (TypeIndex & atExcludedLeakable ) ? L", Excluded Leakable"   : L"",
                (TypeIndex & atTypeLeakable     ) ? L", Leakable"            : L"",
                a->What ? " What:" : "", a->What ? a->What : ""
            );
            if (a->Path) {
                MsgLog (" Path:");
                DumpLeakablePath (a->Path);
            }
            MsgLog ("\n");
            DumpOneCallStack (a->Stack, LoadedImages);
        }

        if (AllocTypes) {
            AllocTypes[TypeIndex]++;
        }
        TotalAllocs++;

        a = a->Next;
    } // while

    if (AllocTypes) {
        BOOLEAN CountsTitle = TRUE;
        for (TypeIndex = 0; TypeIndex < atTotal; TypeIndex++) {
            if (AllocTypes[TypeIndex]) {
                if (CountsTitle) {
                    MsgLog ("Counts:\n");
                    CountsTitle = FALSE;
                }
                MsgLog ("  %5d:%s%s%s%s%s%s\n",
                    AllocTypes[TypeIndex],
                    (!TypeIndex)                      ? L", Normal"              : L"",
                    (TypeIndex & atExcludedRange    ) ? L", Excluded Range"      : L"",
                    (TypeIndex & atExcludedStackSize) ? L", Excluded Stack Size" : L"",
                    (TypeIndex & atExcludedExternal ) ? L", Excluded External"   : L"",
                    (TypeIndex & atExcludedLeakable ) ? L", Excluded Leakable"   : L"",
                    (TypeIndex & atTypeLeakable     ) ? L", Leakable"            : L""
                );
            }
        }
        LeaksFreePool ((VOID **) &AllocTypes);
    }
    
    if (TotalAllocs > 0) {
        MsgLog ("  %5d:,Total\n", TotalAllocs);
    }
    DumpLoadedImages (LoadedImages);
    FreeLoadedImages (LoadedImages);

    MsgLog ("] DumpAllocations\n");
}


UINTN
GetNextAllocationNum () {
    CheckStackPointer ();
    MsgLog ("[] GetNextAllocationNum\n");
    return NextAllocationNum;
}


STATIC
EFI_STATUS
EFIAPI
AllocatePoolEx (
    EFI_MEMORY_TYPE PoolType,
    UINTN Size,
    VOID **Buffer
) {
    CheckStackPointer ();
    EFI_STATUS Status = OrigAllocatePool (PoolType, Size, Buffer);
    
    //MsgLog ("[ AllocatePoolEx %p (%d)\n", Buffer, Size);
    if (EFI_ERROR(Status)) {
        MsgLog ("Allocation Error: cannot allocate %d\n", Size);
        //DumpCallStack (NULL, FALSE);
    }
    else if (Buffer && *Buffer) {
        AddAllocation (*Buffer, Size, kAllocationTypeAllocatePool);
    }
    else {
        MsgLog ("Allocation Error: buffer is null\n");
        //DumpCallStack (NULL, FALSE);
    }
    //MsgLog ("] AllocatePoolEx %p\n", *Buffer);

    return Status;
}


STATIC
EFI_STATUS
EFIAPI
FreePoolEx (
    VOID *Buffer
) {
    CheckStackPointer ();
    BOOLEAN Found = FALSE;
    BOOLEAN DoDumpCStack = FALSE;

    UINTN Type;
    POOL_HEAD1 *head1;
    POOL_TAIL1 *tail1;
    INTN size;
    INTN aSize = -1;

    //MsgLog ("[ FreePoolEx %p\n", Buffer);
    if (Buffer) {
        Allocation *prev = (Allocation *)&AllocationsList;
        Allocation *a = AllocationsList;
        while (a) {
            if (a->Where == Buffer) {
                RemoveAllocation (prev, a);
                Found = TRUE;
                break;
            }
            prev = a;
            a = a->Next;
        }

        if (Found) {
            aSize = a->Size;
        }
        
        size = LogPoolProc(Buffer, &Buffer, "Buffer", &Type, (VOID **)&head1, (VOID **)&tail1, __FILE__, __LINE__, FALSE, FALSE);

        if (size >= 0) {
            if (Found) {
                // it's normal for the pool size to be greater than the amount needed for
                // the allocation as the pool blocks get reused without moving the tail
                if (size < ((a->Size + 7) & ~7)) {
                    MsgLog ("Allocation Error: size mismatch %p (pool:%d allocated:%d)\n", Buffer, size, a->Size);
                    DoDumpCStack = TRUE;
                }
            }
            if (size > 0) {
                #if LEAKS_FILL_FREED
                    VOID *e = Buffer + size;
                    UINTN *p = Buffer;
                    while ((VOID *)p < e) *p++ = 0x0000006700660065; // "efg"
                    UINT8 *q = (VOID *)p;
                    while ((VOID *)q < e) *q++ = 0x65;
                #endif
            }
            if (tail1->Signature != POOL_TAIL_SIGNATURE) {
                MsgLog ("Allocation Error: tail mismatch %p (pool:%d allocated:%d tail:%X)\n", Buffer, size, a->Size, tail1->Signature);
                DoDumpCStack = TRUE;
            }
        }
        else {
            MsgLog ("Allocation Error: not a pool object %p (%d)\n", Buffer, size);
            DoDumpCStack = TRUE;
        }
    }
    
    EFI_STATUS Status = OrigFreePool (Buffer);
    if (Buffer && size >= 0) {
        if (tail1->Signature != POOL_TAIL_SIGNATURE) {
            MsgLog ("Allocation Error: tail mismatch %p (pool:%d allocated:%d tail:%X)\n", Buffer, size, aSize, tail1->Signature);
            DoDumpCStack = TRUE;
        }
        else {
            #if LEAKS_INVALIDATE_TAIL
                tail1->Signature = EFI_SIGNATURE_32('d','t','a','l'); // change the 'p' to a 'd' for "deleted"
            #endif
        }
    }

    {
        if (EFI_ERROR(Status)) {
            MsgLog ("Allocation Error: cannot free %p %r\n", Buffer, Status);
            DoDumpCStack = TRUE;
        }
        else if (!Found) {
            if (!LEAKABLEEXTERNALNEXT && !DoingAlloc) {
                #if 1
                MsgLog ("Allocation Error: attempt to free unallocated %p\n", Buffer);
                DoDumpCStack = TRUE;
                #endif
            }
        }
        //MsgLog ("] FreePoolEx %p\n", Buffer);
    }
    
    if (DoDumpCStack) {
        DumpCallStack (NULL, FALSE);
    }

    return Status;
}


STATIC
EFI_STATUS
EFIAPI
OpenProtocolInformationEx (
    EFI_HANDLE Handle,
    EFI_GUID *Protocol,
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **EntryBuffer,
    UINTN *EntryCount
) {
    CheckStackPointer ();
    EFI_STATUS Status = OrigOpenProtocolInformation(Handle, Protocol, EntryBuffer, EntryCount);
    {
        if (!EFI_ERROR(Status) && EntryBuffer && *EntryBuffer) {
            AddAllocation (*EntryBuffer, *EntryCount * sizeof(*EntryBuffer), kAllocationTypeOpenProtocolInformation);
        }
    }
    return Status;
}


STATIC
EFI_STATUS
EFIAPI
ProtocolsPerHandleEx (
    EFI_HANDLE Handle,
    EFI_GUID ***ProtocolBuffer,
    UINTN *ProtocolBufferCount
) {
    CheckStackPointer ();
    EFI_STATUS Status = OrigProtocolsPerHandle(Handle, ProtocolBuffer, ProtocolBufferCount);
    {
        if (!EFI_ERROR(Status) && ProtocolBuffer && *ProtocolBuffer) {
            AddAllocation (*ProtocolBuffer, *ProtocolBufferCount * sizeof(*ProtocolBuffer), kAllocationTypeProtocolsPerHandle);
        }
    }
    return Status;
}


STATIC
EFI_STATUS
EFIAPI
LocateHandleBufferEx (
    EFI_LOCATE_SEARCH_TYPE SearchType,
    EFI_GUID *Protocol,
    void *SearchKey,
    UINTN *NoHandles,
    EFI_HANDLE **Buffer
) {
    CheckStackPointer ();
    EFI_STATUS Status = OrigLocateHandleBuffer(SearchType, Protocol, SearchKey, NoHandles, Buffer);
    {
        if (!EFI_ERROR(Status) && Buffer && *Buffer) {
            AddAllocation (*Buffer, *NoHandles * sizeof(*Buffer), kAllocationTypeLocateHandleBuffer);
        }
    }
    return Status;
}


STATIC
EFI_STATUS
EFIAPI
ConnectControllerEx (
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE *DriverImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath,
    BOOLEAN Recursive
) {
    CheckStackPointer ();
    MsgLog ("[ ConnectControllerEx\n");

    // Probably shouldn't log to disk while connecting controllers
    BootLogPause ();
        LEAKABLEEXTERNALSTART ("ConnectControllerEx OrigConnectController");
            EFI_STATUS Status = OrigConnectController (ControllerHandle, DriverImageHandle, RemainingDevicePath, Recursive);
        LEAKABLEEXTERNALSTOP ();
    BootLogResume ();

    MsgLog ("] ConnectControllerEx ...%r\n", Status);
    return Status;
}


VOID
ReMapPoolFunctions (
    VOID
) {
    CheckStackPointer ();
    #define OVERRIDE(a) do { Orig##a = gBS->a; gBS->a = a##Ex; } while (0)
        OVERRIDE(AllocatePool);
        OVERRIDE(FreePool);
        OVERRIDE(OpenProtocolInformation);
        OVERRIDE(ProtocolsPerHandle);
        OVERRIDE(LocateHandleBuffer);
        OVERRIDE(ConnectController);
    #undef OVERRIDE

    gBS->Hdr.CRC32 = 0;
    gBS->CalculateCrc32 (gBS, gBS->Hdr.HeaderSize, &gBS->Hdr.CRC32);
} // ReMapOpenProtocol()


// LEAKABLE functions

#define LEAKABLEPATHMAXLENGTH (19)
UINT16 LEAKABLEPATH[LEAKABLEPATHMAXLENGTH + 1] = {0}; // begins with path length
UINT16 LEAKABLEROOTOBJECTID = 0;

VOID
LEAKABLEPATHINIT (
    UINT16 LeakableObjectID
) {
    CheckStackPointer ();
    if (LEAKABLEPATH[0] > 0) {
        MsgLog ("Allocation Error: Leakable Path was not done\n");
    }
    LEAKABLEPATH[0] = 1;
    LEAKABLEPATH[1] = LeakableObjectID;
    LEAKABLEROOTOBJECTID = LeakableObjectID;
}

VOID
LEAKABLEPATHDONE (
) {
    CheckStackPointer ();
    LEAKABLEPATHDEC ();
    if (LEAKABLEPATH[0] > 0) {
        MsgLog ("Allocation Error: Leakable Path is not done\n");
    }
}

VOID
LEAKABLEPATHINC (
) {
    CheckStackPointer ();
    if (LEAKABLEPATH[0] < LEAKABLEPATHMAXLENGTH) {
        LEAKABLEPATH[0] += 1;
        LEAKABLEPATH[LEAKABLEPATH[0]] = 0;
    }
    else {
        MsgLog ("Allocation Error: Leakable Path maxed\n");
    }
}

VOID
LEAKABLEPATHDEC (
) {
    CheckStackPointer ();
    if (LEAKABLEPATH[0] > 0) {
        LEAKABLEPATH[0]--;
    } else {
        MsgLog ("Allocation Error: Leakable Path is already empty\n");
    }
}


VOID
LEAKABLEPATHCHECK (
) {
    CheckStackPointer ();
    if (LEAKABLEPATH[0] > 0) {
        if (LEAKABLEPATH[0] == 1 && LEAKABLEPATH[1] != LEAKABLEROOTOBJECTID) {
            MsgLog ("Allocation Error: Leakable Root Object ID should not be incremented\n");
        }
    } else {
        MsgLog ("Allocation Error: Leakable Path is empty\n");
        DumpCallStack (NULL, FALSE);
    }
}


VOID
LEAKABLEPATHSETID (
    UINT16 ID
) {
    CheckStackPointer ();
    LEAKABLEPATHCHECK ();
    if (LEAKABLEPATH[0] > 0) {
        LEAKABLEPATH[LEAKABLEPATH[0]] = ID;
    }
}

VOID
LEAKABLEWITHPATH (
    VOID *object,
    CHAR8 *description
) {
    CheckStackPointer ();
    LEAKABLEPATHCHECK ();

#if LEAKS_SHOW_PATHS
    // show the leakable hierarchy
    DumpLeakablePath (LEAKABLEPATH);
    MsgLog (" : %8p %a\n", object, description);
#endif

    LeakableProc (object, description, TRUE);

    if (LEAKABLEPATH[0] > 0) {
        if (LEAKABLEPATH[LEAKABLEPATH[0]] != MAX_UINT16) {
            LEAKABLEPATH[LEAKABLEPATH[0]]++;
        }
        else {
            MsgLog ("Allocation Error: Too many Leakable items\n");
        }
    }
}

VOID
LeakableProc (
    IN VOID *Buffer,
    IN CHAR8 *What,
    IN BOOLEAN IncludePath
) {
    CheckStackPointer ();
    if (!What) {
        MsgLog ("Allocation Error: Missing leakable description\n");
        return;
    }

    if (Buffer) {
        LOGPOOL(Buffer);
        Allocation *a = AllocationsList;
        while (a) {
            if (a->Where == Buffer) {
                if (a->What && !LeakablePathsAreEqual (a->What, What, a->Path, LEAKABLEPATH, FALSE)) {
                    MsgLog ("Allocation Error: %p (%d) already marked leakable:%a (Was:%a)\n", a->Where, a->Size, What, a->What);
                    MsgLog ("  Old Path:");
                    DumpLeakablePath (a->Path);
                    MsgLog ("\n");
                    MsgLog ("  New Path:");
                    DumpLeakablePath (LEAKABLEPATH);
                    MsgLog ("\n");
                }
                a->What = What;
                if (a->Path) {
                    LeaksFreePool ((VOID **) &a->Path);
                }
                if (IncludePath) {
                    UINTN LeakablePathSize = (LEAKABLEPATH[0]+1) * sizeof(LEAKABLEPATH[0]);
                    a->Path = LeaksAllocatePool (LeakablePathSize);
                    if (a->Path) {
                        CopyMem (a->Path, LEAKABLEPATH, LeakablePathSize);
                    }
                }
                break;
            }
            a = a->Next;
        }
    }
}


VOID
LEAKABLEEXTERNALSTART (
    CHAR8 *description
) {
    CheckStackPointer ();
    if (LEAKABLEEXTERNALNEXT >= LEAKABLEEXTERNALMAX) {
        MsgLog ("Allocation Error: LEAKABLEEXTERNAL stack is full\n");
        DumpCallStack (NULL, FALSE);
        while (1) {
        }
    }
    else {
        LEAKABLEEXTERNALSTACK[LEAKABLEEXTERNALNEXT++] = description;
        //MsgLog ("[ LEAKABLEEXTERNAL %d:%a\n", LEAKABLEEXTERNALNEXT - 1, description);
    }
}


VOID
LEAKABLEEXTERNALSTOP () {
    CheckStackPointer ();
    if (LEAKABLEEXTERNALNEXT == 0) {
        MsgLog ("Allocation Error: LEAKABLEEXTERNAL stack is empty\n");
        DumpCallStack (NULL, FALSE);
        while (1) {
        }
    }
    else {
        //MsgLog ("] LEAKABLEEXTERNAL %d:%a\n", LEAKABLEEXTERNALNEXT - 1, LEAKABLEEXTERNALSTACK[LEAKABLEEXTERNALNEXT - 1]);
        --LEAKABLEEXTERNALNEXT;
    }
}


#endif // REFIT_DEBUG > 0
