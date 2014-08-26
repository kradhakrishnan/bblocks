#ifndef _KCOMMON_LOGGER_H_
#define _KCOMMON_LOGGER_H_

#include <vector>
#include <string>
#include <sstream>
#include <streambuf>
#include <stdio.h>
#include <memory>
#include <pthread.h>

#include "util.hpp"

/**
 TODO:
 Please note. This is an interim solution but good enough since we don't dump
 logs at runtime. Please rewrite this at a later time using C++ IO stream buffer
 manipulations.
 **/

namespace bblocks {

using namespace std;

//............................................................... LogWriter ....

/**
 */
class LogWriter
{
    public:

    enum Priority
    {
        DEFAULT = 0,
        HIGHPRIORITY,
        LOWPRIORITY
    };

    virtual ~LogWriter()
    {
    }

    virtual void Append(const string & data, const Priority & priority) = 0;
};

//.................................................................. Logger ....

/**
 */
class Logger : public Singleton<Logger>
{
public:

    friend class Singleton<Logger>;

    enum LogType
    {
	LEVEL_VERBOSE = 'v',
        LEVEL_DEBUG = 'd',
        LEVEL_INFO = 'i',
        LEVEL_WARNING = 'w',
        LEVEL_ERROR = 'e',
    };

    void AttachWriter(const SharedPtr<LogWriter> & writer)
    {
        writer_ = writer;
    }

    void Append(const LogType & type, const string & msg)
    {
        const LogWriter::Priority p = type & (LEVEL_ERROR) ? LogWriter::HIGHPRIORITY
							   : LogWriter::DEFAULT;
        writer_->Append(msg, p);
    }

private:

    Logger() {}

    SharedPtr<LogWriter> writer_;
};

//.............................................................. LogMessage ....

/**
 */
class LogMessage
{
public:

    LogMessage(const Logger::LogType & type, const string & path)
        : type_(type)
        , path_(path)
    {}

    ~LogMessage()
    {
        ostringstream tmp;
        tmp << (char) type_ << " " << Timestamp() << " [" << path_ << "] ";

	for (auto it = msg_.begin(); it != msg_.end(); ++it) {
            tmp << *it;
        }

        Logger::Instance().Append(type_, tmp.str());
    }

    LogMessage & operator<<(const string & t)
    {
        msg_.push_back(t);
        return *this;
    }

    template<class T>
    LogMessage & operator<<(const T & t)
    {
        ostringstream ss;
        ss << t;
        msg_.push_back(ss.str());
        return *this;
    }

protected:

    const string Timestamp()
    {
        time_t secondsSinceEpoch = time(NULL);
        tm* brokenTime = localtime(&secondsSinceEpoch);
        char buf[80];
        strftime(buf, sizeof(buf), " %d/%m/%y %T ", brokenTime);
        return string(buf);
    }

    typedef list<string> StringListType;

    const Logger::LogType type_;
    const string path_;
    StringListType msg_;
};

//........................................................... ConsoleWriter ....

/**
 */
class ConsoleLogWriter : public LogWriter
{
public:

	ConsoleLogWriter()
	{
		int status = pthread_mutex_init(&lock_, NULL);
		INVARIANT(status == 0);
	}

	~ConsoleLogWriter()
	{
		int status = pthread_mutex_destroy(&lock_);
		INVARIANT(status == 0);
	}

	void Append(const string & data, const LogWriter::Priority & priority)
	{
		int status;

		status = pthread_mutex_lock(&lock_);
		INVARIANT(status == 0);

		cerr << data << endl;

		status = pthread_mutex_unlock(&lock_);
		INVARIANT(status == 0);
	}

private:

	pthread_mutex_t lock_;
};

//............................................................... LogHelper ....

/**
 */
class LogHelper
{
public:

    static void InitConsoleLogger()
    {
        Logger::Init();
        Logger::Instance().AttachWriter(MakeSharedPtr(new ConsoleLogWriter()));
    }

    static void DestroyLogger()
    {
        Logger::Destroy();
    }
};

};

#endif
