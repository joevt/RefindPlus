/*
 *  BootLog.h
 *
 *
 *  Created by Slice on 19.08.11.
 *  Edited by apianti 2012-09-08
 *  Initial idea from Kabyl
 */


#ifndef _BOOTLOG_H_
#define _BOOTLOG_H_

#include "../include/tiano_includes.h"


BOOLEAN
BootLogIsPaused (
);


INTN
BootLogPause (
);


VOID
BootLogResume (
);


VOID
EFIAPI
DeepLoggger (
    IN INTN     DebugMode,
    IN INTN     level,
    IN INTN     type,
    IN CHAR16 **Message
);


VOID
EFIAPI
DebugLog (
    IN INTN DebugMode,
    IN CONST CHAR8 *FormatString, ...
);


VOID InitBooterLog(
    VOID
);

#endif
