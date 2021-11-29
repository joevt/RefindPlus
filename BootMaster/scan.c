/*
 * BootMaster/scan.c
 * Code related to scanning for boot loaders
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
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
 *
 * Modifications distributed under the preceding terms.
 */

#include "global.h"
#include "config.h"
#include "screenmgt.h"
#include "lib.h"
#include "leaks.h"
#include "icns.h"
#include "menu.h"
#include "mok.h"
#include "apple.h"
#include "mystrings.h"
#include "security_policy.h"
#include "driver_support.h"
#include "launch_efi.h"
#include "launch_legacy.h"
#include "linux.h"
#include "scan.h"
#include "install.h"
#include "../include/refit_call_wrapper.h"


//
// constants

#if defined (EFIX64)
    #define SHELL_NAMES L"\\EFI\\BOOT\\tools\\x64_Shell.efi,\\EFI\\BOOT\\tools\\Shell_x64.efi,\\EFI\\BOOT\\tools\\Shell.efi,\\EFI\\BOOT\\tools_x64\\x64_Shell.efi,\\EFI\\BOOT\\tools_x64\\Shell_x64.efi,\\EFI\\BOOT\\tools_x64\\Shell.efi,\\EFI\\tools_x64\\x64_Shell.efi,\\EFI\\tools_x64\\shell_x64.efi,\\EFI\\tools_x64\\shell.efi,\\EFI\\tools\\x64_Shell.efi,\\EFI\\tools\\shell_x64.efi,\\EFI\\tools\\shell.efi,\\EFI\\x64_Shell.efi,\\EFI\\shell_x64.efi,\\EFI\\shell.efi,\\x64_Shell.efi,\\shell_x64.efi,\\shell.efi"
    #define GPTSYNC_NAMES L"\\EFI\\BOOT\\tools\\x64_gptsync.efi,\\EFI\\BOOT\\tools\\gptsync_x64.efi,\\EFI\\BOOT\\tools\\gptsync.efi,\\EFI\\BOOT\\tools_x64\\x64_gptsync.efi,\\EFI\\BOOT\\tools_x64\\gptsync_x64.efi,\\EFI\\BOOT\\tools_x64\\gptsync.efi,\\EFI\\tools_x64\\gptsync.efi,\\EFI\\tools_x64\\gptsync_x64.efi,\\EFI\\tools\\gptsync.efi,\\EFI\\tools\\gptsync_x64.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_x64.efi,\\gptsync.efi,\\gptsync_x64.efi"
    #define GDISK_NAMES L"\\EFI\\BOOT\\tools\\x64_gdisk.efi,\\EFI\\BOOT\\tools\\gdisk_x64.efi,\\EFI\\BOOT\\tools\\gdisk.efi,\\EFI\\BOOT\\tools_x64\\x64_gdisk.efi,\\EFI\\BOOT\\tools_x64\\gdisk_x64.efi,\\EFI\\BOOT\\tools_x64\\gdisk.efi,\\EFI\\tools_x64\\gdisk.efi,\\EFI\\tools_x64\\gdisk_x64.efi,\\EFI\\tools\\gdisk.efi,\\EFI\\tools\\gdisk_x64.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_x64.efi,\\gdisk.efi,\\gdisk_x64.efi"
    #define NETBOOT_NAMES L"\\EFI\\BOOT\\tools\\x64_ipxe.efi,\\EFI\\BOOT\\tools\\ipxe_x64.efi,\\EFI\\BOOT\\tools\\ipxe.efi,\\EFI\\BOOT\\tools_x64\\x64_ipxe.efi,\\EFI\\BOOT\\tools_x64\\ipxe_x64.efi,\\EFI\\BOOT\\tools_x64\\ipxe.efi,\\EFI\\tools_x64\\ipxe.efi,\\EFI\\tools_x64\\ipxe_x64.efi,\\EFI\\tools\\ipxe.efi,\\EFI\\tools\\ipxe_x64.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_x64.efi,\\ipxe.efi,\\ipxe_x64.efi"
    #define MEMTEST_NAMES L"x64_memtest.efi,memtest.efi,memtest_x64.efi,x64_memtest86.efi,memtest86.efi,memtest86_x64.efi"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootx64.efi"
    #define FALLBACK_BASENAME       L"bootx64.efi"
#elif defined (EFI32)
    #define SHELL_NAMES L"\\EFI\\BOOT\\tools\\ia32_Shell.efi,\\EFI\\BOOT\\tools\\Shell_ia32.efi,\\EFI\\BOOT\\tools\\Shell.efi,\\EFI\\BOOT\\tools_ia32\\ia32_Shell.efi,\\EFI\\BOOT\\tools_ia32\\Shell_ia32.efi,\\EFI\\BOOT\\tools_ia32\\Shell.efi,\\EFI\\tools_ia32\\ia32_Shell.efi,\\EFI\\tools_ia32\\shell_ia32.efi,\\EFI\\tools_ia32\\shell.efi,\\EFI\\tools\\ia32_Shell.efi,\\EFI\\tools\\shell_ia32.efi,\\EFI\\tools\\shell.efi,\\EFI\\ia32_Shell.efi,\\EFI\\shell_ia32.efi,\\EFI\\shell.efi,\\ia32_Shell.efi,\\shell_ia32.efi,\\shell.efi"
    #define GPTSYNC_NAMES L"\\EFI\\BOOT\\tools\\ia32_gptsync.efi,\\EFI\\BOOT\\tools\\gptsync_ia32.efi,\\EFI\\BOOT\\tools\\gptsync.efi,\\EFI\\BOOT\\tools_ia32\\ia32_gptsync.efi,\\EFI\\BOOT\\tools_ia32\\gptsync_ia32.efi,\\EFI\\BOOT\\tools_ia32\\gptsync.efi,\\EFI\\tools_ia32\\gptsync.efi,\\EFI\\tools_ia32\\gptsync_ia32.efi,\\EFI\\tools\\gptsync.efi,\\EFI\\tools\\gptsync_ia32.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_ia32.efi,\\gptsync.efi,\\gptsync_ia32.efi"
    #define GDISK_NAMES L"\\EFI\\BOOT\\tools\\ia32_gdisk.efi,\\EFI\\BOOT\\tools\\gdisk_ia32.efi,\\EFI\\BOOT\\tools\\gdisk.efi,\\EFI\\BOOT\\tools_ia32\\ia32_gdisk.efi,\\EFI\\BOOT\\tools_ia32\\gdisk_ia32.efi,\\EFI\\BOOT\\tools_ia32\\gdisk.efi,\\EFI\\tools_ia32\\gdisk.efi,\\EFI\\tools_ia32\\gdisk_ia32.efi,\\EFI\\tools\\gdisk.efi,\\EFI\\tools\\gdisk_ia32.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_ia32.efi,\\gdisk.efi,\\gdisk_ia32.efi"
    #define NETBOOT_NAMES L"\\EFI\\BOOT\\tools\\ia32_ipxe.efi,\\EFI\\BOOT\\tools\\ipxe_ia32.efi,\\EFI\\BOOT\\tools\\ipxe.efi,\\EFI\\BOOT\\tools_ia32\\ia32_ipxe.efi,\\EFI\\BOOT\\tools_ia32\\ipxe_ia32.efi,\\EFI\\BOOT\\tools_ia32\\ipxe.efi,\\EFI\\tools_ia32\\ipxe.efi,\\EFI\\tools_ia32\\ipxe_ia32.efi,\\EFI\\tools\\ipxe.efi,\\EFI\\tools\\ipxe_ia32.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_ia32.efi,\\ipxe.efi,\\ipxe_ia32.efi"
    #define MEMTEST_NAMES L"ia32_memtest.efi,memtest.efi,memtest_ia32.efi,ia32_memtest86.efi,memtest86.efi,memtest86_ia32.efi"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootia32.efi"
    #define FALLBACK_BASENAME       L"bootia32.efi"
#elif defined (EFIAARCH64)
    #define SHELL_NAMES L"\\EFI\\BOOT\\tools\\aa64_Shell.efi,\\EFI\\BOOT\\tools\\Shell_aa64.efi,\\EFI\\BOOT\\tools\\Shell.efi,\\EFI\\BOOT\\tools_aa64\\aa64_Shell.efi,\\EFI\\BOOT\\tools_aa64\\Shell_aa64.efi,\\EFI\\BOOT\\tools_aa64\\Shell.efi,\\EFI\\tools_aa64\\aa64_Shell.efi,\\EFI\\tools_aa64\\shell_aa64.efi,\\EFI\\tools_aa64\\shell.efi,\\EFI\\tools\\aa64_Shell.efi,\\EFI\\tools\\shell_aa64.efi,\\EFI\\tools\\shell.efi,\\EFI\\aa64_Shell.efi,\\EFI\\shell_aa64.efi,\\EFI\\shell.efi,\\aa64_Shell.efi,\\shell_aa64.efi,\\shell.efi"
    #define GPTSYNC_NAMES L"\\EFI\\BOOT\\tools\\aa64_gptsync.efi,\\EFI\\BOOT\\tools\\gptsync_aa64.efi,\\EFI\\BOOT\\tools\\gptsync.efi,\\EFI\\BOOT\\tools_aa64\\aa64_gptsync.efi,\\EFI\\BOOT\\tools_aa64\\gptsync_aa64.efi,\\EFI\\BOOT\\tools_aa64\\gptsync.efi,\\EFI\\tools_aa64\\gptsync.efi,\\EFI\\tools_aa64\\gptsync_aa64.efi,\\EFI\\tools\\gptsync.efi,\\EFI\\tools\\gptsync_aa64.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_aa64.efi,\\gptsync.efi,\\gptsync_aa64.efi"
    #define GDISK_NAMES L"\\EFI\\BOOT\\tools\\aa64_gdisk.efi,\\EFI\\BOOT\\tools\\gdisk_aa64.efi,\\EFI\\BOOT\\tools\\gdisk.efi,\\EFI\\BOOT\\tools_aa64\\aa64_gdisk.efi,\\EFI\\BOOT\\tools_aa64\\gdisk_aa64.efi,\\EFI\\BOOT\\tools_aa64\\gdisk.efi,\\EFI\\tools_aa64\\gdisk.efi,\\EFI\\tools_aa64\\gdisk_aa64.efi,\\EFI\\tools\\gdisk.efi,\\EFI\\tools\\gdisk_aa64.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_aa64.efi,\\gdisk.efi,\\gdisk_aa64.efi"
    #define NETBOOT_NAMES L"\\EFI\\BOOT\\tools\\aa64_ipxe.efi,\\EFI\\BOOT\\tools\\ipxe_aa64.efi,\\EFI\\BOOT\\tools\\ipxe.efi,\\EFI\\BOOT\\tools_aa64\\aa64_ipxe.efi,\\EFI\\BOOT\\tools_aa64\\ipxe_aa64.efi,\\EFI\\BOOT\\tools_aa64\\ipxe.efi,\\EFI\\tools_aa64\\ipxe.efi,\\EFI\\tools_aa64\\ipxe_aa64.efi,\\EFI\\tools\\ipxe.efi,\\EFI\\tools\\ipxe_aa64.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_aa64.efi,\\ipxe.efi,\\ipxe_aa64.efi"
    #define MEMTEST_NAMES L"aa64_memtest.efi,memtest.efi,memtest_aa64.efi,aa64_memtest86.efi,memtest86.efi,memtest86_aa64.efi"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootaa64.efi"
    #define FALLBACK_BASENAME       L"bootaa64.efi"
#else
    #define SHELL_NAMES             L"\\EFI\\tools\\shell.efi,\\shell.efi"
    #define GPTSYNC_NAMES           L"\\EFI\\tools\\gptsync.efi"
    #define GDISK_NAMES             L"\\EFI\\tools\\gdisk.efi"
    #define NETBOOT_NAMES           L"\\EFI\\tools\\ipxe.efi"
    #define MEMTEST_NAMES           L"\\memtest.efi,\\memtest86.efi"
    #define DRIVER_DIRS             L"drivers"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\boot.efi" // Not really correct
    #define FALLBACK_BASENAME       L"boot.efi"            // Not really correct
#endif

#define IPXE_DISCOVER_NAME      L"\\efi\\tools\\ipxe_discover.efi"
#define IPXE_NAME               L"\\efi\\tools\\ipxe.efi"

// Patterns that identify Linux kernels. Added to the loader match pattern when the
// scan_all_linux_kernels option is set in the configuration file. Causes kernels WITHOUT
// a ".efi" extension to be found when scanning for boot loaders.
#define LINUX_MATCH_PATTERNS    L"vmlinuz*,bzImage*,kernel*"

EFI_GUID GlobalGuid      = EFI_GLOBAL_VARIABLE;
extern EFI_GUID GuidAPFS;

BOOLEAN  ScanningLoaders = FALSE;
BOOLEAN  FirstLoaderScan = FALSE;

// Structure used to hold boot loader filenames and time stamps in
// a linked list; used to sort entries within a directory.
struct LOADER_LIST {
    CHAR16              *FileName;
    EFI_TIME             TimeStamp;
    struct LOADER_LIST  *NextEntry;
};

// Creates a new LOADER_ENTRY data structure and populates it with
// default values from the specified Entry, or NULL values if Entry
// is unspecified (NULL).
// Returns a pointer to the new data structure, or NULL if it
// couldn't be allocated
LOADER_ENTRY * InitializeLoaderEntry (
    IN LOADER_ENTRY *Entry
) {
    LOADER_ENTRY *NewEntry = NULL;

    NewEntry = AllocateZeroPool (sizeof (LOADER_ENTRY));
    if (NewEntry != NULL) {
        NewEntry->Enabled         = TRUE;
//      NewEntry->UseGraphicsMode = FALSE;
//      NewEntry->EfiLoaderPath   = NULL;
//      NewEntry->me.Title        = NULL;
        NewEntry->me.Tag          = TAG_LOADER;
//      NewEntry->OSType          = 0;
//      NewEntry->EfiBootNum      = 0;
        if (Entry != NULL) {
            AssignVolume (&NewEntry->Volume, Entry->Volume);
            NewEntry->EfiBootNum = Entry->EfiBootNum;
            NewEntry->UseGraphicsMode = Entry->UseGraphicsMode;
            CopyFromPoolStr (&NewEntry->LoaderPath, &Entry->LoaderPath);
            CopyFromPoolStr (&NewEntry->LoadOptions, &Entry->LoadOptions);
            CopyFromPoolStr (&NewEntry->InitrdPath, &Entry->InitrdPath);
            NewEntry->EfiLoaderPath = (Entry->EfiLoaderPath) ? DuplicateDevicePath (Entry->EfiLoaderPath) : NULL;
        }
    }

    return (NewEntry);
} // LOADER_ENTRY *InitializeLoaderEntry()

// Prepare a REFIT_MENU_SCREEN data structure for a subscreen entry. This sets up
// the default entry that launches the boot loader using the same options as the
// main Entry does. Subsequent options can be added by the calling function.
// If a subscreen already exists in the Entry that is passed to this function,
// it is left unchanged and a pointer to it is returned.
// Returns a pointer to the new subscreen data structure, or NULL if there
// were problems allocating memory.
REFIT_MENU_SCREEN * InitializeSubScreen (
    IN LOADER_ENTRY *Entry
) {
    LOGPROCENTRY();
    CHAR16              *FileName;
    REFIT_MENU_SCREEN   *SubScreen   = NULL;
    LOADER_ENTRY        *SubEntry;

    FileName = Basename (GetPoolStr (&Entry->LoaderPath));
    if (Entry->me.SubScreen) {
        // existing subscreen ... less initialization and just add new entry later.
        SubScreen = Entry->me.SubScreen;
    }
    else {
        // No subscreen yet; initialize default entry
        SubScreen = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
        if (SubScreen) {
            CHAR16 *DisplayName = NULL;

            if (GlobalConfig.SyncAPFS) {
                EFI_STATUS                 Status;
                EFI_GUID               VolumeGuid;
                EFI_GUID            ContainerGuid;
                APPLE_APFS_VOLUME_ROLE VolumeRole;

                #ifdef __MAKEWITH_GNUEFI
                Status = EFI_NOT_FOUND;
                #else
                // DA-TAG: Limit to TianoCore
                Status = RP_GetApfsVolumeInfo (
                    Entry->Volume->DeviceHandle,
                    &ContainerGuid,
                    &VolumeGuid,
                    &VolumeRole
                );
                #endif

                if (!EFI_ERROR(Status)) {
                    if (VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT) {
                        DisplayName = GetVolumeGroupName (GetPoolStr (&Entry->LoaderPath), Entry->Volume);
                    }
                }
            } // if GlobalConfig.SyncAFPS

            AssignPoolStr (&SubScreen->Title, PoolPrint (
                L"Boot Options for %s on %s",
                (GetPoolStr (&Entry->Title) != NULL) ? GetPoolStr (&Entry->Title)  : FileName,
                (DisplayName) ? DisplayName : GetPoolStr (&Entry->Volume->VolName)
            ));

            MY_FREE_POOL(DisplayName);

            #if REFIT_DEBUG > 0
            LOG(4, LOG_THREE_STAR_MID, L"Build Subscreen:- '%s'", GetPoolStr (&SubScreen->Title));
            #endif

            CopyFromPoolImage (&SubScreen->TitleImage, &Entry->me.Image);

            // default entry
            SubEntry = InitializeLoaderEntry (Entry);

            if (SubEntry != NULL) {
                #if REFIT_DEBUG > 0
                LOG(3, LOG_LINE_NORMAL, L"Setting Entries for '%s'", GetPoolStr (&SubScreen->Title));
                #endif

                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot With Default Options");
                AssignPoolStr (&SubEntry->LoadOptions, AddInitrdToOptions (GetPoolStr (&SubEntry->LoadOptions), GetPoolStr (&SubEntry->InitrdPath)));
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            }

            AssignCachedPoolStr (&SubScreen->Hint1, SUBSCREEN_HINT1);

            if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
                AssignCachedPoolStr (&SubScreen->Hint2, SUBSCREEN_HINT2_NO_EDITOR);
            }
            else {
                AssignCachedPoolStr (&SubScreen->Hint2, SUBSCREEN_HINT2);
            }
        }
    } // if Entry->me.SubScreen

    MY_FREE_POOL(FileName);

    LOGPROCEXIT("%p", SubScreen);
    return SubScreen;
} // REFIT_MENU_SCREEN *InitializeSubScreen()

VOID GenerateSubScreen (
    IN OUT LOADER_ENTRY *Entry,
    IN     REFIT_VOLUME *Volume,
    IN     BOOLEAN       GenerateReturn
) {
    LOGPROCENTRY();

    REFIT_MENU_SCREEN  *SubScreen;
    LOADER_ENTRY       *SubEntry;
    CHAR16             *InitrdName;
    CHAR16             *KernelVersion = NULL;
    CHAR16            **TokenList;
    CHAR16              DiagsFileName[256];
    UINTN               TokenCount;
    REFIT_FILE         *File;

    LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen ... A - START MAIN");

    // create the submenu
    if (StrLen (GetPoolStr (&Entry->Title)) == 0) {
        FreePoolStr (&Entry->Title);
    }

    SubScreen = InitializeSubScreen (Entry);

    // InitializeSubScreen cannot return NULL but guard against this regardless
    if (SubScreen != NULL) {
        // loader-specific submenu entries
        if (Entry->OSType == 'M') {          // entries for Mac OS

#if defined (EFIX64)
            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS with a 64-bit Kernel");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"arch=x86_64");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS with a 32-bit Kernel");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"arch=i386");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
#endif

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SINGLEUSER)) {
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS in Verbose Mode");
                    SubEntry->UseGraphicsMode = FALSE;
                    AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v");
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }

#if defined (EFIX64)
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS in Verbose Mode (64-bit)");
                    SubEntry->UseGraphicsMode = FALSE;
                    AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v arch=x86_64");
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }

                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS in Verbose Mode (32-bit)");
                    SubEntry->UseGraphicsMode = FALSE;
                    AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v arch=i386");
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
#endif

                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS in Single User Mode");
                    SubEntry->UseGraphicsMode = FALSE;
                    AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v -s");
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
            } // single-user mode allowed

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SAFEMODE)) {
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS in Safe Mode");
                    SubEntry->UseGraphicsMode = FALSE;
                    AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v -x");
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
            } // safe mode allowed

            // check for Apple hardware diagnostics
            StrCpy (DiagsFileName, L"System\\Library\\CoreServices\\.diagnostics\\diags.efi");
            if (FileExists (Volume->RootDir, DiagsFileName) &&
                !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HWTEST)
            ) {
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Run Apple Hardware Test");
                    CopyPoolStr (&SubEntry->LoaderPath, DiagsFileName);
                    AssignVolume (&SubEntry->Volume, Volume);
                    SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                }
            }
        }
        else if (Entry->OSType == 'L') {   // entries for Linux kernels with EFI stub loaders
            LOG(5, LOG_BLANK_LINE_SEP, L"X");
            LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 1 - START for OSType L");
            File = ReadLinuxOptionsFile (GetPoolStr (&Entry->LoaderPath), Volume);

            LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2");
            if (File) {
                LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 1");
                InitrdName    = FindInitrd (GetPoolStr (&Entry->LoaderPath), Volume);

                LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 2");
                TokenCount    = ReadTokenLine (File, &TokenList);

                LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 3");
                KernelVersion = FindNumbers (GetPoolStr (&Entry->LoaderPath));

                LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 4");
                ReplaceSubstring (&(TokenList[1]), KERNEL_VERSION, KernelVersion);

                LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 5");
                // first entry requires special processing, since it was initially set
                // up with a default title but correct options by InitializeSubScreen(),
                // earlier.
                if ((TokenCount > 1) && (SubScreen->Entries != NULL) && (SubScreen->Entries[0] != NULL)) {
                    LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 5a 1");
                    if (TokenList[0]) {
                        CopyPoolStr (&SubScreen->Entries[0]->Title, TokenList[0]);
                    }
                    else {
                        AssignCachedPoolStr (&SubScreen->Entries[0]->Title, L"Boot Linux");
                    }

                    LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 5a 2");
                }

                LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 6");
                FreeTokenLine (&TokenList, &TokenCount);

                LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7");
                while ((TokenCount = ReadTokenLine (File, &TokenList)) > 1) {
                    LOG(5, LOG_BLANK_LINE_SEP, L"X");
                    LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7a 1 START WHILE LOOP");
                    ReplaceSubstring (&(TokenList[1]), KERNEL_VERSION, KernelVersion);

                    LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7a 2");
                    SubEntry = InitializeLoaderEntry (Entry);

                    LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7a 3");
                    if (SubEntry != NULL) {
                        LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7a 3a 1");
                        if (TokenList[0]) {
                            CopyPoolStr (&SubEntry->me.Title, TokenList[0]);
                        }
                        else {
                            AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Linux");
                        }

                        LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7a 3a 2");

                        LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7a 3a 3");
                        AssignPoolStr (&SubEntry->LoadOptions, AddInitrdToOptions (TokenList[1], InitrdName));

                        LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7a 3a 4");
                        SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;

                        LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7a 3a 5");
                        AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
                    }

                    LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7a 4");
                    FreeTokenLine (&TokenList, &TokenCount);

                    LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 7a 5 END WHILE LOOP");
                    LOG(5, LOG_BLANK_LINE_SEP, L"X");
                } // while
                FreeTokenLine (&TokenList, &TokenCount);

                LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 8");
                MY_FREE_POOL(KernelVersion);
                MY_FREE_POOL(InitrdName);
                MY_FREE_POOL(File);

                LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 2a 9");
            } // if File
            LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen OSType L ... 3 END for OSType L");
        }
        else if (Entry->OSType == 'E') {   // entries for ELILO
            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Run ELILO in interactive mode");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-p");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Linux for a 17\" iMac or a 15\" MacBook Pro (*)");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-d 0 i17");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Linux for a 20\" iMac (*)");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-d 0 i20");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Linux for a Mac Mini (*)");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-d 0 mini");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            AddMenuInfoLineCached (SubScreen, L"NOTE: This is an example. Entries");
            AddMenuInfoLineCached (SubScreen, L"marked with (*) may not work.");

        }
        else if (Entry->OSType == 'X') {   // entries for xom.efi
            // by default, skip the built-in selection and boot from hard disk only
            AssignCachedPoolStr (&Entry->LoadOptions, L"-s -h");

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Windows from Hard Disk");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-s -h");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Windows from CD-ROM");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-s -c");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Run XOM in text mode");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *) SubEntry);
            }
        } // entries for xom.efi

        LOG(5, LOG_BLANK_LINE_SEP, L"X");
        LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen ... Z 1 - START");
        if (GenerateReturn) {
            LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen ... Z 1a 1");
            AddMenuEntryCopy (SubScreen, &TagMenuEntry[TAG_RETURN]);

            LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen ... Z 1a 2");
        }
        Entry->me.SubScreen = SubScreen;

        LOG(5, LOG_LINE_FORENSIC, L"In GenerateSubScreen ... Z 2 END MAIN - VOID");
        LOG(5, LOG_BLANK_LINE_SEP, L"X");
    }

    LOGPROCEXIT();
} // VOID GenerateSubScreen()

// Sets a few defaults for a loader entry -- mainly the icon, but also the OS type
// code and shortcut letter. For Linux EFI stub loaders, also sets kernel options
// that will (with luck) work fairly automatically.
VOID SetLoaderDefaults (
    LOADER_ENTRY *Entry,
    CHAR16       *LoaderPath,
    REFIT_VOLUME *Volume
) {
    LOGPROCENTRY("for '%s'", GetPoolStr (&Entry->me.Title));

    CHAR16  *PathOnly;
    CHAR16  *NameClues;
    CHAR16  *OSIconName     = NULL;
    CHAR16   ShortcutLetter = 0;
    BOOLEAN  MergeFsName    = FALSE;

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL,
        L"Getting Default Setting for Loader:- '%s'",
        GetPoolStr (&Entry->me.Title) ? GetPoolStr (&Entry->me.Title) : GetPoolStr (&Entry->Title)
    );
    #endif

    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 1 - START");
    NameClues = Basename (LoaderPath);

    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 2");
    PathOnly  = FindPath (LoaderPath);

    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3");
    if (!AllowGraphicsMode) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3a 1");
        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID,
            L"In SetLoaderDefaults ... Skipped Loading Icon in Text Screen Mode"
        );
        #endif
    }
    else {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1");
        if (Volume->DiskKind == DISK_KIND_NET) {
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 1");
            MergeStrings (&NameClues, GetPoolStr (&Entry->me.Title), L' ');
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2");
        }
        else {
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 1");
            if (!GetPoolImage (&Entry->me.Image) && !GlobalConfig.IgnoreHiddenIcons && GlobalConfig.PreferHiddenIcons) {
                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 1a 1");
                #if REFIT_DEBUG > 0
                LOG(3, LOG_LINE_NORMAL, L"Trying to Display '.VolumeIcon' Image");
                #endif

                // use a ".VolumeIcon" image icon for the loader
                // Takes precedence all over options
                CopyFromPoolImage (&Entry->me.Image, &Volume->VolIconImage);
                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 1a 2");
            }

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2");
            if (!GetPoolImage (&Entry->me.Image)) {
                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 1");
                #if REFIT_DEBUG > 0
                if (!GlobalConfig.IgnoreHiddenIcons && GlobalConfig.PreferHiddenIcons) {
                    LOG(3, LOG_LINE_NORMAL, L"Could Not Display '.VolumeIcon' Image!!");
                }
                #endif

                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 2");
                BOOLEAN MacFlag = FALSE;
                if (LoaderPath && MyStrStrIns (LoaderPath, L"System\\Library\\CoreServices")) {
                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 2a 1");
                    MacFlag = TRUE;
                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 2a 2");
                }

                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3");
                if (!GlobalConfig.SyncAPFS || !MacFlag) {
                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3a 1");
                    CHAR16 *NoExtension = StripEfiExtension (NameClues);
                    if (NoExtension != NULL) {
                        // locate a custom icon for the loader
                        // Anything found here takes precedence over the "hints" in the OSIconName variable
                        #if REFIT_DEBUG > 0
                        LOG(3, LOG_LINE_NORMAL, L"Search for Icon in Bootloader Directory");
                        #endif

                        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3a 1a 1");
                        if (!GetPoolImage (&Entry->me.Image)) {
                            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3a 1a 1a 1");
                            AssignPoolImage (&Entry->me.Image, egLoadIconAnyType (
                                Volume->RootDir,
                                PathOnly,
                                NoExtension,
                                GlobalConfig.IconSizes[ICON_SIZE_BIG]
                            ));
                            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3a 1a 1a 2");
                        }

                        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3a 1a 2");
                        if (!GetPoolImage (&Entry->me.Image) && !GlobalConfig.IgnoreHiddenIcons) {
                            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3a 1a 2a 1");
                            CopyFromPoolImage (&Entry->me.Image, &Volume->VolIconImage);
                            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3a 1a 2a 2");
                        }

                        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3a 1a 3");
                        MY_FREE_POOL(NoExtension);
                        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3a 1a 4");
                    }
                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 3a 2");
                }
                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 2a 4");
            }

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 3");
            // Begin creating icon "hints" by using last part of directory path leading
            // to the loader
            #if REFIT_DEBUG > 0
            if (GetPoolImage (&Entry->me.Image) == NULL) {
                LOG(4, LOG_THREE_STAR_MID,
                    L"Creating Icon Hint from Loader Path:- '%s'",
                    LoaderPath
                );
            }
            #endif

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 4");
            CHAR16 *Temp = FindLastDirName (LoaderPath);

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 5");
            MergeStrings (&OSIconName, Temp, L',');

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 6");
            MY_FREE_POOL(Temp);

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 7");
            if (OSIconName != NULL) {
                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 7a 1");
                ShortcutLetter = OSIconName[0];
                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 7a 2");
            }

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8");
            // Add every "word" in the filesystem and partition names, delimited by
            // spaces, dashes (-), underscores (_), or colons (:), to the list of
            // hints to be used in searching for OS icons.
            if (GetPoolStr (&Volume->FsName) && (GetPoolStr (&Volume->FsName)[0] != L'\0')) {
                MergeFsName = TRUE;

                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 1");
                if (MyStrStrIns (GetPoolStr (&Volume->FsName), L"PreBoot") && GlobalConfig.SyncAPFS) {
                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 1a 1");
                    MergeFsName = FALSE;
                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 1a 2");
                }

                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 2");
                if (MergeFsName) {
                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 2a 1");
                    #if REFIT_DEBUG > 0
                    if (GetPoolImage (&Entry->me.Image) == NULL) {
                        LOG(3, LOG_LINE_NORMAL,
                            L"Merge Hints Based on Filesystem Name:- '%s'",
                            GetPoolStr (&Volume->FsName)
                        );
                    }
                    #endif

                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 2a 2");
                    MergeUniqueWords (&OSIconName, GetPoolStr (&Volume->FsName), L',');
                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 2a 3");
                }
                else {
                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 2b 1");
                    if (GetPoolStr (&Volume->VolName) && (GetPoolStr (&Volume->VolName)[0] != L'\0')) {
                        CHAR16 *DisplayName = NULL;

                        if (GlobalConfig.SyncAPFS) {
                            EFI_STATUS                 Status;
                            EFI_GUID               VolumeGuid;
                            EFI_GUID            ContainerGuid;
                            APPLE_APFS_VOLUME_ROLE VolumeRole;

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
                                if (VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT) {
                                    DisplayName = GetVolumeGroupName (GetPoolStr (&Entry->LoaderPath), Volume);
                                }
                            }
                        } // if GlobalConfig.SyncAFPS

                        // Do not free TargetName
                        CHAR16 *TargetName = DisplayName
                            ? DisplayName
                            : GetPoolStr (&Volume->VolName);

                        #if REFIT_DEBUG > 0
                        if (GetPoolImage (&Entry->me.Image) == NULL) {
                            LOG(3, LOG_LINE_NORMAL,
                                L"Merge Hints Based on Volume Name:- '%s'",
                                TargetName
                            );
                        }
                        #endif

                        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 2b 1a 1");
                        MergeUniqueWords (&OSIconName, TargetName, L',');
                        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 2b 1a 2");

                        MY_FREE_POOL(DisplayName);
                    }
                    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 2b 2");
                }
                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 8a 3");
            }

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 9");
            if (GetPoolStr (&Volume->PartName) && (GetPoolStr (&Volume->PartName)[0] != L'\0')) {
                #if REFIT_DEBUG > 0
                if (GetPoolImage (&Entry->me.Image) == NULL) {
                    LOG(3, LOG_LINE_NORMAL,
                        L"Merge Hints Based on Partition Name:- '%s'",
                        GetPoolStr (&Volume->PartName)
                    );
                }
                #endif

                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 9a 1");
                MergeUniqueWords (&OSIconName, GetPoolStr (&Volume->PartName), L',');
                LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 2a 9a 2");
            }
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 1a 3");
        } // if/else Volume->DiskKind == DISK_KIND_NET
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 3b 2");
    }

    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 4");
    if (!AllowGraphicsMode) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 4a 1");

        #if REFIT_DEBUG > 0
        if (GetPoolImage (&Entry->me.Image) == NULL) {
            LOG(3, LOG_LINE_NORMAL, L"Add Hints Based on Specific Loaders");
        }
        #endif

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 4a 2");
    }

    // detect specific loaders
    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5");
    if (StriSubCmp (L"bzImage", NameClues) ||
        StriSubCmp (L"vmlinuz", NameClues) ||
        StriSubCmp (L"kernel",  NameClues)
    ) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5a 1");
        if (Volume->DiskKind != DISK_KIND_NET) {
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5a 1a 1");
            GuessLinuxDistribution (&OSIconName, Volume, LoaderPath);

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5a 1a 2");
            AssignPoolStr (&Entry->LoadOptions, GetMainLinuxOptions (LoaderPath, Volume));

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5a 1a 3");
        }

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5a 2");
        MergeStrings (&OSIconName, L"linux", L',');
        Entry->OSType = 'L';

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5a 3");
        if (ShortcutLetter == 0) {
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5a 3a 1");
            ShortcutLetter = 'L';
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5a 3a 2");
        }

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5a 4");
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5a 5");
    }
    else if (StriSubCmp (L"refit", LoaderPath)) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5b 1");
        MergeStrings (&OSIconName, L"refit", L',');

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5b 2");
        Entry->OSType = 'R';
        ShortcutLetter = 'R';

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5b 3");
    }
    else if (StriSubCmp (L"refind", LoaderPath)) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5c 1");
        MergeStrings (&OSIconName, L"refind", L',');

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5c 2");
        Entry->OSType = 'R';
        ShortcutLetter = 'R';

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5c 3");
    }
    else if (StriSubCmp (MACOSX_LOADER_PATH, LoaderPath)) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5d 1");
        if (FileExists (Volume->RootDir, L"EFI\\refind\\config.conf") ||
            FileExists (Volume->RootDir, L"EFI\\refind\\refind.conf")
        ) {
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5d 1a 1");
            MergeStrings (&OSIconName, L"refind", L',');

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5d 1a 2");
            Entry->OSType = 'R';
            ShortcutLetter = 'R';

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5d 1a 3");
        }
        else {
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5d 1b 1");
            MergeStrings (&OSIconName, L"mac", L',');

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5d 1b 2");
            Entry->OSType = 'M';
            ShortcutLetter = 'M';
            Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;

            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5d 1b 3");
        }
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5d 2");
    }
    else if (MyStriCmp (NameClues, L"diags.efi")) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5e 1");
        MergeStrings (&OSIconName, L"hwtest", L',');
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5e 2");
    }
    else if (MyStriCmp (NameClues, L"e.efi") ||
        MyStriCmp (NameClues, L"elilo.efi")  ||
        StriSubCmp (L"elilo", NameClues)
    ) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5f 1");
        MergeStrings (&OSIconName, L"elilo,linux", L',');
        Entry->OSType = 'E';

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5f 2");
        if (ShortcutLetter == 0) {
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5f 2a 1");
            ShortcutLetter = 'L';
            LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5f 2a 2");
        }
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5f 3");
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5f 4");
    }
    else if (StriSubCmp (L"grub", NameClues)) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5g 1");
        MergeStrings (&OSIconName, L"grub,linux", L',');

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5g 2");
        Entry->OSType = 'G';
        ShortcutLetter = 'G';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_GRUB;
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5g 3");
    }
    else if (MyStriCmp (NameClues, L"cdboot.efi") ||
        MyStriCmp (NameClues, L"bootmgr.efi")  ||
        MyStriCmp (NameClues, L"bootmgfw.efi") ||
        MyStriCmp (NameClues, L"bkpbootmgfw.efi")
    ) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5h 1");
        MergeStrings (&OSIconName, L"win8", L',');

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5h 2");
        Entry->OSType = 'W';
        ShortcutLetter = 'W';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5h 3");
    }
    else if (MyStriCmp (NameClues, L"xom.efi")) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5i 1");
        MergeStrings (&OSIconName, L"xom,win,win8", L',');

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5i 2");
        Entry->OSType = 'X';
        ShortcutLetter = 'W';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5i 3");
    }
    else if (MyStriCmp (NameClues, L"opencore")) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5j 1");
        MergeStrings (&OSIconName, L"opencore", L',');

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5j 2");
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OPENCORE;

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5j 3");
    }
    else if (MyStriCmp (NameClues, L"clover")) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5k 1");
        MergeStrings (&OSIconName, L"clover", L',');

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5k 2");
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_CLOVER;

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5k 3");
    }
    else if (StriSubCmp (L"ipxe", NameClues)) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5l 1");
        MergeStrings (&OSIconName, L"network", L',');

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5l 2");
        Entry->OSType = 'N';
        ShortcutLetter = 'N';
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 5l 3");
    }

    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 6");
    if ((ShortcutLetter >= 'a') && (ShortcutLetter <= 'z')) {
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 6a 1");
        ShortcutLetter = ShortcutLetter - 'a' + 'A'; // convert lowercase to uppercase
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 6a 2");
    }

    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 7");
    Entry->me.ShortcutLetter = ShortcutLetter;

    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 8");
    if (AllowGraphicsMode && GetPoolImage (&Entry->me.Image) == NULL) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"Trying to Locate an Icon Based on Hints:- '%s'",
            OSIconName
        );
        #endif

        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 8a 1");
        AssignPoolImage (&Entry->me.Image, LoadOSIcon (OSIconName, L"unknown", FALSE));
        LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 8a 2");
    }

    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 9");
    MY_FREE_POOL(PathOnly);
    MY_FREE_POOL(OSIconName);
    MY_FREE_POOL(NameClues);

    LOG(5, LOG_LINE_FORENSIC, L"In SetLoaderDefaults ... 10 - END:- VOID");
    LOG(5, LOG_BLANK_LINE_SEP, L"X");
    LOGPROCEXIT();
} // VOID SetLoaderDefaults()

CHAR16 * GetVolumeGroupName (
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
) {
    UINTN   i;
    CHAR16 *VolumeGroupName = NULL;

    for (i = 0; i < SystemVolumesCount; i++) {
        if (
            MyStrStrIns (
                LoaderPath,
                GuidAsString (&(SystemVolumes[i]->VolUuid))
            )
        ) {
            VolumeGroupName = StrDuplicate (GetPoolStr (&SystemVolumes[i]->VolName));
            break;
        }
    } // for

    if (!VolumeGroupName) {
        for (i = 0; i < DataVolumesCount; i++) {
            if (
                MyStrStrIns (
                    LoaderPath,
                    GuidAsString (&(DataVolumes[i]->VolUuid))
                )
            ) {
                VolumeGroupName = StrDuplicate (GetPoolStr (&DataVolumes[i]->VolName));
                break;
            }
        } // for
    }

    return VolumeGroupName;
} // CHAR16 * GetVolumeGroupName()

// Add an NVRAM-based EFI boot loader entry to the menu.
static
LOADER_ENTRY * AddEfiLoaderEntry (
    IN EFI_DEVICE_PATH *EfiLoaderPath,
    IN CHAR16          *LoaderTitle,
    IN UINT16           EfiBootNum,
    IN UINTN            Row,
    IN PoolImage       *Icon_PI_
) {
    CHAR16        *TempStr;
    CHAR16        *FullTitle  = NULL;
    CHAR16        *OSIconName = NULL;
    LOADER_ENTRY  *Entry;

    Entry = InitializeLoaderEntry (NULL);
    if (Entry) {
        Entry->DiscoveryType = DISCOVERY_TYPE_AUTO;

        if (LoaderTitle) {
            FullTitle = PoolPrint (L"Reboot to %s", LoaderTitle);
        }

        Entry->me.Row        = Row;
        Entry->me.Tag        = TAG_FIRMWARE_LOADER;
        if (FullTitle) {
            AssignPoolStr (&Entry->me.Title, FullTitle);
        }
        else {
            AssignCachedPoolStr (&Entry->me.Title, L"Unknown");
        }
        if (LoaderTitle) {
            // without "Reboot to"
            CopyPoolStr (&Entry->Title, LoaderTitle);
        }
        else {
            AssignCachedPoolStr (&Entry->Title, L"Unknown");
        }
        Entry->EfiLoaderPath = DuplicateDevicePath (EfiLoaderPath);
        TempStr              = DevicePathToStr (EfiLoaderPath);

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"UEFI Loader Path:- '%s'", TempStr);
        #endif

        MY_FREE_POOL(TempStr);

        Entry->EfiBootNum = EfiBootNum;

        MergeUniqueWords (&OSIconName, GetPoolStr (&Entry->me.Title), L',');
        MergeUniqueStrings (&OSIconName, L"Unknown", L',');

        if (GetPoolImage (Icon)) {
            CopyFromPoolImage (&Entry->me.Image, Icon);
        }
        else {
            AssignPoolImage (&Entry->me.Image, LoadOSIcon (OSIconName, NULL, FALSE));
        }

        if (Row == 0) {
            CopyFromPoolImage_PI_ (&Entry->me.BadgeImage_PI_, BuiltinIcon (BUILTIN_ICON_VOL_EFI));
        }

        MY_FREE_POOL(OSIconName);

/*      // these are allready FALSE/NULL/0
        Entry->Volume      = NULL;
        Entry->LoaderPath  = NULL;
        Entry->LoadOptions = NULL;
        Entry->InitrdPath  = NULL;
*/
        Entry->Enabled     = TRUE;

        AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);
    } // if Entry


    return Entry;
} // LOADER_ENTRY * AddEfiLoaderEntry()


// Add a specified EFI boot loader to the list, using automatic settings
// for icons, options, etc.
static
LOADER_ENTRY * AddLoaderEntry (
    IN CHAR16       *LoaderPath,
    IN CHAR16       *LoaderTitle,
    IN REFIT_VOLUME *Volume,
    IN BOOLEAN       SubScreenReturn
) {
    LOGPROCENTRY();
    EFI_STATUS     Status;
    LOADER_ENTRY  *Entry;
    CHAR16        *TitleEntry  = NULL;
    CHAR16        *DisplayName = NULL;

    CleanUpPathNameSlashes (LoaderPath);
    Entry = InitializeLoaderEntry (NULL);

    if (Entry == NULL) {
        LOGPROCEXIT("%p", Entry);
        return NULL;
    }

    if (GlobalConfig.SyncAPFS && Volume->FSType == FS_TYPE_APFS) {
        EFI_GUID               VolumeGuid;
        EFI_GUID            ContainerGuid;
        APPLE_APFS_VOLUME_ROLE VolumeRole;

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
            if (VolumeRole == APPLE_APFS_VOLUME_ROLE_PREBOOT) {
                DisplayName = GetVolumeGroupName (LoaderPath, Volume);

                if (!DisplayName) {
                    // Do not display this PreBoot Volume Menu Entry
                    LOGPROCEXIT("%p", Entry);
                    return NULL;
                }
            }
        }
    } // if GlobalConfig.SyncAFPS

    Entry->DiscoveryType = DISCOVERY_TYPE_AUTO;
    TitleEntry   = (LoaderTitle) ? LoaderTitle : LoaderPath;
    CopyPoolStr (&Entry->Title, TitleEntry);

    #if REFIT_DEBUG > 0
    if (DisplayName) {
        LOG(3, LOG_THREE_STAR_MID, L"Synced PreBoot:- '%s'", DisplayName);
    }
    LOG(3, LOG_LINE_NORMAL, L"Add Loader Entry:- '%s'", GetPoolStr (&Entry->Title));
    LOG(3, LOG_LINE_NORMAL, L"UEFI Loader File:- '%s'", LoaderPath);
    #endif

    if (DisplayName || GetPoolStr (&Volume->VolName)) {
        AssignPoolStr (&Entry->me.Title, PoolPrint (
            L"Boot %s from %s",
            (LoaderTitle) ? LoaderTitle : LoaderPath,
            DisplayName   ? DisplayName : GetPoolStr (&Volume->VolName)
        ));
    }
    else {
        AssignPoolStr (&Entry->me.Title, PoolPrint (
            L"Boot %s",
            (LoaderTitle) ? LoaderTitle : LoaderPath
        ));
    }

    Entry->me.Row = 0;
    CopyFromPoolImage (&Entry->me.BadgeImage, &Volume->VolBadgeImage);

    if ((LoaderPath != NULL) && (LoaderPath[0] != L'\\')) {
        AssignCachedPoolStr (&Entry->LoaderPath, L"\\");
    }
    else {
        FreePoolStr (&Entry->LoaderPath);
    }

    AssignPoolStr (&Entry->LoaderPath, MergeStringsNew (GetPoolStr (&Entry->LoaderPath), LoaderPath, 0));
    AssignVolume (&Entry->Volume, Volume);
    SetLoaderDefaults (Entry, LoaderPath, Volume);
    GenerateSubScreen (Entry, Volume, SubScreenReturn);
    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);

    #if REFIT_DEBUG > 0
    MsgLog ("\n");
    MsgLog (
        "  - Found '%s' on '%s'\n",
        TitleEntry,
        DisplayName
            ? DisplayName
            : GetPoolStr (&Volume->VolName)
                ? GetPoolStr (&Volume->VolName)
                : GetPoolStr (&Entry->LoaderPath)
    );

    LOG(2, LOG_THREE_STAR_END, L"Successfully Created Menu Entry for %s", GetPoolStr (&Entry->Title));
    #endif

    MY_FREE_POOL(DisplayName);
    LOGPROCEXIT("%p", Entry);
    return Entry;
} // LOADER_ENTRY * AddLoaderEntry()

// Returns -1 if (Time1 < Time2), +1 if (Time1 > Time2), or 0 if
// (Time1 == Time2). Precision is only to the nearest second; since
// this is used for sorting boot loader entries, differences smaller
// than this are likely to be meaningless (and unlikely!).
static
INTN TimeComp (
    IN EFI_TIME *Time1,
    IN EFI_TIME *Time2
) {
    INT64 Time1InSeconds, Time2InSeconds;

    // Following values are overestimates; I'm assuming 31 days in every month.
    // This is fine for the purpose of this function, which is limited
    Time1InSeconds = (INT64) (Time1->Second)
        + ((INT64) (Time1->Minute)      * 60)
        + ((INT64) (Time1->Hour)        * 3600)
        + ((INT64) (Time1->Day)         * 86400)
        + ((INT64) (Time1->Month)       * 2678400)
        + ((INT64) (Time1->Year - 1998) * 32140800);

    Time2InSeconds = (INT64) (Time2->Second)
        + ((INT64) (Time2->Minute)      * 60)
        + ((INT64) (Time2->Hour)        * 3600)
        + ((INT64) (Time2->Day)         * 86400)
        + ((INT64) (Time2->Month)       * 2678400)
        + ((INT64) (Time2->Year - 1998) * 32140800);

    if (Time1InSeconds < Time2InSeconds) {
        return (-1);
    }
    else if (Time1InSeconds > Time2InSeconds) {
        return (1);
    }

    return 0;
} // INTN TimeComp()

// Adds a loader list element, keeping it sorted by date. EXCEPTION: Fedora's rescue
// kernel, which begins with "vmlinuz-0-rescue," should not be at the top of the list,
// since that will make it the default if kernel folding is enabled, so float it to
// the end.
// Returns the new first element (the one with the most recent date).
static struct
LOADER_LIST * AddLoaderListEntry (
    struct LOADER_LIST *LoaderList,
    struct LOADER_LIST *NewEntry
) {
    struct LOADER_LIST *LatestEntry  = NULL;
    struct LOADER_LIST *CurrentEntry = NULL;
    struct LOADER_LIST *PrevEntry    = NULL;
    BOOLEAN LinuxRescue              = FALSE;

    if (LoaderList == NULL) {
        LatestEntry = NewEntry;
    }
    else {
        LatestEntry = CurrentEntry = LoaderList;
        if (StriSubCmp (L"vmlinuz-0-rescue", NewEntry->FileName)) {
            LinuxRescue = TRUE;
        }

        while ((CurrentEntry != NULL) &&
            !StriSubCmp (L"vmlinuz-0-rescue", CurrentEntry->FileName) &&
            (LinuxRescue || (TimeComp (&(NewEntry->TimeStamp), &(CurrentEntry->TimeStamp)) < 0))
        ) {
            PrevEntry   = CurrentEntry;
            CurrentEntry = CurrentEntry->NextEntry;
        } // while

        NewEntry->NextEntry = CurrentEntry;

        if (PrevEntry == NULL) {
            LatestEntry = NewEntry;
        }
        else {
            PrevEntry->NextEntry = NewEntry;
        }
    }

    return (LatestEntry);
} // static VOID AddLoaderListEntry()

// Delete the LOADER_LIST linked list
static
VOID CleanUpLoaderList (
    struct LOADER_LIST *LoaderList
) {
    struct LOADER_LIST *Temp;

    while (LoaderList != NULL) {
        Temp = LoaderList;
        LoaderList = LoaderList->NextEntry;
        MY_FREE_POOL(Temp->FileName);
        MY_FREE_POOL(Temp);
    } // while
} // static VOID CleanUpLoaderList()

// Returns FALSE if the specified file/volume matches the GlobalConfig.DontScanDirs
// or GlobalConfig.DontScanVolumes specification, or if Path points to a volume
// other than the one specified by Volume, or if the specified path is SelfDir.
// Returns TRUE if none of these conditions is met -- that is, if the path is
// eligible for scanning.
static
BOOLEAN ShouldScan (
    REFIT_VOLUME *Volume,
    CHAR16       *Path
) {
    UINTN    i            = 0;
    CHAR16  *VolName      = NULL;
    CHAR16  *VolGuid      = NULL;
    CHAR16  *PathCopy     = NULL;
    CHAR16  *DontScanDir  = NULL;
    BOOLEAN  ScanIt       = TRUE;

    if (MyStriCmp (Path, SelfDirPath)
        && (Volume->DeviceHandle == SelfVolume->DeviceHandle)
    ) {
        return FALSE;
    }

    if (GlobalConfig.SyncAPFS && Volume->FSType == FS_TYPE_APFS) {
        EFI_STATUS                 Status;
        CHAR16       *TmpVolNameA  = NULL;
        CHAR16       *TmpVolNameB  = NULL;
        EFI_GUID               VolumeGuid;
        EFI_GUID            ContainerGuid;
        APPLE_APFS_VOLUME_ROLE VolumeRole;

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
            TmpVolNameB = PoolPrint (L"%s - DATA", GetPoolStr (&Volume->VolName));
            if (IsIn (TmpVolNameB, GlobalConfig.DontScanVolumes)) {
                ScanIt = FALSE;
            }
            MY_FREE_POOL(TmpVolNameB);
            if (!ScanIt) return FALSE;

            TmpVolNameA = SanitiseString (GetPoolStr (&Volume->VolName));
            TmpVolNameB = PoolPrint (L"%s - DATA", TmpVolNameA);
            if (IsIn (TmpVolNameB, GlobalConfig.DontScanVolumes)) {
                ScanIt = FALSE;
            }
            MY_FREE_POOL(TmpVolNameA);
            MY_FREE_POOL(TmpVolNameB);
            if (!ScanIt) return FALSE;
        }
    } // if GlobalConfig.SyncAFPS

    VolGuid = GuidAsString (&(Volume->PartGuid));
    if (IsIn (VolGuid, GlobalConfig.DontScanVolumes)
        || IsIn (GetPoolStr (&Volume->FsName), GlobalConfig.DontScanVolumes)
        || IsIn (GetPoolStr (&Volume->VolName), GlobalConfig.DontScanVolumes)
        || IsIn (GetPoolStr (&Volume->PartName), GlobalConfig.DontScanVolumes)
    ) {
        ScanIt = FALSE;
    }
    MY_FREE_POOL(VolGuid);
    if (!ScanIt) return FALSE;

    // See if Path includes an explicit volume declaration that is NOT Volume.
    PathCopy = StrDuplicate (Path);
    if (SplitVolumeAndFilename (&PathCopy, &VolName)) {
        if (VolName) {
            if (!MyStriCmp (VolName, GetPoolStr (&Volume->FsName))
                || !MyStriCmp(VolName, GetPoolStr (&Volume->PartName))
            ) {
                ScanIt = FALSE;
            }
        }
    }
    MY_FREE_POOL(PathCopy);
    MY_FREE_POOL(VolName);

    // See if Volume is in GlobalConfig.DontScanDirs.
    while (ScanIt &&
        (DontScanDir = FindCommaDelimited (GlobalConfig.DontScanDirs, i++))
    ) {
        SplitVolumeAndFilename (&DontScanDir, &VolName);
        CleanUpPathNameSlashes (DontScanDir);
        if (VolName != NULL) {
            if (VolumeMatchesDescription (Volume, VolName)
                && MyStriCmp (DontScanDir, Path)
            ) {
                ScanIt = FALSE;
            }
        }
        else {
            if (MyStriCmp (DontScanDir, Path)) {
                ScanIt = FALSE;
            }
        }

        MY_FREE_POOL(DontScanDir);
        MY_FREE_POOL(VolName);
    } // while

    return ScanIt;
} // BOOLEAN ShouldScan()

// Returns TRUE if the file is byte-for-byte identical with the fallback file
// on the volume AND if the file is not itself the fallback file; returns
// FALSE if the file is not identical to the fallback file OR if the file
// IS the fallback file. Intended for use in excluding the fallback boot
// loader when it is a duplicate of another boot loader.
static
BOOLEAN DuplicatesFallback (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *FileName
) {
    EFI_STATUS       Status;
    EFI_FILE_HANDLE  FileHandle;
    EFI_FILE_HANDLE  FallbackHandle;
    EFI_FILE_INFO   *FileInfo;
    EFI_FILE_INFO   *FallbackInfo;
    CHAR8           *FileContents;
    CHAR8           *FallbackContents;
    UINTN            FileSize     = 0;
    UINTN            FallbackSize = 0;
    BOOLEAN          AreIdentical = FALSE;

    if (!FileExists (Volume->RootDir, FileName) ||
        !FileExists (Volume->RootDir, FALLBACK_FULLNAME)
    ) {
        return FALSE;
    }

    CleanUpPathNameSlashes (FileName);

    if (MyStriCmp (FileName, FALLBACK_FULLNAME)) {
        // identical filenames, so not a duplicate.
        return FALSE;
    }

    Status = REFIT_CALL_5_WRAPPER(
        Volume->RootDir->Open,
        Volume->RootDir,
        &FileHandle,
        FileName,
        EFI_FILE_MODE_READ,
        0
    );

    if (Status == EFI_SUCCESS) {
        FileInfo = LibFileInfo (FileHandle);
        FileSize = FileInfo->FileSize;
    }
    else {
        return FALSE;
    }

    MY_FREE_POOL(FileInfo);

    Status = REFIT_CALL_5_WRAPPER(
        Volume->RootDir->Open,
        Volume->RootDir,
        &FallbackHandle,
        FALLBACK_FULLNAME,
        EFI_FILE_MODE_READ,
        0
    );

    if (Status == EFI_SUCCESS) {
        FallbackInfo = LibFileInfo (FallbackHandle);
        FallbackSize = FallbackInfo->FileSize;
    }
    else {
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
        return FALSE;
    }

    MY_FREE_POOL(FallbackInfo);

    if (FallbackSize == FileSize) { // could be identical; do full check.
        FileContents = AllocatePool (FileSize);
        FallbackContents = AllocatePool (FallbackSize);
        if (FileContents && FallbackContents) {
            Status = REFIT_CALL_3_WRAPPER(
                FileHandle->Read,
                FileHandle,
                &FileSize,
                FileContents
            );
            if (Status == EFI_SUCCESS) {
                Status = REFIT_CALL_3_WRAPPER
                (FallbackHandle->Read,
                    FallbackHandle,
                    &FallbackSize,
                    FallbackContents
                );
            }
            if (Status == EFI_SUCCESS) {
                AreIdentical = (CompareMem (
                    FileContents,
                    FallbackContents,
                    FileSize
                ) == 0);
            }
        }

        MY_FREE_POOL(FileContents);
        MY_FREE_POOL(FallbackContents);
    }

    // BUG ALERT: Some systems (e.g., DUET, some Macs with large displays) crash if the
    // following two calls are reversed.
    REFIT_CALL_1_WRAPPER(FileHandle->Close, FallbackHandle);
    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);

    return AreIdentical;
} // BOOLEAN DuplicatesFallback()

// Returns FALSE if two measures of file size are identical for a single file,
// TRUE if not or if the file can't be opened and the other measure is non-0.
// Despite the function's name, this is not really a direct test of symbolic
// link status, since EFI does not officially support symlinks. It does seem
// to be a reliable indicator, though. (OTOH, some disk errors might cause a
// file to fail to open, which would return a false positive -- but as I use
// this function to exclude symbolic links from the list of boot loaders,
// that would be fine, since such boot loaders wouldn't work.)
// CAUTION: *FullName MUST be properly cleaned up (via CleanUpPathNameSlashes())
static
BOOLEAN IsSymbolicLink (
    REFIT_VOLUME  *Volume,
    CHAR16        *FullName,
    EFI_FILE_INFO *DirEntry
) {
    EFI_FILE_HANDLE  FileHandle;
    EFI_FILE_INFO   *FileInfo = NULL;
    EFI_STATUS       Status;
    UINTN            FileSize2 = 0;

    LEAKABLEEXTERNALSTART ("IsSymbolicLink Open");
    Status = REFIT_CALL_5_WRAPPER(
        Volume->RootDir->Open,
        Volume->RootDir,
        &FileHandle,
        FullName,
        EFI_FILE_MODE_READ,
        0
    );
    LEAKABLEEXTERNALSTOP ();

    if (Status == EFI_SUCCESS) {
        FileInfo = LibFileInfo (FileHandle);
        if (FileInfo != NULL) {
            FileSize2 = FileInfo->FileSize;
        }
    }

    MY_FREE_POOL(FileInfo);

    return (DirEntry->FileSize != FileSize2);
} // BOOLEAN IsSymbolicLink()

// Scan an individual directory for EFI boot loader files and, if found,
// add them to the list. Exception: Ignores FALLBACK_FULLNAME, which is picked
// up in ScanEfiFiles(). Sorts the entries within the loader directory so that
// the most recent one appears first in the list.
// Returns TRUE if a duplicate for FALLBACK_FILENAME was found, FALSE if not.
static
BOOLEAN ScanLoaderDir (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *Path,
    IN CHAR16       *Pattern
) {
    EFI_STATUS               Status;
    REFIT_DIR_ITER           DirIter;
    EFI_FILE_INFO           *DirEntry;
    CHAR16                  *Message;
    CHAR16                  *Extension;
    CHAR16                  *FullName;
    struct LOADER_LIST      *NewLoader;
    struct LOADER_LIST      *LoaderList  = NULL;
    LOADER_ENTRY            *FirstKernel = NULL;
    LOADER_ENTRY            *LatestEntry = NULL;
    BOOLEAN                  FoundFallbackDuplicate = FALSE, IsLinux, InSelfPath;

    #if REFIT_DEBUG > 0
    CHAR16 *PathStr;

    if (MyStriCmp (Path, L"\\")) {
        PathStr = StrDuplicate (L"\\");
    }
    else {
        PathStr = PoolPrint (L"%s", Path);
    }

    LOG(3, LOG_LINE_NORMAL, L"Scanning for '%s' in '%s'", Pattern, PathStr);

    MY_FREE_POOL(PathStr);
    #endif

    InSelfPath = MyStriCmp (Path, SelfDirPath);

    if ((!SelfDirPath || !Path ||
        (InSelfPath && (Volume->DeviceHandle != SelfVolume->DeviceHandle)) ||
        (!InSelfPath)) && (ShouldScan (Volume, Path))
    ) {
        // look through contents of the directory
        DirIterOpen (Volume->RootDir, Path, &DirIter);

        while (DirIterNext (&DirIter, 2, Pattern, &DirEntry)) {
            Extension = FindExtension (DirEntry->FileName);
            FullName  = StrDuplicate (Path);

            MergeStrings (&FullName, DirEntry->FileName, L'\\');
            CleanUpPathNameSlashes (FullName);

            // IsSymbolicLink (Volume, FullName, DirEntry) = is symbolic link
            // HasSignedCounterpart (Volume, FullName) = file with same name plus ".efi.signed" is present
            if (DirEntry->FileName[0] == '.' ||
                MyStriCmp (Extension, L".icns") ||
                MyStriCmp (Extension, L".png") ||
                (MyStriCmp (DirEntry->FileName, FALLBACK_BASENAME) &&
                (MyStriCmp (Path, L"EFI\\BOOT"))) ||
                FilenameIn (Volume, Path, DirEntry->FileName, SHELL_NAMES) ||
                IsSymbolicLink (Volume, FullName, DirEntry) ||
                HasSignedCounterpart (Volume, FullName) ||
                FilenameIn (Volume, Path, DirEntry->FileName, GlobalConfig.DontScanFiles) ||
                !IsValidLoader (Volume->RootDir, FullName)
            ) {
                // skip this
            }
            else {

                NewLoader = AllocateZeroPool (sizeof (struct LOADER_LIST));
                if (NewLoader != NULL) {
                    NewLoader->FileName  = StrDuplicate (FullName);
                    NewLoader->TimeStamp = DirEntry->ModificationTime;
                    LoaderList           = AddLoaderListEntry (LoaderList, NewLoader);

                    if (DuplicatesFallback (Volume, FullName)) {
                        FoundFallbackDuplicate = TRUE;
                    }
                }
            }

            MY_FREE_POOL(Extension);
            MY_FREE_POOL(FullName);
            MY_FREE_POOL(DirEntry);
        } // while

        if (LoaderList != NULL) {
            IsLinux   = FALSE;
            NewLoader = LoaderList;

            while (NewLoader != NULL) {
                IsLinux = (
                    StriSubCmp (L"bzImage", NewLoader->FileName) ||
                    StriSubCmp (L"vmlinuz", NewLoader->FileName) ||
                    StriSubCmp (L"kernel", NewLoader->FileName)
                );

                if ((FirstKernel != NULL) && IsLinux && GlobalConfig.FoldLinuxKernels) {
                    AddKernelToSubmenu (FirstKernel, NewLoader->FileName, Volume);
                }
                else {
                    LatestEntry = AddLoaderEntry (
                        NewLoader->FileName,
                        NULL, Volume,
                        !(IsLinux && GlobalConfig.FoldLinuxKernels)
                    );
                    if (IsLinux && (FirstKernel == NULL)) {
                        FirstKernel = LatestEntry;
                    }
                }

                NewLoader = NewLoader->NextEntry;
            } // while

            if (FirstKernel != NULL && IsLinux && GlobalConfig.FoldLinuxKernels) {
                #if REFIT_DEBUG > 0
                LOG(3, LOG_LINE_NORMAL, L"Adding 'Return' entry to folded Linux kernels");
                #endif

                AddMenuEntryCopy (FirstKernel->me.SubScreen, &TagMenuEntry[TAG_RETURN]);
            }

            CleanUpLoaderList (LoaderList);
        }

        Status = DirIterClose (&DirIter);
        // NOTE: EFI_INVALID_PARAMETER really is an error that should be reported;
        // but I've gotten reports from users who are getting this error occasionally
        // and I can't find anything wrong or reproduce the problem, so I'm putting
        // it down to buggy EFI implementations and ignoring that particular error.
        if ((Status != EFI_NOT_FOUND) && (Status != EFI_INVALID_PARAMETER)) {
            if (Path) {
                Message = PoolPrint (
                    L"While Scanning the '%s' Directory on '%s'",
                    Path, GetPoolStr (&Volume->VolName)
                );
            }
            else {
                Message = PoolPrint (
                    L"While Scanning the Root Directory on '%s'",
                    GetPoolStr (&Volume->VolName)
                );
            }

            CheckError (Status, Message);
            MY_FREE_POOL(Message);
        }
    } // if !SelfDirPath

    return FoundFallbackDuplicate;
} // static VOID ScanLoaderDir()

// Run the IPXE_DISCOVER_NAME program, which obtains the IP address of the boot
// server and the name of the boot file it delivers.
static
CHAR16 * RuniPXEDiscover (
    EFI_HANDLE Volume
) {
    EFI_STATUS        Status;
    EFI_DEVICE_PATH  *FilePath;
    EFI_HANDLE        iPXEHandle;
    CHAR16           *boot_info = NULL;
    UINTN             boot_info_size = 0;

    FilePath = FileDevicePath (Volume, IPXE_DISCOVER_NAME);
    Status = REFIT_CALL_6_WRAPPER(
        gBS->LoadImage, FALSE,
        SelfImageHandle, FilePath,
        NULL, 0,
        &iPXEHandle
    );
    if (Status != 0) {
        return NULL;
    }

    REFIT_CALL_3_WRAPPER(
        gBS->StartImage,
        iPXEHandle,
        &boot_info_size,
        &boot_info
    );

    return boot_info;
} // RuniPXEDiscover()

// Scan for network (PXE) boot servers. This function relies on the presence
// of the IPXE_DISCOVER_NAME and IPXE_NAME program files on the volume from
// which RefindPlus launched. As of December 6, 2014, these tools are not
// entirely reliable. See BUILDING.txt for information on building them.
static
VOID ScanNetboot (VOID) {
    CHAR16        *iPXEFileName = IPXE_NAME;
    CHAR16        *Location;
    REFIT_VOLUME  *NetVolume;

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Scanning for iPXE boot options");
    #endif

    if (FileExists (SelfVolume->RootDir, IPXE_NAME) &&
        FileExists (SelfVolume->RootDir, IPXE_DISCOVER_NAME) &&
        IsValidLoader (SelfVolume->RootDir, IPXE_DISCOVER_NAME) &&
        IsValidLoader (SelfVolume->RootDir, IPXE_NAME)
    ) {
        Location = RuniPXEDiscover (SelfVolume->DeviceHandle);
        if (Location != NULL && FileExists (SelfVolume->RootDir, iPXEFileName)) {
            NetVolume = CopyVolume (SelfVolume);
            if (NetVolume) {
                NetVolume->DiskKind = DISK_KIND_NET;

                CopyFromPoolImage_PI_ (&NetVolume->VolBadgeImage_PI_, BuiltinIcon (BUILTIN_ICON_VOL_NET));

                FreePoolStr (&NetVolume->PartName);
                FreePoolStr (&NetVolume->VolName);
                FreePoolStr (&NetVolume->FsName);

                AddLoaderEntry (iPXEFileName, Location, NetVolume, TRUE);

                FreeVolume (&NetVolume);
            }
        }
    }
} // VOID ScanNetboot()


// Adds *FullFileName as a Mac OS loader, if it exists.
// Returns TRUE if the fallback loader is NOT a duplicate of this one,
// FALSE if it IS a duplicate.
static
BOOLEAN ScanMacOsLoader (
    REFIT_VOLUME *Volume,
    CHAR16       *FullFileName
) {
    BOOLEAN   AddThisEntry       = TRUE;
    BOOLEAN   ScanFallbackLoader = TRUE;
    CHAR16   *VolName            = NULL;
    CHAR16   *PathName           = NULL;
    CHAR16   *FileName           = NULL;
    UINTN     i;

    SplitPathName (FullFileName, &VolName, &PathName, &FileName);
    if (FileExists (Volume->RootDir, FullFileName) &&
        !FilenameIn (Volume, PathName, L"boot.efi", GlobalConfig.DontScanFiles)
    ) {
        if (FileExists (Volume->RootDir, L"EFI\\refind\\config.conf") ||
            FileExists (Volume->RootDir, L"EFI\\refind\\refind.conf")
        ) {
            AddLoaderEntry (FullFileName, L"RefindPlus", Volume, TRUE);
        }
        else {
            if (GlobalConfig.SyncAPFS && Volume->FSType == FS_TYPE_APFS) {
                for (i = 0; i < SystemVolumesCount; i++) {
                    if (GuidsAreEqual (&(SystemVolumes[i]->VolUuid), &(Volume->VolUuid))) {
                        AddThisEntry = FALSE;
                        break;
                    }
                }
            }

            if (AddThisEntry) {
                AddLoaderEntry (FullFileName, L"Mac OS", Volume, TRUE);
            }
        }

        if (DuplicatesFallback (Volume, FullFileName)) {
            ScanFallbackLoader = FALSE;
        }
    }

    MY_FREE_POOL(VolName);
    MY_FREE_POOL(PathName);
    MY_FREE_POOL(FileName);

    return ScanFallbackLoader;
} // VOID ScanMacOsLoader()

static
VOID ScanEfiFiles (
    REFIT_VOLUME *Volume
) {
    EFI_STATUS        Status;
    REFIT_DIR_ITER    EfiDirIter;
    EFI_FILE_INFO    *EfiDirEntry;
    UINTN             i, Length;
    CHAR16           *Temp;
    CHAR16           *FileName;
    CHAR16           *SelfPath;
    CHAR16           *MatchPatterns;
    CHAR16           *VolName            = NULL;
    CHAR16           *Directory          = NULL;
    BOOLEAN           ScanFallbackLoader = TRUE;
    BOOLEAN           FoundBRBackup      = FALSE;

    #if REFIT_DEBUG > 0
    UINTN    LogLineType;
    #endif

    if (!Volume ||
        !Volume->RootDir ||
        !GetPoolStr (&Volume->VolName) ||
        !Volume->IsReadable
    ) {
        // Early Return on Invalid Volume
        return;
    }

    if (Volume->FSType == FS_TYPE_APFS) {
        EFI_GUID               VolumeGuid;
        EFI_GUID            ContainerGuid;
        APPLE_APFS_VOLUME_ROLE VolumeRole = 0;

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
            if (VolumeRole != APPLE_APFS_VOLUME_ROLE_PREBOOT &&
                VolumeRole != APPLE_APFS_VOLUME_ROLE_SYSTEM &&
                VolumeRole != APPLE_APFS_VOLUME_ROLE_UNDEFINED
            ) {
                // Early Return on APFS Support Volume
                return;
            }
        }

        if (GlobalConfig.SyncAPFS) {
            for (i = 0; i < SystemVolumesCount; i++) {
                if (GuidsAreEqual (&(SystemVolumes[i]->VolUuid), &(Volume->VolUuid))) {
                    // Early Return on ReMapped Volume
                    return;
                }
            }
        }
    }

    #if REFIT_DEBUG > 0
    if (FirstLoaderScan) {
        LogLineType = LOG_THREE_STAR_MID;
    }
    else {
        LogLineType = LOG_THREE_STAR_SEP;
    }

    /* Exception for LOG_THREE_STAR_SEP */
    LOG(3, LogLineType,
        L"Scanning Volume '%s' for UEFI Loaders",
        GetPoolStr (&Volume->VolName) ? GetPoolStr (&Volume->VolName) : L"** No Name **"
    );
    #endif

    FirstLoaderScan = FALSE;

    MatchPatterns = StrDuplicate (LOADER_MATCH_PATTERNS);
    if (GlobalConfig.ScanAllLinux) {
        MergeStrings (&MatchPatterns, LINUX_MATCH_PATTERNS, L',');
    }

    // check for Mac OS boot loader
    if (ShouldScan (Volume, MACOSX_LOADER_DIR)) {
        FileName = StrDuplicate (MACOSX_LOADER_PATH);
        ScanFallbackLoader &= ScanMacOsLoader (Volume, FileName);
        MY_FREE_POOL(FileName);
        DirIterOpen (Volume->RootDir, L"\\", &EfiDirIter);

        while (DirIterNext (&EfiDirIter, 1, NULL, &EfiDirEntry)) {
            if (IsGuid (EfiDirEntry->FileName)) {
                FileName = PoolPrint (L"%s\\%s", EfiDirEntry->FileName, MACOSX_LOADER_PATH);
                ScanFallbackLoader &= ScanMacOsLoader (Volume, FileName);
                MY_FREE_POOL(FileName);
                FileName = PoolPrint (L"%s\\%s", EfiDirEntry->FileName, L"boot.efi");

                if (!StriSubCmp (FileName, GlobalConfig.MacOSRecoveryFiles)) {
                    MergeStrings (&GlobalConfig.MacOSRecoveryFiles, FileName, L',');
                    LEAKABLE (GlobalConfig.MacOSRecoveryFiles, "MacOSRecoveryFiles");
                }

                MY_FREE_POOL(FileName);
            }
            MY_FREE_POOL(EfiDirEntry);
        } // while
        DirIterClose (&EfiDirIter);

        // check for XOM
        FileName = StrDuplicate (L"System\\Library\\CoreServices\\xom.efi");
        if (FileExists (Volume->RootDir, FileName) &&
            !FilenameIn (Volume, MACOSX_LOADER_DIR, L"xom.efi", GlobalConfig.DontScanFiles)
        ) {
            AddLoaderEntry (FileName, L"Windows XP (XoM)", Volume, TRUE);
            if (DuplicatesFallback (Volume, FileName)) {
                ScanFallbackLoader = FALSE;
            }
        }

        MY_FREE_POOL(FileName);
    } // if ShouldScan

    // check for Microsoft boot loader/menu
    if (ShouldScan (Volume, L"EFI\\Microsoft\\Boot")) {
        FileName = StrDuplicate (L"EFI\\Microsoft\\Boot\\bkpbootmgfw.efi");
        if (FileExists (Volume->RootDir, FileName) &&
            !FilenameIn (
                Volume,
                L"EFI\\Microsoft\\Boot",
                L"bkpbootmgfw.efi",
                GlobalConfig.DontScanFiles
            )
        ) {
            // Boot Repair Backup
            AddLoaderEntry (FileName, L"UEFI Windows (BRBackup)", Volume, TRUE);
            FoundBRBackup = TRUE;
            if (DuplicatesFallback (Volume, FileName)) {
                ScanFallbackLoader = FALSE;
            }
        }

        MY_FREE_POOL(FileName);

        FileName = StrDuplicate (L"EFI\\Microsoft\\Boot\\bootmgfw.efi");
        if (FileExists (Volume->RootDir, FileName) &&
            !FilenameIn (
                Volume,
                L"EFI\\Microsoft\\Boot",
                L"bootmgfw.efi",
                GlobalConfig.DontScanFiles
            )
        ) {
            if (FoundBRBackup) {
                AddLoaderEntry (
                    FileName,
                    L"Assumed UEFI Windows (Potentially GRUB)",
                    Volume, TRUE
                );
            }
            else {
                AddLoaderEntry (
                    FileName,
                    L"Windows (UEFI)",
                    Volume, TRUE
                );
            }

            if (DuplicatesFallback (Volume, FileName)) {
                ScanFallbackLoader = FALSE;
            }
        }

        MY_FREE_POOL(FileName);
    } // if ShouldScan

    // scan the root directory for EFI executables
    if (ScanLoaderDir (Volume, L"\\", MatchPatterns)) {
        ScanFallbackLoader = FALSE;
    }

    // scan subdirectories of the EFI directory (as per the standard)
    DirIterOpen (Volume->RootDir, L"EFI", &EfiDirIter);
    while (DirIterNext (&EfiDirIter, 1, NULL, &EfiDirEntry)) {

        if (MyStriCmp (EfiDirEntry->FileName, L"tools") ||
            EfiDirEntry->FileName[0] == '.'
        ) {
            // skip this, does not contain boot loaders or is scanned later
        }
        else {

            FileName = PoolPrint (L"EFI\\%s", EfiDirEntry->FileName);
            if (ScanLoaderDir (Volume, FileName, MatchPatterns)) {
                ScanFallbackLoader = FALSE;
            }

            MY_FREE_POOL(FileName);
        }

        MY_FREE_POOL(EfiDirEntry);
    } // while

    Status = DirIterClose (&EfiDirIter);
    if ((Status != EFI_NOT_FOUND) && (Status != EFI_INVALID_PARAMETER)) {
        Temp = PoolPrint (
            L"While Scanning the EFI System Partition on '%s'",
            GetPoolStr (&Volume->VolName)
        );
        CheckError (Status, Temp);
        MY_FREE_POOL(Temp);
    }

    // Scan user-specified (or additional default) directories.
    i = 0;
    while ((Directory = FindCommaDelimited (GlobalConfig.AlsoScan, i++)) != NULL) {
        if (ShouldScan (Volume, Directory)) {
            SplitVolumeAndFilename (&Directory, &VolName);
            CleanUpPathNameSlashes (Directory);

            Length = StrLen (Directory);
            if ((Length > 0) && ScanLoaderDir (Volume, Directory, MatchPatterns)) {
                ScanFallbackLoader = FALSE;
            }

            MY_FREE_POOL(VolName);
        }

        MY_FREE_POOL(Directory);
    } // while

    // Do not scan the fallback loader if it is on the same volume and a duplicate of RefindPlus itself.
    SelfPath = DevicePathToStr (SelfLoadedImage->FilePath);
    CleanUpPathNameSlashes (SelfPath);

    if ((Volume->DeviceHandle == SelfLoadedImage->DeviceHandle) &&
        DuplicatesFallback (Volume, SelfPath)
    ) {
        ScanFallbackLoader = FALSE;
    }
    MY_FREE_POOL(SelfPath);

    // If not a duplicate & if it exists & if it is not us, create an entry
    // for the fallback boot loader
    if (ScanFallbackLoader &&
        FileExists (Volume->RootDir, FALLBACK_FULLNAME) &&
        ShouldScan (Volume, L"EFI\\BOOT") &&
        !FilenameIn (Volume, L"EFI\\BOOT", FALLBACK_BASENAME, GlobalConfig.DontScanFiles)
    ) {
        AddLoaderEntry (FALLBACK_FULLNAME, L"Fallback Boot Loader", Volume, TRUE);
    }

    MY_FREE_POOL(MatchPatterns);
} // static VOID ScanEfiFiles()

// Scan user-configured menu entries from config.conf
static VOID
ScanUserConfigured0 (
    CHAR16 *FileName
) {
    LOG(1, LOG_LINE_THIN_SEP, L"Process User Configured Stanzas");
    ScanUserConfigured (FileName);
}

// Scan internal disks for valid EFI boot loaders.
static
VOID ScanInternal (VOID) {
    UINTN VolumeIndex;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_THIN_SEP, L"Scan for Internal Disk Volumes with Mode:- 'UEFI'");
    #endif

    FirstLoaderScan = TRUE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_INTERNAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for

    FirstLoaderScan = FALSE;
} // static VOID ScanInternal()

// Scan external disks for valid EFI boot loaders.
static
VOID ScanExternal (VOID) {
    UINTN VolumeIndex;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_THIN_SEP, L"Scan for External Disk Volumes with Mode:- 'UEFI'");
    #endif

    FirstLoaderScan = TRUE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_EXTERNAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for

    FirstLoaderScan = FALSE;

} // static VOID ScanExternal()

// Scan internal disks for valid EFI boot loaders.
static
VOID ScanOptical (VOID) {
    UINTN VolumeIndex;

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_THIN_SEP, L"Scan for Optical Discs with Mode:- 'UEFI'");
    #endif

    FirstLoaderScan = TRUE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_OPTICAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for
    FirstLoaderScan = FALSE;

} // static VOID ScanOptical()

// Scan options stored in UEFI firmware's boot list. Adds discovered and allowed
// items to the specified Row.
// If MatchThis != NULL, only adds items with labels containing any element of
// the MatchThis comma-delimited string; otherwise, searches for anything that
// does not match GlobalConfig.DontScanFirmware or the contents of the
// HiddenFirmware UEFI variable.
// If Icon != NULL, uses the specified icon; otherwise tries to find one to
// match the label.
static
BOOLEAN ScanFirmwareDefined (
    IN UINTN     Row,
    IN CHAR16   *MatchThis,
    IN PoolImage *Icon_PI_
) {
    LOGPROCENTRY("Row:%d Match:%s Icon:%p", Row, MatchThis, GetPoolImage(Icon));
    BOOT_ENTRY_LIST *BootEntries;
    BOOT_ENTRY_LIST *CurrentEntry;
    BOOLEAN          ScanIt;
    CHAR16          *OneElement  = NULL;
    CHAR16          *DontScanFirmware;
    UINTN            i;
    BOOLEAN          result = FALSE;

    #if REFIT_DEBUG > 0
    if (Row == 0) {
        LOG(1, LOG_LINE_THIN_SEP,
            L"Scanning for UEFI Firmware Defined Boot Options"
        );
    }
    else {
        LOG(1, LOG_LINE_THIN_SEP,
            L"Scanning for UEFI firmware-defined boot options '%s'", MatchThis ? MatchThis : L"NULL"
        );
    }
    #endif

    DontScanFirmware = ReadHiddenTags (L"HiddenFirmware");

    #if REFIT_DEBUG > 0
    if (GlobalConfig.DontScanFirmware != NULL) {
        LOG(3, LOG_LINE_NORMAL,
            L"GlobalConfig.DontScanFirmware:- '%s'",
            GlobalConfig.DontScanFirmware
        );
    }
    if (DontScanFirmware != NULL) {
        LOG(3, LOG_LINE_NORMAL,
            L"Firmware Hidden Tags:- '%s'",
            DontScanFirmware
        );
    }
    #endif

    if (GlobalConfig.DontScanFirmware != NULL || DontScanFirmware != NULL) {
        MergeStrings (&DontScanFirmware, GlobalConfig.DontScanFirmware, L',');
    }

    if (Row == 0) {
        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID, L"NB: Excluding UEFI Shell from Scan");
        #endif

        MergeStrings(&DontScanFirmware, L"shell", L',');
    }

    #if REFIT_DEBUG > 0
    if (DontScanFirmware != NULL) {
        LOG(3, LOG_LINE_NORMAL,
            L"Merged Firmware Scan Exclusion List:- '%s'",
            DontScanFirmware
        );
    }
    #endif

    BootEntries  = FindBootOrderEntries();
    CurrentEntry = BootEntries;

    while (CurrentEntry != NULL) {
        if (DontScanFirmware && *DontScanFirmware && IsInSubstring (CurrentEntry->BootEntry.Label, DontScanFirmware)) {
            ScanIt = FALSE;
        }
        else if (MatchThis) {
            ScanIt = FALSE;
            i = 0;
            while (!ScanIt && (OneElement = FindCommaDelimited (MatchThis, i++))) {
                if (StriSubCmp (OneElement, CurrentEntry->BootEntry.Label)) {
                    ScanIt = TRUE;
                }
                MY_FREE_POOL(OneElement);
            } // while
        }
        else {
            ScanIt = TRUE;
        }

        if (ScanIt) {
            #if REFIT_DEBUG > 0
            LOG(3, LOG_LINE_NORMAL,
                L"Adding UEFI Loader Entry for '%s'",
                CurrentEntry->BootEntry.Label
            );
            #endif

            AddEfiLoaderEntry (
                CurrentEntry->BootEntry.DevPath,
                CurrentEntry->BootEntry.Label,
                CurrentEntry->BootEntry.BootNum,
                Row,
                Icon_PI_
            );
            result = TRUE;
        }

        CurrentEntry = CurrentEntry->NextBootEntry;
    } // while

    MY_FREE_POOL(DontScanFirmware);
    DeleteBootOrderEntries (BootEntries);

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Processed UEFI Firmware Defined Boot Options");
    #endif
    LOGPROCEXIT("result:%d", result);
    return result;
} // static VOID ScanFirmwareDefined()

// default volume badge icon based on disk kind
PoolImage * GetDiskBadge (IN UINTN DiskType) {
    PoolImage * Badge = NULL;

    if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_BADGES) {
        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID, L"Skipped ... 3onfig Setting is Active:- 'HideUI Badges'");
        #endif

        return NULL;
    }

    switch (DiskType) {
        case BBS_HARDDISK: Badge = BuiltinIcon (BUILTIN_ICON_VOL_INTERNAL); break;
        case BBS_USB:      Badge = BuiltinIcon (BUILTIN_ICON_VOL_EXTERNAL); break;
        case BBS_CDROM:    Badge = BuiltinIcon (BUILTIN_ICON_VOL_OPTICAL);  break;
    } // switch

    return Badge;
} // EG_IMAGE * GetDiskBadge()

//
// pre-boot tool functions
//

static
LOADER_ENTRY * AddToolEntry (
    IN REFIT_VOLUME *Volume,
    IN CHAR16       *LoaderPath,
    IN CHAR16       *LoaderTitle,
    IN PoolImage    *Image_PI_,
    IN CHAR16        ShortcutLetter,
    IN BOOLEAN       UseGraphicsMode
) {
    LOADER_ENTRY *Entry;

    Entry = AllocateZeroPool (sizeof (LOADER_ENTRY));
    if (LoaderTitle) {
        CopyPoolStr (&Entry->me.Title, LoaderTitle);
    }
    else {
        AssignCachedPoolStr (&Entry->me.Title, L"Unknown Tool");
    }
    Entry->me.Tag            = TAG_TOOL;
    Entry->me.Row            = 1;
    Entry->me.ShortcutLetter = ShortcutLetter;
    CopyFromPoolImage (&Entry->me.Image, Image);
    CopyPoolStr (&Entry->LoaderPath, LoaderPath);
    AssignVolume (&Entry->Volume, Volume);
    Entry->UseGraphicsMode   = UseGraphicsMode;

    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *)Entry);

    return Entry;
} // static LOADER_ENTRY * AddToolEntry()

// Locates boot loaders.
// NOTE: This assumes that GlobalConfig.LegacyType is correctly set.
VOID ScanForBootloaders (
    BOOLEAN ShowMessage
) {
    LOGPROCENTRY();
    UINTN     i;
    BOOLEAN   ScanForLegacy   = FALSE;
    BOOLEAN   DeleteItem      = FALSE;
    BOOLEAN   AmendedDontScan = FALSE;
    EG_PIXEL  BGColor         = COLOR_LIGHTBLUE;
    CHAR16   *DontScanItem    = NULL;
    CHAR16   *HiddenTags, *HiddenLegacy;
    CHAR16   *OrigDontScanDirs = NULL;
    CHAR16   *OrigDontScanFiles;
    CHAR16   *OrigDontScanVolumes;
    CHAR16    ShortCutKey;

    #if REFIT_DEBUG > 0
    CHAR16  *MsgStr = NULL;
    #endif

    ScanningLoaders = TRUE;

    #if REFIT_DEBUG > 0
    MsgStr = StrDuplicate (L"Seek Boot Loaders");
    LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
    MsgLog ("%s...\n", MsgStr);
    MY_FREE_POOL(MsgStr);
    #endif

    if (ShowMessage){
        egDisplayMessage (L"Seeking Boot Loaders. Please Wait...", &BGColor, CENTER);
    }

    // Determine up-front if we will be scanning for legacy loaders
    for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
        if (GlobalConfig.ScanFor[i] == 'c' || GlobalConfig.ScanFor[i] == 'C' ||
            GlobalConfig.ScanFor[i] == 'h' || GlobalConfig.ScanFor[i] == 'H' ||
            GlobalConfig.ScanFor[i] == 'b' || GlobalConfig.ScanFor[i] == 'B'
        ) {
            ScanForLegacy = TRUE;
            break;
        }
    } // for

    // If UEFI & scanning for legacy loaders & deep legacy scan, update NVRAM boot manager list
    if ((GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) &&
        ScanForLegacy && GlobalConfig.DeepLegacyScan
    ) {
        BdsDeleteAllInvalidLegacyBootOptions();
        BdsAddNonExistingLegacyBootOptions();
    }

    if (GlobalConfig.HiddenTags) {
        // We temporarily modify GlobalConfig.DontScanFiles and GlobalConfig.DontScanVolumes
        // to include contents of EFI HiddenTags and HiddenLegacy variables so that we do not
        // have to re-load these UEFI variables in several functions called from this one. To
        // do this, we must be able to restore the original contents, so back them up first.
        // We do *NOT* do this with the GlobalConfig.DontScanFirmware and
        // GlobalConfig.DontScanTools variables because they are used in only one function
        // each, so it is easier to create a temporary variable for the merged contents
        // there and not modify the global variable.
        OrigDontScanFiles   = StrDuplicate (GlobalConfig.DontScanFiles);
        OrigDontScanVolumes = StrDuplicate (GlobalConfig.DontScanVolumes);

        // Add hidden tags to two GlobalConfig.DontScan* variables.
        HiddenTags = ReadHiddenTags (L"HiddenTags");
        if ((HiddenTags) && (StrLen (HiddenTags) > 0)) {
            #if REFIT_DEBUG > 0
            LOG(3, LOG_LINE_NORMAL,
                L"Merging HiddenTags into 'Dont Scan Files':- '%s'",
                HiddenTags
            );
            LOG(3, LOG_BLANK_LINE_SEP, L"X");
            #endif

            MergeStrings (&GlobalConfig.DontScanFiles, HiddenTags, L',');
        }
        MY_FREE_POOL(HiddenTags);

        HiddenLegacy = ReadHiddenTags (L"HiddenLegacy");
        if ((HiddenLegacy) && (StrLen (HiddenLegacy) > 0)) {
            #if REFIT_DEBUG > 0
            LOG(3, LOG_LINE_NORMAL,
                L"Merging HiddenLegacy into 'Dont Scan Volumes':- '%s'",
                HiddenLegacy
            );
            LOG(3, LOG_BLANK_LINE_SEP, L"X");
            #endif

            MergeStrings (&GlobalConfig.DontScanVolumes, HiddenLegacy, L',');
        }

        MY_FREE_POOL(HiddenLegacy);
    } // if GlobalConfig.HiddenTags

    // DA-TAG: Override Hidden/Disabled PreBoot Volumes if SyncAPFS is Active
    if (GlobalConfig.SyncAPFS) {
        OrigDontScanDirs = StrDuplicate (GlobalConfig.DontScanDirs);

        if (GlobalConfig.DontScanFiles) {
            while ((DontScanItem = FindCommaDelimited (GlobalConfig.DontScanFiles, i)) != NULL) {
                DeleteItem = (MyStrStrIns (DontScanItem, L"Preboot:"))
                    ? TRUE
                    : (MyStriCmp (DontScanItem, L"Preboot"))
                        ? TRUE
                        : FALSE;

                if (!DeleteItem) {
                    i++;
                }
                else {
                    DeleteItemFromCsvList (DontScanItem, GlobalConfig.DontScanFiles);
                    AmendedDontScan = TRUE;
                }

                MY_FREE_POOL(DontScanItem);
            } // while
        } // if GlobalConfig

        if (GlobalConfig.DontScanDirs) {
            while ((DontScanItem = FindCommaDelimited (GlobalConfig.DontScanDirs, i)) != NULL) {
                DeleteItem = (MyStrStrIns (DontScanItem, L"Preboot:"))
                    ? TRUE
                    : (MyStriCmp (DontScanItem, L"Preboot"))
                        ? TRUE
                        : FALSE;

                if (!DeleteItem) {
                    i++;
                }
                else {
                    DeleteItemFromCsvList (DontScanItem, GlobalConfig.DontScanDirs);
                    AmendedDontScan = TRUE;
                }

                MY_FREE_POOL(DontScanItem);
            } // while
        } // if GlobalConfig

        if (GlobalConfig.DontScanVolumes) {
            while ((DontScanItem = FindCommaDelimited (GlobalConfig.DontScanVolumes, i)) != NULL) {
                DeleteItem = (MyStrStrIns (DontScanItem, L"Preboot:"))
                    ? TRUE
                    : (MyStriCmp (DontScanItem, L"Preboot"))
                        ? TRUE
                        : FALSE;

                if (!DeleteItem) {
                    i++;
                }
                else {
                    DeleteItemFromCsvList (DontScanItem, GlobalConfig.DontScanVolumes);
                    AmendedDontScan = TRUE;
                }

                MY_FREE_POOL(DontScanItem);
            } // while
        } // if GlobalConfig

        #if REFIT_DEBUG > 0
        if (AmendedDontScan) {
            LOG(3, LOG_STAR_SEPARATOR, L"Ignored PreBoot Volumes in 'Dont Scan' List ... SyncAPFS is Active");
        }
        #endif
    } // if GlobalConfig.SyncAPFS

/*
    // get count of options set to be scanned
    UINTN SetOptions = 0;
    for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
        switch (GlobalConfig.ScanFor[i]) {
            case 'm': case 'M':
            case 'i': case 'I':
            case 'h': case 'H':
            case 'e': case 'E':
            case 'b': case 'B':
            case 'o': case 'O':
            case 'c': case 'C':
            case 'n': case 'N':
            case 'f': case 'F':
            SetOptions = SetOptions + 1;
        } // switch
    } // for
*/

    // scan for loaders and tools, add them to the menu
    for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
        switch (GlobalConfig.ScanFor[i]) {

            #define DOONEMENUKEY(letter, msg, what) \
                case letter : case letter - 32: \
                    LOGBLOCKENTRY(msg); \
                    MsgLog(msg ":\n"); \
                    what ; \
                    LOGBLOCKEXIT(msg); \
                    break;

            DOONEMENUKEY('m', "Scan Manual"            , ScanUserConfigured0 (GlobalConfig.ConfigFilename));
            DOONEMENUKEY('i', "Scan Internal"          , ScanInternal());
            DOONEMENUKEY('h', "Scan Internal (Legacy)" , ScanLegacyInternal());
            DOONEMENUKEY('e', "Scan External"          , ScanExternal());
            DOONEMENUKEY('b', "Scan External (Legacy)" , ScanLegacyExternal());
            DOONEMENUKEY('o', "Scan Optical"           , ScanOptical());
            DOONEMENUKEY('c', "Scan Optical (Legacy)"  , ScanLegacyDisc());
            DOONEMENUKEY('n', "Scan Net Boot"          , ScanNetboot());
            DOONEMENUKEY('f', "Scan Firmware"          , ScanFirmwareDefined(0, NULL, NULL));
        } // switch
    } // for

    if (GlobalConfig.HiddenTags) {
        // Restore the backed-up GlobalConfig.DontScan* variables
        MY_FREE_POOL(GlobalConfig.DontScanFiles);
        MY_FREE_POOL(GlobalConfig.DontScanVolumes);
        GlobalConfig.DontScanFiles   = OrigDontScanFiles;
        GlobalConfig.DontScanVolumes = OrigDontScanVolumes;
        LEAKABLE (GlobalConfig.DontScanFiles, "DontScanFiles");
        LEAKABLE (GlobalConfig.DontScanVolumes, "DontScanVolumes");
    }

    if (OrigDontScanDirs) {
        if (GlobalConfig.SyncAPFS && AmendedDontScan) {
            MY_FREE_POOL(GlobalConfig.DontScanDirs);
            GlobalConfig.DontScanDirs = OrigDontScanDirs;
        }
        else {
            MY_FREE_POOL(OrigDontScanDirs);
        }
    }

    if (MainMenu->EntryCount < 1) {
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Could Not Find Boot Loaders");
        LOG(4, LOG_THREE_STAR_MID, L"%s", MsgStr);
        MsgLog ("* WARN: %s\n\n", MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif
    }
    else {
        // assign shortcut keys
        #if REFIT_DEBUG > 0
        MsgStr = StrDuplicate (L"Assign Shortcut Keys");
        LOG(1, LOG_LINE_SEPARATOR, L"%s", MsgStr);
        MsgLog ("\n\n");
        MsgLog ("%s:\n", MsgStr);
        MY_FREE_POOL(MsgStr);

        UINTN KeyNum = 0;
        #endif

        for (i = 0; i < MainMenu->EntryCount && MainMenu->Entries[i]->Row == 0; i++) {
            if (i < 9) {
                #if REFIT_DEBUG > 0
                KeyNum = i + 1;
                #endif

                ShortCutKey = (CHAR16) ('1' + i);
            }
            else if (i == 9) {
                ShortCutKey = (CHAR16) ('9' - i);

                #if REFIT_DEBUG > 0
                KeyNum = 0;
                #endif
            }
            else {
                break;
            }
            MainMenu->Entries[i]->ShortcutDigit = ShortCutKey;

            #if REFIT_DEBUG > 0
            MsgStr = PoolPrint (
                L"Set Key '%d' to %s",
                KeyNum, GetPoolStr (&MainMenu->Entries[i]->Title)
            );
            LOG(3, LOG_LINE_NORMAL, L"%s", MsgStr);
            MsgLog ("  - %s", MsgStr);
            MY_FREE_POOL(MsgStr);

            if (KeyNum < MainMenu->EntryCount &&
                MainMenu->Entries[i]->Row == 0 &&
                KeyNum != 0
            ) {
                MsgLog ("\n");
            }
            else {
                MsgLog ("\n\n");
            }
            #endif
        }  // for

        #if REFIT_DEBUG > 0
        MsgStr = PoolPrint (
            L"Assigned Shortcut Key%s to %d of %d Loader%s",
            (i == 1) ? L"" : L"s",
            i, MainMenu->EntryCount,
            (MainMenu->EntryCount == 1) ? L"" : L"s"
        );
        LOG(2, LOG_THREE_STAR_SEP, L"%s", MsgStr);
        MsgLog ("INFO: %s\n\n", MsgStr);
        MY_FREE_POOL(MsgStr);
        #endif
    }

    // wait for user ACK when there were errors
    FinishTextScreen (FALSE);

    ScanningLoaders = FALSE;
    LOGPROCEXIT();
} // VOID ScanForBootloaders()

// Checks to see if a specified file seems to be a valid tool.
// Returns TRUE if it passes all tests, FALSE otherwise
static
BOOLEAN IsValidTool (
    REFIT_VOLUME *BaseVolume,
    CHAR16       *PathName
) {
    CHAR16  *TestVolName = NULL, *TestPathName = NULL, *TestFileName = NULL, *DontScanTools = NULL;
    BOOLEAN  retval = TRUE;
    UINTN    i      = 0;

    if (!FileExists (BaseVolume->RootDir, PathName)) {
        // Early return if file does not exist
        return FALSE;
    }

    if (gHiddenTools == NULL) {
        DontScanTools = ReadHiddenTags (L"HiddenTools");
        gHiddenTools  = (DontScanTools) ? StrDuplicate (DontScanTools) : StrDuplicate (L"NotSet");
        LEAKABLE (gHiddenTools, "gHiddenTools");
    }
    else if (!MyStriCmp (gHiddenTools, L"NotSet")) {
        DontScanTools = StrDuplicate (gHiddenTools);
    }

    if (!MyStriCmp (gHiddenTools, L"NotSet")) {
        MergeStrings (&DontScanTools, GlobalConfig.DontScanTools, L',');
    }

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL,
        L"Check File is Valid:- '%s'",
        PathName
    );
    #endif

    if (FileExists (BaseVolume->RootDir, PathName) &&
        IsValidLoader (BaseVolume->RootDir, PathName)
    ) {
        SplitPathName (PathName, &TestVolName, &TestPathName, &TestFileName);

        CHAR16 *DontScanThis = NULL;
        while (retval && (DontScanThis = FindCommaDelimited(DontScanTools, i++))) {
            CHAR16 *DontVolName = NULL, *DontPathName = NULL, *DontFileName = NULL;
            SplitPathName (DontScanThis, &DontVolName, &DontPathName, &DontFileName);
            if (MyStriCmp (TestFileName, DontFileName) &&
                ((DontPathName == NULL) || (MyStriCmp (TestPathName, DontPathName))) &&
                ((DontVolName == NULL) || (VolumeMatchesDescription (BaseVolume, DontVolName)))
            ) {
                retval = FALSE;
            }

            MY_FREE_POOL(DontVolName);
            MY_FREE_POOL(DontScanThis);
            MY_FREE_POOL(DontPathName);
            MY_FREE_POOL(DontFileName);
        } // while
    }
    else {
        retval = FALSE;
    }

    MY_FREE_POOL(TestVolName);
    MY_FREE_POOL(TestPathName);
    MY_FREE_POOL(TestFileName);
    MY_FREE_POOL(DontScanTools);

    return retval;
} // BOOLEAN IsValidTool()

// Locate a single tool from the specified Locations using one of the
// specified Names and add it to the menu.
static
BOOLEAN FindTool (
    CHAR16 *Locations,
    CHAR16 *Names,
    CHAR16 *Description,
    UINTN   Icon
) {
    UINTN    j = 0;
    UINTN    k = 0;
    UINTN    VolumeIndex;
    CHAR16  *DirName   = NULL;
    CHAR16  *FileName  = NULL;
    CHAR16  *PathName  = NULL;
    BOOLEAN  FoundTool = FALSE;

    #if REFIT_DEBUG > 0
    CHAR16 *ToolStr = NULL;
    #endif

    while ((DirName = FindCommaDelimited (Locations, j++)) != NULL) {
        k = 0;
        while ((FileName = FindCommaDelimited (Names, k++)) != NULL) {
            PathName  = StrDuplicate (DirName);
            MergeStrings (&PathName, FileName, MyStriCmp (PathName, L"\\") ? 0 : L'\\');
            for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                if (Volumes[VolumeIndex]->RootDir != NULL &&
                    IsValidTool (Volumes[VolumeIndex], PathName)
                ) {
                    #if REFIT_DEBUG > 0
                    LOG(3, LOG_LINE_NORMAL,
                        L"Adding tag for '%s' on '%s'",
                        FileName, GetPoolStr (&Volumes[VolumeIndex]->VolName)
                    );
                    #endif

                    AddToolEntry (
                        Volumes[VolumeIndex],
                        PathName, Description,
                        BuiltinIcon (Icon),
                        0, FALSE
                    );

                    #if REFIT_DEBUG > 0
                    ToolStr = PoolPrint (
                        L"Added Tool:- '%s' : %s%s",
                        Description, DirName, FileName
                    );

                    LOG(3, LOG_THREE_STAR_MID, L"%s", ToolStr);

                    if (FoundTool) {
                        MsgLog ("\n                               ");
                    }
                    MsgLog ("%s", ToolStr);
                    MY_FREE_POOL(ToolStr);
                    #endif

                    FoundTool = TRUE;
                } // if Volumes[VolumeIndex]->RootDir
            } // for

            MY_FREE_POOL(PathName);
            MY_FREE_POOL(FileName);
        } // while Names

        MY_FREE_POOL(DirName);
    } // while Locations

    return FoundTool;
} // VOID FindTool()

// Add the second-row tags containing built-in and external tools
VOID ScanForTools (VOID) {
    UINTN             i, k;
    UINTN             VolumeIndex;
    VOID             *ItemBuffer = 0;
    CHAR16           *TmpStr     = NULL;
    CHAR16           *FileName   = NULL;
    CHAR16           *VolumeTag  = NULL;
    CHAR16           *RecoverVol = NULL;
    CHAR16           *MokLocations;
    CHAR16           *Description;
    UINT64            osind;
    UINT32            CsrValue;

    LOGPROCENTRY();

    #if REFIT_DEBUG > 0
    CHAR16 *ToolStr   = NULL;
    UINTN   ToolTotal = 0;
    #endif

    #if REFIT_DEBUG > 0
    LOG(1, LOG_LINE_SEPARATOR, L"Scan for UEFI Tools");
    MsgLog ("Scan for UEFI Tools...\n");
    #endif

    MokLocations = MergeStringsNew (MOK_LOCATIONS, SelfDirPath, L',');

    for (i = 0; i < NUM_TOOLS; i++) {
        // Reset Vars
        BOOLEAN FoundTool = FALSE;
        BOOLEAN Skipped = FALSE;

        UINTN j = 0;
        BOOLEAN OtherFind = FALSE;

        UINTN TheTag = GlobalConfig.ShowTools[i];

        CHAR16 *TheTitle = GetPoolStr (&TagMenuEntry[TheTag].Title);

        CHAR16 *ToolName = NULL;
        UINTN TheIcon = 0;
        CHAR8 TheShortcut = 0;
        switch (TheTag) {
            #define TAGS_TAG_TO_TOOL_AND_ICON_AND_SHORTCUT
            #include "tags.include"
        }
        if (!ToolName) {
            Skipped = TRUE;
            ToolName = L"Skipped";
        }

        if (!Skipped) {
            ToolTotal = ToolTotal + 1;
            LOGBLOCKENTRY("Tool Type %02d ...%s", ToolTotal, ToolName);
        }

        #if REFIT_DEBUG > 0
            #define TAGLOG() \
                ToolStr = PoolPrint (L"Added Tool:- '%s'", ToolName); \
                LOG(3, LOG_THREE_STAR_END, L"%s", ToolStr); \
                MsgLog ("%s\n", ToolStr); \
                MY_FREE_POOL(ToolStr);
        #else
            #define TAGLOG()
        #endif

        #define TagAddMenuEntry() \
            do { \
                REFIT_MENU_ENTRY *TempMenuEntry; \
                FoundTool = TRUE; \
                TempMenuEntry = CopyMenuEntry (&TagMenuEntry[TheTag]); \
                CopyFromPoolImage_PI_ (&TempMenuEntry->Image_PI_, BuiltinIcon (TheIcon)); \
                TempMenuEntry->ShortcutLetter = TheShortcut; \
                AddMenuEntry (MainMenu, TempMenuEntry); \
                TAGLOG(); \
            } while (0)


        switch (TheTag) {

            case TAG_PRE_NVRAMCLEAN:
            case TAG_PRE_BOOTKICKER:
            case TAG_SHUTDOWN      :
            case TAG_REBOOT        :
            case TAG_ABOUT         :
            case TAG_EXIT          :
            case TAG_INSTALL       :
            case TAG_BOOTORDER     : TagAddMenuEntry(); break;

            case TAG_MOK_TOOL     : FoundTool = FindTool (MokLocations                        , MOK_NAMES     , TheTitle, TheIcon); break;
            case TAG_FWUPDATE_TOOL: FoundTool = FindTool (MokLocations                        , FWUPDATE_NAMES, TheTitle, TheIcon); break;
            case TAG_MEMTEST      : FoundTool = FindTool (MEMTEST_LOCATIONS MEMTEST_LOCATIONS2, MEMTEST_NAMES , TheTitle, TheIcon); break;

            case TAG_HIDDEN:
                if (GlobalConfig.HiddenTags) {
                    TagAddMenuEntry();
                }
                break;

            case TAG_CSR_ROTATE:
                if ((GetCsrStatus (&CsrValue) == EFI_SUCCESS) && (GlobalConfig.CsrValues)) {
                    TagAddMenuEntry();
                } // if
                break;

            case TAG_FIRMWARE:
                if (EfivarGetRaw (
                    &GlobalGuid,
                    L"OsIndicationsSupported",
                    &ItemBuffer,
                    NULL
                ) != EFI_SUCCESS) {
                    #if REFIT_DEBUG > 0
                    LOG(3, LOG_LINE_NORMAL,
                        L"'Showtools' Includes Firmware Tool but 'OsIndicationsSupported' Variable is Missing!!"
                    );
                    #endif
                }
                else {
                    osind = *(UINT64*) ItemBuffer;
                    if (osind & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) {
                        TagAddMenuEntry();
                    }
                    MY_FREE_POOL(ItemBuffer);
                }
                break;

            case TAG_SHELL:
                while ((FileName = FindCommaDelimited (SHELL_NAMES, j++)) != NULL) {
                    if (IsValidTool (SelfVolume, FileName)) {
                        #if REFIT_DEBUG > 0
                        LOG(3, LOG_LINE_NORMAL,
                            L"Adding Shell Tag:- '%s' on '%s'",
                            FileName,
                            GetPoolStr (&SelfVolume->VolName)
                        );
                        #endif

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume, FileName,
                            TheTitle,
                            BuiltinIcon (TheIcon),
                            TheShortcut, FALSE
                        );

                        #if REFIT_DEBUG > 0
                        ToolStr = PoolPrint (L"Added Tool:- '%s' ... %s", ToolName, FileName);
                        LOG(3, LOG_THREE_STAR_MID, L"%s", ToolStr);
                        if (OtherFind) {
                            MsgLog ("\n                               ");
                        }
                        MsgLog ("%s\n", ToolStr);
                        MY_FREE_POOL(ToolStr);
                        #endif

                        OtherFind = TRUE;
                    }

                    MY_FREE_POOL(FileName);
                } // while

                // joevt - look for Shell from firmware even if we added Shells from disk
                {
                    #if REFIT_DEBUG > 0
                    LOG(3, LOG_BLANK_LINE_SEP, L"X");
                    #endif

                    if (ScanFirmwareDefined (1, L"Shell", BuiltinIcon(TheIcon))) {
                        FoundTool = TRUE;
                    }

                    #if REFIT_DEBUG > 0
                    LOG(3, LOG_BLANK_LINE_SEP, L"X");
                    #endif
                }

                break;

            case TAG_GPTSYNC:
                while ((FileName = FindCommaDelimited (GPTSYNC_NAMES, j++)) != NULL) {
                    if (IsValidTool (SelfVolume, FileName)) {
                        #if REFIT_DEBUG > 0
                        LOG(3, LOG_LINE_NORMAL,
                            L"Adding Hybrid MBR Tag:- '%s' on '%s'",
                            FileName, GetPoolStr (&SelfVolume->VolName)
                        );
                        #endif

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume, FileName,
                            TheTitle,
                            BuiltinIcon (TheIcon),
                            TheShortcut, FALSE
                        );

                        #if REFIT_DEBUG > 0
                        ToolStr = PoolPrint (L"Added Tool:- '%s' ... %s", ToolName, FileName);
                        LOG(3, LOG_THREE_STAR_END, L"%s", ToolStr);
                        if (j > 0) {
                            MsgLog ("\n                               ");
                        }
                        MsgLog ("%s\n", ToolStr);
                        MY_FREE_POOL(ToolStr);
                        #endif
                    }

                    MY_FREE_POOL(FileName);
                } // while
                break;

            case TAG_GDISK:
                while ((FileName = FindCommaDelimited (GDISK_NAMES, j++)) != NULL) {
                    if (IsValidTool (SelfVolume, FileName)) {
                        #if REFIT_DEBUG > 0
                        LOG(3, LOG_LINE_NORMAL,
                            L"Adding GDisk Tag:- '%s' on '%s'",
                            FileName, GetPoolStr (&SelfVolume->VolName)
                        );
                        #endif

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume, FileName,
                            TheTitle,
                            BuiltinIcon (TheIcon),
                            TheShortcut, FALSE
                        );

                        #if REFIT_DEBUG > 0
                        ToolStr = PoolPrint (L"Added Tool:- '%s' ... %s", ToolName, FileName);
                        LOG(3, LOG_THREE_STAR_END, L"%s", ToolStr);
                        if (OtherFind) {
                            MsgLog ("\n                               ");
                        }
                        MsgLog ("%s\n", ToolStr);
                        MY_FREE_POOL(ToolStr);
                        #endif

                        OtherFind = TRUE;
                    }

                    MY_FREE_POOL(FileName);
                } // while
                break;

            case TAG_NETBOOT:
                while ((FileName = FindCommaDelimited (NETBOOT_NAMES, j++)) != NULL) {
                    if (IsValidTool (SelfVolume, FileName)) {
                        #if REFIT_DEBUG > 0
                        LOG(3, LOG_LINE_NORMAL,
                            L"Adding Netboot Tag:- '%s' on '%s'",
                            FileName, GetPoolStr (&SelfVolume->VolName)
                        );
                        #endif

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume, FileName,
                            TheTitle,
                            BuiltinIcon (TheIcon),
                            TheShortcut, FALSE
                        );

                        #if REFIT_DEBUG > 0
                        ToolStr = PoolPrint (L"Added Tool:- '%s' ... %s", ToolName, FileName);
                        LOG(3, LOG_THREE_STAR_END, L"%s", ToolStr);
                        if (OtherFind) {
                            MsgLog ("\n                               ");
                        }
                        MsgLog ("%s\n", ToolStr);
                        MY_FREE_POOL(ToolStr);
                        #endif

                        OtherFind = TRUE;
                    }

                    MY_FREE_POOL(FileName);
                } // while
                break;

            case TAG_APPLE_RECOVERY:
                for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                    j = 0;
                    while (
                        (
                            FileName = FindCommaDelimited (
                                GlobalConfig.MacOSRecoveryFiles, j++
                            )
                        ) != NULL
                    ) {
                        if ((Volumes[VolumeIndex]->RootDir != NULL) &&
                            (IsValidTool (Volumes[VolumeIndex], FileName))
                        ) {
                            // Get a meaningful tag for the recovery volume if available
                            for (k = 0; k < VolumesCount; k++) {
                                TmpStr = GuidAsString (&(Volumes[k]->VolUuid));
                                if (MyStrStrIns (FileName, TmpStr)) {
                                    MY_FREE_POOL(TmpStr);
                                    RecoverVol = StrDuplicate (GetPoolStr (&Volumes[k]->VolName));

                                    break;
                                }
                                MY_FREE_POOL(TmpStr);
                            } // for

                            VolumeTag = RecoverVol ? RecoverVol : GetPoolStr (&Volumes[VolumeIndex]->VolName);

                            #if REFIT_DEBUG > 0
                            LOG(3, LOG_LINE_NORMAL,
                                L"Adding Mac Recovery Tag:- '%s' for '%s'",
                                FileName, VolumeTag
                            );
                            #endif

                            FoundTool = TRUE;
                            Description = PoolPrint (
                                L"%s for %s",
                                ToolName, VolumeTag
                            );
                            AddToolEntry (
                                Volumes[VolumeIndex],
                                FileName, Description,
                                BuiltinIcon (TheIcon),
                                TheShortcut, TRUE
                            );
                            MY_FREE_POOL(Description);

                            #if REFIT_DEBUG > 0
                            ToolStr = PoolPrint (
                                L"Added Tool:- '%s' ... %s for %s",
                                ToolName, FileName, VolumeTag
                            );
                            LOG(3, LOG_THREE_STAR_END, L"%s", ToolStr);
                            if (OtherFind) {
                                MsgLog ("\n                               ");
                            }
                            MsgLog ("%s\n", ToolStr);
                            MY_FREE_POOL(ToolStr);
                            #endif

                            OtherFind = TRUE;
                        }

                        MY_FREE_POOL(FileName);
                        MY_FREE_POOL(RecoverVol);
                    } // while
                } // for
                break;

            case TAG_WINDOWS_RECOVERY:
                while (
                    (FileName = FindCommaDelimited (GlobalConfig.WindowsRecoveryFiles, j++)) != NULL
                ) {
                    SplitVolumeAndFilename (&FileName, &VolumeTag);
                    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                        if ((Volumes[VolumeIndex]->RootDir != NULL) &&
                            (MyStrStr (FileName, L"\\BOOT\\BOOT") != NULL) &&
                            (IsValidTool (Volumes[VolumeIndex], FileName)) &&
                            ((VolumeTag == NULL) || MyStriCmp (VolumeTag, GetPoolStr (&Volumes[VolumeIndex]->VolName)))
                        ) {
                            #if REFIT_DEBUG > 0
                            LOG(3, LOG_LINE_NORMAL,
                                L"Adding Windows Recovery Tag:- '%s' on '%s'",
                                FileName, GetPoolStr (&Volumes[VolumeIndex]->VolName)
                            );
                            #endif

                            FoundTool = TRUE;
                            Description = PoolPrint (
                                L"%s on %s",
                                ToolName,
                                GetPoolStr (&Volumes[VolumeIndex]->VolName)
                            );
                            AddToolEntry (
                                Volumes[VolumeIndex],
                                FileName,
                                Description,
                                BuiltinIcon (TheIcon),
                                TheShortcut,
                                TRUE
                            );
                            MY_FREE_POOL(Description);

                            #if REFIT_DEBUG > 0
                            ToolStr = PoolPrint (L"Added Tool:- '%s' ... %s", ToolName, FileName);
                            LOG(3, LOG_THREE_STAR_END, L"%s", ToolStr);
                            if (OtherFind) {
                                MsgLog ("\n                               ");
                            }
                            MsgLog ("%s\n", ToolStr);
                            MY_FREE_POOL(ToolStr);
                            #endif

                            OtherFind = TRUE;
                        } // if Volumes[VolumeIndex]->RootDir
                    } // for

                    MY_FREE_POOL(FileName);
                    MY_FREE_POOL(VolumeTag);
                } // while
                break;

        } // switch

        #if REFIT_DEBUG > 0
        if (Skipped) {
            if (FoundTool) {
                ToolStr = PoolPrint (L"Skipped Found Tool:- '%s'", ToolName);
                LOG(3, LOG_THREE_STAR_END, L"%s", ToolStr);
                MsgLog ("** WARN ** %s\n", ToolStr);
                MY_FREE_POOL(ToolStr);
            }
        }
        else {
            if (!FoundTool) {
                ToolStr = PoolPrint (L"Could Not Find Tool:- '%s'", ToolName);
                LOG(3, LOG_THREE_STAR_END, L"%s", ToolStr);
                MsgLog ("** WARN ** %s\n", ToolStr);
                MY_FREE_POOL(ToolStr);
            }
            LOGBLOCKEXIT("Tool Type %02d ...%s", ToolTotal, ToolName);
        }
        #endif

        MY_FREE_POOL(FileName);
    } // for

    #if REFIT_DEBUG > 0
    ToolStr = PoolPrint (L"Processed %d Tool Types", ToolTotal);
    LOG(3, LOG_THREE_STAR_SEP, L"%s", ToolStr);
    LOG2(3, LOG_BLANK_LINE_SEP, L"\n\nINFO: ", L"\n\n", L"%s", ToolStr);
    MY_FREE_POOL(ToolStr);
    #endif

    MY_FREE_POOL(MokLocations);

    LOGPROCEXIT();
} // VOID ScanForTools
