/** @file
  BDS Lib functions which relate with connect the device

Copyright (c) 2004 - 2008, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
/**
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
**/

#include "Platform.h"
#include "../BootMaster/lib.h"
#include "../BootMaster/screenmgt.h"
#include "../BootMaster/mystrings.h"
#include "../BootMaster/launch_efi.h"
#include "../include/refit_call_wrapper.h"
#include "../BootMaster/leaks.h"

#if REFIT_DEBUG > 0
#include "../../ShellPkg/Include/Library/HandleParsingLib.h"
#endif

#define IS_PCI_GFX(_p) IS_CLASS2 (_p, PCI_CLASS_DISPLAY, PCI_CLASS_DISPLAY_OTHER)

BOOLEAN FoundGOP        = FALSE;
BOOLEAN ReLoaded        = FALSE;
BOOLEAN AcquireErrorGOP = FALSE;
BOOLEAN DetectedDevices = FALSE;

UINTN   AllHandleCount;


extern EFI_STATUS AmendSysTable (VOID);
extern EFI_STATUS AcquireGOP (VOID);

#ifdef __MAKEWITH_TIANO
// DA-TAG: Limit to TianoCore
//         Technically Not Needed
extern EFI_STATUS OcConnectDrivers (VOID);
#endif

static
EFI_STATUS EFIAPI daConnectController (
    IN  EFI_HANDLE                ControllerHandle,
    IN  EFI_HANDLE               *DriverImageHandle   OPTIONAL,
    IN  EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath OPTIONAL,
    IN  BOOLEAN                   Recursive
) {
    EFI_STATUS   Status;
    VOID        *DevicePath;

    LOGPROCENTRY("ControllerHandle:%p Recursive:%d", ControllerHandle, Recursive);

    if (ControllerHandle == NULL) {
        Status = EFI_INVALID_PARAMETER;
        LOGPROCEXIT("ControllerHandle... %r", Status);
        return Status;
    }

    // DA-TAG: Do not connect controllers without device paths.
    //         REF: https://bugzilla.tianocore.org/show_bug.cgi?id=2460
    Status = gBS->HandleProtocol (
        ControllerHandle,
        &gEfiDevicePathProtocolGuid,
        &DevicePath
    );

    if (EFI_ERROR(Status)) {
        LOGPROCEXIT("HandleProtocol... %r", Status);
        return EFI_NOT_STARTED;
    }

    LEAKABLEEXTERNALSTART (kLeakableWhatdaConnectControllerConnectController);
    Status = gBS->ConnectController (
        ControllerHandle,
        DriverImageHandle,
        RemainingDevicePath,
        Recursive
    );
    LEAKABLEEXTERNALSTOP ();

    LOGPROCEXIT("ConnectController... %r", Status);
    return Status;
} // EFI_STATUS daConnectController()

EFI_STATUS ScanDeviceHandles (
    EFI_HANDLE   ControllerHandle,
    UINTN       *HandleCount,
    EFI_HANDLE **HandleBuffer,
    UINT32     **HandleType
) {
    EFI_STATUS                            Status;
    EFI_GUID                            **ProtocolGuidArray;
    UINTN                                 k, ArrayCount;
    UINTN                                 ProtocolIndex;
    UINTN                                 OpenInfoCount;
    UINTN                                 OpenInfoIndex;
    UINTN                                 ChildIndex;
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *OpenInfo;

    *HandleCount  = 0;
    *HandleBuffer = NULL;
    *HandleType   = NULL;

    // DA-TAG: Retrieve a list of handles with device paths
    //         REF: https://bugzilla.tianocore.org/show_bug.cgi?id=2460
    Status = gBS->LocateHandleBuffer (
        ByProtocol,
        &gEfiDevicePathProtocolGuid,
        NULL,
        HandleCount,
        HandleBuffer
    );


    if (EFI_ERROR(Status)) {
        goto Error;
    }

    *HandleType = AllocatePool (*HandleCount * sizeof (UINT32));

    if (*HandleType == NULL) {
        goto Error;
    }

    for (k = 0; k < *HandleCount; k++) {
        (*HandleType)[k] = EFI_HANDLE_TYPE_UNKNOWN;

        // Retrieve a list of all the protocols on each handle
        LEAKABLEEXTERNALSTART("ProtocolsPerHandle");
        Status = gBS->ProtocolsPerHandle (
            (*HandleBuffer)[k],
            &ProtocolGuidArray,
            &ArrayCount
        );
        LEAKABLEEXTERNALSTOP("ProtocolsPerHandle");

        if (!EFI_ERROR(Status)) {
            for (ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++) {
                if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiLoadedImageProtocolGuid)) {
                    (*HandleType)[k] |= EFI_HANDLE_TYPE_IMAGE_HANDLE;
                }

                if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverBindingProtocolGuid)) {
                    (*HandleType)[k] |= EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE;
                }

                if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverConfigurationProtocolGuid)) {
                    (*HandleType)[k] |= EFI_HANDLE_TYPE_DRIVER_CONFIGURATION_HANDLE;
                }

                if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDriverDiagnosticsProtocolGuid)) {
                    (*HandleType)[k] |= EFI_HANDLE_TYPE_DRIVER_DIAGNOSTICS_HANDLE;
                }

                if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiComponentName2ProtocolGuid)) {
                    (*HandleType)[k] |= EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE;
                }

                if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiComponentNameProtocolGuid)) {
                    (*HandleType)[k] |= EFI_HANDLE_TYPE_COMPONENT_NAME_HANDLE;
                }

                if (CompareGuid (ProtocolGuidArray[ProtocolIndex], &gEfiDevicePathProtocolGuid)) {
                    (*HandleType)[k] |= EFI_HANDLE_TYPE_DEVICE_HANDLE;
                }

                // Retrieve the list of agents that have opened each protocol
                LEAKABLEEXTERNALSTART("OpenProtocolInformation");
                Status = gBS->OpenProtocolInformation (
                    (*HandleBuffer)[k],
                    ProtocolGuidArray[ProtocolIndex],
                    &OpenInfo,
                    &OpenInfoCount
                );
                LEAKABLEEXTERNALSTOP("OpenProtocolInformation");

                if (!EFI_ERROR(Status)) {
                    for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
                        if (OpenInfo[OpenInfoIndex].ControllerHandle == ControllerHandle) {
                            if ((OpenInfo[OpenInfoIndex].Attributes &
                                EFI_OPEN_PROTOCOL_BY_DRIVER) == EFI_OPEN_PROTOCOL_BY_DRIVER
                            ) {
                                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                                    if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                                        (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_DEVICE_DRIVER;
                                        #if REFIT_DEBUG > 0
                                        LOG2(4, LOG_LINE_NORMAL, L"", L"\n", L"          %3X %g %d %3X Device",
                                            ConvertHandleToHandleIndex ((*HandleBuffer)[k]),
                                            ProtocolGuidArray[ProtocolIndex],
                                            OpenInfoIndex,
                                            ConvertHandleToHandleIndex((*HandleBuffer)[ChildIndex])
                                        );
                                        #endif
                                    }
                                }
                            }

                            if ((OpenInfo[OpenInfoIndex].Attributes &
                                EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                            ) {
                                (*HandleType)[k] |= EFI_HANDLE_TYPE_PARENT_HANDLE;
                                for (ChildIndex = 0; ChildIndex < *HandleCount; ChildIndex++) {
                                    if ((*HandleBuffer)[ChildIndex] == OpenInfo[OpenInfoIndex].AgentHandle) {
                                        (*HandleType)[ChildIndex] |= EFI_HANDLE_TYPE_BUS_DRIVER;
                                        #if REFIT_DEBUG > 0
                                        LOG2(4, LOG_LINE_NORMAL, L"", L"\n", L"          %3X %g %d %3X Bus",
                                            ConvertHandleToHandleIndex ((*HandleBuffer)[k]),
                                            ProtocolGuidArray[ProtocolIndex],
                                            OpenInfoIndex,
                                            ConvertHandleToHandleIndex((*HandleBuffer)[ChildIndex])
                                        );
                                        #endif
                                    }
                                }
                            }
                        }
                    } // for OpenInfoIndex = 0

                    MY_FREE_POOL(OpenInfo);
                }
            } // for for ProtocolIndex = 0

            MY_FREE_POOL(ProtocolGuidArray);
        } // if !EFI_ERROR Status
    } // for k = 0

    return EFI_SUCCESS;

    Error:
    MY_FREE_POOL(*HandleType);
    MY_FREE_POOL(*HandleBuffer);

    *HandleCount  = 0;

    return Status;
} // EFI_STATUS ScanDeviceHandles()


EFI_STATUS BdsLibConnectMostlyAllEfi (VOID) {
    EFI_STATUS            XStatus;
    EFI_STATUS            Status          = EFI_SUCCESS;
    EFI_HANDLE           *AllHandleBuffer = NULL;
    EFI_HANDLE           *HandleBuffer    = NULL;
    UINTN                 i, k;
    UINTN                 HandleCount;
    UINT32               *HandleType = NULL;
    BOOLEAN               Parent;
    BOOLEAN               Device;
    BOOLEAN               DevTag;
    BOOLEAN               MakeConnection;
    PCI_TYPE_GENERIC      Pci;
    EFI_PCI_IO_PROTOCOL *PciIo;

    UINTN       GOPCount;
    EFI_HANDLE *GOPArray         = NULL;

    UINTN  SegmentPCI;
    UINTN  BusPCI;
    UINTN  DevicePCI;
    UINTN  FunctionPCI;
    UINTN  m;


    #if REFIT_DEBUG > 0
    CHAR16 *GopDevicePathStr = NULL;
    #endif

    DetectedDevices = FALSE;

    LOGPROCENTRY ("%s", ReLoaded ? L"(Reconnect)" : L"(Link)");
    #if REFIT_DEBUG > 0
    LOG2(1, LOG_LINE_SEPARATOR, L"", L"...\n", L"%s Device Handles to Controllers",
        ReLoaded ? L"Reconnect" : L"Link"
    );
    #endif

    // DISABLE scan all handles
    //Status = gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &AllHandleCount, &AllHandleBuffer);
    //
    // Only connect controllers with device paths.
    // REF: https://bugzilla.tianocore.org/show_bug.cgi?id=2460
    Status = gBS->LocateHandleBuffer (
        ByProtocol,
        &gEfiDevicePathProtocolGuid,
        NULL,
        &AllHandleCount,
        &AllHandleBuffer
    );

    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        LOG2(1, LOG_STAR_SEPARATOR, L"", L"\n\n", L"ERROR: Could Not Locate Device Handles");
        #endif
    }
    else {
        #if REFIT_DEBUG > 0
        LOG2(3, LOG_LINE_NORMAL, L"", L"\n", L"Located %d Device Handles", AllHandleCount);
        UINTN AllHandleCountTrigger = AllHandleCount - 1;
        #endif

        for (i = 0; i < AllHandleCount; i++) {
            MakeConnection = TRUE;

            #if REFIT_DEBUG > 0
            CHAR16 *DevicePathStr    = NULL;
            CHAR16 *DevicePathHexStr = NULL;

            UINTN HexIndex = ConvertHandleToHandleIndex (AllHandleBuffer[i]);
            CHAR16 *DeviceData = NULL;

            VOID *DevicePath = DevicePathFromHandle (AllHandleBuffer[i]);
            if (DevicePath) {
                UINTN DevicePathSize = GetDevicePathSize(DevicePath);
                UINTN DevicePathHexSize = DevicePathSize * 2 * sizeof(*DevicePathHexStr) + 1;
                DevicePathHexStr = AllocatePool (DevicePathHexSize);
                if (DevicePathHexStr) {
                    DevicePathHexStr[0] = 0;
                    for (UINTN j = 0; j < DevicePathSize; j++) {
                        UnicodeSPrint(DevicePathHexStr + j * 2, 6, L"%02X", ((UINT8*)DevicePath)[j]);
                    }
                }

                LOG2(1, LOG_LINE_NORMAL, L"", L"\n", L"%3X %s", HexIndex, DevicePathHexStr);
                DevicePathStr = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
            }

            LOG2(1, LOG_LINE_NORMAL, L"", L"\n", L"%3X %s", HexIndex, DevicePathStr);
            #endif

            XStatus = ScanDeviceHandles (
                AllHandleBuffer[i],
                &HandleCount,
                &HandleBuffer,
                &HandleType
            );

            if (EFI_ERROR(XStatus)) {
                #if REFIT_DEBUG > 0
                LOG2(3, LOG_THREE_STAR_MID, L"", L"", L"    - ERROR: %r", XStatus);
                #endif
            }
            else if (HandleType == NULL) {
                #if REFIT_DEBUG > 0
                LOG2(3, LOG_THREE_STAR_MID, L"", L"", L"    - ERROR: Invalid Handle Type");
                #endif
            }
            else if (HandleBuffer == NULL) {
                #if REFIT_DEBUG > 0
                LOG2(3, LOG_THREE_STAR_MID, L"", L"", L"    - ERROR: Invalid Handle Buffer");
                #endif
            }
            else {
                // Assume Device
                Device = TRUE;

                for (k = 0; k < HandleCount; k++) {
                    if (HandleBuffer[k] == AllHandleBuffer[i] && HandleType[k] & EFI_HANDLE_TYPE_DRIVER_BINDING_HANDLE) {
                        Device = FALSE;
                        break;
                    }
                    if (HandleBuffer[k] == AllHandleBuffer[i] && HandleType[k] & EFI_HANDLE_TYPE_IMAGE_HANDLE) {
                        Device = FALSE;
                        break;
                    }
                } // for

                if (!Device) {
                    #if REFIT_DEBUG > 0
                    LOG2(2, LOG_LINE_NORMAL, L"", L"", L"    ... Discounted [Other Item]");
                    #endif
                }
                else {
                    // Assume Not Parent
                    Parent = FALSE;

                    for (k = 0; k < HandleCount; k++) {
                        if (HandleType[k] & EFI_HANDLE_TYPE_PARENT_HANDLE) {
                            MakeConnection = FALSE;
                            Parent         = TRUE;
                            break;
                        }
                    } // for

                    // Assume Not Device
                    DevTag = FALSE;

                    for (k = 0; k < HandleCount; k++) {
                        if (HandleBuffer[k] == AllHandleBuffer[i] && HandleType[k] & EFI_HANDLE_TYPE_DEVICE_HANDLE) {
                            DevTag = TRUE;
                            break;
                        }
                    } // for

                    // Assume Success
                    XStatus = EFI_SUCCESS;

                    if (DevTag) {
                        XStatus = REFIT_CALL_3_WRAPPER(
                            gBS->HandleProtocol,
                            AllHandleBuffer[i],
                            &gEfiPciIoProtocolGuid,
                            (void **) &PciIo
                        );

                        if (EFI_ERROR(XStatus)) {
                            #if REFIT_DEBUG > 0
                            DeviceData = StrDuplicate (L" - Not PCIe Device");
                            #endif
                        }
                        else {
                            // Read PCI BUS
                            PciIo->GetLocation (PciIo, &SegmentPCI, &BusPCI, &DevicePCI, &FunctionPCI);
                            XStatus = PciIo->Pci.Read (
                                PciIo,
                                EfiPciIoWidthUint32,
                                0,
                                sizeof (Pci) / sizeof (UINT32),
                                &Pci
                            );

                            if (EFI_ERROR(XStatus)) {
                                MakeConnection = FALSE;

                                #if REFIT_DEBUG > 0
                                DeviceData = StrDuplicate (L" - Unreadable Item");
                                #endif
                            }
                            else {
                                #if REFIT_DEBUG > 0

                                BOOLEAN VGADevice = IS_PCI_VGA(&Pci.Device);
                                BOOLEAN GFXDevice = IS_PCI_GFX(&Pci.Device);

                                if (VGADevice) {
                                    // DA-TAG: Unable to reconnect later after disconnecting here
                                    //         Comment out and set MakeConnection to FALSE
                                    // gBS->DisconnectController (AllHandleBuffer[i], NULL, NULL);

                                    // joevt: Why include this for REFIT_DEBUG only? If you comment
                                    //        this out because it crashes for REFIT_DEBUG then
                                    //        there's a bug in REFIT_DEBUG that needs to be fixed.

                                    /*
                                    MakeConnection = FALSE;
                                    */
                                }
                                else if (GFXDevice) {
                                    // DA-TAG: Currently unable to detect GFX Device
                                    //         Revisit Clover implementation later
                                    //         Not currently missed but may allow new options
                                    // UPDATE: Actually works on a Non-Mac Firmware Laptop
                                    //         Is this because it is a laptop or Non-Mac Firmware?
                                }

                                CHAR16 *Buses;
                                if (IS_PCI_BRIDGE(&Pci.Bridge) || IS_CARDBUS_BRIDGE(&Pci.Bridge)) {
                                    if (Pci.Bridge.Bridge.SecondaryBus == Pci.Bridge.Bridge.SubordinateBus) {
                                        Buses = PoolPrint (L"-[%02X]", Pci.Bridge.Bridge.SecondaryBus);
                                    } else {
                                        Buses = PoolPrint (L"-[%02X-%02X]", Pci.Bridge.Bridge.SecondaryBus, Pci.Bridge.Bridge.SubordinateBus);
                                    }
                                }
                                else {
                                    Buses = StrDuplicate (L"");
                                }

                                CHAR16 *OptionRom;
                                if (PciIo->RomImage || PciIo->RomSize) {
                                    OptionRom = PoolPrint (L" Rom:%d bytes", PciIo->RomSize);
                                }
                                else {
                                    OptionRom = StrDuplicate (L"");
                                }

                                DeviceData = PoolPrint (
                                    L" - PCI(%02llX|%02llX:%02llX.%llX%-8s [%04X:%04X] [%02X%02X%02X]%s)%s",
                                    SegmentPCI,
                                    BusPCI,
                                    DevicePCI,
                                    FunctionPCI,
                                    Buses,
                                    Pci.Device.Hdr.VendorId,
                                    Pci.Device.Hdr.DeviceId,
                                    Pci.Device.Hdr.ClassCode[2],
                                    Pci.Device.Hdr.ClassCode[1],
                                    Pci.Device.Hdr.ClassCode[0],
                                    OptionRom,
                                    (
                                        VGADevice ? L" - Monitor Display" :
                                        (GFXDevice ? L" - GraphicsFX Card" : L"")
                                    )
                                );

                                MY_FREE_POOL(Buses);
                                MY_FREE_POOL(OptionRom);

                                #endif
                            } // if/else EFI_ERROR(XStatus) // is readable PCI device
                        } // if/else !EFI_ERROR(XStatus) // is PCI device
                    } // if DevTag // is device
                    else {
                        LOG2(3, LOG_LINE_NORMAL, L"", L"\n", L"    ... Not a device");
                    }

                    if (GOPArray == NULL) {
                        XStatus = REFIT_CALL_5_WRAPPER(
                            gBS->LocateHandleBuffer,
                            ByProtocol,
                            &gEfiGraphicsOutputProtocolGuid,
                            NULL,
                            &GOPCount,
                            &GOPArray
                        );
                    }

                    if (!FoundGOP) {
                        if (GOPArray) {
                            for (m = 0; m < GOPCount; m++) {
                                if (GOPArray[m] != gST->ConsoleOutHandle) {
                                    #if REFIT_DEBUG > 0
                                    GopDevicePathStr = ConvertDevicePathToText (
                                        DevicePathFromHandle (GOPArray[m]),
                                        FALSE, FALSE
                                    );
                                    if (GopDevicePathStr) {
                                        LOG2(3, LOG_LINE_NORMAL, L"", L"\n", L"    GopDevicePathStr %s", GopDevicePathStr);
                                    }
                                    #endif

                                    FoundGOP = TRUE;
                                    break;
                                }
                            }
                        }
                    }

                    #if REFIT_DEBUG > 0

                    if (FoundGOP && GopDevicePathStr != NULL) {
                        if (StrCmp (GopDevicePathStr, DevicePathStr) == 0) {
                            CHAR16 *oldDeviceData = DeviceData;
                            DeviceData = PoolPrint (L"%s : 1st GOP", DeviceData);
                            MY_FREE_POOL(oldDeviceData);
                        }
                        else if (StrStr (GopDevicePathStr, DevicePathStr)) {
                            CHAR16 *oldDeviceData = DeviceData;
                            DeviceData = PoolPrint ( L"%s : Parent of 1st GOP", DeviceData);
                            MY_FREE_POOL(oldDeviceData);
                        }
                    }

                    if (GOPArray) {
                        for (m = 0; m < GOPCount; m++) {
                            if (GOPArray[m] == AllHandleBuffer[i]) {
                                CHAR16 *oldDeviceData = DeviceData;
                                DeviceData = PoolPrint ( L"%s : GOP%d", DeviceData, m);
                                MY_FREE_POOL(oldDeviceData);
                            }
                        }
                    }

                    if (gST->ConsoleOutHandle == AllHandleBuffer[i]) {
                        CHAR16 *oldDeviceData = DeviceData;
                        DeviceData = PoolPrint ( L"%s : ConsoleOut", DeviceData);
                        MY_FREE_POOL(oldDeviceData);
                    }

                    #endif
                    // Temp from Clover END

                    if (MakeConnection) {
                        LOG2(3, LOG_LINE_NORMAL, L"", L"\n", L"    MakeConnection");
                        XStatus = daConnectController (AllHandleBuffer[i], NULL, NULL, TRUE);
                    }

                    #if REFIT_DEBUG > 0
                    if (DeviceData == NULL) {
                        DeviceData = StrDuplicate (L"");
                    }
                    #endif

                    if (Parent) {
                        #if REFIT_DEBUG > 0
                        LOG2(2, LOG_LINE_NORMAL, L"", L"", L"    ... Skipped [Parent Device]%s", DeviceData);
                        #endif
                    }
                    else if (!EFI_ERROR(XStatus)) {
                        DetectedDevices = TRUE;

                        #if REFIT_DEBUG > 0
                        LOG2(2, LOG_LINE_NORMAL, L"", L"", L"     * %r                %s", XStatus, DeviceData);
                        #endif
                    }
                    else {
                        #if REFIT_DEBUG > 0

                        if (XStatus == EFI_NOT_STARTED) {
                            LOG2(2, LOG_LINE_NORMAL, L"", L"", L"    ... Declined [Empty Device]%s", DeviceData);
                        }
                        else if (XStatus == EFI_NOT_FOUND) {
                            LOG2(2, LOG_LINE_NORMAL, L"", L"", L"    ... Bypassed [Not Linkable]%s", DeviceData);
                        }
                        else if (XStatus == EFI_INVALID_PARAMETER) {
                            LOG2(2, LOG_LINE_NORMAL, L"", L"", L"    - ERROR: Invalid Param%s", DeviceData);
                        }
                        else {
                            LOG2(2, LOG_LINE_NORMAL, L"", L"", L"    - WARN: %r%s", XStatus, DeviceData);
                        }

                        #endif
                    } // if Parent elseif !EFI_ERROR(XStatus) else
                } // if !Device
            } // if EFI_ERROR(XStatus)

            if (EFI_ERROR(XStatus)) {
                // Change Overall Status on Error
                Status = XStatus;
            }

            #if REFIT_DEBUG > 0
            if (i == AllHandleCountTrigger) {
                MsgLog ("\n\n");
            }
            else {
                MsgLog ("\n");
            }

            MY_FREE_POOL(DevicePathHexStr);
            MY_FREE_POOL(DevicePathStr);
            MY_FREE_POOL(DeviceData);
            #endif

            MY_FREE_POOL(HandleBuffer);
            MY_FREE_POOL(HandleType);
        }  // for

        #if REFIT_DEBUG > 0
        MY_FREE_POOL(GopDevicePathStr);
        MY_FREE_POOL(GOPArray);
        #endif
    } // if !EFI_ERROR(Status)

    MY_FREE_POOL(AllHandleBuffer);

    LOGPROCEXIT();
    return Status;
} // EFI_STATUS BdsLibConnectMostlyAllEfi()

/**
  Connects all drivers to all controllers.
  This function make sure all the current system driver will manage
  the corresponding controllers if have. And at the same time, make
  sure all the system controllers have driver to manage it if have.
**/
static
EFI_STATUS BdsLibConnectAllDriversToAllControllersEx (VOID) {
    LOGPROCENTRY();
    EFI_STATUS  Status;

    // First Pass Driver Connection
    // DA-TAG: Limit to TianoCore
    //         Technically Not Needed
    #ifdef __MAKEWITH_TIANO
    LOGBLOCKENTRY("OcConnectDrivers");
    OcConnectDrivers();
    LOGBLOCKEXIT("OcConnectDrivers");
    #endif

    LOGBLOCKENTRY("Connect and Dispatch");
    do {
        FoundGOP = FALSE;

        // Connect All drivers
        BdsLibConnectMostlyAllEfi();

        // Check if possible to dispatch additional DXE drivers as
        // BdsLibConnectAllEfi() may have revealed new DXE drivers.
        // If Dispatched Status == EFI_SUCCESS, attempt to reconnect.
        LOG2(1, LOG_LINE_NORMAL, L"INFO: ", L"\n\n", L"Calling Driver Services Dispatch");
        Status = gDS->Dispatch();

        #if REFIT_DEBUG > 0
        if (EFI_ERROR(Status)) {
            if (!FoundGOP && DetectedDevices) {
                MsgLog ("INFO: Could Not Find Path to GOP on Any Device Handle");
            }
            LOG2(4, LOG_LINE_NORMAL, L"INFO: ", L"\n\n", L"Dispatch ... %r", Status);
        }
        else {
            LOG2(3, LOG_THREE_STAR_MID, L"INFO: ", L"\n\n", L"Additional DXE Drivers Revealed ... Relink Handles");
        }
        #endif

    } while (!EFI_ERROR(Status));
    LOGBLOCKEXIT("Connect and Dispatch");

    #if REFIT_DEBUG > 0
    LOG2(1, LOG_THREE_STAR_SEP, L"INFO: ", L"\n", L"Processed %d Handle%s",
        AllHandleCount, (AllHandleCount == 1) ? L"" : L"s"
    );
    #endif

    LOGPROCEXIT("FoundGOP:%d", FoundGOP);
    if (FoundGOP) {
        return EFI_SUCCESS;
    }
    else {
        return EFI_NOT_FOUND;
    }
} // EFI_STATUS BdsLibConnectAllDriversToAllControllersEx()

// Many cases of of GPUs not working on EFI 1.x Units such as Classic MacPros are due
// to the GPU's GOP drivers failing to install on not detecting UEFI 2.x. This function
// amends SystemTable Revision information, provides the missing CreateEventEx capability
// then reloads the GPU's ROM from RAM (If Present) which will install GOP (If Available).
EFI_STATUS ApplyGOPFix (VOID) {
    EFI_STATUS Status;

    // Update Boot Services to permit reloading GPU OptionROM
    Status = AmendSysTable();
    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_SEPARATOR, L"Reload OptionROM");

    LOG2(2, LOG_LINE_NORMAL, L"\nINFO: ", L"\n\n", L"Amend System Table ... %r", Status);
    #endif

    if (!EFI_ERROR(Status)) {
        Status = AcquireGOP();

        #if REFIT_DEBUG > 0
        LOG2(2, LOG_LINE_NORMAL, L"      ", L"\n\n", L"Acquire OptionROM on Volatile Storage ... %r", Status);
        #endif

        // connect all devices if no error
        if (EFI_ERROR(Status)) {
            AcquireErrorGOP = TRUE;
        }
        else {
            MsgLog ("\n\n");
            Status = BdsLibConnectAllDriversToAllControllersEx();
            LOG2(2, LOG_LINE_NORMAL, L"INFO: ", L"\n\n", L"BdsLibConnectAllDriversToAllControllersEx ... %r", Status);
        }
    }

    return Status;
} // BOOLEAN ApplyGOPFix()


/**
  Connects all drivers to all controllers.
  This function make sure all the current system driver will manage
  the correspoinding controllers if have. And at the same time, make
  sure all the system controllers have driver to manage it if have.
**/
VOID EFIAPI BdsLibConnectAllDriversToAllControllers (
    IN BOOLEAN ResetGOP
) {
    EFI_STATUS Status;

    // Clear Keystrokes
    ReadAllKeyStrokes();

    Status = BdsLibConnectAllDriversToAllControllersEx();
    if (GlobalConfig.ReloadGOP) {
        if (EFI_ERROR(Status) && ResetGOP && !ReLoaded && DetectedDevices) {
            ReLoaded = TRUE;
            Status   = ApplyGOPFix();

            #if REFIT_DEBUG > 0
            LOG2(1, LOG_STAR_SEPARATOR, L"INFO: ", L"\n\n", L"Issue Option ROM from Volatile Storage ... %r", Status);
            #endif

            ReLoaded = FALSE;
        }
    }
} // VOID BdsLibConnectAllDriversToAllControllers()
