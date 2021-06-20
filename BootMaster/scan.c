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
    #define SHELL_NAMES \
        L"\\EFI\\tools_x64\\x64_Shell.efi,\\EFI\\tools_x64\\shell_x64.efi,\\EFI\\tools_x64\\shell.efi,\
\\EFI\\tools\\x64_Shell.efi,\\EFI\\tools\\shell_x64.efi,\\EFI\\tools\\shell.efi,\
\\EFI\\x64_Shell.efi,\\EFI\\shell_x64.efi,\\EFI\\shell.efi,\\x64_Shell.efi,\\shell_x64.efi,\\shell.efi"
    #define GPTSYNC_NAMES \
        L"\\EFI\\tools_x64\\gptsync.efi,\\EFI\\tools_x64\\gptsync_x64.efi,\\EFI\\tools\\gptsync.efi,\
\\EFI\\tools\\gptsync_x64.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_x64.efi,\\gptsync.efi,\\gptsync_x64.efi"
    #define GDISK_NAMES \
        L"\\EFI\\tools_x64\\gdisk.efi,\\EFI\\tools_x64\\gdisk_x64.efi,\\EFI\\tools\\gdisk.efi,\
\\EFI\\tools\\gdisk_x64.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_x64.efi,\\gdisk.efi,\\gdisk_x64.efi"
    #define NETBOOT_NAMES \
        L"\\EFI\\tools_x64\\ipxe.efi,\\EFI\\tools_x64\\ipxe_x64.efi,\\EFI\\tools\\ipxe.efi,\\EFI\\tools\\ipxe_x64.efi,\
\\EFI\\ipxe.efi,\\EFI\\ipxe_x64.efi,\\ipxe.efi,\\ipxe_x64.efi"
    #define MEMTEST_NAMES \
        L"memtest86_x64.efi,memtest_x64.efi,memtest86.efi,memtest.efi"
    #define BOOTKICKER_NAMES \
        L"\\EFI\\tools_x64\\x64_BootKicker.efi,\\EFI\\tools_x64\\BootKicker_x64.efi,\\EFI\\tools_x64\\BootKicker.efi,\
\\EFI\\tools\\x64_BootKicker.efi\\EFI\\tools\\BootKicker_x64.efi,\\EFI\\tools\\BootKicker.efi,\
\\EFI\\Bx64_BootKicker.efi,\\EFI\\BootKicker_x64.efi,\\EFI\\BootKicker.efi,\\x64_BootKicker.efi,\
\\BootKicker_x64.efi,\\BootKicker.efi"
    #define NVRAMCLEAN_NAMES \
        L"\\EFI\\tools_x64\\x64_CleanNvram.efi,\\EFI\\tools_x64\\CleanNvram_x64.efi,\\EFI\\tools_x64\\CleanNvram.efi,\
\\EFI\\tools\\x64_CleanNvram.efi,\\EFI\\tools\\CleanNvram_x64.efi,\\EFI\\tools\\CleanNvram.efi,\
\\EFI\\x64_CleanNvram.efi,\\EFI\\CleanNvram_x64.efi,\\EFI\\CleanNvram.efi,\\x64_CleanNvram.efi,\
\\CleanNvram_x64.efi,\\CleanNvram.efi"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootx64.efi"
    #define FALLBACK_BASENAME       L"bootx64.efi"
#elif defined (EFI32)
    #define SHELL_NAMES \
        L"\\EFI\\tools_ia32\\ia32_Shell.efi,\\EFI\\tools_ia32\\shell_ia32.efi,\\EFI\\tools_ia32\\shell.efi,\
\\EFI\\tools\\ia32_Shell.efi,\\EFI\\tools\\shell_ia32.efi,\\EFI\\tools\\shell.efi,\
\\EFI\\ia32_Shell.efi,\\EFI\\shell_ia32.efi,\\EFI\\shell.efi,\\ia32_Shell.efi,\\shell_ia32.efi,\\shell.efi"
    #define GPTSYNC_NAMES \
        L"\\EFI\\tools_ia32\\gptsync.efi,\\EFI\\tools_ia32\\gptsync_ia32.efi,\\EFI\\tools\\gptsync.efi,\
\\EFI\\tools\\gptsync_ia32.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_ia32.efi,\\gptsync.efi,\\gptsync_ia32.efi"
    #define GDISK_NAMES \
        L"\\EFI\\tools_ia32\\gdisk.efi,\\EFI\\tools_ia32\\gdisk_ia32.efi,\\EFI\\tools\\gdisk.efi,\
\\EFI\\tools\\gdisk_ia32.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_ia32.efi,\\gdisk.efi,\\gdisk_ia32.efi"
    #define NETBOOT_NAMES \
        L"\\EFI\\tools_ia32\\ipxe.efi,\\EFI\\tools_ia32\\ipxe_ia32.efi,\\EFI\\tools\\ipxe.efi,\
\\EFI\\tools\\ipxe_ia32.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_ia32.efi,\\ipxe.efi,\\ipxe_ia32.efi"
    #define MEMTEST_NAMES \
        L"memtest86_ia32.efi,memtest_ia32.efi,memtest86.efi,memtest.efi"
    #define BOOTKICKER_NAMES \
        L"\\EFI\\tools_ia32\\ia32_BootKicker.efi,\\EFI\\tools_ia32\\BootKicker_ia32.efi,\
\\EFI\\tools_ia32\\BootKicker.efi,\\EFI\\tools\\ia32_BootKicker.efi\\EFI\\tools\\BootKicker_ia32.efi,\
\\EFI\\tools\\BootKicker.efi,\\EFI\\Bia32_BootKicker.efi,\\EFI\\BootKicker_ia32.efi,\\EFI\\BootKicker.efi,\
\\ia32_BootKicker.efi,\\BootKicker_ia32.efi,\\BootKicker.efi"
    #define NVRAMCLEAN_NAMES \
        L"\\EFI\\tools_ia32\\ia32_CleanNvram.efi,\\EFI\\tools_ia32\\CleanNvram_ia32.efi,\
\\EFI\\tools_ia32\\CleanNvram.efi,\\EFI\\tools\\ia32_CleanNvram.efi,\\EFI\\tools\\CleanNvram_ia32.efi,\
\\EFI\\tools\\CleanNvram.efi,\\EFI\\ia32_CleanNvram.efi,\\EFI\\CleanNvram_ia32.efi,\\EFI\\CleanNvram.efi,\
\\ia32_CleanNvram.efi,\\CleanNvram_ia32.efi,\\CleanNvram.efi"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootia32.efi"
    #define FALLBACK_BASENAME       L"bootia32.efi"
#elif defined (EFIAARCH64)
    #define SHELL_NAMES \
        L"\\EFI\\tools_aa64\\aa64_Shell.efi,\\EFI\\tools_aa64\\shell_aa64.efi,\\EFI\\tools_aa64\\shell.efi,\
\\EFI\\tools\\aa64_Shell.efi,\\EFI\\tools\\shell_aa64.efi,\\EFI\\tools\\shell.efi,\
\\EFI\\aa64_Shell.efi,\\EFI\\shell_aa64.efi,\\EFI\\shell.efi,\\aa64_Shell.efi,\\shell_aa64.efi,\\shell.efi"
    #define GPTSYNC_NAMES \
        L"\\EFI\\tools_aa64\\gptsync.efi,\\EFI\\tools_aa64\\gptsync_aa64.efi,\\EFI\\tools\\gptsync.efi,\
\\EFI\\tools\\gptsync_aa64.efi,\\EFI\\gptsync.efi,\\EFI\\gptsync_aa64.efi,\\gptsync.efi,\\gptsync_aa64.efi"
    #define GDISK_NAMES \
        L"\\EFI\\tools_aa64\\gdisk.efi,\\EFI\\tools_aa64\\gdisk_aa64.efi,\\EFI\\tools\\gdisk.efi,\
\\EFI\\tools\\gdisk_aa64.efi,\\EFI\\gdisk.efi,\\EFI\\gdisk_aa64.efi,\\gdisk.efi,\\gdisk_aa64.efi"
    #define NETBOOT_NAMES \
        L"\\EFI\\tools_aa64\\ipxe.efi,\\EFI\\tools_aa64\\ipxe_aa64.efi,\\EFI\\tools\\ipxe.efi,\
\\EFI\\tools\\ipxe_aa64.efi,\\EFI\\ipxe.efi,\\EFI\\ipxe_aa64.efi,\\ipxe.efi,\\ipxe_aa64.efi"
    #define MEMTEST_NAMES \
        L"memtest86_aa64.efi,memtest_aa64.efi,memtest86.efi,memtest.efi"
    #define BOOTKICKER_NAMES \
        L"\\EFI\\tools_aa64\\aa64_BootKicker.efi,\\EFI\\tools_aa64\\BootKicker_aa64.efi,\
\\EFI\\tools_aa64\\BootKicker.efi,\\EFI\\tools\\aa64_BootKicker.efi\\EFI\\tools\\BootKicker_aa64.efi,\
\\EFI\\tools\\BootKicker.efi,\\EFI\\Baa64_BootKicker.efi,\\EFI\\BootKicker_aa64.efi,\\EFI\\BootKicker.efi,\
\\aa64_BootKicker.efi,\\BootKicker_aa64.efi,\\BootKicker.efi"
    #define NVRAMCLEAN_NAMES \
        L"\\EFI\\tools_aa64\\aa64_CleanNvram.efi,\\EFI\\tools_aa64\\CleanNvram_aa64.efi,\
\\EFI\\tools_aa64\\CleanNvram.efi,\\EFI\\tools\\aa64_CleanNvram.efi,\\EFI\\tools\\CleanNvram_aa64.efi,\
\\EFI\\tools\\CleanNvram.efi,\\EFI\\aa64_CleanNvram.efi,\\EFI\\CleanNvram_aa64.efi,\\EFI\\CleanNvram.efi,\
\\aa64_CleanNvram.efi,\\CleanNvram_aa64.efi,\\CleanNvram.efi"
    #define FALLBACK_FULLNAME       L"EFI\\BOOT\\bootaa64.efi"
    #define FALLBACK_BASENAME       L"bootaa64.efi"
#else
    #define SHELL_NAMES             L"\\EFI\\tools\\shell.efi,\\shell.efi"
    #define GPTSYNC_NAMES           L"\\EFI\\tools\\gptsync.efi"
    #define GDISK_NAMES             L"\\EFI\\tools\\gdisk.efi"
    #define NETBOOT_NAMES           L"\\EFI\\tools\\ipxe.efi"
    #define MEMTEST_NAMES           L"memtest.efi,memtest86.efi"
    #define BOOTKICKER_NAMES        L"\\BootKicker.efi"
    #define NVRAMCLEAN_NAMES        L"\\CleanNvram.efi"
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

BOOLEAN  ScanningLoaders = FALSE;
BOOLEAN  FirstLoaderScan = FALSE;


// Structure used to hold boot loader filenames and time stamps in
// a linked list; used to sort entries within a directory.
struct LOADER_LIST {
    CHAR16              *FileName;
    EFI_TIME            TimeStamp;
    struct LOADER_LIST  *NextEntry;
};

// Creates a new LOADER_ENTRY data structure and populates it with
// default values from the specified Entry, or NULL values if Entry
// is unspecified (NULL).
// Returns a pointer to the new data structure, or NULL if it
// couldn't be allocated
LOADER_ENTRY *InitializeLoaderEntry (IN LOADER_ENTRY *Entry) {
    LOADER_ENTRY *NewEntry = NULL;

    NewEntry = AllocateZeroPool (sizeof (LOADER_ENTRY));
    if (NewEntry != NULL) {
        NewEntry->me.Tag = TAG_LOADER;
        NewEntry->Enabled = TRUE;

        if (Entry != NULL) {
            AssignVolume (&NewEntry->Volume, Entry->Volume);
            NewEntry->EfiBootNum = Entry->EfiBootNum;
            NewEntry->UseGraphicsMode = Entry->UseGraphicsMode;
            CopyFromPoolStr (&NewEntry->LoaderPath, &Entry->LoaderPath);
            CopyFromPoolStr (&NewEntry->LoadOptions, &Entry->LoadOptions);
            CopyFromPoolStr (&NewEntry->InitrdPath, &Entry->InitrdPath);
            NewEntry->EfiLoaderPath = Entry->EfiLoaderPath ? DuplicateDevicePath (Entry->EfiLoaderPath) : NULL;
            LOGPOOL (NewEntry->EfiLoaderPath);
        }
    } // if

    return (NewEntry);
} // LOADER_ENTRY *InitializeLoaderEntry()

// Prepare a REFIT_MENU_SCREEN data structure for a subscreen entry. This sets up
// the default entry that launches the boot loader using the same options as the
// main Entry does. Subsequent options can be added by the calling function.
// If a subscreen already exists in the Entry that's passed to this function,
// it's left unchanged and a pointer to it is returned.
// Returns a pointer to the new subscreen data structure, or NULL if there
// were problems allocating memory.
REFIT_MENU_SCREEN *InitializeSubScreen (IN LOADER_ENTRY *Entry) {
    MsgLog ("[ InitializeSubScreen\n");

    CHAR16              *FileName;
    REFIT_MENU_SCREEN   *SubScreen = NULL;
    LOADER_ENTRY        *SubEntry;

    FileName = Basename (GetPoolStr (&Entry->LoaderPath));
    if (Entry->me.SubScreen == NULL) {
        // No subscreen yet; initialize default entry
        SubScreen = AllocateZeroPool (sizeof (REFIT_MENU_SCREEN));
        if (SubScreen != NULL) {
            AssignPoolStr (&SubScreen->Title, PoolPrint (
                L"Boot Options for %s on %s",
                GetPoolStr (&Entry->Title) ? GetPoolStr (&Entry->Title) : FileName, GetPoolStr (&Entry->Volume->VolName)
            ));

            LOG(3, LOG_LINE_NORMAL, L"Creating subscreen '%s'", GetPoolStr (&SubScreen->Title));

            CopyFromPoolImage (&SubScreen->TitleImage, &Entry->me.Image);
            // default entry
            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                LOG(3, LOG_LINE_NORMAL, L"Setting Entries for '%s'", GetPoolStr (&SubScreen->Title));
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot using default options");
                AssignPoolStr (&SubEntry->LoadOptions, AddInitrdToOptions (GetPoolStr (&SubEntry->LoadOptions), GetPoolStr (&SubEntry->InitrdPath)));
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            } // if (SubEntry != NULL)
            AssignCachedPoolStr (&SubScreen->Hint1, SUBSCREEN_HINT1);
            if (GlobalConfig.HideUIFlags & HIDEUI_FLAG_EDITOR) {
                AssignCachedPoolStr (&SubScreen->Hint2, SUBSCREEN_HINT2_NO_EDITOR);
            }
            else {
                AssignCachedPoolStr (&SubScreen->Hint2, SUBSCREEN_HINT2);
            } // if/else
        } // if (SubScreen != NULL)
    }
    else { // existing subscreen; less initialization, and just add new entry later....
        SubScreen = Entry->me.SubScreen;
    } // if/else

    MyFreePool (&FileName);

    MsgLog ("] InitializeSubScreen %p\n", SubScreen);
    return SubScreen;
} // REFIT_MENU_SCREEN *InitializeSubScreen()

VOID GenerateSubScreen (LOADER_ENTRY *Entry, IN REFIT_VOLUME *Volume, IN BOOLEAN GenerateReturn) {
    REFIT_MENU_SCREEN  *SubScreen;
    LOADER_ENTRY       *SubEntry;
    CHAR16             *InitrdName, *KernelVersion = NULL;
    CHAR16             DiagsFileName[256];
    REFIT_FILE         *File;
    UINTN              TokenCount;
    CHAR16             **TokenList;
    
    MsgLog ("[ GenerateSubScreen\n");

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
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS with a 64-bit kernel");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"arch=x86_64");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            } // if

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS with a 32-bit kernel");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"arch=i386");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            } // if
#endif

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SINGLEUSER)) {
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS in verbose mode");
                    SubEntry->UseGraphicsMode = FALSE;
                    AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v");
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
                } // if

#if defined (EFIX64)
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS in verbose mode (64-bit)");
                    SubEntry->UseGraphicsMode = FALSE;
                    AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v arch=x86_64");
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
                }

                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS in verbose mode (32-bit)");
                    SubEntry->UseGraphicsMode = FALSE;
                    AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v arch=i386");
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
                }
#endif

                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS in single user mode");
                    SubEntry->UseGraphicsMode = FALSE;
                    AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v -s");
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
                } // if
            } // single-user mode allowed

            if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_SAFEMODE)) {
                SubEntry = InitializeLoaderEntry (Entry);
                if (SubEntry != NULL) {
                    AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Mac OS in safe mode");
                    SubEntry->UseGraphicsMode = FALSE;
                    AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v -x");
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
                } // if
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
                    AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
                } // if
            } // if diagnostics entry found

        }
        else if (Entry->OSType == 'L') {   // entries for Linux kernels with EFI stub loaders
            File = ReadLinuxOptionsFile (GetPoolStr (&Entry->LoaderPath), Volume);
            if (File != NULL) {
                InitrdName    = FindInitrd (GetPoolStr (&Entry->LoaderPath), Volume);
                TokenCount    = ReadTokenLine (File, &TokenList);
                KernelVersion = FindNumbers (GetPoolStr (&Entry->LoaderPath));
                ReplaceSubstring (&(TokenList[1]), KERNEL_VERSION, KernelVersion);

                // first entry requires special processing, since it was initially set
                // up with a default title but correct options by InitializeSubScreen(),
                // earlier....
                if ((TokenCount > 1) && (SubScreen->Entries != NULL) && (SubScreen->Entries[0] != NULL)) {
                    if (TokenList[0]) {
                        CopyPoolStr (&SubScreen->Entries[0]->Title, TokenList[0]);
                    }
                    else {
                        AssignCachedPoolStr (&SubScreen->Entries[0]->Title, L"Boot Linux");
                    }
                } // if

                FreeTokenLine (&TokenList, &TokenCount);
                while ((TokenCount = ReadTokenLine (File, &TokenList)) > 1) {
                    ReplaceSubstring (&(TokenList[1]), KERNEL_VERSION, KernelVersion);
                    SubEntry = InitializeLoaderEntry (Entry);
                    if (SubEntry != NULL) {
                        if (TokenList[0]) {
                            CopyPoolStr (&SubEntry->me.Title, TokenList[0]);
                        }
                        else {
                            AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Linux");
                        }
                        AssignPoolStr (&SubEntry->LoadOptions, AddInitrdToOptions (TokenList[1], InitrdName));
                        SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
                        AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
                    } // if
                    FreeTokenLine (&TokenList, &TokenCount);
                } // while
                FreeTokenLine (&TokenList, &TokenCount);

                MyFreePool (&KernelVersion);
                MyFreePool (&InitrdName);
                MyFreePool (&File);
            } // if
        }
        else if (Entry->OSType == 'E') {   // entries for ELILO
            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Run ELILO in interactive mode");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-p");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Linux for a 17\" iMac or a 15\" MacBook Pro (*)");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-d 0 i17");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Linux for a 20\" iMac (*)");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-d 0 i20");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Linux for a Mac Mini (*)");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-d 0 mini");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
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
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Boot Windows from CD-ROM");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-s -c");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            }

            SubEntry = InitializeLoaderEntry (Entry);
            if (SubEntry != NULL) {
                AssignCachedPoolStr (&SubEntry->me.Title, L"Run XOM in text mode");
                AssignCachedPoolStr (&SubEntry->LoadOptions, L"-v");
                SubEntry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
                AddMenuEntry (SubScreen, (REFIT_MENU_ENTRY *)SubEntry);
            }
        } // entries for xom.efi
        if (GenerateReturn) {
            AddMenuEntryCopy (SubScreen, &TagMenuEntry[TAG_RETURN]);
        }
        Entry->me.SubScreen = SubScreen;
    }

    MsgLog ("] GenerateSubScreen\n");
} // VOID GenerateSubScreen()

// Sets a few defaults for a loader entry -- mainly the icon, but also the OS type
// code and shortcut letter. For Linux EFI stub loaders, also sets kernel options
// that will (with luck) work fairly automatically.
VOID SetLoaderDefaults (
    LOADER_ENTRY *Entry, CHAR16 *LoaderPath, REFIT_VOLUME *Volume
) {
    CHAR16      *NameClues, *PathOnly, *NoExtension, *OSIconName = NULL, *Temp;
    CHAR16      ShortcutLetter = 0;

    NameClues   = Basename (LoaderPath);
    PathOnly    = FindPath (LoaderPath);

    LOG(3, LOG_LINE_NORMAL, L"Get Default Loader Settings for '%s'", GetPoolStr (&Entry->me.Title));

    if (Volume->DiskKind == DISK_KIND_NET) {
        MergeStrings (&NameClues, GetPoolStr (&Entry->me.Title), L' ');
    }
    else {
        if (GetPoolImage (&Entry->me.Image) == NULL) {
            NoExtension = StripEfiExtension (NameClues);
            if (NoExtension != NULL) {
        // locate a custom icon for the loader
        // Anything found here takes precedence over the "hints" in the OSIconName variable
                LOG(4, LOG_LINE_NORMAL, L"Trying to load icon from boot loader's directory");

        if (!GetPoolImage (&Entry->me.Image)) {
            AssignPoolImage (&Entry->me.Image, egLoadIconAnyType (
                Volume->RootDir,
                PathOnly,
                NoExtension,
                GlobalConfig.IconSizes[ICON_SIZE_BIG]
            ));
        }
        if (!GetPoolImage (&Entry->me.Image)) {
            CopyFromPoolImage (&Entry->me.Image, &Volume->VolIconImage);
        }
            }
        }

        // Begin creating icon "hints" by using last part of directory path leading
        // to the loader
        #if REFIT_DEBUG > 0
        if (GetPoolImage (&Entry->me.Image) == NULL) {
            LOG(4, LOG_LINE_NORMAL, L"Creating Icon Hint from Loader Path: '%s'", LoaderPath);
        }
        #endif

        Temp = FindLastDirName (LoaderPath);
        MergeStrings (&OSIconName, Temp, L',');
        MyFreePool (&Temp);
        Temp = NULL;
        if (OSIconName != NULL) {
            ShortcutLetter = OSIconName[0];
        }

        // Add every "word" in the filesystem and partition names, delimited by
        // spaces, dashes (-), underscores (_), or colons (:), to the list of
        // hints to be used in searching for OS icons.
        if (GetPoolStr (&Volume->FsName) && (GetPoolStr (&Volume->FsName)[0] != L'\0')) {
            BOOLEAN MergeFsName = TRUE;

            if (MyStrStr (GetPoolStr (&Volume->FsName), L"PreBoot") != NULL &&
                GlobalConfig.SyncAPFS
            ) {
                MergeFsName = FALSE;
            }

            if (MergeFsName) {
                if (GetPoolImage (&Entry->me.Image) == NULL) {
                    LOG(4, LOG_LINE_NORMAL, L"Merging hints based on filesystem name ('%s')", GetPoolStr (&Volume->FsName));
                }

                MergeWords(&OSIconName, GetPoolStr (&Volume->FsName), L',');
            }
            else {
                if (GetPoolStr (&Volume->VolName) && (GetPoolStr (&Volume->VolName)[0] != L'\0')) {
                    #if REFIT_DEBUG > 0
                    if (GetPoolImage (&Entry->me.Image) == NULL) {
                        LOG(4, LOG_LINE_NORMAL, L"Merging hints based on volume name ('%s')", GetPoolStr (&Volume->VolName));
                    }
                    #endif

                    MergeWords(&OSIconName, GetPoolStr (&Volume->VolName), L',');
                }
            }
        }
        if (GetPoolStr (&Volume->PartName) && (GetPoolStr (&Volume->PartName)[0] != L'\0')) {
            #if REFIT_DEBUG > 0
            if (GetPoolImage (&Entry->me.Image) == NULL) {
                LOG(4, LOG_LINE_NORMAL, L"Merging hints based on partition name ('%s')", GetPoolStr (&Volume->PartName));
            }
            #endif

            MergeWords(&OSIconName, GetPoolStr (&Volume->PartName), L',');
        }
    } // if/else network boot

    #if REFIT_DEBUG > 0
    if (GetPoolImage (&Entry->me.Image) == NULL) {
        LOG(4, LOG_LINE_NORMAL, L"Adding hints based on specific loaders");
    }
    #endif

    // detect specific loaders
    if (StriSubCmp (L"bzImage", NameClues) ||
        StriSubCmp (L"vmlinuz", NameClues) ||
        StriSubCmp (L"kernel", NameClues)
    ) {
        if (Volume->DiskKind != DISK_KIND_NET) {
            GuessLinuxDistribution (&OSIconName, Volume, LoaderPath);
            AssignPoolStr (&Entry->LoadOptions, GetMainLinuxOptions (LoaderPath, Volume));
        }
        MergeStrings (&OSIconName, L"linux", L',');
        Entry->OSType = 'L';
        if (ShortcutLetter == 0) {
            ShortcutLetter = 'L';
        }
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_LINUX;
    }
    else if (StriSubCmp (L"refit", LoaderPath)) {
        MergeStrings (&OSIconName, L"refit", L',');
        Entry->OSType = 'R';
        ShortcutLetter = 'R';
    }
    else if (StriSubCmp (L"refind", LoaderPath)) {
        MergeStrings (&OSIconName, L"refind", L',');
        Entry->OSType = 'R';
        ShortcutLetter = 'R';
    }
    else if (StriSubCmp (MACOSX_LOADER_PATH, LoaderPath)) {
        if (FileExists (Volume->RootDir, L"EFI\\refind\\config.conf") ||
            FileExists (Volume->RootDir, L"EFI\\refind\\refind.conf")
        ) {
            MergeStrings (&OSIconName, L"refind", L',');
            Entry->OSType = 'R';
            ShortcutLetter = 'R';
        }
        else {
            MergeStrings (&OSIconName, L"mac", L',');
            Entry->OSType = 'M';
            ShortcutLetter = 'M';
            Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OSX;
        }
    }
    else if (MyStriCmp (NameClues, L"diags.efi")) {
        MergeStrings (&OSIconName, L"hwtest", L',');
    }
    else if (MyStriCmp (NameClues, L"e.efi") ||
        MyStriCmp (NameClues, L"elilo.efi")  ||
        StriSubCmp (L"elilo", NameClues)
    ) {
        MergeStrings (&OSIconName, L"elilo,linux", L',');
        Entry->OSType = 'E';
        if (ShortcutLetter == 0) {
            ShortcutLetter = 'L';
        }
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_ELILO;
    }
    else if (StriSubCmp (L"grub", NameClues)) {
        MergeStrings (&OSIconName, L"grub,linux", L',');
        Entry->OSType = 'G';
        ShortcutLetter = 'G';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_GRUB;
    }
    else if (MyStriCmp (NameClues, L"cdboot.efi") ||
               MyStriCmp (NameClues, L"bootmgr.efi") ||
               MyStriCmp (NameClues, L"bootmgfw.efi") ||
        MyStriCmp (NameClues, L"bkpbootmgfw.efi")
    ) {
        MergeStrings (&OSIconName, L"win8", L',');
        Entry->OSType = 'W';
        ShortcutLetter = 'W';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
    }
    else if (MyStriCmp (NameClues, L"xom.efi")) {
        MergeStrings (&OSIconName, L"xom,win,win8", L',');
        Entry->OSType = 'X';
        ShortcutLetter = 'W';
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_WINDOWS;
    }
    else if (MyStriCmp (NameClues, L"opencore")) {
        MergeStrings (&OSIconName, L"opencore", L',');
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_OPENCORE;
    }
    else if (MyStriCmp (NameClues, L"clover")) {
        MergeStrings (&OSIconName, L"clover", L',');
        Entry->UseGraphicsMode = GlobalConfig.GraphicsFor & GRAPHICS_FOR_CLOVER;
    }
    else if (StriSubCmp (L"ipxe", NameClues)) {
        Entry->OSType = 'N';
        ShortcutLetter = 'N';
        MergeStrings (&OSIconName, L"network", L',');
    }

    if ((ShortcutLetter >= 'a') && (ShortcutLetter <= 'z')) {
        ShortcutLetter = ShortcutLetter - 'a' + 'A'; // convert lowercase to uppercase
    }

    Entry->me.ShortcutLetter = ShortcutLetter;
    if (!GetPoolImage (&Entry->me.Image)) {
        LOG(4, LOG_LINE_NORMAL, L"Trying to locate an icon based on hints '%s'", OSIconName);

        AssignPoolImage (&Entry->me.Image, LoadOSIcon (OSIconName, L"unknown", FALSE));
    }

    MyFreePool (&PathOnly);
    MyFreePool (&OSIconName);
    MyFreePool (&NoExtension);
    MyFreePool (&NameClues);
} // VOID SetLoaderDefaults()

// Add an NVRAM-based EFI boot loader entry to the menu.
static
LOADER_ENTRY * AddEfiLoaderEntry (
    IN EFI_DEVICE_PATH *EfiLoaderPath,
    IN CHAR16 *LoaderTitle,
    IN UINT16 EfiBootNum,
    IN UINTN Row,
    PoolImage *Icon_PI_
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
        if (FullTitle) {
            AssignPoolStr (&Entry->me.Title, FullTitle);
        }
        else {
            AssignCachedPoolStr (&Entry->me.Title, L"Unknown");
        }
        Entry->me.Row = Row;
        Entry->me.Tag = TAG_FIRMWARE_LOADER;
        if (LoaderTitle) {
            // without "Reboot to"
            CopyPoolStr (&Entry->Title, LoaderTitle); 
        }
        else {
            AssignCachedPoolStr (&Entry->Title, L"Unknown");
        }
        
        Entry->EfiLoaderPath = DuplicateDevicePath (EfiLoaderPath);

        TempStr = DevicePathToStr (EfiLoaderPath);

        LOG(2, LOG_LINE_NORMAL, L"EFI Loader Path:- '%s'", TempStr);

        MyFreePool (&TempStr);

        Entry->EfiBootNum = EfiBootNum;

        MergeWords (&OSIconName, GetPoolStr (&Entry->me.Title), L',');
        MergeStrings (&OSIconName, L"unknown", L',');

        if (GetPoolImage (Icon)) {
            CopyFromPoolImage (&Entry->me.Image, Icon);
        }
        else {
            AssignPoolImage (&Entry->me.Image, LoadOSIcon (OSIconName, NULL, FALSE));
        }

        if (Row == 0) {
            CopyFromPoolImage_PI_ (&Entry->me.BadgeImage_PI_, BuiltinIcon (BUILTIN_ICON_VOL_EFI));
        }

        MyFreePool (&OSIconName);

        Entry->Enabled     = TRUE;

        AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *) Entry);
    } // if (Entry)

    return Entry;
} // LOADER_ENTRY * AddEfiLoaderEntry()


// Add a specified EFI boot loader to the list, using automatic settings
// for icons, options, etc.
static
LOADER_ENTRY * AddLoaderEntry (
    IN CHAR16       *LoaderPath,
    IN CHAR16       *LoaderTitle,
    IN REFIT_VOLUME *Volume,
    IN BOOLEAN      SubScreenReturn
) {
    MsgLog ("[ AddLoaderEntry\n");
    LOADER_ENTRY  *Entry;
    CHAR16        *TitleEntry = NULL;

    CleanUpPathNameSlashes (LoaderPath);
    Entry = InitializeLoaderEntry (NULL);
    if (Entry != NULL) {
        Entry->DiscoveryType = DISCOVERY_TYPE_AUTO;
        if (LoaderTitle == NULL) {
            TitleEntry = LoaderPath;
        }
        else {
            TitleEntry = LoaderTitle;
        }

        CopyPoolStr (&Entry->Title, LoaderTitle ? TitleEntry : LoaderPath);

        LOG(1, LOG_LINE_NORMAL, L"Adding loader entry for '%s'", GetPoolStr (&Entry->Title));
        LOG(2, LOG_LINE_NORMAL, L"Loader path is '%s'", LoaderPath);

        // Extra space at end of Entry->me.Title enables searching on Volume->VolName even if another volume
        // name is identical except for something added to the end (e.g., VolB1 vs. VolB12).
        // Note: Volume->VolName will be NULL for network boot programs.
        if ((GetPoolStr (&Volume->VolName)) && (!MyStriCmp (GetPoolStr (&Volume->VolName), L"Recovery HD"))) {
            AssignPoolStr (&Entry->me.Title, PoolPrint (
                L"Boot %s from %s",
                LoaderTitle ? TitleEntry : LoaderPath,
                GetPoolStr (&Volume->VolName)
            ));
        }
        else {
            AssignPoolStr (&Entry->me.Title, PoolPrint (
                L"Boot %s ",
                LoaderTitle ? TitleEntry : LoaderPath
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
        DebugOneVolume(Volume, TitleEntry, GetPoolStr (&Entry->LoaderPath));
        LOG(3, LOG_THREE_STAR_MID, L"Successfully Created Menu Entry for '%s'", GetPoolStr (&Entry->Title));
    }
    else {
        LOG(1, LOG_THREE_STAR_MID, L"Could not initialise loader entry");
    }

    MsgLog ("] AddLoaderEntry %p\n", Entry);
    return (Entry);
} // LOADER_ENTRY * AddLoaderEntry()

// Returns -1 if (Time1 < Time2), +1 if (Time1 > Time2), or 0 if
// (Time1 == Time2). Precision is only to the nearest second; since
// this is used for sorting boot loader entries, differences smaller
// than this are likely to be meaningless (and unlikely!).
static
INTN TimeComp (IN EFI_TIME *Time1, IN EFI_TIME *Time2) {
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
    } // if/else

    return (LatestEntry);
} // static VOID AddLoaderListEntry()

// Delete the LOADER_LIST linked list
static
VOID CleanUpLoaderList (struct LOADER_LIST *LoaderList) {
    struct LOADER_LIST *Temp;

    while (LoaderList != NULL) {
        Temp = LoaderList;
        LoaderList = LoaderList->NextEntry;
        MyFreePool (&Temp->FileName);
        MyFreePool (&Temp);
    } // while
} // static VOID CleanUpLoaderList()

// Returns FALSE if the specified file/volume matches the GlobalConfig.DontScanDirs
// or GlobalConfig.DontScanVolumes specification, or if Path points to a volume
// other than the one specified by Volume, or if the specified path is SelfDir.
// Returns TRUE if none of these conditions is met -- that is, if the path is
// eligible for scanning.
static
BOOLEAN ShouldScan (REFIT_VOLUME *Volume, CHAR16 *Path) {
    UINTN    i            = 0;
    CHAR16   *VolName     = NULL;
    CHAR16   *VolGuid     = NULL;
    CHAR16   *PathCopy    = NULL;
    CHAR16   *DontScanDir = NULL;
    BOOLEAN  ScanIt       = TRUE;
    BOOLEAN  FilerScan    = TRUE;

    // Skip initial scan filter for 'PreBoot' volumes when 'SyncAPFS' is active
    // DA-TAG: Review Requirement
    if ((MyStriCmp (L"PreBoot", GetPoolStr (&Volume->VolName)) || MyStriCmp (L"PreBoot", GetPoolStr (&Volume->PartName))) &&
        (GlobalConfig.SyncAPFS)
    ) {
        FilerScan = FALSE;
    }

    if (FilerScan) {
        VolGuid = GuidAsString (&(Volume->PartGuid));
        if ((IsIn (GetPoolStr (&Volume->FsName), GlobalConfig.DontScanVolumes)) ||
            (IsIn (GetPoolStr (&Volume->PartName), GlobalConfig.DontScanVolumes)) ||
            (IsIn (VolGuid, GlobalConfig.DontScanVolumes))
        ) {
            MyFreePool (&VolGuid);

            return FALSE;
        }
        MyFreePool (&VolGuid);
    }

    if (MyStriCmp (Path, SelfDirPath) && (Volume->DeviceHandle == SelfVolume->DeviceHandle)) {
        return FALSE;
    }

    // See if Path includes an explicit volume declaration that's NOT Volume....
    PathCopy = StrDuplicate (Path);
    if (SplitVolumeAndFilename (&PathCopy, &VolName)) {
        if (VolName && (!MyStriCmp (VolName, GetPoolStr (&Volume->FsName)) ||
            !MyStriCmp(VolName, GetPoolStr (&Volume->PartName)))
        ) {
                ScanIt = FALSE;
        } // if
    } // if Path includes volume specification
    MyFreePool (&PathCopy);
    MyFreePool (&VolName);
    VolName = NULL;

    // See if Volume is in GlobalConfig.DontScanDirs....
    while (ScanIt && (DontScanDir = FindCommaDelimited (GlobalConfig.DontScanDirs, i++))) {
        SplitVolumeAndFilename (&DontScanDir, &VolName);
        CleanUpPathNameSlashes (DontScanDir);
        if (VolName != NULL) {
            if (VolumeMatchesDescription (Volume, VolName) && MyStriCmp (DontScanDir, Path)) {
                ScanIt = FALSE;
            }
        }
        else {
            if (MyStriCmp (DontScanDir, Path)) {
                ScanIt = FALSE;
            }
        }
        MyFreePool (&DontScanDir);
        MyFreePool (&VolName);
        DontScanDir = NULL;
        VolName = NULL;
    } // while()

    return ScanIt;
} // BOOLEAN ShouldScan()

// Returns TRUE if the file is byte-for-byte identical with the fallback file
// on the volume AND if the file is not itself the fallback file; returns
// FALSE if the file is not identical to the fallback file OR if the file
// IS the fallback file. Intended for use in excluding the fallback boot
// loader when it's a duplicate of another boot loader.
static
BOOLEAN DuplicatesFallback (IN REFIT_VOLUME *Volume, IN CHAR16 *FileName) {
    CHAR8           *FileContents, *FallbackContents;
    EFI_FILE_HANDLE FileHandle, FallbackHandle;
    EFI_FILE_INFO   *FileInfo, *FallbackInfo;
    UINTN           FileSize = 0, FallbackSize = 0;
    EFI_STATUS      Status;
    BOOLEAN         AreIdentical = FALSE;

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

    Status = refit_call5_wrapper(
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
    MyFreePool (&FileInfo);

    Status = refit_call5_wrapper(
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
        refit_call1_wrapper(FileHandle->Close, FileHandle);
        return FALSE;
    }
    MyFreePool (&FallbackInfo);

    if (FallbackSize == FileSize) { // could be identical; do full check....
        FileContents = AllocatePool (FileSize);
        FallbackContents = AllocatePool (FallbackSize);
        if (FileContents && FallbackContents) {
            Status = refit_call3_wrapper(
                FileHandle->Read,
                FileHandle,
                &FileSize,
                FileContents
            );
            if (Status == EFI_SUCCESS) {
                Status = refit_call3_wrapper
                (FallbackHandle->Read,
                    FallbackHandle,
                    &FallbackSize,
                    FallbackContents
                );
            }
            if (Status == EFI_SUCCESS) {
                AreIdentical = (CompareMem (FileContents, FallbackContents, FileSize) == 0);
            } // if
        } // if
        MyFreePool (&FileContents);
        MyFreePool (&FallbackContents);
    } // if/else

    // BUG ALERT: Some systems (e.g., DUET, some Macs with large displays) crash if the
    // following two calls are reversed. Go figure....
    refit_call1_wrapper(FileHandle->Close, FallbackHandle);
    refit_call1_wrapper(FileHandle->Close, FileHandle);
    return AreIdentical;
} // BOOLEAN DuplicatesFallback()

// Returns FALSE if two measures of file size are identical for a single file,
// TRUE if not or if the file can't be opened and the other measure is non-0.
// Despite the function's name, this isn't really a direct test of symbolic
// link status, since EFI doesn't officially support symlinks. It does seem
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
    EFI_FILE_HANDLE FileHandle;
    EFI_FILE_INFO   *FileInfo = NULL;
    EFI_STATUS      Status;
    UINTN           FileSize2 = 0;

    LEAKABLEEXTERNALSTART ("IsSymbolicLink Open");
    Status = refit_call5_wrapper(
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

    MyFreePool (&FileInfo);

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
    MyFreePool (&PathStr);
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
                continue;
            }

            NewLoader = AllocateZeroPool (sizeof (struct LOADER_LIST));
            if (NewLoader != NULL) {
                NewLoader->FileName  = StrDuplicate (FullName);
                NewLoader->TimeStamp = DirEntry->ModificationTime;
                LoaderList           = AddLoaderListEntry (LoaderList, NewLoader);

                if (DuplicatesFallback (Volume, FullName)) {
                    FoundFallbackDuplicate = TRUE;
                }
            }

            MyFreePool (&Extension);
            MyFreePool (&FullName);
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
                LOG(4, LOG_LINE_NORMAL, L"Adding 'Return' entry to folded Linux kernels");
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
                    Path,
                    GetPoolStr (&Volume->VolName)
                );
            }
            else {
                Message = PoolPrint (
                    L"While Scanning the Root Directory on '%s'",
                    GetPoolStr (&Volume->VolName)
                );
            }

            CheckError (Status, Message);
            MyFreePool (&Message);
        } // if Status != EFI_NOT_FOUND
    } // if not scanning a blacklisted directory

    return FoundFallbackDuplicate;
} /* static VOID ScanLoaderDir() */

// Run the IPXE_DISCOVER_NAME program, which obtains the IP address of the boot
// server and the name of the boot file it delivers.
static
CHAR16 * RuniPXEDiscover (EFI_HANDLE Volume) {
    EFI_STATUS       Status;
    EFI_DEVICE_PATH  *FilePath;
    EFI_HANDLE       iPXEHandle;
    CHAR16           *boot_info = NULL;
    UINTN            boot_info_size = 0;

    FilePath = FileDevicePath (Volume, IPXE_DISCOVER_NAME);
    Status = refit_call6_wrapper(
        gBS->LoadImage,
        FALSE,
        SelfImageHandle,
        FilePath,
        NULL,
        0,
        &iPXEHandle
    );
    if (Status != 0) {
        return NULL;
    }

    refit_call3_wrapper(
        gBS->StartImage,
        iPXEHandle,
        &boot_info_size,
        &boot_info
    );

    return boot_info;
} // RuniPXEDiscover()

// Scan for network (PXE) boot servers. This function relies on the presence
// of the IPXE_DISCOVER_NAME and IPXE_NAME program files on the volume from
// which RefindPlus launched. As of December 6, 2014, these tools aren't entirely
// reliable. See BUILDING.txt for information on building them.
static
VOID ScanNetboot() {
    CHAR16        *iPXEFileName = IPXE_NAME;
    CHAR16        *Location;
    REFIT_VOLUME  *NetVolume;

    LOG(1, LOG_LINE_NORMAL, L"Scanning for iPXE boot options");

    if (FileExists (SelfVolume->RootDir, IPXE_DISCOVER_NAME) &&
        FileExists (SelfVolume->RootDir, IPXE_NAME) &&
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
                    FreeVolume (&NetVolume); // reference counted
                }
            } // if support files exist and are valid
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
    BOOLEAN  AddThisEntry       = TRUE;
    BOOLEAN  ScanFallbackLoader = TRUE;
    CHAR16   *VolName           = NULL;
    CHAR16   *PathName          = NULL;
    CHAR16   *FileName          = NULL;

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
            if (MyStrStr (GetPoolStr (&Volume->VolName), L"Cloaked_SkipThis_") != NULL &&
                GlobalConfig.SyncAPFS
            ) {
                AddThisEntry = FALSE;
            }

            if (AddThisEntry) {
                AddLoaderEntry (FullFileName, L"Mac OS", Volume, TRUE);
            }
        }

        if (DuplicatesFallback (Volume, FullFileName)) {
            ScanFallbackLoader = FALSE;
        }
    } // if
    MyFreePool (&VolName);
    MyFreePool (&PathName);
    MyFreePool (&FileName);

    return ScanFallbackLoader;
} // VOID ScanMacOsLoader()

static
VOID ScanEfiFiles (REFIT_VOLUME *Volume) {
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

    if (!Volume) {
        return;
    }

    if (GlobalConfig.SyncAPFS &&
        MyStrStr (GetPoolStr (&Volume->VolName), L"Cloaked_SkipThis_") != NULL
    ) {
        // Early Return on Cloaked Volume
        return;
    }

    if (!Volume->IsReadable ||
        Volume->RootDir == NULL ||
        GetPoolStr (&Volume->VolName) == NULL
        ) {
        // Early Return on Invalid Volume
        return;
    }

    LOG(1, FirstLoaderScan ? LOG_THREE_STAR_MID : LOG_THREE_STAR_SEP,
        L"Scanning Volume '%s' for EFI Loaders",
        GetPoolStr (&Volume->VolName)
        );

    FirstLoaderScan = FALSE;

        MatchPatterns = StrDuplicate (LOADER_MATCH_PATTERNS);
        if (GlobalConfig.ScanAllLinux) {
            MergeStrings (&MatchPatterns, LINUX_MATCH_PATTERNS, L',');
        }

        // check for Mac OS boot loader
        if (ShouldScan (Volume, MACOSX_LOADER_DIR)) {
            FileName = StrDuplicate (MACOSX_LOADER_PATH);
            ScanFallbackLoader &= ScanMacOsLoader (Volume, FileName);
        MyFreePool (&FileName);
            DirIterOpen (Volume->RootDir, L"\\", &EfiDirIter);

            while (DirIterNext (&EfiDirIter, 1, NULL, &EfiDirEntry)) {
                if (IsGuid (EfiDirEntry->FileName)) {
                    FileName = PoolPrint (L"%s\\%s", EfiDirEntry->FileName, MACOSX_LOADER_PATH);
                    ScanFallbackLoader &= ScanMacOsLoader (Volume, FileName);
                MyFreePool (&FileName);
                    FileName = PoolPrint (L"%s\\%s", EfiDirEntry->FileName, L"boot.efi");

                    if (!StriSubCmp (FileName, GlobalConfig.MacOSRecoveryFiles)) {
                        MergeStrings (&GlobalConfig.MacOSRecoveryFiles, FileName, L',');
                    }

                MyFreePool (&FileName);
                } // if
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
        MyFreePool (&FileName);
        } // if should scan Mac directory

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
                AddLoaderEntry (FileName, L"Windows (UEFI - Boot Repair Backup)", Volume, TRUE);
                FoundBRBackup = TRUE;
                if (DuplicatesFallback (Volume, FileName)) {
                    ScanFallbackLoader = FALSE;
                }
            }
        MyFreePool (&FileName);

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
                        L"Assumed Windows (UEFI - Probably GRUB)",
                        Volume,
                        TRUE
                    );
                }
                else {
                    AddLoaderEntry (
                        FileName,
                        L"Windows (UEFI)",
                        Volume,
                        TRUE
                    );
                }

                if (DuplicatesFallback (Volume, FileName)) {
                    ScanFallbackLoader = FALSE;
                }
        } // if FileExists Volume->RootDir

        MyFreePool (&FileName);
    } // if ShouldScan Windows directory

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
                // skip this, doesn't contain boot loaders or is scanned later
                continue;
            }

            FileName = PoolPrint (L"EFI\\%s", EfiDirEntry->FileName);
            if (ScanLoaderDir (Volume, FileName, MatchPatterns)) {
                ScanFallbackLoader = FALSE;
            }

        MyFreePool (&FileName);
        } // while()

        Status = DirIterClose (&EfiDirIter);
        if ((Status != EFI_NOT_FOUND) && (Status != EFI_INVALID_PARAMETER)) {
            Temp = PoolPrint (L"While Scanning the EFI System Partition on '%s'", GetPoolStr (&Volume->VolName));
            CheckError (Status, Temp);
        MyFreePool (&Temp);
        } // if

        // Scan user-specified (or additional default) directories....
        i = 0;
        while ((Directory = FindCommaDelimited (GlobalConfig.AlsoScan, i++)) != NULL) {
            if (ShouldScan (Volume, Directory)) {
                SplitVolumeAndFilename (&Directory, &VolName);
                CleanUpPathNameSlashes (Directory);

                Length = StrLen (Directory);
                if ((Length > 0) && ScanLoaderDir (Volume, Directory, MatchPatterns)) {
                    ScanFallbackLoader = FALSE;
                }

            MyFreePool (&VolName);
            } // if should scan dir

        MyFreePool (&Directory);
        } // while

        // Don't scan the fallback loader if it's on the same volume and a duplicate of RefindPlus itself....
        SelfPath = DevicePathToStr (SelfLoadedImage->FilePath);
        CleanUpPathNameSlashes (SelfPath);

        if ((Volume->DeviceHandle == SelfLoadedImage->DeviceHandle) &&
            DuplicatesFallback (Volume, SelfPath)
        ) {
            ScanFallbackLoader = FALSE;
        }
    MyFreePool (&SelfPath);

        // If not a duplicate & if it exists & if it's not us, create an entry
        // for the fallback boot loader
        if (ScanFallbackLoader &&
            FileExists (Volume->RootDir, FALLBACK_FULLNAME) &&
            ShouldScan (Volume, L"EFI\\BOOT") &&
            !FilenameIn (Volume, L"EFI\\BOOT", FALLBACK_BASENAME, GlobalConfig.DontScanFiles)
        ) {
            AddLoaderEntry (FALLBACK_FULLNAME, L"Fallback Boot Loader", Volume, TRUE);
        }

    MyFreePool (&MatchPatterns);
} // static VOID ScanEfiFiles()

// Scan user-configured menu entries from config.conf
static VOID
ScanUserConfigured0 (
    CHAR16 *FileName
) {
    LOG(1, LOG_LINE_THIN_SEP, L"Scanning for User Configured Boot Stanzas");
    ScanUserConfigured (FileName);
}

// Scan internal disks for valid EFI boot loaders....
static
VOID ScanInternal (
    VOID
) {
    UINTN VolumeIndex;

    LOG(1, LOG_LINE_THIN_SEP, L"Scanning Internal Disk Volumes with Mode:- 'EFI'");

    FirstLoaderScan = TRUE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_INTERNAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for
    FirstLoaderScan = FALSE;

} // static VOID ScanInternal()

// Scan external disks for valid EFI boot loaders....
static
VOID ScanExternal (
    VOID
) {
    UINTN VolumeIndex;

    LOG(1, LOG_LINE_THIN_SEP, L"Scanning External Disk Volumes with Mode:- 'EFI'");

    FirstLoaderScan = TRUE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_EXTERNAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for
    FirstLoaderScan = FALSE;

} // static VOID ScanExternal()

// Scan internal disks for valid EFI boot loaders....
static
VOID ScanOptical (
    VOID
) {
    UINTN VolumeIndex;

    LOG(1, LOG_LINE_THIN_SEP, L"Scanning Optical Discs with Mode:- 'EFI'");

    FirstLoaderScan = TRUE;
    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
        if (Volumes[VolumeIndex]->DiskKind == DISK_KIND_OPTICAL) {
            ScanEfiFiles (Volumes[VolumeIndex]);
        }
    } // for
    FirstLoaderScan = FALSE;

} // static VOID ScanOptical()

// Scan options stored in EFI firmware's boot list. Adds discovered and allowed
// items to the specified Row.
// If MatchThis != NULL, only adds items with labels containing any element of
// the MatchThis comma-delimited string; otherwise, searches for anything that
// doesn't match GlobalConfig.DontScanFirmware or the contents of the
// HiddenFirmware EFI variable.
// If Icon != NULL, uses the specified icon; otherwise tries to find one to
// match the label.
STATIC
BOOLEAN
ScanFirmwareDefined (
    IN UINTN Row,
    IN CHAR16 *MatchThis,
    IN PoolImage *Icon_PI_
) {
    BOOT_ENTRY_LIST *BootEntries;
    BOOT_ENTRY_LIST *CurrentEntry;
    BOOLEAN         ScanIt;
    CHAR16          *OneElement = NULL;
    CHAR16          *DontScanFirmware;
    UINTN           i;
    BOOLEAN         result = FALSE;

    #if REFIT_DEBUG > 0
    if (Row == 0) {
        LOG(1, LOG_LINE_THIN_SEP,
            L"Scanning for EFI Firmware-Defined Boot Options"
        );
    }
    else {
        LOG(1, LOG_LINE_THIN_SEP,
            L"Scanning for EFI firmware-defined boot options '%s'", MatchThis ? MatchThis : L"NULL"
        );
    }
    #endif

    DontScanFirmware = ReadHiddenTags (L"HiddenFirmware");

    LOG(2, LOG_LINE_NORMAL,
        L"GlobalConfig.DontScanFirmware = '%s'",
        GlobalConfig.DontScanFirmware
    );
    LOG(2, LOG_LINE_NORMAL,
        L"Firmware hidden tags = '%s'",
        DontScanFirmware
    );

    MergeStrings (&DontScanFirmware, GlobalConfig.DontScanFirmware, L',');

    if (Row == 0) {
        LOG(2, LOG_LINE_NORMAL, L"Also not scanning for shells");
        MergeStrings(&DontScanFirmware, L"shell", L',');
    }

    LOG(3, LOG_LINE_NORMAL,
        L"Merged firmware dont-scan list is '%s'",
        DontScanFirmware
    );

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
                MyFreePool (&OneElement);
            } // while()
        }
        else {
            ScanIt = TRUE;
        }

        if (ScanIt) {
            LOG(1, LOG_LINE_NORMAL,
                L"Adding EFI Loader Entry for '%s'",
                CurrentEntry->BootEntry.Label
            );

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
    } // while()

    MyFreePool (&DontScanFirmware);
    DeleteBootOrderEntries (BootEntries);
    return result;
} // ScanFirmwareDefined

// default volume badge icon based on disk kind
PoolImage * GetDiskBadge (IN UINTN DiskType) {
    PoolImage * Badge = NULL;

    switch (DiskType) {
        case BBS_HARDDISK:
            Badge = BuiltinIcon (BUILTIN_ICON_VOL_INTERNAL);
            break;
        case BBS_USB:
            Badge = BuiltinIcon (BUILTIN_ICON_VOL_EXTERNAL);
            break;
        case BBS_CDROM:
            Badge = BuiltinIcon (BUILTIN_ICON_VOL_OPTICAL);
            break;
    } // switch()

    return Badge;
} // GetDiskBadge

//
// pre-boot tool functions
//

static
LOADER_ENTRY * AddToolEntry (
    IN REFIT_VOLUME *Volume,
    IN CHAR16 *LoaderPath,
    IN CHAR16 *LoaderTitle,
    IN PoolImage *Image_PI_,
    IN CHAR16 ShortcutLetter,
    IN BOOLEAN UseGraphicsMode
) {
    LOADER_ENTRY *Entry;

    Entry = AllocateZeroPool (sizeof (LOADER_ENTRY));
    AssignPoolStr (&Entry->me.Title, PoolPrint (L"Load %s", LoaderTitle)); // if you change this, then change ./launch_efi.c:572
    Entry->me.Tag = TAG_TOOL;
    Entry->me.Row = 1;
    Entry->me.ShortcutLetter = ShortcutLetter;
    CopyFromPoolImage (&Entry->me.Image, Image);
    CopyPoolStr (&Entry->LoaderPath, LoaderPath);
    AssignVolume (&Entry->Volume, Volume);
    Entry->UseGraphicsMode = UseGraphicsMode;

    AddMenuEntry (MainMenu, (REFIT_MENU_ENTRY *)Entry);

    return Entry;
} // AddToolEntry

// Locates boot loaders.
// NOTE: This assumes that GlobalConfig.LegacyType is correctly set.
VOID ScanForBootloaders (
    BOOLEAN ShowMessage
) {
    #if REFIT_DEBUG > 0
    UINTN    k;
    #endif

    UINTN    i;
    CHAR8    ScanOption;
    BOOLEAN  ScanForLegacy = FALSE;
    EG_PIXEL BGColor       = COLOR_LIGHTBLUE;
    CHAR16   *HiddenTags, *HiddenLegacy;
    CHAR16   *OrigDontScanFiles;
    CHAR16   *OrigDontScanVolumes;
    CHAR16   ShortCutKey;

    ScanningLoaders = TRUE;

    LOG2(1, LOG_LINE_SEPARATOR, L"", L"...\n", L"Seek Boot Loaders");

    if (ShowMessage){
        egDisplayMessage (L"Seeking Boot Loaders. Please Wait...", &BGColor, CENTER);
    }

    // Determine up-front if we will be scanning for legacy loaders
    for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
        ScanOption = GlobalConfig.ScanFor[i];
        if (ScanOption == 'c' || ScanOption == 'C' ||
            ScanOption == 'h' || ScanOption == 'H' ||
            ScanOption == 'b' || ScanOption == 'B'
        ) {
            ScanForLegacy = TRUE;
        }
    } // for

    // If UEFI & scanning for legacy loaders & deep legacy scan, update NVRAM boot manager list
    if ((GlobalConfig.LegacyType == LEGACY_TYPE_UEFI) &&
        ScanForLegacy &&
        GlobalConfig.DeepLegacyScan
    ) {
        BdsDeleteAllInvalidLegacyBootOptions();
        BdsAddNonExistingLegacyBootOptions();
    } // if

    if (GlobalConfig.HiddenTags) {
    // We temporarily modify GlobalConfig.DontScanFiles and GlobalConfig.DontScanVolumes
    // to include contents of EFI HiddenTags and HiddenLegacy variables so that we don't
    // have to re-load these EFI variables in several functions called from this one.
    // To do this, we must be able to restore the original contents, so back them up
    // first.
    // We do *NOT* do this with GlobalConfig.DontScanFirmware and
    // GlobalConfig.DontScanTools variables because they're used in only one function
    // each, so it's easier to create a temporary variable for the merged contents
    // there and not modify the global variable.
    OrigDontScanFiles   = StrDuplicate (GlobalConfig.DontScanFiles);
    OrigDontScanVolumes = StrDuplicate (GlobalConfig.DontScanVolumes);

    // Add hidden tags to two GlobalConfig.DontScan* variables....
    HiddenTags = ReadHiddenTags (L"HiddenTags");
    if ((HiddenTags) && (StrLen (HiddenTags) > 0)) {
            LOG(2, LOG_LINE_NORMAL,
                L"Merging to 'Dont Scan Files':- '%s'",
                HiddenTags
            );

        MergeStrings (&GlobalConfig.DontScanFiles, HiddenTags, L',');
    }
        MyFreePool (&HiddenTags);

    HiddenLegacy = ReadHiddenTags (L"HiddenLegacy");
    if ((HiddenLegacy) && (StrLen (HiddenLegacy) > 0)) {
            LOG(2, LOG_LINE_NORMAL,
                L"Merging to 'Dont Scan Volumes':- '%s'",
                HiddenLegacy
            );

        MergeStrings (&GlobalConfig.DontScanVolumes, HiddenLegacy, L',');
    }
        MyFreePool (&HiddenLegacy);
    }

    // scan for loaders and tools, add them to the menu
    for (i = 0; i < NUM_SCAN_OPTIONS; i++) {
        switch (GlobalConfig.ScanFor[i]) {

            #define DOONEMENUKEY(letter, msg, what) \
            case letter : case letter - 32: \
                MsgLog ("[ " msg "\n"); \
                what ; \
                MsgLog ("] " msg "\n"); \
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
        } // switch()
    } // for

    if (GlobalConfig.HiddenTags) {
    // Restore the backed-up GlobalConfig.DontScan* variables
        MyFreePool (&GlobalConfig.DontScanFiles);
        MyFreePool (&GlobalConfig.DontScanVolumes);
        GlobalConfig.DontScanFiles = OrigDontScanFiles;
        GlobalConfig.DontScanVolumes = OrigDontScanVolumes;
        LEAKABLE (GlobalConfig.DontScanFiles, "DontScanFiles");
        LEAKABLE (GlobalConfig.DontScanVolumes, "DontScanVolumes");
    }

    if (MainMenu->EntryCount < 1) {
        LOG2(1, LOG_THREE_STAR_MID, L"* WARN: ", L"\n\n", L"Could Not Find Boot Loaders");
    }
    else {
        // assign shortcut keys
        LOG2(1, LOG_LINE_SEPARATOR, L"\n\n", L":\n", L"Assigning Boot Shortcut Keys");

        for (i = 0; i < MainMenu->EntryCount && MainMenu->Entries[i]->Row == 0; i++) {
            if (i < 9) {
                #if REFIT_DEBUG > 0
                k = i + 1;
                #endif

                ShortCutKey = (CHAR16) ('1' + i);
            }
            else if (i == 9) {
                ShortCutKey = (CHAR16) ('9' - i);

                #if REFIT_DEBUG > 0
                k = 0;
                #endif
            }
            else {
                break;
            }
            MainMenu->Entries[i]->ShortcutDigit = ShortCutKey;

            #if REFIT_DEBUG > 0
            LOG2(3, LOG_LINE_NORMAL, L"  - ", (k < MainMenu->EntryCount && MainMenu->Entries[i]->Row == 0 && k != 0) ? L"\n" : L"\n\n",
                L"Set Key '%d' to %s",
                k, GetPoolStr (&MainMenu->Entries[i]->Title)
            );
            #endif
        }  // for

        LOG2(4, LOG_THREE_STAR_SEP, L"INFO: ", L"\n\n",
            L"Assigned Shortcut Key%s to %d of %d Loader%s",
            i == 1 ? L"" : L"s", i,
            MainMenu->EntryCount,
            MainMenu->EntryCount == 1 ? L"" : L"s"
        );
    }

    // wait for user ACK when there were errors
    FinishTextScreen (FALSE);
} // VOID ScanForBootloaders()

// Checks to see if a specified file seems to be a valid tool.
// Returns TRUE if it passes all tests, FALSE otherwise
static
BOOLEAN IsValidTool (IN REFIT_VOLUME *BaseVolume, CHAR16 *PathName) {
    CHAR16 *DontVolName = NULL, *DontPathName = NULL, *DontFileName = NULL, *DontScanThis;
    CHAR16 *TestVolName = NULL, *TestPathName = NULL, *TestFileName = NULL, *DontScanTools;
    BOOLEAN retval = TRUE;
    UINTN i = 0;

    LOG(4, LOG_LINE_NORMAL,
        L"Checking validity of tool '%s' on '%s' %p(#%d)",
        PathName,
        BaseVolume
            ? (
                GetPoolStr (&BaseVolume->PartName)
                    ? GetPoolStr (&BaseVolume->PartName)
                    : (
                        GetPoolStr (&BaseVolume->VolName)
                            ? GetPoolStr (&BaseVolume->VolName)
                            : L"<no name>"
                    )
            )
            : L"<no volume>",
        BaseVolume, BaseVolume ? BaseVolume->ReferenceCount : -1
    );

    if (gHiddenTools == NULL) {
        gHiddenTools = ReadHiddenTags (L"HiddenTools");
        if (gHiddenTools == NULL) {
            gHiddenTools = StrDuplicate (L"");
        }
        LEAKABLE (gHiddenTools, "gHiddenTools");
    }
    DontScanTools = StrDuplicate (gHiddenTools);

    MergeStrings(&DontScanTools, GlobalConfig.DontScanTools, L',');

    if (FileExists (BaseVolume->RootDir, PathName) && IsValidLoader (BaseVolume->RootDir, PathName)) {
        LOGPOOL(TestVolName);
        LOGPOOL(TestPathName);
        LOGPOOL(TestFileName);
        SplitPathName (PathName, &TestVolName, &TestPathName, &TestFileName);

        while (retval && (DontScanThis = FindCommaDelimited(DontScanTools, i++))) {
            LOGPOOL(DontVolName);
            LOGPOOL(DontPathName);
            LOGPOOL(DontFileName);
            SplitPathName (DontScanThis, &DontVolName, &DontPathName, &DontFileName);
            if (MyStriCmp (TestFileName, DontFileName) &&
                ((DontPathName == NULL) || (MyStriCmp (TestPathName, DontPathName))) &&
                ((DontVolName == NULL) || (VolumeMatchesDescription (BaseVolume, DontVolName)))
            ) {
                retval = FALSE;
            } // if
            MyFreePool (&DontScanThis);
            MyFreePool (&DontVolName);
            MyFreePool (&DontPathName);
            MyFreePool (&DontFileName);
            DontVolName = DontPathName = DontFileName = NULL;
        } // while
    }
    else {
        retval = FALSE;
    } // if

    MyFreePool (&TestVolName);
    MyFreePool (&TestPathName);
    MyFreePool (&TestFileName);
    MyFreePool (&DontVolName);
    MyFreePool (&DontPathName);
    MyFreePool (&DontFileName);
    MyFreePool (&DontScanTools);

    /*
    LOG(4, LOG_LINE_NORMAL,
        L"Done checking validity of tool '%s' on '%s'",
        PathName,
        BaseVolume->PartName ? BaseVolume->PartName : BaseVolume->VolName
    );
    */

    return retval;
} // BOOLEAN IsValidTool()

// Locate a single tool from the specified Locations using one of the
// specified Names and add it to the menu.
static
BOOLEAN FindTool (
    CHAR16 *Locations,
    CHAR16 *Names,
    CHAR16 *Description,
    UINTN  Icon
) {
    UINTN   j = 0;
    UINTN   k;
    UINTN   VolumeIndex;
    CHAR16  *DirName;
    CHAR16  *FileName;
    CHAR16  *PathName;
    BOOLEAN FoundTool = FALSE;

//DA-TAG: Commented Out
//    CHAR16 FullDescription;

    while ((DirName = FindCommaDelimited (Locations, j++)) != NULL) {
        k = 0;
        while ((FileName = FindCommaDelimited (Names, k++)) != NULL) {
            PathName = StrDuplicate (DirName);
            MergeStrings (&PathName, FileName, MyStriCmp (PathName, L"\\") ? 0 : L'\\');
            for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                if ((Volumes[VolumeIndex]->RootDir != NULL)
                    && (IsValidTool (Volumes[VolumeIndex], PathName))
                ) {
                    LOG(1, LOG_LINE_NORMAL,
                        L"Adding tag for '%s' on '%s'",
                        FileName,
                        GetPoolStr (&Volumes[VolumeIndex]->VolName)
                    );

                    FoundTool = TRUE;
//DA-TAG: Commented Out
//                    FullDescription = PoolPrint (
//                        L"%s at %s on %s",
//                        Description,
//                        PathName,
//                        GetPoolStr (&Volumes[VolumeIndex]->VolName)
//                    );

//DA-TAG: Changed "FullDescription" to "Description" below
                    AddToolEntry (
                        Volumes[VolumeIndex],
                        PathName,
                        Description,
                        BuiltinIcon (Icon),
                        0,
                        FALSE
                    );

                    MsgLog (
                        "Added Tool:- '%s' : %s%s",
                        Description,
                        StrDuplicate (DirName),
                        StrDuplicate (FileName)
                    );

                } // if
            } // for
            MyFreePool (&PathName);
            MyFreePool (&FileName);
        } // while Names

        MyFreePool (&DirName);
    } // while Locations

    return FoundTool;
} // VOID FindTool()

// Add the second-row tags containing built-in and external tools
VOID ScanForTools (VOID) {
    UINTN             i, j;
    UINTN             VolumeIndex;
    VOID             *ItemBuffer = NULL;
    CHAR16           *FileName   = NULL;
    CHAR16           *VolName    = NULL;
    CHAR16           *MokLocations;
    CHAR16           *Description;
    UINT64            osind;
    UINT32            CsrValue;

    MsgLog ("[ ScanForTools\n");
    //LOG2(1, LOG_LINE_SEPARATOR, L"", L"...\n", L"Scan for Builtin/External Tools");

    MokLocations = MergeStringsNew (MOK_LOCATIONS, SelfDirPath, L',');

    for (i = 0; i < NUM_TOOLS; i++) {
        // Reset Vars
        BOOLEAN FoundTool = FALSE;
        BOOLEAN Skipped = FALSE;

        UINTN TheTag = GlobalConfig.ShowTools[i];

        CHAR16 *TheTitle = GetPoolStr (&TagMenuEntry[TheTag].Title);

        CHAR16 *ToolName = NULL;
        UINTN TheIcon = 0;
        switch (TheTag) {
            #define TAGS_TAG_TO_TOOL_AND_ICON
            #include "tags.include"
        }
        if (!ToolName) {
            Skipped = TRUE;
            ToolName = L"Skipped";
        }
        
        if (!Skipped) {
            MsgLog ("[ Tool Type %02d ...%s\n", i + 1, ToolName);
        }

        #define TagAddMenuEntry() \
            do { \
                REFIT_MENU_ENTRY *TempMenuEntry; \
                FoundTool = TRUE; \
                TempMenuEntry = CopyMenuEntry (&TagMenuEntry[TheTag]); \
                CopyFromPoolImage_PI_ (&TempMenuEntry->Image_PI_, BuiltinIcon (TheIcon)); \
                AddMenuEntry (MainMenu, TempMenuEntry); \
                LOG2(2, LOG_THREE_STAR_END, L"              - ", L"\n", L"Added Tool:- '%s'", ToolName); \
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
                ) == EFI_SUCCESS) {
                    osind = *(UINT64*) ItemBuffer;
                    if (osind & EFI_OS_INDICATIONS_BOOT_TO_FW_UI) {
                        TagAddMenuEntry();
                    } // if
                    MyFreePool (&ItemBuffer);
                }
                else {
                    LOG(1, LOG_LINE_NORMAL,
                        L"Showtools includes firmware, but 'OsIndicationsSupported' is missing"
                    );
                }
                break;

            case TAG_SHELL:
                j = 0;
                while ((FileName = FindCommaDelimited (SHELL_NAMES, j++)) != NULL) {
                    if (IsValidTool (SelfVolume, FileName)) {
                        LOG(1, LOG_LINE_NORMAL,
                            L"Adding Shell tag for '%s' on '%s'",
                            FileName,
                            GetPoolStr (&SelfVolume->VolName)
                        );

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume,
                            FileName,
                            TheTitle,
                            BuiltinIcon (TheIcon),
                            'S',
                            FALSE
                        );

                        LOG2(1, LOG_THREE_STAR_END, L"              - ", L"\n", L"Added %s:- '%s'", ToolName, FileName);
                    } // if

                    MyFreePool (&FileName);
                    FileName = NULL;
                } // while

                if (ScanFirmwareDefined (1, L"Shell", BuiltinIcon(TheIcon))) {
                    FoundTool = TRUE;
                }
                break;

            case TAG_GPTSYNC:
                j = 0;
                while ((FileName = FindCommaDelimited (GPTSYNC_NAMES, j++)) != NULL) {
                    if (IsValidTool (SelfVolume, FileName)) {
                        LOG(1, LOG_LINE_NORMAL,
                            L"Adding Hybrid MBR tool tag for '%s' on '%s'",
                            FileName,
                            GetPoolStr (&SelfVolume->VolName)
                        );

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume,
                            FileName,
                            TheTitle,
                            BuiltinIcon (TheIcon),
                            'P',
                            FALSE
                        );

                        LOG2(1, LOG_THREE_STAR_END, L"              - ", L"\n", L"Added %s:- '%s'", ToolName, FileName);
                    } // if

                    MyFreePool (&FileName);
                    FileName = NULL;
                } // while
                break;

            case TAG_GDISK:
                j = 0;
                while ((FileName = FindCommaDelimited (GDISK_NAMES, j++)) != NULL) {
                    if (IsValidTool (SelfVolume, FileName)) {
                        LOG(1, LOG_LINE_NORMAL,
                            L"Adding GPT fdisk tag for '%s' on '%s'",
                            FileName,
                            GetPoolStr (&SelfVolume->VolName)
                        );

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume,
                            FileName,
                            TheTitle,
                            BuiltinIcon (TheIcon),
                            'G',
                            FALSE
                        );

                        LOG2(1, LOG_THREE_STAR_END, L"              - ", L"\n", L"Added %s:- '%s'", ToolName, FileName);
                  } // if
                    MyFreePool (&FileName);
                } // while
                break;

            case TAG_NETBOOT:
                j = 0;
                while ((FileName = FindCommaDelimited (NETBOOT_NAMES, j++)) != NULL) {
                    if (IsValidTool (SelfVolume, FileName)) {
                        LOG(1, LOG_LINE_NORMAL,
                            L"Adding Netboot tag for '%s' on '%s'",
                            FileName,
                            GetPoolStr (&SelfVolume->VolName)
                        );

                        FoundTool = TRUE;
                        AddToolEntry (
                            SelfVolume,
                            FileName,
                            TheTitle,
                            BuiltinIcon (TheIcon),
                            'N',
                            FALSE
                        );

                        LOG2(1, LOG_THREE_STAR_END, L"              - ", L"\n", L"Added %s:- '%s'", ToolName, FileName);
                    } // if

                    MyFreePool (&FileName);
                    FileName = NULL;
                } // while
                break;

            case TAG_APPLE_RECOVERY:
                for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                    j = 0;
                    while (
                        (FileName = FindCommaDelimited (GlobalConfig.MacOSRecoveryFiles, j++)) != NULL
                    ) {
                        if ((Volumes[VolumeIndex]->RootDir != NULL)) {
                            if ((IsValidTool (Volumes[VolumeIndex], FileName))) {
                                LOG(1, LOG_LINE_NORMAL,
                                    L"Adding Apple Recovery tag for '%s' on '%s'",
                                    FileName,
                                    GetPoolStr (&Volumes[VolumeIndex]->VolName)
                                );

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
                                    'R',
                                    TRUE
                                );

                                LOG2(1, LOG_THREE_STAR_END, L"              - ", L"\n", L"Added %s:- '%s'", ToolName, FileName);
                            } // if
                        } // if
                        MyFreePool (&FileName);
                    } // while
                } // for
                break;

            case TAG_WINDOWS_RECOVERY:
                j = 0;
                while (
                    (FileName = FindCommaDelimited (GlobalConfig.WindowsRecoveryFiles, j++)) != NULL
                ) {
                    SplitVolumeAndFilename (&FileName, &VolName);
                    for (VolumeIndex = 0; VolumeIndex < VolumesCount; VolumeIndex++) {
                        if ((Volumes[VolumeIndex]->RootDir != NULL) &&
                            (IsValidTool (Volumes[VolumeIndex], FileName)) &&
                            ((VolName == NULL) || MyStriCmp (VolName, GetPoolStr (&Volumes[VolumeIndex]->VolName)))
                        ) {
                            LOG(1, LOG_LINE_NORMAL,
                                L"Adding Windows Recovery tag for '%s' on '%s'",
                                FileName, GetPoolStr (&Volumes[VolumeIndex]->VolName)
                            );

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
                                'R',
                                TRUE
                            );

                            MyFreePool (&Description);

                            LOG2(1, LOG_THREE_STAR_END, L"              - ", L"\n", L"Added %s:- '%s'", ToolName, FileName);
                        } // if
                    } // for

                    MyFreePool (&FileName);
                    MyFreePool (&VolName);
                    FileName = VolName = NULL;
                } // while
                break;

        } // switch()
        
        #if REFIT_DEBUG > 0
        if (Skipped) {
            if (FoundTool) {
                MsgLog ("* WARN: Found tool for '%s'\n", ToolName);
            }
        }
        else {
            if (!FoundTool) {
                MsgLog ("* WARN: Could not find '%s'\n", ToolName);
            }
            MsgLog ("] Tool Type %02d ...%s\n", i + 1, ToolName);
        }
        #endif

        MyFreePool (&FileName);
        MyFreePool (&VolName);
    } // for

    MyFreePool (&MokLocations);

    MsgLog ("] ScanForTools\n");
} // VOID ScanForTools
