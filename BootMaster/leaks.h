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

typedef struct {
    UINTN StackAddress;
    UINTN IPAddress;
    UINTN FramePointerAddress;
} StackFrame;


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

enum {
    kLeakableVolumes = 1000,
    kLeakablePartitions = 2000,
    kLeakableHandles = 3000,
    kLeakableBuiltinIcons = 4000,
    kLeakableMenuMain = 5000,
    kLeakableMenuAbout,
    kLeakableMenuCleanNvram,
    kLeakableMenuBootKicker
};


VOID LeakableProc (VOID *Buffer, CHAR8 *What, BOOLEAN IncludePath);
VOID LEAKABLEPATHINIT (UINT16 LeakableObjectID);
VOID LEAKABLEPATHDONE ();
VOID LEAKABLEPATHINC ();
VOID LEAKABLEPATHDEC ();
VOID LEAKABLEPATHSETID (UINT16 ID);
VOID LEAKABLEWITHPATH (VOID *object, CHAR8 *description);
#define LEAKABLE(object,discription) LeakableProc(object,discription, FALSE)


INTN PoolVersion (VOID *Pointer);
#define _END_
#define _ENDIGNORE_
/*
find:
^([ \t].*;)[ \t]*$

replace:
\1 _END_

make sure none of these exist:
^\s*(if|while|else|(for\s*\([^;]*;[^;]*;))[^{;]+; _END_
*/

extern CHAR8 *_F;
extern UINTN _L;

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

VOID
DumpCallStack (
    StackFrame *Stack,
    BOOLEAN DoFullScan
);

#else

#define LogPoolProc(...) (0)
#define LEAKABLEPATHINIT(...)
#define LEAKABLEPATHDONE(...)
#define LEAKABLEPATHINC(...)
#define LEAKABLEPATHDEC(...)
#define LEAKABLEPATHSETID(...)
#define LEAKABLEWITHPATH(...)
#define LEAKABLE(...)

#define PoolVersion(...) (0)
#define _END_
#define _ENDIGNORE_

#define DumpCallStack(...)

#endif

#endif // __LEAKS_H
