/** @file
 * AcquireGOP.c
 * Installs GOP by reloading a copy of the GPU's OptionROM from RAM
 *
 * Copyright (c) 2020 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2020 Joe van Tunen (joevt@shaw.ca)
 * Portions Copyright (c) 2004-2008 The Intel Corporation
 *
 * THIS PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef __MAKEWITH_TIANO

/**
  @retval EFI_INCOMPATIBLE_VERSION  Not running on compatible TianoCore compiled version
**/
EFI_STATUS AcquireGOP (
    VOID
) {
    // NOOP if not compiled using EDK II
    return EFI_INCOMPATIBLE_VERSION;
}

#else

#include "Platform.h"
#include "../BootMaster/lib.h"
#include "../include/refit_call_wrapper.h"
#include "../../ShellPkg/Include/Library/HandleParsingLib.h"

/**
  @param[in] RomBar       The Rom Base address.
  @param[in] RomSize      The Rom size.
  @param[in] FileName     The file name.

  @retval EFI_SUCCESS               The command completed successfully.
  @retval EFI_INVALID_PARAMETER     Command usage error.
  @retval EFI_UNSUPPORTED           Protocols unsupported.
  @retval EFI_OUT_OF_RESOURCES      Out of memory.
  @retval EFI_VOLUME_CORRUPTED      Inconsistent signatures.
  @retval EFI_NOT_FOUND             Failed to Locate Suitable Option ROM.
  @retval Other value               Unknown error.
**/
EFI_STATUS ReloadOptionROM (
    IN       VOID    *RomBar,
    IN       UINT64   RomSize,
    IN const CHAR16  *FileName
) {
    VOID                          *ImageBuffer;
    VOID                          *DecompressedImageBuffer;
    UINTN                          ImageIndex;
    UINTN                          RomBarOffset;
    UINT8                         *Scratch;
    UINT16                         ImageOffset;
    UINT32                         ImageSize;
    UINT32                         ScratchSize;
    UINT32                         ImageLength;
    UINT32                         DestinationSize;
    UINT32                         InitializationSize;
    CHAR16                        *RomFileName;
    BOOLEAN                        LoadROM;
    EFI_STATUS                     Status;
    EFI_HANDLE                     ImageHandle;
    PCI_DATA_STRUCTURE            *Pcir;
    EFI_DECOMPRESS_PROTOCOL       *Decompress;
    EFI_DEVICE_PATH_PROTOCOL      *FilePath;
    EFI_PCI_EXPANSION_ROM_HEADER  *EfiRomHeader;

    ImageIndex    = 0;
    Status        = EFI_NOT_FOUND;
    RomBarOffset  = (UINTN) RomBar;

    do {
        LoadROM      = FALSE;
        EfiRomHeader = (EFI_PCI_EXPANSION_ROM_HEADER *) (UINTN) RomBarOffset;

        if (EfiRomHeader->Signature != PCI_EXPANSION_ROM_HEADER_SIGNATURE) {
            return EFI_VOLUME_CORRUPTED;
        }

        // If the pointer to the PCI Data Structure is invalid, no further images can be located.
        // The PCI Data Structure must be DWORD aligned.
        if (EfiRomHeader->PcirOffset == 0 ||
            (EfiRomHeader->PcirOffset & 3) != 0 ||
            RomBarOffset - (UINTN) RomBar + EfiRomHeader->PcirOffset + sizeof (PCI_DATA_STRUCTURE) > RomSize
        ) {
            break;
        }

        Pcir = (PCI_DATA_STRUCTURE *) (UINTN) (RomBarOffset + EfiRomHeader->PcirOffset);

        // If a valid signature is not present in the PCI Data Structure, no further images can be located.
        if (Pcir->Signature != PCI_DATA_STRUCTURE_SIGNATURE) {
            break;
        }

        ImageSize = Pcir->ImageLength * 512;

        if (RomBarOffset - (UINTN)RomBar + ImageSize > RomSize) {
            break;
        }

        if ((Pcir->CodeType == PCI_CODE_TYPE_EFI_IMAGE) &&
            (EfiRomHeader->EfiSignature  == EFI_PCI_EXPANSION_ROM_HEADER_EFISIGNATURE) &&
            ((EfiRomHeader->EfiSubsystem == EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER) ||
            (EfiRomHeader->EfiSubsystem  == EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER))
        ) {
            ImageOffset         = EfiRomHeader->EfiImageHeaderOffset;
            InitializationSize  = EfiRomHeader->InitializationSize * 512;

            if (InitializationSize <= ImageSize && ImageOffset < InitializationSize) {
                ImageBuffer             = (VOID *) (UINTN) (RomBarOffset + ImageOffset);
                ImageLength             = InitializationSize - ImageOffset;
                DecompressedImageBuffer = NULL;

                if (EfiRomHeader->CompressionType == EFI_PCI_EXPANSION_ROM_HEADER_COMPRESSED) {
                    Status = REFIT_CALL_3_WRAPPER(
                        gBS->LocateProtocol,
                        &gEfiDecompressProtocolGuid,
                        NULL,
                        (VOID **) &Decompress
                    );

                    if (!EFI_ERROR(Status)) {
                        Status = REFIT_CALL_5_WRAPPER(
                            Decompress->GetInfo,
                            Decompress,
                            ImageBuffer,
                            ImageLength,
                            &DestinationSize,
                            &ScratchSize
                        );

                        if (!EFI_ERROR(Status)) {
                            DecompressedImageBuffer = AllocateZeroPool (DestinationSize);

                            if (DecompressedImageBuffer == NULL) {
                                return EFI_OUT_OF_RESOURCES;
                            }

                            if (ImageBuffer != NULL) {
                                Scratch = AllocateZeroPool (ScratchSize);

                                if (Scratch == NULL) {
                                    return EFI_OUT_OF_RESOURCES;
                                }
                                else {
                                    Status = REFIT_CALL_7_WRAPPER(
                                        Decompress->Decompress,
                                        Decompress,
                                        ImageBuffer,
                                        ImageLength,
                                        DecompressedImageBuffer,
                                        DestinationSize,
                                        Scratch,
                                        ScratchSize
                                    );

                                    if (!EFI_ERROR(Status)) {
                                        LoadROM     = TRUE;
                                        ImageBuffer = DecompressedImageBuffer;
                                        ImageLength = DestinationSize;
                                    }

                                    MyFreePool (&Scratch);
                                }
                            }
                        } // if !EFI_ERROR Status = REFIT_CALL_5_WRAPPER
                    } // if !EFI_ERROR Status = REFIT_CALL_3_WRAPPER
                } // if EfiRomHeader

                if (LoadROM) {
                    RomFileName = PoolPrint (L"%s[%d]", FileName, ImageIndex);
                    FilePath  = REFIT_CALL_2_WRAPPER(
                        FileDevicePath, NULL, RomFileName
                    );
                    Status = REFIT_CALL_6_WRAPPER(
                        gBS->LoadImage,
                        TRUE,
                        gImageHandle,
                        FilePath,
                        ImageBuffer,
                        ImageLength,
                        &ImageHandle
                    );

                    if (EFI_ERROR(Status)) {
                        if (Status == EFI_SECURITY_VIOLATION) {
                            REFIT_CALL_1_WRAPPER(gBS->UnloadImage, ImageHandle);
                        }
                    }
                    else {
                        Status = REFIT_CALL_3_WRAPPER(gBS->StartImage, ImageHandle, NULL, NULL);
                    }
                    MsgLog("Loaded Option ROM '%s'...%r\n", RomFileName, Status);

                    MyFreePool (&RomFileName);
                }

                MyFreePool (&DecompressedImageBuffer);
            } // if InitializationSize
        } // if Pcir->CodeType

        RomBarOffset = RomBarOffset + ImageSize;
        ImageIndex++;
    } while (((Pcir->Indicator & 0x80) == 0x00) && ((RomBarOffset - (UINTN) RomBar) < RomSize));

    return Status;
}

/**
  @retval EFI_SUCCESS               The command completed successfully.
  @retval EFI_INVALID_PARAMETER     Command usage error.
  @retval EFI_UNSUPPORTED           Protocols unsupported.
  @retval EFI_OUT_OF_RESOURCES      Out of memory.
  @retval EFI_VOLUME_CORRUPTED      Inconsistent signatures.
  @retval EFI_PROTOCOL_ERROR        PciIoProtocolGuid not found.
  @retval EFI_LOAD_ERROR            Failed to get PciIoProtocolGuid handle.
  @retval EFI_INCOMPATIBLE_VERSION  Running on incompatible GNU-EFI compiled version.
  @retval EFI_NO_MAPPING            Invalid Binding Handle Count.
  @retval EFI_NOT_FOUND             Failed to Locate Suitable Option ROM.
  @retval Other value               Unknown error.
**/
EFI_STATUS AcquireGOP (
    VOID
) {
    UINTN                 Index                = 0;
    UINTN                 HandleIndex          = 0;
    UINTN                 HandleArrayCount     = 0;
    UINTN                 BindingHandleCount   = 0;
    CHAR16               *RomFileName          = NULL;
    EFI_HANDLE           *HandleArray          = NULL;
    EFI_HANDLE           *BindingHandleBuffer  = NULL;
    EFI_STATUS            ReturnStatus;
    EFI_STATUS            Status;
    EFI_PCI_IO_PROTOCOL  *PciIo;

    UINTN  SegmentPCI;
    UINTN  BusPCI;
    UINTN  DevicePCI;
    UINTN  FunctionPCI;

    Status = REFIT_CALL_5_WRAPPER(
        gBS->LocateHandleBuffer,
        ByProtocol,
        &gEfiPciIoProtocolGuid,
        NULL,
        &HandleArrayCount,
        &HandleArray
    );

    if (EFI_ERROR(Status)) {
        ReturnStatus = EFI_PROTOCOL_ERROR;
    }
    else {
        ReturnStatus = EFI_LOAD_ERROR;

        for (Index = 0; Index < HandleArrayCount; Index++) {
            Status = REFIT_CALL_3_WRAPPER(
                gBS->HandleProtocol,
                HandleArray[Index],
                &gEfiPciIoProtocolGuid,
                (void **) &PciIo
            );

            if (!EFI_ERROR(Status)) {
                if (!PciIo->RomImage || !PciIo->RomSize) {
                    Status = EFI_NOT_FOUND;
                }
                else {
                    BindingHandleBuffer = NULL;
                    REFIT_CALL_3_WRAPPER(
                        PARSE_HANDLE_DATABASE_UEFI_DRIVERS,
                        HandleArray[Index],
                        &BindingHandleCount,
                        &BindingHandleBuffer
                    );

                    if (BindingHandleCount != 0) {
                        Status = EFI_NO_MAPPING;
                    }
                    else {
                        HandleIndex = ConvertHandleToHandleIndex (HandleArray[Index]);
                        RomFileName = PoolPrint (L"Handle%X", HandleIndex);

                        PciIo->GetLocation (PciIo, &SegmentPCI, &BusPCI, &DevicePCI, &FunctionPCI);
                        
                        MsgLog("Loading option ROM at PCI(%02llX|%02llX:%02llX.%llX)\n",
                            SegmentPCI,
                            BusPCI,
                            DevicePCI,
                            FunctionPCI,
                            Status
                        );

                        Status = ReloadOptionROM (
                            PciIo->RomImage,
                            PciIo->RomSize,
                            (const CHAR16 *) RomFileName
                        );

                        MsgLog("Loading option ROM result:%r\n", Status);

                        MyFreePool (&RomFileName);
                    }

                    MyFreePool (&BindingHandleBuffer);
                }
            }

            if (EFI_ERROR(ReturnStatus)) {
                ReturnStatus = Status;
            }
        } // for
        MyFreePool (&HandleArray);
    } // if/else EFI_ERROR Status

    return ReturnStatus;
}

#endif
