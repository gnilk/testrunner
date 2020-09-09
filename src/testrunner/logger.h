/*-------------------------------------------------------------------------
File    :	Logger.cpp
Author  :   FKling
Orginal :	2005-7-19, 11:39
Descr   :	Logger, simplistic stuff, mimics Log4Net/Log4Java/Log4Delphi

No support for layout's, output is fixed

Loglevel filtering can be set on:
 - Globally through "Logger::GetProperties()->SetXYZ()"
 - Appender, each appender can have their own filtering

 Global filtering is early rejection, if it does not pass through it won't
 make it to the log regardless.
 I.e. If global is set to CRITICAL than _ONLY_ critical messages will be
 pushed down the chain.

 So, for production set global to "INFO" (kMCInfo) and during development 
 set it to "NONE" (kMCNone)

--------------------------------------------------------------------------- 
Todo [-:undone,+:inprogress,!:done]:

 
Changes: 

-- Date -- | -- Name ------- | -- Did what...                              
2018-10-19 | FKling          | LoggerInstance in map, added 'Fatal' as level
2010-10-21 | FKling          | Redefined the message classes to be ranges
2010-10-21 | FKling          | Added automatic indentation logging
2010-10-19 | FKling          | Added sink management
2009-04-16 | FKling          | Imported from other projects
2009-01-26 | FKling          | Imported to iPhone
2006-11-08 | FKling          | Rewrote critical section handling, now specific lock's
2006-10-19 | FKling          | Timers are multi-core, multi-cpu safe
2006-01-12 | FKling          | upon creation of timers calling mark, to reset states
2005-12-12 | FKling          | memory leak, free generated debugstring

---------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>

#include <list>
#include <queue>
#include <map>
#include <vector>
#include <string>

#ifndef __LOGGER_H__
#define __LOGGER_H__


using namespace std;

namespace gnilk
{
#ifdef WIN32
#define LOG_CALLCONV __stdcall
#else
#define LOG_CALLCONV
#endif

#define MAX_INDENT 256

	// Main public interface - this is the one you will normally use
	class ILogger
	{
	public:
		// properties
		virtual int GetIndent() = 0;
		virtual int SetIndent(int nIndent) = 0;
		virtual char *GetName() = 0;
		virtual void Enable(int flag) = 0;
		virtual void Disable(int flag) = 0;
		
		// functions
		virtual void WriteLine(const char *sFormat,...) = 0;
		virtual void WriteLine(int iDbgLevel, const char *sFormat,...) = 0;
		virtual void Fatal(const char *sFormat,...) = 0;
		virtual void Critical(const char *sFormat,...) = 0;
		virtual void Error(const char *sFormat, ...) = 0;
		virtual void Warning(const char *sFormat, ...) = 0;
		virtual void Info(const char *sFormat, ...) = 0;
		virtual void Debug(const char *sFormat, ...) = 0;
		
		virtual void Enter() = 0;
		virtual void Leave() = 0;
	};
	class LogPropertyReader
	{
	private:
		std::map<std::string, std::string> properties;
		void ParseLine(char *line);
	public:
		LogPropertyReader();
		virtual ~LogPropertyReader();

		void ReadFromFile(const char *filename);
		void WriteToFile(const char *filename);

		virtual char *GetValue(const char *key, char *dst, int nMax, const char *defValue);
		virtual void SetValue(const char *key, const char *value);

		virtual int GetAllStartingWith(std::vector<std::pair<std::string, std::string> > *result, const char *filter);

		virtual void OnValueChanged(const char *key, const char *value) = 0;
	};
	// Holds properties for the logger and/or sink
	class LogProperties : public LogPropertyReader
	{
	protected:
		int iDebugLevel;	// Everything above this becomes written to the sink
		char *name;
		int nMaxBackupIndex;
		long nMaxLogfileSize;
		char *logFileName;
		char *className;

		void SetDefaults();
	public:
		LogProperties();

		__inline bool IsLevelEnabled(int iDbgLevel) { return ((iDbgLevel>=iDebugLevel)?true:false); }
		__inline int GetDebugLevel() { return iDebugLevel; }
		__inline void SetDebugLevel(int newLevel) { iDebugLevel = newLevel; }

		__inline const char *GetName() { return name; };
		void SetName(const char *newName);

		__inline const char *GetClassName() { return className; };
		void SetClassName(const char *newName);

		__inline const char *GetLogfileName() { return logFileName; };
		void SetLogfileName(const char *newName);

		__inline const long GetMaxLogfileSize() { return nMaxLogfileSize; };
		__inline void SetMaxLogfileSize(const long nSize) { nMaxLogfileSize = nSize; };
		__inline const int GetMaxBackupIndex() { return nMaxBackupIndex; };
		__inline void SetMaxBackupIndex(const int nIndex) { nMaxBackupIndex = nIndex; }; 


		// Event from reader
		void OnValueChanged(const char *key, const char *value);
	};

	// Used to wrap up indentation when using exceptions
	// Use this class for automatic and proper indent handling
	class LogIndent
	{
	private:
		ILogger *pLogger;
	public:
		LogIndent(ILogger *_pLogger) : pLogger(_pLogger) { pLogger->Enter(); }
		virtual ~LogIndent() { pLogger->Leave(); }
	};


	// return valus from 'WriteLine'
#define SINK_WRITE_UNKNOWN_ERROR -100
#define SINK_WRITE_IO_ERROR -1
#define SINK_WRITE_FILTERED 0

	class ILogOutputSink
	{
	public:
		virtual const char *GetName() = 0;
		virtual void Initialize(int argc, char **argv) = 0;
		virtual int WriteLine(int dbgLevel, char *hdr, char *string) = 0;
		virtual void Close() = 0;
	};
	class LogBaseSink : public ILogOutputSink
	{
	protected:
		char *name;
		LogProperties properties;
		bool WithinRange(int iDbgLevel);

//		__inline bool WithinRange(int iDbgLevel) { return (iDbgLevel>=properties.GetDebugLevel())?true:false; }
	public:	
		LogProperties *GetProperties() { return &properties; }
	public:
		virtual void SetName(const char *name) { properties.SetName(name); }
		virtual const char *GetName() { return properties.GetName(); }
		virtual void Initialize(int argc, char **argv) = 0;
		virtual int WriteLine(int dbgLevel, char *hdr, char *string) = 0;
		virtual void Close() = 0;
	};
	class LogConsoleSink : 	public LogBaseSink
	{
	public:
		virtual void Initialize(int argc, char **argv);
		virtual int WriteLine(int dbgLevel, char *hdr, char *string);
		virtual void Close();

		static ILogOutputSink * LOG_CALLCONV CreateInstance(const char *className);
	};
	
	class LogFileSink : public LogBaseSink
	{
	protected:
		FILE *fOut;

		void Open(const char *filename, bool bAppend);
		long Size();
		void ParseArgs(int argc, char **argv);
	public:
		LogFileSink();
		virtual ~LogFileSink();
		virtual void Initialize(int argc, char **argv);
		virtual int WriteLine(int dbgLevel, char *hdr, char *string);
		virtual void Close();

		static ILogOutputSink * LOG_CALLCONV CreateInstance(const char *className);
	};	

	class LogRollingFileSink : public LogFileSink
	{
	private:
		int nMaxBackupIndex;
		long nBytesRollLimit;
		long nBytes;

		char *GetFileName(char *dst, int idx);
		void RollOver();
		void CheckApplyRules();
	public:
		LogRollingFileSink();
		virtual ~LogRollingFileSink();
		virtual void Initialize(int argc, char **argv);
		virtual int WriteLine(int dbgLevel, char *hdr, char *string);

		static ILogOutputSink * LOG_CALLCONV CreateInstance(const char *className);
	};
	
	class LoggerInstance
	{
	public:
		ILogger *pLogger;
		//int iDebugLevel;		// Message class must be above this to become printed
		std::list<char *> lExcludedModeuls;		
	public:
		LoggerInstance();
		LoggerInstance(ILogger *pLogger);
	};



	//typedef std::list<LoggerInstance *> ILoggerList;
	typedef std::map<std::string, LoggerInstance *> ILoggerList;
	typedef std::list<ILogOutputSink *>ILoggerSinkList;

	class Logger : public ILogger
	{
	public:
		typedef enum
		{
			kMCNone = 0,
			kMCDebug = 100,
			kMCInfo = 200,
			kMCWarning = 300,
			kMCError = 400,
			kMCCritical = 500,
			kMCFatal = 600,
		} MessageClass;

		// Logger Flags - can be used to override sinklevel settings and other things
		typedef enum {
			kFlags_None = 0x0000,
			// kFlags_PassThrough, messages printed regardless of sink-levels
			kFlags_PassThrough = 0x0001,
			// kFlags_BlockAll, all messages are blocked, Note: kFlags_PassThrough has higher priority	
			kFlags_BlockAll = 0x0002,	
		} kFlags;

		typedef enum
		{
			kTFDefault,
			kTFLog4Net,
			kTFUnix,			
		} TimeFormat;
	private:
		
		char *sName;
		char *sIndent;
		int iIndentLevel;
		int logFlags;
		Logger(const char *sName);
		void WriteReportString(int mc, char *string);
		void GenerateIndentString();
		
	private:
		static TimeFormat kTimeFormat;
		static bool bInitialized;
		static int iIndentStep;
		static ILoggerList loggers;
		static ILoggerSinkList sinks;
		static LogProperties properties;
		static std::queue<void *> buffers;
		static char *TimeString(int maxchar, char *dst);
		static void SendToSinks(int dbgLevel, char *hdr, char *string);
		static void RebuildSinksFromConfiguration();
		static LoggerInstance *GetInstance(std::string name);

	public:
	
		static ILogOutputSink *CreateSink(const char *className);
		static void Initialize();
		virtual ~Logger();
		static ILogger *GetLogger(const char *name);
		static void SetAllSinkDebugLevel(int iNewDebugLevel);
		static void AddSink(ILogOutputSink *pSink, const char *sName);
		static void AddSink(ILogOutputSink *pSink, const char *sName, int argc, char **argv);

		// Refactor this to a LogManager
		static void *RequestBuffer();
		static void ReleaseBuffer(void *pBuf);

		static const char *MessageClassNameFromInt(int mc);
		static int MessageLevelFromName(const char *level);


		static LogProperties *GetProperties() { return &Logger::properties; }

		__inline bool IsDebugEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCDebug)?true:false);}
		__inline bool IsInfoEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCInfo)?true:false);}
		__inline bool IsWarningEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCWarning)?true:false);}
		__inline bool IsErrorEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCError)?true:false);}
		__inline bool IsCriticalEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCCritical)?true:false);}
		__inline bool IsFatalEnabled() { return (Logger::properties.IsLevelEnabled((int)kMCFatal)?true:false);}
		
		// properties
		virtual int GetIndent() { return iIndentLevel; };
		virtual int SetIndent(int nIndent) { iIndentLevel = nIndent; return iIndentLevel; };
		virtual char *GetName() { return sName;};
		virtual void Enable(int flag) { logFlags |= (flag & 0x7fff); };
		virtual void Disable(int flag) { logFlags = logFlags & ((flag ^ 0x7fff) & 0x7fff); };

		
		// Functions
		virtual void WriteLine(int iDbgLevel, const char *sFormat,...);
		virtual void WriteLine(const char *sFormat,...);
		virtual void Fatal(const char *sFormat,...);
		virtual void Critical(const char *sFormat,...);
		virtual void Error(const char *sFormat, ...);
		virtual void Warning(const char *sFormat, ...);
		virtual void Info(const char *sFormat, ...);
		virtual void Debug(const char *sFormat, ...);

		// Enter leave functions, use to auto-indent flow statements, take care on exceptions!
		virtual void Enter();
		virtual void Leave();
	};
	
}

#endif
