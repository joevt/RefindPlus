/*
 *  BootLog.c
 *
 *
 *  Created by Slice on 19.08.11.
 *  Edited by apianti 2012-09-08
 *  Initial idea from Kabyl
 */

#include "../include/tiano_includes.h"
#include "../Library/MemLogLib/MemLogLib.h"
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include <Guid/FileInfo.h>
#include <Library/UefiBootServicesTableLib.h>
#include "../BootMaster/global.h"
#include "../BootMaster/lib.h"
#include "../BootMaster/mystrings.h"
#include "../BootMaster/leaks.h"

extern  EFI_GUID  gEfiMiscSubClassGuid;

extern  INT16  NowYear;
extern  INT16  NowMonth;
extern  INT16  NowDay;
extern  INT16  NowHour;
extern  INT16  NowMinute;
extern  INT16  NowSecond;

BOOLEAN  TimeStamp             = TRUE;
BOOLEAN  UseMsgLog             = FALSE;


CHAR16 * GetAltMonth (
    VOID
) {
    CHAR16  *AltMonth = NULL;

    switch (NowMonth) {
        case 1:
        AltMonth = L"b";
            break;
        case 2:
        AltMonth = L"d";
            break;
        case 3:
        AltMonth = L"f";
            break;
        case 4:
        AltMonth = L"h";
            break;
        case 5:
        AltMonth = L"j";
            break;
        case 6:
        AltMonth = L"k";
            break;
        case 7:
        AltMonth = L"n";
            break;
        case 8:
        AltMonth = L"p";
            break;
        case 9:
        AltMonth = L"r";
            break;
        case 10:
        AltMonth = L"t";
            break;
        case 11:
        AltMonth = L"v";
            break;
        default:
        AltMonth = L"x";
    } // switch NowMonth

    return AltMonth;
}


CHAR16 * GetAltHour (
    VOID
) {
    CHAR16  *AltHour = NULL;

    switch (NowHour) {
        case 0:
        AltHour = L"a";
            break;
        case 1:
        AltHour = L"b";
            break;
        case 2:
        AltHour = L"c";
            break;
        case 3:
        AltHour = L"d";
            break;
        case 4:
        AltHour = L"e";
            break;
        case 5:
        AltHour = L"f";
            break;
        case 6:
        AltHour = L"g";
            break;
        case 7:
        AltHour = L"h";
            break;
        case 8:
        AltHour = L"i";
            break;
        case 9:
        AltHour = L"j";
            break;
        case 10:
        AltHour = L"k";
            break;
        case 11:
        AltHour = L"m";
            break;
        case 12:
        AltHour = L"n";
            break;
        case 13:
        AltHour = L"p";
            break;
        case 14:
        AltHour = L"q";
            break;
        case 15:
        AltHour = L"r";
            break;
        case 16:
        AltHour = L"s";
            break;
        case 17:
        AltHour = L"t";
            break;
        case 18:
        AltHour = L"u";
            break;
        case 19:
        AltHour = L"v";
            break;
        case 20:
        AltHour = L"w";
            break;
        case 21:
        AltHour = L"x";
            break;
        case 22:
        AltHour = L"y";
            break;
        default:
        AltHour = L"z";
    } // switch NowHour

    return AltHour;
}

CHAR16 * GetDateString (
    VOID
) {
    static CHAR16  *DateStr = NULL;

    if (DateStr != NULL) {
        return DateStr;
    }

    INT16   ourYear    = (NowYear % 100);
    CHAR16  *ourMonth  = GetAltMonth();
    CHAR16  *ourHour   = GetAltHour();
    DateStr = PoolPrint(
        L"%02d%s%02d%s%02d%02d",
        ourYear,
        ourMonth,
        NowDay,
        ourHour,
        NowMinute,
        NowSecond
    );
    LEAKABLE (DateStr, "DateStr");

    return DateStr;
}


EFI_FILE_PROTOCOL * GetDebugLogFile (
    VOID
) {
  EFI_STATUS          Status;
  EFI_LOADED_IMAGE    *LoadedImage;
  EFI_FILE_PROTOCOL   *RootDir;
  EFI_FILE_PROTOCOL   *LogFile;
  CHAR16              *ourDebugLog = NULL;

  // get RootDir from device we are loaded from
  Status = gBS->HandleProtocol(
      gImageHandle,
      &gEfiLoadedImageProtocolGuid,
      (VOID **) &LoadedImage
  );
  if (EFI_ERROR (Status)) {
    return NULL;
  }
  RootDir = EfiLibOpenRoot(LoadedImage->DeviceHandle);
  if (RootDir == NULL) {
    return NULL;
  }

  CHAR16 *DateStr = GetDateString();
  ourDebugLog = PoolPrint(
      L"EFI\\%s.log",
      DateStr
  );

  // Open log file from current root
  Status = RootDir->Open(
      RootDir,
      &LogFile,
      ourDebugLog,
      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
      0
  );

  // If the log file is not found try to create it
  if (Status == EFI_NOT_FOUND) {
    Status = RootDir->Open(
        RootDir,
        &LogFile,
        ourDebugLog,
        EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
        0
    );
  }
  RootDir->Close(RootDir);
  RootDir = NULL;

  if (EFI_ERROR (Status)) {
    // try on first EFI partition
    Status = egFindESP(&RootDir);
    if (!EFI_ERROR (Status)) {
      Status = RootDir->Open(
          RootDir,
          &LogFile,
          ourDebugLog,
          EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
          0
      );
      // If the log file is not found try to create it
      if (Status == EFI_NOT_FOUND) {
        Status = RootDir->Open(
            RootDir,
            &LogFile,
            ourDebugLog,
            EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
            0
        );
      }
      RootDir->Close(RootDir);
      RootDir = NULL;
    }
  }

  if (EFI_ERROR (Status)) {
    LogFile = NULL;
  }

  MyFreePool (&ourDebugLog);

  return LogFile;
}


VOID SaveMessageToDebugLogFile (IN CHAR8 *LastMessage) {
  static BOOLEAN           FirstTimeSave = TRUE;
  CHAR8                   *MemLogBuffer;
  UINTN                    MemLogLen;
  CHAR8                   *Text;
  UINTN                    TextLen;
  EFI_FILE_HANDLE          LogFile;

  MemLogBuffer = GetMemLogBuffer();
  MemLogLen    = GetMemLogLen();
  Text         = LastMessage;
  TextLen      = AsciiStrLen(LastMessage);
  LogFile      = GetDebugLogFile();

  // Write to the log file
  if (LogFile != NULL) {
    // Advance to the EOF so we append
    EFI_FILE_INFO *Info = EfiLibFileInfo(LogFile);
    if (Info) {
      LogFile->SetPosition(LogFile, Info->FileSize);
      // Write out whole log if we have not had root before this
      if (FirstTimeSave) {
        Text          = MemLogBuffer;
        TextLen       = MemLogLen;
        FirstTimeSave = FALSE;
      }
      // Write out this message
      LogFile->Write(LogFile, &TextLen, Text);
      MyFreePool (&Info);
    }
    LogFile->Close(LogFile);
  }
}


VOID
EFIAPI
MemLogCallback (
    IN INTN DebugMode,
    IN CHAR8 *LastMessage
) {
    // Print message to console
    if (DebugMode >= 2) {
        AsciiPrint (LastMessage);
    }

    if ( (DebugMode >= 1) ) {
        SaveMessageToDebugLogFile (LastMessage);
    }
}


#if REFIT_DEBUG > 0

VOID
EFIAPI
DeepLoggger (
    IN INTN     DebugMode,
    IN INTN     level,
    IN INTN     type,
    IN CHAR16 **Message
) {
    CHAR8   FormatString[255];
    CHAR16 *FinalMessage = NULL;

#if REFIT_DEBUG < 1
    // FreePool and return in RELEASE builds
    MyFreePool (Message);

    return;
#endif

    // Make sure we are able to write
    if ( DONTLOG(DebugMode, level) ||
        !(*Message)
    ) {
        MyFreePool (Message);

        return;
    }

    // Disable Timestamp
    TimeStamp = FALSE;

    switch (type) {
        case LOG_BLANK_LINE_SEP:
            FinalMessage = StrDuplicate (L"\n");
            break;
        case LOG_STAR_HEAD_SEP:
            FinalMessage = PoolPrint (L"\n              ***[ %s ]\n", *Message);
            break;
        case LOG_STAR_SEPARATOR:
            FinalMessage = PoolPrint (L"\n* ** ** *** *** ***[ %s ]*** *** *** ** ** *\n\n", *Message);
            break;
        case LOG_LINE_SEPARATOR:
            FinalMessage = PoolPrint (L"\n===================[ %s ]===================\n", *Message);
            break;
        case LOG_LINE_THIN_SEP:
            FinalMessage = PoolPrint (L"\n-------------------[ %s ]-------------------\n", *Message);
            break;
        case LOG_LINE_DASH_SEP:
            FinalMessage = PoolPrint (L"\n- - - - - - - - - -[ %s ]- - - - - - - - - -\n", *Message);
            break;
        case LOG_THREE_STAR_SEP:
            FinalMessage = PoolPrint (L"\n. . . . . . . . ***[ %s ]*** . . . . . . . .\n", *Message);
            break;
        case LOG_THREE_STAR_MID:
            FinalMessage = PoolPrint (L"              ***[ %s ]\n", *Message);
            break;
        case LOG_THREE_STAR_END:
            FinalMessage = PoolPrint (L"              ***[ %s ]***\n\n", *Message);
            break;
        default:
            // Normally 'LOG_LINE_NORMAL', but use this default to also catch coding errors
            FinalMessage = PoolPrint (L"%s\n", *Message);

            // Also Enable Timestamp
            TimeStamp = TRUE;
    } // switch

    if (FinalMessage) {
        // Use Native Logging
        UseMsgLog = TRUE;
        // Convert Unicode Message String to Ascii
        MyUnicodeStrToAsciiStr (FinalMessage, FormatString);
        // Write the Message String
        DebugLog (DebugMode, (CONST CHAR8 *) FormatString);
        // Disable Native Logging
        UseMsgLog = FALSE;
    }

    MyFreePool (Message);
    MyFreePool (&FinalMessage);
}


VOID
EFIAPI
DebugLog(
    IN INTN DebugMode,
    IN CONST CHAR8 *FormatString, ...
) {
    VA_LIST Marker;

    // Make sure the buffer is intact for writing
    if (FormatString == NULL) {
      return;
    }

    // Abort on higher log levels if not forcing
    if DONTMSG(DebugMode) {
      return;
    }

    // Print message to log buffer
    VA_START(Marker, FormatString);
    MemLogVA(TimeStamp, DebugMode, FormatString, Marker);
    VA_END(Marker);

    TimeStamp = TRUE;
}

#endif

VOID InitBooterLog(
    VOID
) {
  SetMemLogCallback(MemLogCallback);
}
