/*-------------------------------------------------------------------------
File    :	Logger.cpp
Author  :   FKling
Orginal :	2005-7-19, 11:39
Descr   :	Logger, simplistic stuff, mimics Log4Net/Log4Java/Log4Delphi

--------------------------------------------------------------------------- 
Todo [-:undone,+:inprogress,!:done]:

 
Changes: 

-- Date -- | -- Name ------- | -- Did what...                              
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
#include <string>

#ifndef __LOGGER_INTERNAL_H__
#define __LOGGER_INTERNAL_H__

#include "logger.h"

using namespace std;

namespace trun
{
	#define LOG_CONF_LOGFILE ("logfilename")
	#define LOG_CONF_MAXLOGSIZE ("maxlogsize")
	#define LOG_CONF_MAXBACKUPINDEX ("maxbackupindex")
	#define LOG_CONF_DEBUGLEVEL ("debuglevel")
	#define LOG_CONF_NAME ("name")
	#define LOG_CONF_CLASSNAME ("class")

	extern "C"
	{
		typedef ILogOutputSink *(LOG_CALLCONV *LOG_PFNSINKFACTORY)(const char *className);
	}
	typedef struct
	{
		const char *name;
		LOG_PFNSINKFACTORY factory;
		
	} LOG_SINK_FACTORY;


	// Internal class, not available to outside..
	class MsgBuffer
	{
	private:
		char *buffer;
		int sz;
	public:
		MsgBuffer();
		virtual ~MsgBuffer();
		
		__inline char *GetBuffer() { return buffer; }
		__inline int GetSize() { return sz; }
		
		void Extend();
	};

	class LogEvent
	{
	private:
		MsgBuffer *pBuffer;
	public:
		LogEvent() {
			pBuffer = (MsgBuffer *)Logger::RequestBuffer();
		}
		virtual ~LogEvent() {
			Logger::ReleaseBuffer(pBuffer);
		}
		__inline MsgBuffer *GetBuffer() {
			return pBuffer;
		}
	};

	typedef std::pair<std::string, std::string> strStrPair;

}

#endif
