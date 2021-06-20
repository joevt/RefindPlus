/** @file
    Provides simple log services to memory buffer.
**/

#ifndef __MEMLOG_LIB_H__
#define __MEMLOG_LIB_H__


//
// Mem log sizes
//
#define MEM_LOG_INITIAL_SIZE    (128 * 1024)
#define MEM_LOG_MAX_SIZE        (10 * 1024 * 1024)
#define MEM_LOG_MAX_LINE_SIZE   1024


/** Callback that can be installed to be called when some message is printed with MemLog() or MemLogVA(). **/
typedef UINTN (EFIAPI *MEM_LOG_CALLBACK) (IN INTN DebugMode, IN CHAR8 *LastMessage);

/**
  Prints a log message to memory buffer.
 
  @param  Timing      TRUE to prepend timing to log.
  @param  DebugMode   DebugMode will be passed to Callback function if it is set.
  @param  Format      The format string for the debug message to print.
  @param  Marker      VA_LIST with variable arguments for Format.
 
**/
VOID
EFIAPI
MemLogVA (
  IN  CONST BOOLEAN Timing,
  IN  CONST INTN    DebugMode,
  IN  CONST CHAR8   *Format,
  IN  VA_LIST       Marker
  );

/**
  Prints a log message to memory buffer.

  If Format is NULL, then does nothing.

  @param  Timing      TRUE to prepend timing to log.
  @param  DebugMode   DebugMode will be passed to Callback function if it is set.
  @param  Format      The format string for the debug message to print.
  @param  ...         The variable argument list whose contents are accessed 
                      based on the format string specified by Format.

**/
VOID
EFIAPI
MemLog (
  IN  CONST BOOLEAN Timing,
  IN  CONST INTN    DebugMode,
  IN  CONST CHAR8   *Format,
  ...
  );


/**
  Returns pointer to MemLog buffer.
**/
CHAR8*
EFIAPI
GetMemLogBuffer (
  VOID
  );


/**
  Returns the length of log (number of chars written) in mem buffer.
 **/
UINTN
EFIAPI
GetMemLogLen (
  VOID
  );


/**
  Sets callback that will be called when message is added to mem log.
  Returns a callback index.
 **/
INTN
EFIAPI
SetMemLogCallback (
  MEM_LOG_CALLBACK  Callback
  );


/**
  Check if a callback is paused.
 **/
BOOLEAN
MemLogCallbackIsPaused (
  INTN CallbackIndex
);


/**
  Pauses a callback - messages are still added to the mem log.
 **/
INTN
PauseMemLogCallback (
  INTN CallbackIndex
);


/**
  Resumes a callback - messages that were added to the mem log will be output after the next message is added.
 **/
VOID
ResumeMemLogCallback (
  INTN CallbackIndex
);


/**
  Returns TSC ticks per second.
 **/
UINT64
EFIAPI
GetMemLogTscTicksPerSecond (VOID);


#endif // __MEMLOG_LIB_H__
