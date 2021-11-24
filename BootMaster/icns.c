/*
 * BootMaster/icns.c
 * Loader for .icns icon files
 *
 * Copyright (c) 2006-2007 Christoph Pfisterer
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
 * Modified for eEFInd
 * Copyright (c) 2021 Roderick W Smith
 */
/*
 * Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2021 Joe van Tunen (joevt@shaw.ca)
 *
 * Modifications distributed under the preceding terms.
 */


#include "global.h"
#include "lib.h"
#include "leaks.h"
#include "icns.h"
#include "config.h"
#include "mystrings.h"
#include "../BootMaster/screenmgt.h"
#include "../include/egemb_tool_bootscreen.h"
#include "../include/egemb_tool_clean_nvram.h"


//
// well-known icons
//

typedef struct {
    PoolImage   Image_PI_;
    CHAR16      *FileName;
    UINTN       IconSize;
} BUILTIN_ICON;

#define ONEICON(id, file, type) { NULLPI, file, type },
BUILTIN_ICON BuiltinIconTable[] = {
    #include "icns.include"
};

PoolImage * BuiltinIcon(IN UINTN Id)
{
    if (Id >= BUILTIN_ICON_COUNT) {
        return NULL;
    }

    if (!GetPoolImage (&BuiltinIconTable[Id].Image)) {
        AssignCachedPoolImage (&BuiltinIconTable[Id].Image, egFindIcon(
            BuiltinIconTable[Id].FileName,
            GlobalConfig.IconSizes[BuiltinIconTable[Id].IconSize]
        ));
        if (!GetPoolImage (&BuiltinIconTable[Id].Image)) {
            if (Id == BUILTIN_ICON_TOOL_BOOTKICKER) {
                AssignCachedPoolImage (&BuiltinIconTable[Id].Image, egPrepareEmbeddedImage(&egemb_tool_bootscreen, FALSE));
            }
            else if (Id == BUILTIN_ICON_TOOL_NVRAMCLEAN) {
                AssignCachedPoolImage (&BuiltinIconTable[Id].Image, egPrepareEmbeddedImage(&egemb_tool_clean_nvram, FALSE));
            }
            if (!GetPoolImage (&BuiltinIconTable[Id].Image)) {
                AssignCachedPoolImage (&BuiltinIconTable[Id].Image, DummyImage(GlobalConfig.IconSizes[BuiltinIconTable[Id].IconSize]));
            }
        }

        #if REFIT_DEBUG > 0
            LOGBLOCKENTRY("LEAKABLEBUILTINICON %d", Id);
            LEAKABLEPATHINIT (kLeakableBuiltinIcons+Id);
            LEAKABLEIMAGE (GetPoolImage (&BuiltinIconTable[Id].Image));
            LEAKABLEPATHDONE ();
            LOGBLOCKEXIT("LEAKABLEBUILTINICON");
        #endif
    } // if

    return &BuiltinIconTable[Id].Image_PI_;
}

//
// Load an icon for an operating system
//

// Load an OS icon from among the comma-delimited list provided in OSIconName.
// Searches for icons with extensions in the ICON_EXTENSIONS list (via
// egFindIcon()).
// Returns image data. On failure, returns an ugly "dummy" icon.
EG_IMAGE * LoadOSIcon(
    IN  CHAR16  *OSIconName OPTIONAL,
    IN  CHAR16  *FallbackIconName,
    IN  BOOLEAN  BootLogo
) {
    EG_IMAGE        *Image = NULL;
    CHAR16          *CutoutName, *BaseName;
    UINTN            Index = 0;

    if (!AllowGraphicsMode) {
        // skip loading if it is not used anyway
        return NULL;
    }

    // First, try to find an icon from the OSIconName list.
    while ((Image == NULL) &&
        ((CutoutName = FindCommaDelimited (OSIconName, Index++)) != NULL)
    ) {
        BaseName = PoolPrint (L"%s_%s", BootLogo ? L"boot" : L"os", CutoutName);
        Image    = egFindIcon (BaseName, GlobalConfig.IconSizes[ICON_SIZE_BIG]);
        MyFreePool (&CutoutName);
        MyFreePool (&BaseName);
    }

    // If that fails, try again using the FallbackIconName.
    if (Image == NULL) {
        BaseName = PoolPrint (L"%s_%s", BootLogo ? L"boot" : L"os", FallbackIconName);

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"Trying to find an icon from '%s'", BaseName);
        #endif

        Image = egFindIcon (BaseName, GlobalConfig.IconSizes[ICON_SIZE_BIG]);
        MyFreePool (&BaseName);
    }

    // If that fails and if BootLogo was set, try again using the "os_" start of the name.
    if (BootLogo && (Image == NULL)) {
        BaseName = PoolPrint (L"os_%s", FallbackIconName);

        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"Trying to find an icon from '%s'", BaseName);
        #endif

        Image = egFindIcon (BaseName, GlobalConfig.IconSizes[ICON_SIZE_BIG]);
        MyFreePool (&BaseName);
    }

    // If all of these fail, return the dummy image.
    if (Image == NULL) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"Setting dummy image");
        #endif

        Image = DummyImage (GlobalConfig.IconSizes[ICON_SIZE_BIG]);
    }

    return Image;
} /* EG_IMAGE * LoadOSIcon() */


static
EG_PIXEL BlackPixel  = { 0x00, 0x00, 0x00, 0 };
//static
// EG_PIXEL YellowPixel = { 0x00, 0xff, 0xff, 0 };

EG_IMAGE * DummyImage (
    IN UINTN PixelSize
) {
    EG_IMAGE        *Image;
    UINTN            x, y, LineOffset;
    CHAR8           *Ptr, *YPtr;

    Image = egCreateFilledImage (PixelSize, PixelSize, TRUE, &BlackPixel);

    if (Image == NULL) {
        return NULL;
    }

    LineOffset = PixelSize * 4;

    YPtr = (CHAR8 *) Image->PixelData + ((PixelSize - 32) >> 1) * (LineOffset + 4);
    for (y = 0; y < 32; y++) {
        Ptr = YPtr;
        for (x = 0; x < 32; x++) {
            if (((x + y) % 12) < 6) {
                *Ptr++ = 0;
                *Ptr++ = 0;
                *Ptr++ = 0;
            }
            else {
                *Ptr++ = 0;
                *Ptr++ = 255;
                *Ptr++ = 255;
            }
            *Ptr++ = 144;
        }
        YPtr += LineOffset;
    }

    return Image;
}
