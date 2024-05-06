/*-------------------------------------------------------------------------
File    :	Logger.cpp
Author  :   FKling
Orginal :	2005-7-19, 11:39
Descr   :	Stripped down version of an old logger

 Part of testrunner
 BSD3 License!

 ---------------------------------------------------------------------------
Changes:

-- Date -- | -- Name ------- | -- Did what...                              
2024-05-06 | FKling          | Stripped for embedded use
---------------------------------------------------------------------------*/
#ifndef GNK_TRUN_EMBEDDED_LOGGER_H
#define GNK_TRUN_EMBEDDED_LOGGER_H

#include <string.h>

#include <list>
#include <map>
#include <vector>
#include <string>

using namespace std;

namespace gnilk
{

#ifndef TRUN_LOG_DEFAULT_DEBUG_LEVEL
#define TRUN_LOG_DEFAULT_DEBUG_LEVEL 0
#endif

#ifndef TRUN_MAX_LOG_NAME
#define TRUN_MAX_LOG_NAME 16
#endif

#ifndef TRUN_MAX_LOG_STRING
#define TRUN_MAX_LOG_STRING 256
#endif

	// Main public interface - this is the one you will normally use
	class ILogger
	{
	public:
		// properties
		virtual const char *GetName() = 0;
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
	};

    // This is a bit convoluted - but keeping as I can see someone wanting to add specifics for SWD/JTAG/WIFI/etc..
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
	public:
        LogBaseSink() = default;
        virtual ~LogBaseSink() = default;

		void Initialize(int argc, char **argv) override {}
		int WriteLine(int dbgLevel, char *hdr, char *string) { return -1; }
        void Close() {};

        const char *GetName() override {
            return name;
        }
        void SetName(const char *newName) {
            strncpy(name, newName,TRUN_MAX_LOG_NAME-1);
        }
    protected:
        char name[TRUN_MAX_LOG_NAME];
	};

	class LogConsoleSink : 	public LogBaseSink
	{
	public:
        LogConsoleSink() = default;
        virtual ~LogConsoleSink() = default;

        static ILogOutputSink * CreateInstance(const char *className);
		int WriteLine(int dbgLevel, char *hdr, char *string) override;
	};

	class LoggerInstance
	{
	public:
		ILogger *pLogger;
		std::list<char *> excludedModules;
	public:
		LoggerInstance();
		LoggerInstance(ILogger *pLogger);
        virtual ~LoggerInstance() = default;
	};


	typedef std::map<std::string, LoggerInstance *> ILoggerList;

    typedef enum
    {
        kNone = 0,
        kDebug = 100,
        kInfo = 200,
        kWarning = 300,
        kError = 400,
        kCritical = 500,
        kFatal = 600,
    } LogLevel;


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

	public:
        Logger(const std::string &newName);
        virtual ~Logger() = default;

		static void Initialize();


		static ILogger *GetLogger(const std::string &name);
		static void SetAllSinkDebugLevel(int iNewDebugLevel);
		static void AddSink(ILogOutputSink *pSink, const char *sName);
		static void AddSink(ILogOutputSink *pSink, const char *sName, int argc, char **argv);


		static const char *MessageClassNameFromInt(int mc);
		static int MessageLevelFromName(const char *level);


		const char *GetName() override { return name.c_str();};
		void Enable(int flag) override { logFlags |= (flag & 0x7fff); };
		void Disable(int flag) override { logFlags = logFlags & ((flag ^ 0x7fff) & 0x7fff); };

		
		// Functions
		void WriteLine (int iDbgLevel, const char *sFormat,...) override;
		void WriteLine (const char *sFormat,...) override;
		void Fatal(const char *sFormat,...) override;
		void Critical(const char *sFormat,...) override;
		void Error(const char *sFormat, ...) override;
		void Warning(const char *sFormat, ...) override;
		void Info(const char *sFormat, ...) override;
		void Debug(const char *sFormat, ...) override;

    protected:

        __inline bool IsDebugEnabled() { return (IsLevelEnabled((int)kMCDebug)?true:false);}
        __inline bool IsInfoEnabled() { return (IsLevelEnabled((int)kMCInfo)?true:false);}
        __inline bool IsWarningEnabled() { return (IsLevelEnabled((int)kMCWarning)?true:false);}
        __inline bool IsErrorEnabled() { return (IsLevelEnabled((int)kMCError)?true:false);}
        __inline bool IsCriticalEnabled() { return (IsLevelEnabled((int)kMCCritical)?true:false);}
        __inline bool IsFatalEnabled() { return (IsLevelEnabled((int)kMCFatal)?true:false);}

        __inline bool IsLevelEnabled(int iDbgLevel) { return ((iDbgLevel>=iDebugLevel)?true:false); }

    private:
        std::string name;
        int logFlags;
        static int iDebugLevel;
        Logger(const char *sName);
        void WriteReportString(int mc, char *string);
        void GenerateIndentString();

    private:
        static bool bInitialized;
        static ILoggerList loggers;
        static ILogOutputSink *sink;        // Embedded only supports one sink...
        static char *TimeString(int maxchar, char *dst);
        static void SendToSinks(int dbgLevel, char *hdr, char *string);
        static LoggerInstance *GetInstance(const std::string &name);
	};
	
}

#endif
