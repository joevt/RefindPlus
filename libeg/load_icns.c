/*
 * libeg/load_icns.c
 * Loading function for .icns Apple icon images
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

#include "libegint.h"

static UINTN IconSizes[] = { 1024, 512, 256, 128, 64, 48, 36, 32, 24, 18, 16, 12, 8 };
#define MAX_ICNS_SIZES (sizeof(IconSizes)/sizeof(IconSizes[0]))

/*
Mac color lookup table is 16bpc but UEFI is 8bpc. For a
given 16 bit component 0xHIJK, we choose the 8 bits 0xLM
that minimizes the difference between 0xLMLM and 0xHIJK.

Only the Mac 4 bit color lookup table has components
0xHIJK where 0xJK does not match 0xHI.

The Mac 8 bit clut has components that can be defined with 4bpc.
It contains 6x6x6 RGB values (216) and 4 bit gradients for red, green, blue, grayscale.
The 6 values are 0,3,6,9,C,F
(16 - 6) * 4 = 40 // 4 bit gradient = 16 colors; minus 6 defined by the 6x6x6 RGB; *4 for r,g,b,gray
216 + 40 = 256 // 8 bit clut = 256 colors
*/

// create 8bpc color using 4bpc input
#define _4(rgb) { (0x ## rgb & 15) * 17, ((0x ## rgb >> 4) & 15) * 17, ((0x ## rgb >> 8) & 15) * 17, 0 },

// create 8bpc color using 8bpc input
#define _8(r,g,b) { 0x ## b, 0x ## g, 0x ## r, 0 },

// create 8bpc color using 16bpc input
#define _16(r,g,b) { (0x ## b + 128) / 257, (0x ## g + 128) / 257, (0x ## r + 128) / 257, 0 },

// in all cluts, first color is white and the last color is black
EG_PIXEL Clut1[] = {
    _4(FFF) _4(000)
};

EG_PIXEL Clut2[] = {
    _4(FFF) _8(AC,AC,AC) _4(555) _4(000)
};

EG_PIXEL Clut4[] = {
    _4(FFF)
    _16(FC00,F37D,052F)
    _16(FFFF,648A,028C)
    _16(DD6B,08C2,06A2)
    _16(F2D7,0856,84EC)
    _16(46E3,0000,A53E)
    _16(0000,0000,D400)
    _16(0241,AB54,EAFF)
    _16(1F21,B793,1431)
    _16(0000,64AF,11B0)
    _16(5600,2C9D,0524)
    _16(90D7,7160,3A34)
    _16(C000,C000,C000)
    _16(8000,8000,8000)
    _16(4000,4000,4000)
    _4(000)
};

EG_PIXEL Clut8[] = {
// r=F
    _4(FFF) _4(FFC) _4(FF9) _4(FF6) _4(FF3) _4(FF0)   _4(FCF) _4(FCC) _4(FC9) _4(FC6) _4(FC3) _4(FC0) // g=F,C
    _4(F9F) _4(F9C) _4(F99) _4(F96) _4(F93) _4(F90)   _4(F6F) _4(F6C) _4(F69) _4(F66) _4(F63) _4(F60) // g=9,6
    _4(F3F) _4(F3C) _4(F39) _4(F36) _4(F33) _4(F30)   _4(F0F) _4(F0C) _4(F09) _4(F06) _4(F03) _4(F00) // g=3,0
// r=C
    _4(CFF) _4(CFC) _4(CF9) _4(CF6) _4(CF3) _4(CF0)   _4(CCF) _4(CCC) _4(CC9) _4(CC6) _4(CC3) _4(CC0) // g=F,C
    _4(C9F) _4(C9C) _4(C99) _4(C96) _4(C93) _4(C90)   _4(C6F) _4(C6C) _4(C69) _4(C66) _4(C63) _4(C60) // g=9,6
    _4(C3F) _4(C3C) _4(C39) _4(C36) _4(C33) _4(C30)   _4(C0F) _4(C0C) _4(C09) _4(C06) _4(C03) _4(C00) // g=3,0
// r=9
    _4(9FF) _4(9FC) _4(9F9) _4(9F6) _4(9F3) _4(9F0)   _4(9CF) _4(9CC) _4(9C9) _4(9C6) _4(9C3) _4(9C0) // g=F,C
    _4(99F) _4(99C) _4(999) _4(996) _4(993) _4(990)   _4(96F) _4(96C) _4(969) _4(966) _4(963) _4(960) // g=9,6
    _4(93F) _4(93C) _4(939) _4(936) _4(933) _4(930)   _4(90F) _4(90C) _4(909) _4(906) _4(903) _4(900) // g=3,0
// r=6
    _4(6FF) _4(6FC) _4(6F9) _4(6F6) _4(6F3) _4(6F0)   _4(6CF) _4(6CC) _4(6C9) _4(6C6) _4(6C3) _4(6C0) // g=F,C
    _4(69F) _4(69C) _4(699) _4(696) _4(693) _4(690)   _4(66F) _4(66C) _4(669) _4(666) _4(663) _4(660) // g=9,6
    _4(63F) _4(63C) _4(639) _4(636) _4(633) _4(630)   _4(60F) _4(60C) _4(609) _4(606) _4(603) _4(600) // g=3,0
// r=3
    _4(3FF) _4(3FC) _4(3F9) _4(3F6) _4(3F3) _4(3F0)   _4(3CF) _4(3CC) _4(3C9) _4(3C6) _4(3C3) _4(3C0) // g=F,C
    _4(39F) _4(39C) _4(399) _4(396) _4(393) _4(390)   _4(36F) _4(36C) _4(369) _4(366) _4(363) _4(360) // g=9,6
    _4(33F) _4(33C) _4(339) _4(336) _4(333) _4(330)   _4(30F) _4(30C) _4(309) _4(306) _4(303) _4(300) // g=3,0
// r=0
    _4(0FF) _4(0FC) _4(0F9) _4(0F6) _4(0F3) _4(0F0)   _4(0CF) _4(0CC) _4(0C9) _4(0C6) _4(0C3) _4(0C0) // g=F,C
    _4(09F) _4(09C) _4(099) _4(096) _4(093) _4(090)   _4(06F) _4(06C) _4(069) _4(066) _4(063) _4(060) // g=9,6
    _4(03F) _4(03C) _4(039) _4(036) _4(033) _4(030)   _4(00F) _4(00C) _4(009) _4(006) _4(003)         // g=3,0
// b=    F       C       9       6       3       0         F       C       9       6       3       0

// gradients:
    _4(E00) _4(D00) _4(B00) _4(A00) _4(800) _4(700) _4(500) _4(400) _4(200) _4(100) // red
    _4(0E0) _4(0D0) _4(0B0) _4(0A0) _4(080) _4(070) _4(050) _4(040) _4(020) _4(010) // green
    _4(00E) _4(00D) _4(00B) _4(00A) _4(008) _4(007) _4(005) _4(004) _4(002) _4(001) // blue
    _4(EEE) _4(DDD) _4(BBB) _4(AAA) _4(888) _4(777) _4(555) _4(444) _4(222) _4(111) // gray
    _4(000) // black
};

//
// Decompress .icns RLE data
//

VOID egDecompressIcnsRLE (
    IN OUT UINT8 **CompData,
    IN OUT UINTN  *CompLen,
    IN UINT8      *PixelData,
    IN UINTN       PixelCount
) {
    UINT8 *cp;
    UINT8 *cp_end;
    UINT8 *pp;
    UINTN  pp_left;
    UINTN  len, i;
    UINT8  value;

    // setup variables
    cp      = *CompData;
    cp_end  =  cp + *CompLen;
    pp      =  PixelData;
    pp_left =  PixelCount;

    // decode
    while (cp + 1 < cp_end && pp_left > 0) {
        len = *cp++;
        if (len & 0x80) {   // compressed data: repeat next byte
            len -= 125;
            if (len > pp_left)
                break;
            value = *cp++;
            for (i = 0; i < len; i++) {
                *pp = value;
                pp += 4;
            }
        } else {            // uncompressed data: copy bytes
            len++;
            if (len > pp_left || cp + len > cp_end)
                break;
            for (i = 0; i < len; i++) {
                *pp = *cp++;
                pp += 4;
            }
        }
        pp_left -= len;
    }

    if (pp_left > 0) {
        MsgLog ("egDecompressIcnsRLE: still need %d bytes of pixel data\n", pp_left);
    }

    // record what's left of the compressed data stream
    *CompData = cp;
    *CompLen = (UINTN)(cp_end - cp);
}


BOOLEAN ISTYPE(UINT8 *x, CHAR8 *y) {
    return ((x)[0] == (y)[0] && (x)[1] == (y)[1] && (x)[2] == (y)[2] && (x)[3] == (y)[3]);
}


//
// Load Apple .icns icons
//

EG_IMAGE * egDecodeICNS (
    IN UINT8   *FileData,
    IN UINTN    FileDataLength,
    IN UINTN    IconSize,
    IN BOOLEAN  WantAlpha,
    IN UINTN    Level
) {
    LOGPROCENTRY("Level:%d IconSize:%d Type:%.4a Size:0x%x", Level, IconSize, FileData, FileDataLength);
    if (
        !FileData || FileDataLength < 8
        || (ISTYPE(FileData, "icns") && Level > 0) // root nested type
        || (ISTYPE(FileData, "slct") && Level == 0) // nested type "Selected"
        || (ISTYPE(FileData, "sbtp") && Level == 0) // nested type "Template"
        || (ISTYPE(FileData, "tile") && Level == 0) // Resorcerer can create icns resources with this nested type "Tile"
        || (ISTYPE(FileData, "drop") && Level == 0) // Resorcerer can create icns resources with this nested type "Drop"
        || (ISTYPE(FileData, "over") && Level == 0) // Resorcerer can create icns resources with this nested type "Rollover"
        || (ISTYPE(FileData, "open") && Level == 0) // Resorcerer can create icns resources with this nested type "Open"
        || (ISTYPE(FileData, "odrp") && Level == 0) // Resorcerer can create icns resources with this nested type "OpenDrop"
        || (ISTYPE(FileData, "\xFD\xD9\x2F\xA8") && Level == 0) // nested type "Dark Mode"
    ) {
        // not an icns file
        LOGPROCEXIT("(null input)");
        return NULL;
    }

    EG_IMAGE *NewImage = NULL;

    UINTN SizesToTry[MAX_ICNS_SIZES];
    UINTN NumSizesToTry;

    if (Level > 0) {
        SizesToTry[0] = IconSize;
        NumSizesToTry = 1;
    }
    else {
        INTN i, j, k;
        NumSizesToTry = MAX_ICNS_SIZES;
        for (i = 0; i < NumSizesToTry && IconSizes[i] >= IconSize; i++);
        for (k = 0, j = i - 1; j >= 0; j--, k++) SizesToTry[k] = IconSizes[j]; // start with icons that are greater than or equal to requested icon size
        for (j = i; j < NumSizesToTry; j++, k++) SizesToTry[k] = IconSizes[j]; // then do icons that are smaller in size
    }

    UINTN SizeToTry = 0;
    //LOGBLOCKENTRY("while SizeToTry < %d", NumSizesToTry);
    while (!NewImage && SizeToTry < NumSizesToTry) {
        IconSize = SizesToTry[SizeToTry++];
        //LOGBLOCKENTRY("SizeToTry:%d", IconSize);

        UINTN Width = (IconSize == 12) ? 16 : IconSize;
        UINTN PixelCount = Width * IconSize;

        //LOGBLOCKENTRY("while DoNested");
        UINTN DoNested = 0;
        while (!NewImage && DoNested < 2) {
            //LOGBLOCKENTRY("DoNested:%d", DoNested);
            UINTN     i;
            UINT8    *SrcPtr;
            EG_PIXEL *DestPtr;

            UINT8 *Ptr = FileData + 8;
            UINT8 *BufferEnd = FileData + FileDataLength;

            #define ONEFORMAT(_format) UINT8 * _format ## Ptr = NULL; UINTN _format ## Len = 0
            ONEFORMAT(Data);                //  RGB
            ONEFORMAT(Mask);                // 8 bit mask
            ONEFORMAT(APic);                // ARGB, JPEG, PNG
            ONEFORMAT(DPic);                //  RGB, JPEG, PNG
            ONEFORMAT(NPic);                //       JPEG, PNG
            ONEFORMAT(RPic);                //       JPEG, PNG      (Retina)
            ONEFORMAT(Bit8);                // 8 bit color
            ONEFORMAT(Bit4);                // 4 bit color
            ONEFORMAT(Bit1);                // 1 bit B&W and mask
            ONEFORMAT(Mono);                // 1 bit B&W no mask
            BOOLEAN IncludesAlpha = FALSE; // for ARGB
            BOOLEAN Use1BitAlpha  = FALSE; // for 8 bit, 4 bit, 1 bit
            BOOLEAN MakeAlpha     = FALSE; // for mono
            BOOLEAN AddedAlpha    = FALSE;

            //LOGBLOCKENTRY("while Ptr < 0x%x", BufferEnd - FileData);
            // iterate over tagged blocks in the file
            while (!NewImage && Ptr + 8 <= BufferEnd) {
                UINT32 BlockLen = ((UINT32)Ptr[4] << 24) + ((UINT32)Ptr[5] << 16) + ((UINT32)Ptr[6] << 8) + (UINT32)Ptr[7];
                //LOGBLOCKENTRY("Ptr:0x%x Len:0x%x", Ptr - FileData, BlockLen);

                if (BlockLen < 8 || Ptr + BlockLen > BufferEnd) {
                    // block continues beyond end of file
                    //LOGBLOCKEXIT("Ptr (invalid)");
                    break;
                }

                if (DoNested) {
                     if (
                           ISTYPE(Ptr, "icns")
                        || ISTYPE(Ptr, "slct")
                        || ISTYPE(Ptr, "sbtp")
                        || ISTYPE(Ptr, "tile")
                        || ISTYPE(Ptr, "drop")
                        || ISTYPE(Ptr, "over")
                        || ISTYPE(Ptr, "open")
                        || ISTYPE(Ptr, "odrp")
                        || ISTYPE(Ptr, "\xFD\xD9\x2F\xA8")
                    ) {
                        NewImage = egDecodeICNS(Ptr, BlockLen, IconSize, WantAlpha, Level + 1);
                    }
                } // nested block
                else {
                    UINT8 *p = Ptr + 8; UINTN len = BlockLen - 8;
                    #define ONEICON(_type, _format, ...) if (ISTYPE(Ptr, _type)) { _format ## Ptr = p + 0 ##__VA_ARGS__; _format ## Len = len - 0 ##__VA_ARGS__; MsgLog("IconType:%.4a Len:0x%x\n", Ptr, len); }

                           if (IconSize == 1024) { ONEICON("ic10", RPic)
                    } else if (IconSize ==  512) { ONEICON("ic14", RPic)
                                              else ONEICON("ic09", NPic)
                    } else if (IconSize ==  256) { ONEICON("ic13", RPic)
                                              else ONEICON("ic08", NPic)
                    } else if (IconSize ==  128) { ONEICON("it32", Data, 4)
                                              else ONEICON("t8mk", Mask)
                                              else ONEICON("ic07", NPic)
                    } else if (IconSize ==   64) { ONEICON("ic12", RPic)
                    } else if (IconSize ==   48) { ONEICON("ih32", Data)
                                              else ONEICON("h8mk", Mask)
                                              else ONEICON("SB24", RPic)
                                              else ONEICON("icp6", NPic)
                                              else ONEICON("ich8", Bit8)
                                              else ONEICON("ich4", Bit4)
                                              else ONEICON("ich#", Bit1)
                    } else if (IconSize ==   36) { ONEICON("icsB", RPic)
                    } else if (IconSize ==   32) { ONEICON("il32", Data)
                                              else ONEICON("l8mk", Mask)
                                              else ONEICON("ic11", RPic)
                                              else ONEICON("icp5", DPic)
                                              else ONEICON("ic05", APic) // this is also Retina
                                              else ONEICON("icl8", Bit8)
                                              else ONEICON("icl4", Bit4)
                                              else ONEICON("ICN#", Bit1)
                                              else ONEICON("ICON", Mono)
                    } else if (IconSize ==   24) { ONEICON("sb24", NPic)
                    } else if (IconSize ==   18) { ONEICON("icsb", APic)
                    } else if (IconSize ==   16) { ONEICON("is32", Data)
                                              else ONEICON("s8mk", Mask)
                                              else ONEICON("icp4", DPic)
                                              else ONEICON("ic04", APic)
                                              else ONEICON("ics8", Bit8)
                                              else ONEICON("ics4", Bit4) // "kcs4",
                                              else ONEICON("ics#", Bit1) // "ksc#", "SICN", "CURS" (first 64 bytes)
                    } else if (IconSize ==   12) { ONEICON("icm8", Bit8)
                                              else ONEICON("icm4", Bit4)
                                              else ONEICON("icm#", Bit1)
                    } else if (IconSize ==    8) { ONEICON("PAT ", Mono) // added this one for fun (not really an icns option)
                    }
                } // not nested block

                //LOGBLOCKEXIT("Ptr");
                Ptr += BlockLen;
            } // while blocks
            //LOGBLOCKEXIT("while Ptr");

            if (!NewImage && (!DataPtr || !MaskPtr)) { // first priority is RGB with mask - if they don't exist then try other 24 bit options
                if (APicPtr && ISTYPE(APicPtr, "ARGB")) { // second priority is ARGB
                    MsgLog("Doing APic\n");
                    DataPtr = APicPtr + 4;
                    DataLen = APicLen - 4;
                    IncludesAlpha = TRUE;
                }
                else { // third priority is PNG or JPEG
                    #define ONEPICTYPE(_format) if (!NewImage && _format ## Ptr) NewImage = egDecodeAny(_format ## Ptr, _format ## Len, IconSize, WantAlpha);
                    ONEPICTYPE(APic)
                    ONEPICTYPE(DPic)
                    ONEPICTYPE(NPic)
                    ONEPICTYPE(RPic)

                    if (NewImage) {
                        // assume JPEG and PNG have alpha
                        // (even though nanojpeg doesn't support transparency - it also doesn't support JPEG 2000 which is what Apple icons use)
                        AddedAlpha = TRUE;
                    }
                    else if (DPicPtr) {
                        if (CompareMem ("\0\0\0\fjP  \r\n\x87\n", DPicPtr, 12) == 0) {
                            // don't want to try RGB data if it looks like jpg
                            MsgLog("Failed to decode jpeg for DPic\n");
                        }
                        else if (!DataPtr) { // for this type, if it's not a PNG or JPEG then it's RGB
                            MsgLog("Doing DPic as Data\n");
                            DataPtr = DPicPtr;
                            DataLen = DPicLen;
                        }
                    }
                }
            }

            if (!NewImage && DataPtr) { // try ARGB & RGB types
                MsgLog("Doing Data\n");
                NewImage = egCreateImage (Width, IconSize, WantAlpha);
                if (!NewImage) {
                    return NULL;
                }

                if (DataLen < PixelCount * (IncludesAlpha ? 4 : 3)) {
                    // pixel data is compressed, RGB planar
                    UINT8 *CompData = DataPtr;
                    UINTN CompLen  = DataLen;
                    if (IncludesAlpha) {
                        egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, a), PixelCount);
                    }
                    egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, r), PixelCount);
                    egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, g), PixelCount);
                    egDecompressIcnsRLE (&CompData, &CompLen, PLPTR(NewImage, b), PixelCount);
                    // possible assertion: CompLen == 0
                    if (CompLen > 0) {
                        MsgLog ("egLoadICNSIcon: %d bytes of compressed data left\n", CompLen);
                        MsgLog ("Ignoring this icon\n");
                        egFreeImage (NewImage);
                        NewImage = NULL;
                    }
                }
                else {
                    // pixel data is uncompressed, RGB interleaved
                    SrcPtr  = DataPtr;
                    DestPtr = NewImage->PixelData;
                    for (i = 0; i < PixelCount; i++, DestPtr++) {
                        if (IncludesAlpha) {
                            DestPtr->a = *SrcPtr++;
                        }
                        DestPtr->r = *SrcPtr++;
                        DestPtr->g = *SrcPtr++;
                        DestPtr->b = *SrcPtr++;
                    }
                }
                AddedAlpha = IncludesAlpha;

                if (WantAlpha && !AddedAlpha && MaskPtr && MaskLen >= PixelCount) {
                    // Add Alpha Mask if Required, Available, and Valid
                    egInsertPlane (MaskPtr, PLPTR(NewImage, a), PixelCount);
                    AddedAlpha = TRUE;
                }
            }

            if (!NewImage) { // try clut types
                //MsgLog("Trying Clut\n");
                EG_PIXEL *Clut;
                UINTN BitShift;
                SrcPtr = NULL;
                     if (Bit8Ptr && Bit8Len >= PixelCount    ) { BitShift = 0; Clut = Clut8; Use1BitAlpha = TRUE; SrcPtr = Bit8Ptr; } // 1st clut priority is 8 bit
                else if (Bit4Ptr && Bit4Len >= PixelCount / 2) { BitShift = 1; Clut = Clut4; Use1BitAlpha = TRUE; SrcPtr = Bit4Ptr; } // 2nd clut priority is 4 bit
                else if (Bit1Ptr && Bit1Len >= PixelCount / 8) { BitShift = 3; Clut = Clut1; Use1BitAlpha = TRUE; SrcPtr = Bit1Ptr; } // 3rd clut priority is 1 bit with mask
                else if (MonoPtr && MonoLen >= PixelCount / 8) { BitShift = 3; Clut = Clut1; MakeAlpha    = TRUE; SrcPtr = MonoPtr; } // 4th clut priority is mono with no mask

                if (SrcPtr) {
                    MsgLog("Doing Clut\n");
                    NewImage = egCreateImage (Width, IconSize, WantAlpha);
                    if (!NewImage) {
                        return NULL;
                    }

                    UINTN revshift = 3 - BitShift;
                    UINTN pixelsperbyte = 1 << BitShift;
                    UINTN bitsperpixel = 1 << revshift;

                    UINTN pixelindexmask = pixelsperbyte - 1;
                    UINTN pixelvaluemask = (1 << bitsperpixel) - 1;
                    UINTN colorindex;

                    DestPtr = NewImage->PixelData;
                    for (i = 0; i < PixelCount; i++, DestPtr++) {
                        colorindex = (SrcPtr[i >> BitShift] >> (((~i) & pixelindexmask) << revshift)) & pixelvaluemask;
                        DestPtr->r = Clut[colorindex].r;
                        DestPtr->g = Clut[colorindex].g;
                        DestPtr->b = Clut[colorindex].b;
                    }

                    if (WantAlpha && Use1BitAlpha && Bit1Ptr && Bit1Len >= PixelCount * 2 / 8) {
                        SrcPtr = Bit1Ptr + PixelCount / 8;
                        DestPtr = NewImage->PixelData;
                        for (i = 0; i < PixelCount; i++, DestPtr++) {
                            DestPtr->a = ((SrcPtr[i >> 3] >> ((~i) & 7)) & 1) ? 255 : 0;
                        }
                        AddedAlpha = TRUE;
                    }
                }
            }

            if (NewImage) {
                if (WantAlpha) {
                    if (!AddedAlpha) {
                        // There is no Alpha in the icon.
                        if (MakeAlpha) {
                            egSetPlane (PLPTR(NewImage, a), 0xff, PixelCount); // set all pixels to opaque
                            EG_PIXEL TestColor = { 0xff, 0xff, 0xff, 0 }; // search for white pixels
                            EG_PIXEL TestMask  = { 0xff, 0xff, 0xff, 0 }; // regardless of alpha
                            EG_PIXEL FillColor = { 0x00, 0x00, 0x00, 0 }; // set alpha of white pixels to transparent
                            EG_PIXEL FillMask  = { 0x00, 0x00, 0x00, 255 }; // without affecting the color
                            egSeedFillImage (NewImage, -1, -1, &FillColor, &FillMask, &TestColor, &TestMask, FALSE, TRUE); // add extra pixel in case the icon stretches from one edge to the other
                        }
                        else {
                            // If we want an image with Alpha, then set Alpha to Opaque (255)
                            egSetPlane (PLPTR(NewImage, a), 255, PixelCount);
                        }
                    }
                }
                else {
                    // Set the unused bytes to zero.
                    egSetPlane (PLPTR(NewImage, a), 0, PixelCount);
                }
            }

            //LOGBLOCKEXIT("DoNested");
            DoNested++;
        } // while normal blocks, nested blocks
        //LOGBLOCKEXIT("while DoNested");

        //LOGBLOCKEXIT("SizeToTry");
    } // while sizes
    //LOGBLOCKEXIT("while SizeToTry");

    // FUTURE: scale to originally requested size if we had to load another size

    LOGPROCEXIT("Image:%p %dx%d", NewImage, NewImage ? NewImage->Width : -1, NewImage ? NewImage->Height : -1);
    return NewImage;
} // EG_IMAGE * egDecodeICNS()

/* EOF */
