/*
 * libeg/image.c
 * Image handling functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
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
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 *
 */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libegint.h"
#include "../BootMaster/global.h"
#include "../BootMaster/lib.h"
#include "../BootMaster/screenmgt.h"
#include "../BootMaster/mystrings.h"
#include "../include/refit_call_wrapper.h"
#include "lodepng.h"
#include "libeg.h"
#include "../BootMaster/leaks.h"

#define MAX_FILE_SIZE (1024*1024*1024)

// Multiplier for pseudo-floating-point operations in egScaleImage().
// A value of 4096 should keep us within limits on 32-bit systems, but I've
// seen some minor artifacts at this level, so give it a bit more precision
// on 64-bit systems.
#if defined(EFIX64) | defined(EFIAARCH64)
    #define FP_MULTIPLIER (UINTN) 65536
#else
    #define FP_MULTIPLIER (UINTN) 4096
#endif

#ifndef __MAKEWITH_GNUEFI
    #define LibLocateHandle gBS->LocateHandleBuffer
    #define LibOpenRoot EfiLibOpenRoot
#endif

//
// Basic image handling
//

EG_IMAGE * egCreateImage (
    IN UINTN    Width,
    IN UINTN    Height,
    IN BOOLEAN  HasAlpha
) {
    EG_IMAGE   *NewImage;

    NewImage = (EG_IMAGE *) AllocatePool (sizeof (EG_IMAGE));
    if (NewImage == NULL) {
        return NULL;
    }
    NewImage->PixelData = (EG_PIXEL *) AllocatePool (Width * Height * sizeof (EG_PIXEL));

    if (NewImage->PixelData == NULL) {
        egFreeImage (NewImage);
        return NULL;
    }

    NewImage->Width    = Width;
    NewImage->Height   = Height;
    NewImage->HasAlpha = HasAlpha;

    return NewImage;
}

EG_IMAGE * egCreateFilledImage (
    IN UINTN      Width,
    IN UINTN      Height,
    IN BOOLEAN    HasAlpha,
    IN EG_PIXEL  *Color
) {
    EG_IMAGE  *NewImage;

    NewImage = egCreateImage (Width, Height, HasAlpha);
    if (NewImage == NULL) {
        return NULL;
    }

    egFillImage (NewImage, Color);

    return NewImage;
}

EG_IMAGE * egCopyImage (
    IN EG_IMAGE *Image
) {
    EG_IMAGE  *NewImage = NULL;

    if (Image != NULL) {
        NewImage = egCreateImage (Image->Width, Image->Height, Image->HasAlpha);
    }
    if (NewImage == NULL) {
        return NULL;
    }

    CopyMem (NewImage->PixelData, Image->PixelData, Image->Width * Image->Height * sizeof (EG_PIXEL));

    return NewImage;
}

// Returns a smaller image composed of the specified crop area from the larger area.
// If the specified area is larger than is in the original, returns NULL.
EG_IMAGE * egCropImage (
    IN EG_IMAGE  *Image,
    IN UINTN      StartX,
    IN UINTN      StartY,
    IN UINTN      Width,
    IN UINTN      Height
) {
    EG_IMAGE *NewImage = NULL;
    UINTN x, y;

    if (((StartX + Width) > Image->Width) || ((StartY + Height) > Image->Height)) {
        return NULL;
    }

    NewImage = egCreateImage (Width, Height, Image->HasAlpha);
    if (NewImage == NULL) {
        return NULL;
    }

    for (y = 0; y < Height; y++) {
        for (x = 0; x < Width; x++) {
            NewImage->PixelData[y * NewImage->Width + x] = Image->PixelData[(y + StartY) * Image->Width + x + StartX];
        }
    }

    return NewImage;
} // EG_IMAGE * egCropImage()

// The following function implements a bilinear image scaling algorithm, based on
// code presented at http://tech-algorithm.com/articles/bilinear-image-scaling/.
// Resize an image; returns pointer to resized image if successful, NULL otherwise.
// Calling function is responsible for freeing allocated memory.
// NOTE: x_ratio, y_ratio, x_diff, and y_diff should really be float values;
// however, I've found that my 32-bit Mac Mini has a buggy EFI (or buggy CPU?), which
// causes this function to hang on float-to-UINT8 conversions on some (but not all!)
// float values. Therefore, this function uses integer arithmetic but multiplies
// all values by FP_MULTIPLIER to achieve something resembling the sort of precision
// needed for good results.
EG_IMAGE * egScaleImage (
    IN EG_IMAGE  *Image,
    IN UINTN      NewWidth,
    IN UINTN      NewHeight
) {
    EG_IMAGE  *NewImage = NULL;
    EG_PIXEL   a, b, c, d;
    UINTN      i, j;
    UINTN      x, y, Index;
    UINTN      Offset = 0;
    UINTN      x_diff, y_diff;
    UINTN      x_ratio, y_ratio;

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Scaling Image to %d x %d", NewWidth, NewHeight);
    #endif

    if ((Image == NULL) || (Image->Height == 0) || (Image->Width == 0) || (NewWidth == 0) || (NewHeight == 0)) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"In egScaleImage ... Image is NULL or a Size is 0!!");
        #endif

        return NULL;
    }

    if ((Image->Width == NewWidth) && (Image->Height == NewHeight)) {
        return (egCopyImage (Image));
    }

    NewImage = egCreateImage (NewWidth, NewHeight, Image->HasAlpha);
    if (NewImage == NULL) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"In egScaleImage ... Could Not Create New Image!!");
        #endif

        return NULL;
    }

    x_ratio = ((Image->Width - 1) * FP_MULTIPLIER) / NewWidth;
    y_ratio = ((Image->Height - 1) * FP_MULTIPLIER) / NewHeight;

    for (i = 0; i < NewHeight; i++) {
        for (j = 0; j < NewWidth; j++) {
            x = (j * (Image->Width - 1)) / NewWidth;
            y = (i * (Image->Height - 1)) / NewHeight;
            x_diff = (x_ratio * j) - x * FP_MULTIPLIER;
            y_diff = (y_ratio * i) - y * FP_MULTIPLIER;
            Index = ((y * Image->Width) + x);
            a = Image->PixelData[Index];
            b = Image->PixelData[Index + 1];
            c = Image->PixelData[Index + Image->Width];
            d = Image->PixelData[Index + Image->Width + 1];

            // blue element
            NewImage->PixelData[Offset].b = ((a.b) * (FP_MULTIPLIER - x_diff) * (FP_MULTIPLIER - y_diff) +
                (b.b) * (x_diff) * (FP_MULTIPLIER - y_diff) +
                (c.b) * (y_diff) * (FP_MULTIPLIER - x_diff) +
                (d.b) * (x_diff * y_diff)) / (FP_MULTIPLIER * FP_MULTIPLIER);

            // green element
            NewImage->PixelData[Offset].g = ((a.g) * (FP_MULTIPLIER - x_diff) * (FP_MULTIPLIER - y_diff) +
                (b.g) * (x_diff) * (FP_MULTIPLIER - y_diff) +
                (c.g) * (y_diff) * (FP_MULTIPLIER - x_diff) +
                (d.g) * (x_diff * y_diff)) / (FP_MULTIPLIER * FP_MULTIPLIER);

            // red element
            NewImage->PixelData[Offset].r = ((a.r) * (FP_MULTIPLIER - x_diff) * (FP_MULTIPLIER - y_diff) +
                (b.r) * (x_diff) * (FP_MULTIPLIER - y_diff) +
                (c.r) * (y_diff) * (FP_MULTIPLIER - x_diff) +
                (d.r) * (x_diff * y_diff)) / (FP_MULTIPLIER * FP_MULTIPLIER);

            // alpha element
            NewImage->PixelData[Offset++].a = ((a.a) * (FP_MULTIPLIER - x_diff) * (FP_MULTIPLIER - y_diff) +
                (b.a) * (x_diff) * (FP_MULTIPLIER - y_diff) +
                (c.a) * (y_diff) * (FP_MULTIPLIER - x_diff) +
                (d.a) * (x_diff * y_diff)) / (FP_MULTIPLIER * FP_MULTIPLIER);
        } // for (j...)
    } // for (i...)

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL, L"Scaling Image Completed");
    #endif

    return NewImage;
} // EG_IMAGE * egScaleImage()

VOID egFreeImage (
    IN EG_IMAGE *Image
) {
    if (Image == NULL) {
        return;
    }

    MyFreePool (&Image->PixelData);
    MyFreePool (&Image);
}

//
// Basic file operations
//

#define LOG_ALL_LOADS 0

EFI_STATUS egLoadFile (
    IN EFI_FILE  *BaseDir,
    IN CHAR16    *FileName,
    OUT UINT8   **FileData,
    OUT UINTN    *FileDataLength
) {
    EFI_STATUS          Status;
    UINT64              ReadSize;
    UINTN               BufferSize;
    UINT8              *Buffer;
    EFI_FILE_INFO      *FileInfo;
    EFI_FILE_HANDLE     FileHandle;

    #if LOG_ALL_LOADS
    LOGPROCENTRY("'%s'", FileName);
    #endif

    if (FileData) *FileData = NULL;

    if ((BaseDir == NULL) || (FileName == NULL)) {
        #if LOG_ALL_LOADS
        LOGPROCEXIT("'%s' invalid parameter!!", FileName);
        #else
        MsgLog ("egLoadFile '%s' invalid parameter!!\n", FileName);
        #endif

        return EFI_INVALID_PARAMETER;
    }

    LEAKABLEEXTERNALSTART (kLeakableWhategLoadFileStart);
    Status = REFIT_CALL_5_WRAPPER(BaseDir->Open, BaseDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
    LEAKABLEEXTERNALSTOP ();
    if (EFI_ERROR(Status)) {
        #if LOG_ALL_LOADS
        LOGPROCEXIT("'%s' not opened (%r)\n", FileName, Status);
        #else
        MsgLog ("egLoadFile '%s' not opened (%r)\n", FileName, Status);
        #endif
        return Status;
    }

    FileInfo = LibFileInfo (FileHandle);
    if (FileInfo == NULL) {
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
        #if LOG_ALL_LOADS
        LOGPROCEXIT("'%s' can't get info\n", FileName);
        #else
        MsgLog ("egLoadFile '%s' can't get info\n", FileName);
        #endif
        return EFI_NOT_FOUND;
    }
    #if REFIT_DEBUG > 0
    if (LOGPOOL(FileInfo) < 0) {
        LOGWHERE("egLoadFile '%s'\n", FileName);
    }
    #endif

    ReadSize = FileInfo->FileSize;

    if (ReadSize > MAX_FILE_SIZE) {
        ReadSize = MAX_FILE_SIZE;
    }

    MyFreePool (&FileInfo);

    BufferSize = (UINTN)ReadSize;   // was limited to 1 GB above, so this is safe
    Buffer = (UINT8 *) AllocatePool (BufferSize);
    if (Buffer == NULL) {
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
        #if LOG_ALL_LOADS
        LOGPROCEXIT("'%s' no memory\n", FileName);
        #else
        MsgLog ("egLoadFile '%s' no memory\n", FileName);
        #endif

        return EFI_OUT_OF_RESOURCES;
    }

    Status = REFIT_CALL_3_WRAPPER(FileHandle->Read, FileHandle, &BufferSize, Buffer);
    REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
    if (EFI_ERROR(Status)) {
        MyFreePool (&Buffer);
        #if LOG_ALL_LOADS
        LOGPROCEXIT("'%s' can't read (%r)\n", FileName, Status);
        #else
        MsgLog ("egLoadFile '%s' can't read (%r)\n", FileName, Status);
        #endif

        return Status;
    }

    if (FileData) {
        *FileData       = Buffer;
    } else {
        MyFreePool (&Buffer);
    }
    if (FileDataLength) {
        *FileDataLength = BufferSize;
    }

    #if LOG_ALL_LOADS
    LOGPROCEXIT("'%s' loaded", FileName);
    #else
    #if REFIT_DEBUG > 0
    LOG(4, LOG_THREE_STAR_MID, L"In egLoadFile ... Loaded File:- '%s'", FileName);
    #endif
    #endif

    return EFI_SUCCESS;
}

EFI_STATUS egFindESP (
    OUT EFI_FILE_HANDLE *RootDir
) {
    EFI_STATUS   Status;
    UINTN        HandleCount = 0;
    EFI_HANDLE  *Handles;
    EFI_GUID     ESPGuid = ESP_GUID_VALUE;

    Status = LibLocateHandle (ByProtocol, &ESPGuid, NULL, &HandleCount, &Handles);
    if (!EFI_ERROR(Status) && HandleCount > 0) {
        *RootDir = LibOpenRoot (Handles[0]);

        if (*RootDir == NULL) {
            Status = EFI_NOT_FOUND;
        }

        MyFreePool (&Handles);
    }

    return Status;
}

EFI_STATUS egSaveFile (
    IN EFI_FILE  *BaseDir OPTIONAL,
    IN CHAR16    *FileName,
    IN UINT8     *FileData,
    IN UINTN      FileDataLength
) {
    EFI_STATUS       Status;
    UINTN            BufferSize;
    EFI_FILE_HANDLE  FileHandle;

    if (BaseDir == NULL) {
        Status = egFindESP (&BaseDir);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    LEAKABLEEXTERNALSTART ("egSaveFile Open");
    Status = REFIT_CALL_5_WRAPPER(
        BaseDir->Open, BaseDir,
        &FileHandle, FileName,
        EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE, 0
    );
    LEAKABLEEXTERNALSTOP ();

    if (EFI_ERROR(Status)) {
        return Status;
    }

    if (FileDataLength == 0) {
        Status = REFIT_CALL_1_WRAPPER(FileHandle->Delete, FileHandle);
    }
    else {
        BufferSize = FileDataLength;
        LEAKABLEEXTERNALSTART ("egSaveFile Write");
        Status     = REFIT_CALL_3_WRAPPER(
            FileHandle->Write, FileHandle,
            &BufferSize, FileData
        );
        LEAKABLEEXTERNALSTOP ();
        LEAKABLEEXTERNALSTART ("egSaveFile Close");
        REFIT_CALL_1_WRAPPER(FileHandle->Close, FileHandle);
        LEAKABLEEXTERNALSTOP ();
    }

    return Status;
}

EFI_STATUS egSaveFileNumbered (
    IN EFI_FILE  *BaseDir,
    IN CHAR16    *FileNamePattern,
    IN UINT8     *FileData,
    IN UINTN     FileDataLength,
    OUT CHAR16   **OutFileName
) {
    CHAR16 *FileName = NULL;
    EFI_STATUS Status;

    // Search for existing screen shot files; increment number to an unused value...
    UINTN i = 0;
    do {
        MyFreePool (&FileName);
        FileName = PoolPrint (FileNamePattern, i++);
    } while (FileExists (BaseDir, FileName));

    // save to file on the ESP
    Status = egSaveFile (BaseDir, FileName, FileData, FileDataLength);

    if (!EFI_ERROR(Status) && OutFileName) {
        *OutFileName = FileName;
    }
    else {
        if (OutFileName) {
            *OutFileName = NULL;
        }
        MyFreePool (&FileName);
    }

    return Status;
} // egSaveFileNumbered

//
// Loading images from files and embedded data
//

// Decode the specified image data. The IconSize parameter is relevant only
// for ICNS, for which it selects which ICNS sub-image is decoded.
// Returns a pointer to the resulting EG_IMAGE or NULL if decoding failed.
EG_IMAGE * egDecodeAny (
    IN UINT8    *FileData,
    IN UINTN     FileDataLength,
    IN UINTN     IconSize,
    IN BOOLEAN   WantAlpha
) {
    EG_IMAGE *NewImage; { NewImage = egDecodePNG  (FileData, FileDataLength, IconSize, WantAlpha   ); if (NewImage) MsgLog("loaded png\n"); }
    if (!NewImage)      { NewImage = egDecodeICNS (FileData, FileDataLength, IconSize, WantAlpha, 0); if (NewImage) MsgLog("loaded icn\n"); }
    if (!NewImage)      { NewImage = egDecodeJPEG (FileData, FileDataLength, IconSize, WantAlpha   ); if (NewImage) MsgLog("loaded jpg\n"); }
    if (!NewImage)      { NewImage = egDecodeBMP  (FileData, FileDataLength, IconSize, WantAlpha   ); if (NewImage) MsgLog("loaded bmp\n"); }

    return NewImage;
}

EG_IMAGE * egLoadImage (
    IN EFI_FILE *BaseDir,
    IN CHAR16   *FileName,
    IN BOOLEAN   WantAlpha
) {
    EFI_STATUS   Status;
    UINTN        FileDataLength;
    UINT8       *FileData;
    EG_IMAGE    *NewImage;

    if (BaseDir == NULL || FileName == NULL) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL, L"In egLoadImage ... Requirements Not Met!!");
        #endif

        return NULL;
    }

    // load file
    Status = egLoadFile (BaseDir, FileName, &FileData, &FileDataLength);
    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"In egLoadImage ... '%r' Returned While Attempting to Load File!!",
            Status
        );
        #endif

        return NULL;
    }

    // decode it
    // '128' can be any arbitrary value
    NewImage = egDecodeAny (FileData, FileDataLength, 128, WantAlpha);
    MyFreePool (&FileData);

    return NewImage;
}

// Load an icon from (BaseDir)/Path, extracting the icon of size IconSize x IconSize.
// Returns a pointer to the image data, or NULL if the icon could not be loaded.
EG_IMAGE * egLoadIcon (
    IN EFI_FILE *BaseDir,
    IN CHAR16   *Path,
    IN UINTN     IconSize
) {
    EFI_STATUS      Status;
    UINTN           FileDataLength;
    UINT8          *FileData;
    EG_IMAGE       *NewImage;
    EG_IMAGE       *Image;

    if ((BaseDir == NULL) || (Path == NULL)) {
        // set error status if unable to get to image
        Status = EFI_INVALID_PARAMETER;
    }
    else if (!AllowGraphicsMode) {
        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID,
            L"In egLoadIcon ... Skipped Loading Icon in Text Screen Mode"
        );
        #endif

        return NULL;
    }
    else {
        // try to load file if able to get to image
        Status = egLoadFile (BaseDir, Path, &FileData, &FileDataLength);
    }

    if (EFI_ERROR(Status)) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"In egLoadIcon ... '%r' When Trying to Load Icon:- '%s'!!",
            Status, Path
        );
        #endif

        // return null if error
        return NULL;
    }

    // decode it
    LOGPOOL (FileData);
    Image = egDecodeAny (FileData, FileDataLength, IconSize, TRUE);
    LOGPOOL (FileData);
    MyFreePool (&FileData);

    // return null if unable to decode
    if (Image == NULL) {
        #if REFIT_DEBUG > 0
        LOG(3, LOG_LINE_NORMAL,
            L"In egLoadIcon ... Could Not Decode File Data!!"
        );
        #endif

        return NULL;
    }

    if ((Image->Width != IconSize) || (Image->Height != IconSize)) {
        NewImage = egScaleImage (Image, IconSize, IconSize);

        // use scaled image if available
        if (NewImage) {
            egFreeImage (Image);
            Image = NewImage;
        }
        else {
            CHAR16 *MsgStr = PoolPrint (
                L"Could Not Scale Icon in '%s' from %d x %d to %d x %d!!",
                Path, Image->Width, Image->Height, IconSize, IconSize
            );

            #if REFIT_DEBUG > 0
            LOG(3, LOG_LINE_NORMAL, L"In egLoadIcon ... %s", MsgStr);
            #endif

            Print(MsgStr);

            MyFreePool (&MsgStr);
        }
    }

    return Image;
} // EG_IMAGE *egLoadIcon()

// Returns an icon of any type from the specified subdirectory using the specified
// base name. All directory references are relative to BaseDir. For instance, if
// SubdirName is "myicons" and BaseName is "os_linux", this function will return
// an image based on "myicons/os_linux.icns" or "myicons/os_linux.png", in that
// order of preference. Returns NULL if no such file is a valid icon file.
EG_IMAGE * egLoadIconAnyType (
    IN EFI_FILE  *BaseDir,
    IN CHAR16    *SubdirName,
    IN CHAR16    *BaseName,
    IN UINTN      IconSize
) {
    EG_IMAGE  *Image = NULL;
    CHAR16    *Extension;
    CHAR16    *FileName;
    UINTN      i = 0;

    LOGPROCENTRY("from '%s\\%s' with extensions '%s'\n",
        SubdirName, BaseName, ICON_EXTENSIONS
    );

    if (!AllowGraphicsMode) {
        #if REFIT_DEBUG > 0
        LOG(4, LOG_THREE_STAR_MID,
            L"In egLoadIconAnyType ... Skipped Loading Icon in Text Screen Mode"
        );
        #endif
        LOGPROCEXIT();

        return NULL;
    }

    #if REFIT_DEBUG > 0
    LOG(4, LOG_THREE_STAR_MID,
        L"Trying to Load Icon from '%s' with Base Name:- '%s'",
        (StrLen (SubdirName) != 0) ? SubdirName : L"\\",
        BaseName
    );
    #endif

    while ((Image == NULL) && ((Extension = FindCommaDelimited (ICON_EXTENSIONS, i++)) != NULL)) {
        FileName = PoolPrint (L"%s\\%s.%s", SubdirName, BaseName, Extension);
        Image    = egLoadIcon (BaseDir, FileName, IconSize);

        MyFreePool (&Extension);
        MyFreePool (&FileName);
    } // while

    #if REFIT_DEBUG > 0
    LOG(3, LOG_LINE_NORMAL,
        L"In egLoadIconAnyType ... %s",
        (Image != NULL) ? L"Loaded Icon" : L"Could Not Load Icon!!"
    );
    #endif
    LOGPROCEXIT("%s", Image ? L"Success" : L"Fail" );

    return Image;
} // EG_IMAGE *egLoadIconAnyType()

// Returns an icon with any extension in ICON_EXTENSIONS from either the directory
// specified by GlobalConfig.IconsDir or DEFAULT_ICONS_DIR. The input BaseName
// should be the icon name without an extension. For instance, if BaseName is
// os_linux, GlobalConfig.IconsDir is myicons, DEFAULT_ICONS_DIR is icons, and
// ICON_EXTENSIONS is "icns,png", this function will return myicons/os_linux.icns,
// myicons/os_linux.png, icons/os_linux.icns, or icons/os_linux.png, in that
// order of preference. Returns NULL if no such icon can be found. All file
// references are relative to SelfDir.
EG_IMAGE * egFindIcon (
    IN CHAR16 *BaseName,
    IN UINTN   IconSize
) {
    EG_IMAGE *Image = NULL;

    if (GlobalConfig.IconsDir != NULL) {
        Image = egLoadIconAnyType (
            SelfDir, GlobalConfig.IconsDir,
            BaseName, IconSize
        );
    }

    if (Image == NULL) {
        Image = egLoadIconAnyType (
            SelfDir, DEFAULT_ICONS_DIR,
            BaseName, IconSize
        );
    }

    return Image;
} // EG_IMAGE * egFindIcon()

EG_IMAGE * egPrepareEmbeddedImage (
    IN EG_EMBEDDED_IMAGE *EmbeddedImage,
    IN BOOLEAN            WantAlpha
) {
    EG_IMAGE  *NewImage;
    UINT8     *CompData;
    UINTN      CompLen;
    UINTN      PixelCount;

    // sanity checks
    if (!EmbeddedImage) {
        return NULL;
    }

    if (EmbeddedImage->PixelMode > EG_MAX_EIPIXELMODE ||
        (EmbeddedImage->CompressMode != EG_EICOMPMODE_NONE &&
        EmbeddedImage->CompressMode  != EG_EICOMPMODE_RLE)
    ) {
        return NULL;
    }

    // allocate image structure and pixel buffer
    NewImage = egCreateImage (EmbeddedImage->Width, EmbeddedImage->Height, WantAlpha);
    if (NewImage == NULL) {
        return NULL;
    }

    CompData   = (UINT8 *) EmbeddedImage->Data;   // drop const
    CompLen    = EmbeddedImage->DataLength;
    PixelCount = EmbeddedImage->Width * EmbeddedImage->Height;

    // FUTURE: for EG_EICOMPMODE_EFICOMPRESS, decompress whole data block here

    if (EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY ||
        EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY_ALPHA
    ) {
        // copy grayscale plane and expand
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, r), PixelCount);
        }
        else {
            egInsertPlane (CompData, PLPTR(NewImage, r), PixelCount);
            CompData += PixelCount;
        }
        egCopyPlane (PLPTR(NewImage, r), PLPTR(NewImage, g), PixelCount);
        egCopyPlane (PLPTR(NewImage, r), PLPTR(NewImage, b), PixelCount);

    }
    else if (EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR ||
        EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR_ALPHA
    ) {
        // copy color planes
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, r), PixelCount);
            egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, g), PixelCount);
            egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, b), PixelCount);
        }
        else {
            egInsertPlane (CompData, PLPTR(NewImage, r), PixelCount);
            CompData += PixelCount;
            egInsertPlane (CompData, PLPTR(NewImage, g), PixelCount);
            CompData += PixelCount;
            egInsertPlane (CompData, PLPTR(NewImage, b), PixelCount);
            CompData += PixelCount;
        }
    }
    else {
        // set color planes to black
        egSetPlane (PLPTR(NewImage, r), 0, PixelCount);
        egSetPlane (PLPTR(NewImage, g), 0, PixelCount);
        egSetPlane (PLPTR(NewImage, b), 0, PixelCount);
    }

    // Handle Alpha
    if (
        WantAlpha && (
            EmbeddedImage->PixelMode == EG_EIPIXELMODE_GRAY_ALPHA ||
            EmbeddedImage->PixelMode == EG_EIPIXELMODE_COLOR_ALPHA ||
            EmbeddedImage->PixelMode == EG_EIPIXELMODE_ALPHA ||
            EmbeddedImage->PixelMode == EG_EIPIXELMODE_ALPHA_INVERT
        )
    ) {
        // Add Alpha Mask if Available and Required
        if (EmbeddedImage->CompressMode == EG_EICOMPMODE_RLE) {
            egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, a), PixelCount);
        }
        else {
            egInsertPlane (CompData, PLPTR(NewImage, a), PixelCount);
            CompData += PixelCount;
        }
        if (EmbeddedImage->PixelMode == EG_EIPIXELMODE_ALPHA_INVERT) {
            egInvertPlane (PLPTR(NewImage, a), PixelCount);
        }
    }
    else {
        // Default to 'Opaque' 255 if Alpha is Required, otherwise clear unused bytes to 0
        egSetPlane (PLPTR(NewImage, a), WantAlpha ? 255 : 0, PixelCount);
    }

    return NewImage;
}

//
// Compositing
//

VOID egRestrictImageArea (
    IN EG_IMAGE   *Image,
    IN UINTN       AreaPosX,
    IN UINTN       AreaPosY,
    IN OUT UINTN  *AreaWidth,
    IN OUT UINTN  *AreaHeight
) {
    if (Image && AreaWidth && AreaHeight) {
        if (AreaPosX >= Image->Width || AreaPosY >= Image->Height) {
            // out of bounds, operation has no effect
            *AreaWidth = *AreaHeight = 0;
        }
        else {
            // calculate affected area
            if (*AreaWidth  > Image->Width  - AreaPosX) *AreaWidth  = Image->Width  - AreaPosX;
            if (*AreaHeight > Image->Height - AreaPosY) *AreaHeight = Image->Height - AreaPosY;
        }
    }
}

VOID egFillImage (
    IN OUT EG_IMAGE  *CompImage,
    IN EG_PIXEL      *Color
) {
    UINTN       i;
    EG_PIXEL    FillColor;
    EG_PIXEL   *PixelPtr;

    if (CompImage && Color) {
        FillColor = *Color;
        if (!CompImage->HasAlpha) {
            FillColor.a = 0;
        }

        PixelPtr = CompImage->PixelData;
        for (i = 0; i < CompImage->Width * CompImage->Height; i++, PixelPtr++) {
            *PixelPtr = FillColor;
        }
    }
}

VOID egFillImageArea (
    IN OUT EG_IMAGE *CompImage,
    IN UINTN         AreaPosX,
    IN UINTN         AreaPosY,
    IN UINTN         AreaWidth,
    IN UINTN         AreaHeight,
    IN EG_PIXEL     *Color
) {
    UINTN        x, y;
    EG_PIXEL     FillColor;
    EG_PIXEL    *PixelPtr;
    EG_PIXEL    *PixelBasePtr;

    if (CompImage && Color) {
        egRestrictImageArea (CompImage, AreaPosX, AreaPosY, &AreaWidth, &AreaHeight);

        if (AreaWidth > 0) {
            FillColor = *Color;
            if (!CompImage->HasAlpha) {
                FillColor.a = 0;
            }

            PixelBasePtr = CompImage->PixelData + AreaPosY * CompImage->Width + AreaPosX;
            for (y = 0; y < AreaHeight; y++) {
                PixelPtr = PixelBasePtr;
                for (x = 0; x < AreaWidth; x++, PixelPtr++) {
                    *PixelPtr = FillColor;
                }
                PixelBasePtr += CompImage->Width;
            }
        }
    }
}

// typedefs and stack functions for egSeedFillImage

typedef struct Span {
    INTN x1, x2, y, dy; // left, right, y, direction, previous span
    struct Span *ps; // previous span that is left of this span
} Span;

/*
egSeedFillImage

See https://en.wikipedia.org/wiki/Flood_fill

Based on:
    1) Heckbert, Paul S. "IV.10: A Seed Fill Algorithm". pp. 275–277, "A Seed Fill Algorithm" pp. 721-722
    2) Fishkin, Ken "IV.11: Filling a Region in a Frame Buffer". pp. 278–284
    In Glassner, Andrew S (ed.). Graphics Gems. Academic Press.
    ISBN 0122861663. (1990)

Modifications by joevt which may or may not be improvements.
*/

VOID egSeedFillImage (
    IN EG_IMAGE *Image,
    INTN x,
    INTN y,
    IN EG_PIXEL *FillColor,
    IN EG_PIXEL *FillMask,
    IN EG_PIXEL *TestColor,
    IN EG_PIXEL *TestMask,
    IN BOOLEAN EightWay,
    IN BOOLEAN ExtraPixel
) {
    UINT32 fc = *(UINT32 *)FillColor;
    UINT32 fm = *(UINT32 *)FillMask;
    UINT32 tc = *(UINT32 *)TestColor;
    UINT32 tm = *(UINT32 *)TestMask;

    UINT32 *p = (UINT32 *)Image->PixelData;
    INTN w = Image->Width;
    INTN h = Image->Height;
    INTN pw = w;

    // pixels at these locations are not mapped in memory and always match the test color
    INTN ignoreX1, ignoreX2, ignoreY1, ignoreY2;

    if (ExtraPixel) {
        // add a single extra pixel border around the image which always matches the test color
        x++;
        y++;
        p = p - w - 1; // up one extra pixel and left one extra pixel
        h += 2; // one extra pixel top and bottom
        w += 2; // one extra pixel left and right
        ignoreX1 = 0;
        ignoreX2 = w - 1;
        ignoreY1 = 0;
        ignoreY2 = h - 1;
    }
    else {
        ignoreX1 = -1;
        ignoreX2 = w;
        ignoreY1 = -1;
        ignoreY2 = h;
    }

    if (x < 0 || x >= w || y < 0 || y >= h) return; // not inside (out of bounds)

    BOOLEAN ignore, ignoreY;
    UINT32 *pc = NULL; // pointer to current p
    UINT32 c = 0; // the color to be tested or set in *pc

    ignoreY = (y == ignoreY1 || y == ignoreY2);
    #define Matched(x, y) ( (ignore = (ignoreY || x == ignoreX1 || x == ignoreX2)) || ( ((c = *(pc = &p[y * pw + x])) & tm) == tc ) )
    if (!Matched (x, y)) return;  // not inside (not test color)

    // [
    /*
    joevt:
    The algorithm requires knowing which pixels were previously filled.
    If fill color is different than test color then it is usually sufficient to just check the pixel color.
    There are 3 reasons why we cannot just check the pixel color to know if the pixel was previously filled:
    1) We use a mask for test and fill color so the resulting pixel color could still match the test color.
    2) If we had a fill pattern instead of a single color then the pattern could contain the test color.
    3) We have the ExtraPixel option which adds an unmapped pixel border that cannot be altered.
    Therefore, we need to store which pixels are filled.
    */
    const INTN fs = 6; // for divide by 64
    const INTN fn = (1 << fs) - 1; // for mod 64
    UINT64 *f = AllocateZeroPool (((h * w + fn) >> fs) * sizeof(*f));
    if (!f) return; // not enough memory
    // ]

    UINT64 *fp; // pointer to current f[] UINT64
    UINT64 fb; // the bit to be tested or set in *fp
    INTN fi; // the bit index to be tested or set in *fp

    fm = ~fm; // bits to be cleared

    #define NotFilled(x, y) (fi = (y * w + x), !(*(fp = &f[fi >> fs]) & (fb = ((UINT64)1 << (fi & fn)))))
    #define Inside(x, y) (NotFilled(x, y) && Matched(x, y))
    #define Set() do { *fp |= fb; if (!ignore) *pc = (c & fm) | fc; } while (0)

    Span *sp; // current stack pointer for next item
    Span *st; // stack start
    Span *sm; // after stack end
    Span *psr = NULL; // previous span in the reverse direction
    Span *psf = NULL; // previous span in the forward direction

    #define SpanStackNew() \
        do { \
            st = AllocatePool (sizeof(Span) * 1000); \
            if (!st) return; \
            sp = st; \
            sm = &st[1000]; \
        } while (0)

    #define SpanStackFree() \
        MyFreePool (&st)

    #define SpanStackPush(_x1, _x2, _dy, _ps) \
        do { \
            INTN yn = y + _dy; \
            if (yn < 0 || yn >= h) break; \
            if (sp == sm) { \
                UINTN oldsize = (VOID*)sp - (VOID*)st; \
                Span *so = st; \
                st = ReallocatePool (oldsize, oldsize * 2, so); \
                if (!st) return; \
                sp = (Span *)((VOID *)st + oldsize); \
                sm = (Span *)((VOID *)st + oldsize * 2); \
                for (Span *si = st; si < sp; si++) si->ps = st + (sp->ps - so); \
                if (psr) psr = st + (psr - so); \
                if (psf) psf = st + (psf - so); \
            } \
            sp->x1 = _x1; \
            sp->x2 = _x2; \
            sp->y = yn; \
            sp->dy = _dy; \
            sp->ps = _ps; \
            _ps = sp; \
            sp++; \
        } while (0)

    #define SpanStackPop(_x1, _x2, _y, _dy, _ps) \
        do { \
            sp--; \
            _x1 = sp->x1; \
            _x2 = sp->x2; \
            _y = sp->y; \
            _dy = sp->dy; \
            _ps = sp->ps; \
            ignoreY = (_y == ignoreY1 || _y == ignoreY2); \
        } while (0)

    #define SpanStack() (sp > st)

    SpanStackNew ();

    // 2nd: test span (single pixel wide) below x,y and move in a downward direction
    SpanStackPush (x - ((EightWay && (x > 0)) ? 1 : 0), x + ((EightWay && x < w - 1) ? 1 : 0), 1, psr);

    // 1st: test span at pixel that is at x,y and move in an upward direction
    y++;
    SpanStackPush (x, x, -1, psf);

    INTN xr1 = -1;
    INTN xr2 = -1;

    while (SpanStack ()) {
        Span *ps; // previous span that is left of this span
        INTN x1, x2, dy;
        SpanStackPop (x1, x2, y, dy, ps);
        if (!dy) continue; // this span was invalidated

        psr = NULL; // previous span for reverse direction
        psf = NULL; // previous span for forward direction

        // x1..x2 is parent span, dy is direction of vertical traversal
/*1*/   for (x = x1; x >= 0 && Inside (x, y); x--) Set ();
        //printicon(Image, f, sp, st);

        // [ joevt: while there are previous spans known to be on the same line and to the left of this span
        while (ps) {
            if (x <= ps->x1) { // if the first non-inside pixel is on or left of the left-most possible inside pixel of the previous span
                ps->dy = 0; // then invalidate the previous span
            }
            else {
                if (x <= ps->x2) // if the first non-inside pixel is on or left of the right-most possible inside pixel of the previous span
                    ps->x2 = x - 1; // then shorten the previous span
                break;
            }
            ps = ps->ps;
        }
        // ]

        if (x == x1) goto skip; // no span - search to the right of x1 for next span

/*2*/   if (EightWay && (x >= 0)) {} else x++; // left most pixel that was set + one more pixel to the left if doing 8-way
/*3*/   if (x < x1) { xr1 = x; xr2 = x1 - 1; } // new span left of parent span may be an overhang in the reverse direction
        // x <= x1 and is the left side of this span
        do {
/*4*/       for (x1++; x1 < w && Inside (x1, y); x1++) Set ();
/*5*/       if (EightWay && (x1 < w)) {} else x1--; // right most pixel of this span + one more pixel to the right if doing 8-way
/*6*/       SpanStackPush (x, x1, dy, psf); // continue this span in the forward direction
            //printicon(Image, f, sp, st);
/*7*/       if (x1 > x2) {
                if (xr2 >= 0) { SpanStackPush (xr1, xr2, -dy, psr); xr2 = -1; }
                SpanStackPush (x2 + 1, x1, -dy, psr); // new span right of parent span may be an overhang in the reverse direction
                //printicon(Image, f, sp, st);
            }
skip:
/*8*/       for (x1++; x1 < w && x1 <= x2; x1++) { if (Inside (x1, y)) { Set(); break; } }
/*9*/       if (EightWay) x1--; // find left pixel of next span + one more pixel to the left if doing 8-way
/*10*/      x = x1; // x is the left side of next span
        } while (x1 <= x2);

        /*
        It maybe beneficial to search reverse direction spans first because according to Ken Fishkin on pp 280:
        "turns are relatively rare and usually lead into small areas; by processing them first, stack size is reduced".
        So we push the reverse direction spans last.
        */
        if (xr2 >= 0) { SpanStackPush (xr1, xr2, -dy, psr); xr2 = -1; }
        //printicon(Image, f, sp, st);
    }
    //printicon(Image, f, sp, st);
    SpanStackFree ();

    MyFreePool (&f);
} // egSeedFillImage

VOID egRawCopy (
    IN OUT EG_PIXEL *CompBasePtr,
    IN EG_PIXEL     *TopBasePtr,
    IN UINTN         Width,
    IN UINTN         Height,
    IN UINTN         CompLineOffset,
    IN UINTN         TopLineOffset
) {
    UINTN       x, y;
    EG_PIXEL   *TopPtr, *CompPtr;

    if (CompBasePtr && TopBasePtr) {
        for (y = 0; y < Height; y++) {
            TopPtr  = TopBasePtr;
            CompPtr = CompBasePtr;

            for (x = 0; x < Width; x++) {
                *CompPtr = *TopPtr;
                TopPtr++, CompPtr++;
            }

            TopBasePtr  += TopLineOffset;
            CompBasePtr += CompLineOffset;
        }
    }
}

VOID egRawCompose (
    IN OUT EG_PIXEL *CompBasePtr,
    IN EG_PIXEL     *TopBasePtr,
    IN UINTN         Width,
    IN UINTN         Height,
    IN UINTN         CompLineOffset,
    IN UINTN         TopLineOffset
) {
    UINTN        x, y;
    EG_PIXEL    *TopPtr, *CompPtr;
    UINTN        Alpha;
    UINTN        RevAlpha;
    UINTN        Temp;

    if (CompBasePtr && TopBasePtr) {
        for (y = 0; y < Height; y++) {
            TopPtr  = TopBasePtr;
            CompPtr = CompBasePtr;

            for (x = 0; x < Width; x++) {
                Alpha    = TopPtr->a;
                RevAlpha = 255 - Alpha;

                Temp       = (UINTN) CompPtr->b * RevAlpha + (UINTN) TopPtr->b * Alpha + 0x80;
                CompPtr->b = (Temp + (Temp >> 8)) >> 8;
                Temp       = (UINTN) CompPtr->g * RevAlpha + (UINTN) TopPtr->g * Alpha + 0x80;
                CompPtr->g = (Temp + (Temp >> 8)) >> 8;
                Temp       = (UINTN) CompPtr->r * RevAlpha + (UINTN) TopPtr->r * Alpha + 0x80;
                CompPtr->r = (Temp + (Temp >> 8)) >> 8;

                TopPtr++, CompPtr++;
            }

            TopBasePtr  += TopLineOffset;
            CompBasePtr += CompLineOffset;
        }
    }
}

VOID egComposeImage (
    IN OUT EG_IMAGE *CompImage,
    IN EG_IMAGE     *TopImage,
    IN UINTN         PosX,
    IN UINTN         PosY
) {
    UINTN CompWidth;
    UINTN CompHeight;

    if (CompImage && TopImage) {
        CompWidth  = TopImage->Width;
        CompHeight = TopImage->Height;

        egRestrictImageArea (CompImage, PosX, PosY, &CompWidth, &CompHeight);

        // compose
        if (CompWidth > 0) {
            if (TopImage->HasAlpha) {
                egRawCompose (
                    CompImage->PixelData + PosY * CompImage->Width + PosX,
                    TopImage->PixelData,
                    CompWidth, CompHeight,
                    CompImage->Width, TopImage->Width
                );
            }
            else {
                egRawCopy (
                    CompImage->PixelData + PosY * CompImage->Width + PosX,
                    TopImage->PixelData,
                    CompWidth, CompHeight,
                    CompImage->Width, TopImage->Width
                );
            }
        }
    }
} // VOID egComposeImage()

//
// misc internal functions
//

VOID egInsertPlane (
    IN UINT8 *SrcDataPtr,
    IN UINT8 *DestPlanePtr,
    IN UINTN  PixelCount
) {
    UINTN i;

    if (SrcDataPtr && DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr  = *SrcDataPtr++;
             DestPlanePtr += 4;
        }
    }
}

VOID egInvertPlane (
    IN UINT8 *DestPlanePtr,
    IN UINTN PixelCount
) {
    UINTN i;

    if (DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr = 255 - *DestPlanePtr;
            DestPlanePtr += 4;
        }
    }
}

VOID egSetPlane (
    IN UINT8 *DestPlanePtr,
    IN UINT8  Value,
    IN UINTN  PixelCount
) {
    UINTN i;

    if (DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr  = Value;
             DestPlanePtr += 4;
        }
    }
}

VOID egCopyPlane (
    IN UINT8 *SrcPlanePtr,
    IN UINT8 *DestPlanePtr,
    IN UINTN  PixelCount
) {
    UINTN i;

    if (SrcPlanePtr && DestPlanePtr) {
        for (i = 0; i < PixelCount; i++) {
            *DestPlanePtr  = *SrcPlanePtr;
             DestPlanePtr += 4, SrcPlanePtr += 4;
        }
    }
}

VOID
LEAKABLEIMAGE (
    EG_IMAGE *Image
) {
    if (Image) {
        LEAKABLEPATHINC ();
            LEAKABLEWITHPATH (Image->PixelData, "Image PixelData");
        LEAKABLEPATHDEC ();
    }
    LEAKABLEWITHPATH (Image, "Image");
}

/* EOF */
