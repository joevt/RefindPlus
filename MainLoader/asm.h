/*
 * Copyright (c) 2017 Paulo Alcantara
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
	Modifications 2021 by Joe van Tunen for RefindPlus
	See https://github.com/pcacjr/edk2-trace for original source.
*/


#ifndef _X86_64_ASM_H
#define _X86_64_ASM_H

#ifdef __MAKEWITH_GNUEFI
#include "efi.h"
#include "efilib.h"
#define EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH
#else
#include "../include/tiano_includes.h"
#endif
#include "compiler.h"

STATIC
__inline UINTN
AsmGetCurrentIpAddress (
  VOID
  )
{
  UINTN IpAddress;

  __asm__ __volatile__ ("lea (%%rip), %0"
                        : "=r" (IpAddress));

  return IpAddress;
}

STATIC
__inline UINTN
AsmGetFramePointerAddress (
  VOID
  )
{
  UINTN FramePointerAddress;

  __asm__ __volatile__ ("movq %%rbp, %0"
                        : "=r" (FramePointerAddress));

  return FramePointerAddress;
}

STATIC
__inline UINTN
AsmGetStackPointerAddress (
  VOID
  )
{
  UINTN StackPointerAddress;

  __asm__ __volatile__ ("movq %%rsp, %0"
                        : "=r" (StackPointerAddress));

  return StackPointerAddress;
}

STATIC
__inline UINTN
AsmGetNextStackPointerAddress (
  UINTN StackPointerAddress
  )
{
  UINTN NextStackPointerAddress;

  //
  // As per Microsoft x64 ABI, the stack pointer must be aligned on a 16 byte
  // boundary.
  //
  __asm__ __volatile__ ("lea 0x10(%1), %0"
                        : "=r" (NextStackPointerAddress)
                        : "r" (StackPointerAddress));

  return NextStackPointerAddress;
}

STATIC
__inline UINTN
AsmGetReturnAddress (
  IN UINTN FrameAddress
  )
{
  UINTN ReturnAddress;

  __asm__ __volatile__ ("movq 8(%1), %0"
                        : "=r" (ReturnAddress)
                        : "r" (FrameAddress));

  return ReturnAddress;
}

STATIC
__inline UINTN
AsmGetLastFramePointerAddress (
  IN UINTN FramePointerAddress
  )
{
  UINTN LastFramePointerAddress;

  __asm__ __volatile__ ("movq 0(%1), %0"
                        : "=r" (LastFramePointerAddress)
                        : "r" (FramePointerAddress));

  return LastFramePointerAddress;
}

#endif // _x86_64_ASM_H
