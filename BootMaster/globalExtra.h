#define LOG_BLANK_LINE_SEP   0
#define LOG_LINE_SPECIAL     1
#define LOG_LINE_SAME        2
#define LOG_LINE_NORMAL      3
#define LOG_LINE_SEPARATOR   4
#define LOG_LINE_THIN_SEP    5
#define LOG_STAR_SEPARATOR   6
#define LOG_LINE_DASH_SEP    7
#define LOG_THREE_STAR_SEP   8
#define LOG_THREE_STAR_MID   9
#define LOG_THREE_STAR_END   10
#define LOG_STAR_HEAD_SEP    11
#define LOG_LINE_FORENSIC    12

VOID DebugLog (
    IN        INTN  DebugMode,
    IN  const CHAR8 *FormatString, ...
);

#if REFIT_DEBUG == 0
    #define DONTMSG(DebugMode, Msg) TRUE
#else
    #define DONTMSG(DebugMode, Msg)    (DebugMode < 1 || !Msg || GlobalConfig.LogLevel < 0 /* || (!UseMsgLog && !NativeLogger && GlobalConfig.LogLevel > 0) */)
#endif
#define DONTLOG(DebugMode, level, Msg) (DebugMode < 1 || !Msg || GlobalConfig.LogLevel < level || GlobalConfig.LogLevel < 1 /* || NativeLogger */ || MuteLogger)

VOID DeepLoggger (
    IN  INTN     DebugMode,
    IN  INTN     level,
    IN  INTN     type,
    IN  CHAR16 **Msg
);

#if REFIT_DEBUG < 1
#   define LOG(...)
#   define MsgLog(...)
#else
#   define MsgLog(...)  DebugLog(REFIT_DEBUG, __VA_ARGS__)

    // NB: '_logTemp' is handled in 'DeepLoggger'
#   define LOG(level, type, ...)                              \
        do {                                                  \
            CHAR16 *_logTemp = PoolPrint(__VA_ARGS__);        \
            DeepLoggger(REFIT_DEBUG, level, type, &_logTemp); \
        } while (FALSE)

// #define MsgLog(f,...) AsciiPrint(f, ##__VA_ARGS__)
// #define LOG(level, type, f, ...) Print(f L"\n", ##__VA_ARGS__)
#endif

#if REFIT_DEBUG < 1

#define LOG4(_v, _l, _p, _x, _p2, _x2, ...)

#else

#define LOG4(_v, _l, _p, _x, _p2, _x2, ...) \
    do { \
        CHAR16 *_MsgStr = PoolPrint (__VA_ARGS__); \
        BOOLEAN _dm = DONTMSG(REFIT_DEBUG, _MsgStr); \
        BOOLEAN _dl = DONTLOG(REFIT_DEBUG, _v, _MsgStr); \
        if (!_dm && !_dl) { \
            _dl = (_l == LOG_LINE_NORMAL); \
            _dm = !_dl; \
        } \
        if (_dm) LOG(_v, _l, L"%s%s%s", _p2, _MsgStr, _x2); \
        if (_dl) MsgLog ("%s%s%s", _p, _MsgStr, _x); \
        MY_FREE_POOL(_MsgStr); \
    } while(0)

#endif // #if REFIT_DEBUG > 0

#define LOG3(_v, _l, _p, _x, _p2, ...) \
    LOG4(_v, _l, _p, _x, _p2, L"", __VA_ARGS__)

#define LOG2(_v, _l, _p, _x, ...) \
    LOG4(_v, _l, _p, _x, L"", L"", __VA_ARGS__)

#define LOGWHERE(format, ...) \
    MsgLog("%a:%d " format "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define LOGPOOL(object) \
    LogPoolProc((VOID *)object, (VOID **)&object, #object, NULL, NULL, NULL, __FILE__, __LINE__, FALSE, TRUE)
#define LOGPOOLALWAYS(object) \
    LogPoolProc((VOID *)object, (VOID **)&object, #object, NULL, NULL, NULL, __FILE__, __LINE__, TRUE, TRUE)

//==================================
// Log indenting functions

#define LOGBLOCK0(  _i, _f, ...) MsgLog (_i " " _f "\n", ##__VA_ARGS__)

#define LOGBLOCKENTRY(_f, ...) LOGBLOCK0 ("[" , _f, ##__VA_ARGS__)
#define LOGBLOCKEXIT( _f, ...) LOGBLOCK0 ("]" , _f, ##__VA_ARGS__)
#define LOGBLOCK(     _f, ...) LOGBLOCK0 ("[]", _f, ##__VA_ARGS__)

#define LOGPROCENTRY(_f, ...) LOGBLOCKENTRY("%a%a" _f, __FUNCTION__, (sizeof("" _f) > 1) ? " " : "", ##__VA_ARGS__)
#define LOGPROCEXIT( _f, ...) LOGBLOCKEXIT ("%a%a" _f, __FUNCTION__, (sizeof("" _f) > 1) ? " " : "", ##__VA_ARGS__)
#define LOGPROC(     _f, ...) LOGBLOCK     ("%a%a" _f, __FUNCTION__, (sizeof("" _f) > 1) ? " " : "", ##__VA_ARGS__)

//==================================
