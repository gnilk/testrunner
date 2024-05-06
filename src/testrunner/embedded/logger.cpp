/*-------------------------------------------------------------------------
 File    : $Archive: $
 Author  : $Author: $
 Version : $Revision: $
 Orginal : 2006-07-26, 15:50
 Descr   : Tiny Log4Net look-alike for C++ 


 Part of testrunner
 BSD3 License!

 Layout can be changed in: Logger::WriteReportString
 
 Comparision in output is '>=' means, setting debug level filtering to 
 INFO gives INFO and everything above..
 
 Levels:
   0..99  - none,
 100..199 - debug,
 200..299 - info,
 300..399 - warning
 400..499 - error
 500..599 - crtical
 600..X   - custom
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
   ! Level filtering [sink levels]
   ! Global properties [early filtering]
   ! Introduce more debug levels to reduce noise
   - Support for library exclusion/inclusion lists
   ! Rolling file appender would be nice!
   - Support for threading
   - Unicode support...
   - Refactor the configuration handling to optional free-standing 'LogManager' class
 </pre>
 
 
 \History
 - 18.10.2018, FKling, LoggerInstance in map, added 'Fatal' as level
 - 25.10.2010, FKling, Property handling and from-file-initialization
 - 25.10.2010, FKling, Simple rolling file appender
 - 21.10.2010, FKling, Changed messages classes to be ranges instead of fixed
 - 20.10.2010, FKling, Refactored the creation of report strings
 - 19.10.2010, FKling, Support for sink management
 - 02.04.2009, FKling, Support for sink levels
 - 23.03.2009, FKling, Implementation

 
 ---------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>


#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <time.h>

#define strdup _strdup	// bla,bla

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

#define snprintf _snprintf
#define vsnprintf _vsnprintf

#else
#include <sys/time.h>
#include <time.h>
#endif

#include <list>
#include <map>

#include "logger.h"

#ifdef TRUN_NO_STRDUP
#warning "No strdup - using internal bad version"
static char *strdup(const char *str) {
	size_t len = strlen (str) + 1;
	char *copy = (char *)malloc (len);
	if (copy) {
		memcpy (copy, str, len);
	}
	return copy;
}
#endif




#define DEFAULT_DEBUG_LEVEL 0		// used by constructors, default is output everything
#define DEFAULT_SINK_NAME ("")
#define DEFAULT_LOGFILE_NAME ("logfile")
#define DEFAULT_BUFFER_SIZE 1024

#define DBGLEVEL_MASK_LEVEL(a) ((a) & 0xffff)
#define DBGLEVEL_MASK_FLAGS(a) ((a>>16) & 0x7fff)
#define DBGLEVEL_COMBINE(__level, __flags) (((__level) & 0xffff) | (((__flags) & 0x7fff) << 16))
	//dbgLevel = dbgLevel & 0xffff | ((flags & 0x7fff) << 16);

#define LOG_SZ_GB(x) ((x)*1024*1024*1024)
#define LOG_SZ_MB(x) ((x)*1024*1024)
#define LOG_SZ_KB(x) ((x)*1024)


using namespace std;

namespace gnilk {
    static int StrExplode(std::vector<std::string> *strList, char *mString, int chrSplit);
    static char *StrTrim(char *s);
}

using namespace gnilk;

// --------------------------------------------------------------------------
//
// Console sink
// outputs all debug messages to the console
//
ILogOutputSink *LogConsoleSink::CreateInstance([[maybe_unused]]const char *className)
{
	return static_cast<ILogOutputSink *>(new LogConsoleSink());
}

int LogConsoleSink::WriteLine(int dbgLevel, char *hdr, char *string)
{
	int res = 0;
    res = fprintf(stdout, "%s%s\n", hdr,string);
	return res;
}


/////////
//
// -- static functions
//
bool Logger::bInitialized = false;
ILoggerList Logger::loggers;
ILogOutputSink *Logger::sink = nullptr;
static char glb_log_buffer[TRUN_MAX_LOG_STRING] = {};
int Logger::iDebugLevel = TRUN_LOG_DEFAULT_DEBUG_LEVEL;


void Logger::SendToSinks(int dbgLevel, char *hdr, char *string)
{
    if (sink == nullptr) {
        return;
    }
    sink->WriteLine(dbgLevel, hdr, string);
}


#ifdef WIN32
#ifdef _MSC_VER
struct timezone 
{
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};
#endif
static int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;

	if (NULL != tv)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		/*converting file time to unix epoch*/
		tmpres /= 10;  /*convert into microseconds*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS; 
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
	}

	if (NULL != tz)
	{
		if (!tzflag)
		{
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}
	return 0;
}
#endif
//
// Returns a formatted time string for logging
// string can be either in default kTFLog4Net format or Unix
//
char *Logger::TimeString(int maxchar, char *dst)
{
	struct timeval tmv;
	gettimeofday(&tmv, NULL);
	
    time_t bla = tmv.tv_sec;
    struct tm *gmt = gmtime(&bla);
    snprintf(dst, maxchar, "%.4u-%.2u-%.2u %.2d:%.2d:%.2d.%.3d",
             gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
             gmt->tm_hour, gmt->tm_min, gmt->tm_sec,(int)tmv.tv_usec/1000);
	return dst;
}


LoggerInstance *Logger::GetInstance(const std::string &name) {

    // We need a maximum
	if (loggers.find(name) == loggers.end()) {
		// Have to create logger
		ILogger *pLogger = (ILogger *)new Logger(name);
		LoggerInstance *pInstance = new LoggerInstance(pLogger);
		// TODO: Support for exclude list		
		loggers.insert(std::pair<std::string, LoggerInstance *>(name, pInstance));

		return pInstance;
	}
	return loggers[name];
}

ILogger *Logger::GetLogger(const std::string &name)
{
	Initialize();
	LoggerInstance *pInstance = GetInstance(name);
	return pInstance->pLogger;
}

void Logger::SetAllSinkDebugLevel(int iNewDebugLevel) {
	// This might very well be the first call, make sure we are initalized
	Initialize();
    iDebugLevel = iNewDebugLevel;
}

// Without initialization
void Logger::AddSink(ILogOutputSink *pSink, const char *sName) {
	LogBaseSink *pBase = static_cast<LogBaseSink *> (pSink);
	if (pBase != NULL) {
		pBase->SetName(sName);
	}
    sink = pSink;
}
// With initialization
void Logger::AddSink(ILogOutputSink *pSink, const char *sName, int argc, char **argv) {
	pSink->Initialize(argc, argv);
	AddSink(pSink, sName);
}


void Logger::Initialize() {
	if (!Logger::bInitialized)
	{
		// Initialize the rest
#ifdef DEBUG
		ILogOutputSink *pSink = (ILogOutputSink *)new LogConsoleSink();
		AddSink(pSink, "console", 0, NULL);
#endif
		Logger::bInitialized = true;
//		SetDebugLevel(DEFAULT_DEBUG_LEVEL);
//		SetName("Logger");
	}
}

// Regular functions

Logger::Logger(const std::string &newName) {
    name = newName;
    this->logFlags = Logger::kFlags_None;
    Logger::Initialize();
}

static const char *lMessageClassNames[] =
{
	"NONE",			// 0
	"DEBUG",		// 1
	"INFO",			// 2
	"WARN",			// 3
	"ERROR",		// 4
	"CRITICAL",		// 5
	"FATAL",		// 6
	"CUSTOM"		// 7
};
const char *Logger::MessageClassNameFromInt(int mc) 
{
	if ((mc>=Logger::kMCNone) && (mc<(int)Logger::kMCDebug)) 
	{
		return lMessageClassNames[0];
	}
	else if ((mc>=(int)Logger::kMCDebug) && (mc<(int)Logger::kMCInfo))
	{
		return lMessageClassNames[1];
	}
	else if ((mc>=(int)Logger::kMCInfo) && (mc<(int)Logger::kMCWarning))
	{
		return lMessageClassNames[2];
	}
	else if ((mc>=(int)Logger::kMCWarning) && (mc<(int)Logger::kMCError))
	{
		return lMessageClassNames[3];
	}
	else if ((mc>=(int)Logger::kMCError) && (mc<(int)Logger::kMCCritical))
	{
		return lMessageClassNames[4];
	}
	else if ((mc>=(int)Logger::kMCCritical) && (mc<(int)Logger::kMCFatal))
	{
		return lMessageClassNames[5];
	}
	else if ((mc>=(int)Logger::kMCCritical))
	{
		return lMessageClassNames[6];
	} 
	return lMessageClassNames[7];

}

int Logger::MessageLevelFromName(const char *level)
{
	if (!strcmp(level, "NONE")) return kMCNone;
	if (!strcmp(level, "DEBUG")) return kMCDebug;
	if (!strcmp(level, "INFO")) return kMCInfo;
	if (!strcmp(level, "WARN")) return kMCWarning;
	if (!strcmp(level, "WARNING")) return kMCWarning;
	if (!strcmp(level, "ERROR")) return kMCError;
	if (!strcmp(level, "CRITICAL")) return kMCCritical;
	if (!strcmp(level, "FATAL")) return kMCFatal;
	return kMCNone;
}

//
// Compiles the final string and header and sends it to the sinks
// NOTE: The layout is fixed
//
void Logger::WriteReportString(int mc, char *string)
{
	char sHdr[64];
	char sTime[32];	// saftey, 26 is enough

	const char *sLevel = MessageClassNameFromInt(mc);

	
	TimeString(32, sTime);
	// Create the special header string
	// Format: "time [thread] msglevel library - "

    // MAX string before truncation is 62 chars; 23 + 2 + 8 + 2 + 8 + 1 + 15 + 3 = 23 + 23+20+15+4 = 43+19 = 62
    snprintf(sHdr, 63, "%-23s [%.8x] %8s %15s - ", sTime, (unsigned int)0, sLevel, name.c_str());

	// gnilk, 2018-10-18, Combine with flags here - does not affect higher level API
	int dbgLevel = DBGLEVEL_COMBINE(mc, logFlags);

	Logger::SendToSinks((int)dbgLevel,sHdr, string);
}


// This functionality is duplicated by all 'write'-functions. It composes the message
// string. The reason why it is not in a function is because of the va_xxx functions.
// Event is essentially a container around the buffer which makes a query for the buffer
// upon creation and releases it in the destructor
#define WRITE_REPORT_STRING(__DBGTYPE__)    \
    va_list values;                         \
    va_start(values, sFormat);              \
    vsnprintf(glb_log_buffer, TRUN_MAX_LOG_STRING, sFormat, values);    \
    va_end(values);                         \
    Logger::WriteReportString(__DBGTYPE__, glb_log_buffer); \


void Logger::WriteLine(int iDbgLevel, const char *sFormat,...)
{
	// Always write stuff without global filtering - let appenders figure it out..
	WRITE_REPORT_STRING(iDbgLevel);
}
void Logger::WriteLine(const char *sFormat,...)
{
	// Always write stuff without global filtering - let appenders figure it out..
	WRITE_REPORT_STRING(kMCNone);
}
void Logger::Fatal(const char *sFormat,...)
{
	if (IsFatalEnabled()) {
		WRITE_REPORT_STRING(kMCFatal);
	}
}

void Logger::Critical(const char *sFormat,...)
{
	if (IsCriticalEnabled()) {
		WRITE_REPORT_STRING(kMCCritical);
	}
}
void Logger::Error(const char *sFormat, ...)
{
	if (IsErrorEnabled()) {
		WRITE_REPORT_STRING(kMCError);
	}
}
void Logger::Warning(const char *sFormat, ...)
{
	if (IsWarningEnabled()) {
		WRITE_REPORT_STRING(kMCWarning);
	}
}
void Logger::Info(const char *sFormat, ...)
{
	if (IsInfoEnabled()) {
		WRITE_REPORT_STRING(kMCInfo);
	}
}
void Logger::Debug(const char *sFormat, ...)
{
	if (IsDebugEnabled()) {
		WRITE_REPORT_STRING(kMCDebug);
	}
}


// ---------------------------------------------------------------------------
//
// Holds an instance of a logger
//

LoggerInstance::LoggerInstance()
{
	this->pLogger = NULL;
}

LoggerInstance::LoggerInstance(ILogger *pLogger)
{
	this->pLogger = pLogger;
}



// -------------------------------------------------------------------------
//
// Internal helpers
//
namespace gnilk {

    static char *StrTrim(char *s) {
        char *ptr;
        if (!s)
            return NULL;   // handle NULL string
        if (!*s)
            return s;      // handle empty string
        for (ptr = s + strlen(s) - 1; (ptr >= s) && isspace(*ptr); --ptr);
        ptr[1] = '\0';
        return s;
    }

//
// Splits the string into substrings for a given character
//
    static int StrExplode(std::vector<std::string> *strList, char *mString, int chrSplit) {
        std::string strTmp, strPart;
        size_t ofs, pos;
        int count;

        strTmp = std::string(mString);
        pos = count = 0;
        do {
            ofs = strTmp.find_first_of(chrSplit, pos);

            if (ofs == std::string::npos) {
                if (pos != std::string::npos) {
                    strPart = strTmp.substr(pos, strTmp.length() - pos);
                    strList->push_back(strPart);
                    count++;
                } else {
                    // We had trailing spaces...
                }

                break;
            }
            strPart = strTmp.substr(pos, ofs - pos);
            strList->push_back(strPart);
            pos = ofs + 1;
            pos = strTmp.find_first_not_of(chrSplit, pos);
            count++;
        } while (1);

        return count;
    } // StrExplode

}