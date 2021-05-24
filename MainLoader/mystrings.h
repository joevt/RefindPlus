/*
 * refit/mystrings.h
 * String functions header file
 *
 * Copyright (c) 2012-2020 Roderick W. Smith
 *
 * Distributed under the terms of the GNU General Public License (GPL)
 * version 3 (GPLv3), or (at your option) any later version.
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

#ifndef __MYSTRINGS_H_
#define __MYSTRINGS_H_

#ifdef __MAKEWITH_GNUEFI
#include <efi.h>
#include <efilib.h>
#else
#include "../include/tiano_includes.h"
#endif
#include "../EfiLib/GenericBdsLib.h"

typedef struct _string_list {
    CHAR16               *Value;
    struct _string_list  *Next;
} STRING_LIST;

// DA_TAG: See here for more if needed:
//         https://www.virtualbox.org/svn/vbox/trunk/src/VBox/Devices/EFI/Firmware/MdePkg/Library/BaseLib/String.c
BOOLEAN StriSubCmp(IN CHAR16 *TargetStr, IN CHAR16 *BigStr);
BOOLEAN MyStriCmp(IN CONST CHAR16 *String1, IN CONST CHAR16 *String2);
BOOLEAN LimitStringLength(CHAR16 *TheString, UINTN Limit);
BOOLEAN DeleteItemFromCsvList(CHAR16 *ToDelete, CHAR16 *List);
BOOLEAN IsIn(IN CHAR16 *SmallString, IN CHAR16 *List);
BOOLEAN IsInSubstring(IN CHAR16 *BigString, IN CHAR16 *List);
BOOLEAN IsValidHex(CHAR16 *Input);
BOOLEAN IsGuid(CHAR16 *UnknownString);
BOOLEAN ReplaceSubstring(
    IN OUT CHAR16 **MainString,
    IN     CHAR16 *SearchString,
    IN     CHAR16 *ReplString
);

CHAR16 *GetTimeString(VOID);
CHAR16 *MyStrStr (IN CHAR16 *String, IN CHAR16 *StrCharSet);
CHAR16 *FindNumbers(IN CHAR16 *InString);
CHAR16 * GuidAsString(EFI_GUID *GuidData);
CHAR16 *FindCommaDelimited(IN CHAR16 *InString, IN UINTN Index);

CHAR8 *MyAsciiStrStr (IN CONST CHAR8 *String, IN CONST CHAR8 *SearchString);

VOID ToLower(CHAR16 * MyString);
CHAR16 * MergeStringsNew(IN CHAR16 *First, IN CHAR16 *Second, CHAR16 AddChar);
VOID MergeStrings(IN OUT CHAR16 **First, IN CHAR16 *Second, CHAR16 AddChar);
VOID MergeWords(CHAR16 **MergeTo, CHAR16 *InString, CHAR16 AddChar);
VOID MyUnicodeStrToAsciiStr (
    IN  CHAR16 *StrCHAR16,
    OUT CHAR8  ArrCHAR8[255]
);

UINTN NumCharsInCommon(IN CHAR16* String1, IN CHAR16* String2);

UINT64 StrToHex(CHAR16 *Input, UINTN Position, UINTN NumChars);

EFI_GUID StringAsGuid(CHAR16 * InString);

VOID DeleteStringList(STRING_LIST *StringList);

#endif
