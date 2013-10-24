#ifndef _KCOMMON_LOGGER_H_
#define _KCOMMON_LOGGER_H_

#include <vector>
#include <string>
#include <sstream>
#include <streambuf>
#include <stdio.h>
#include <tr1/memory>

#include "util.hpp"

/**
 TODO:
 Please note. This is an interim solution but good enough since we don't dump
 logs at runtime. Please rewrite this at a later time using C++ IO stream buffer
 manipulations.
 **/

namespace dh_core {

#if defined(DEBUG_BUILD)
#define LOG_DEBUG LogMessage(Logger::LEVEL_DEBUG, fqn_)
#define DEBUG(x) LogMessage(Logger::LEVEL_DEBUG, x.GetPath())
#else
#define LOG_DEBUG if (1) {} else LogMessage(Logger::LEVEL_DEBUG, fqn_)
#define DEBUG(x) if (1) {} else LogMessage(Logger::LEVEL_DEBUG, x.GetPath())
#endif

#if !defined(DISABLE_VERBOSE)
#define LOG_VERBOSE LogMessage(Logger::LEVEL_VERBOSE, fqn_)
#define VERBOSE(x) LogMessage(Logger::LEVEL_VERBOSE, x.GetPath())
#else
#define LOG_VERBOSE if (1) {} else LogMessage(Logger::LEVEL_VERBOSE, fqn_)
#define VERBOSE(x) if (1) {} else LogMessage(Logger::LEVEL_VERBOSE, x.GetPath())
#endif

#define LOG_ERROR LogMessage(Logger::LEVEL_ERROR, fqn_)
#define ERROR(x) LogMessage(Logger::LEVEL_ERROR, x.GetPath())

#define INFO(x) LogMessage(Logger::LEVEL_INFO, x.GetPath())
#define LOG_INFO LogMessage(Logger::LEVEL_INFO, fqn_)

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

    virtual void Append(const std::string & data, const Priority & priority) = 0;
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
        ASSERT(!writer_);
        writer_ = writer;
    }

    void Append(const LogType & type, const std::string & msg)
    {
        ASSERT(writer_);
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

    LogMessage(const Logger::LogType & type, const std::string & path)
        : type_(type)
        , path_(path)
    {}

    virtual ~LogMessage()
    {
        std::ostringstream tmp;
        tmp << (char) type_ << " " << Timestamp() << " [" << path_ << "] ";
        for (StringListType::const_iterator it = msg_.begin();
             it != msg_.end(); ++it) {
            tmp << *it;
        }
        Logger::Instance().Append(type_, tmp.str());
    }

    LogMessage & operator<<(const std::string & t)
    {
        msg_.push_back(t);
        return *this;
    }

    template<class T>
    LogMessage & operator<<(const T & t)
    {
        std::ostringstream ss;
        ss << t;
        msg_.push_back(ss.str());
        return *this;
    }

protected:

    const std::string Timestamp()
    {
        time_t secondsSinceEpoch = time(NULL);
        tm* brokenTime = localtime(&secondsSinceEpoch);
        char buf[80];
        strftime(buf, sizeof(buf), " %d/%m/%y %T ", brokenTime);
        return std::string(buf);
    }

    typedef std::list<std::string> StringListType;

    const Logger::LogType type_;
    const std::string path_;
    StringListType msg_;
};

//................................................................. LogPath ....

/**
 */
class LogPath
{
public:

    LogPath(const std::string & path)
        : path_(path)
    {
    }

    const std::string & GetPath() const
    {
        return path_;
    }

private:

    std::string path_;
};

//........................................................... ConsoleWriter ....

/**
 */
class ConsoleLogWriter : public LogWriter
{
public:

	ConsoleLogWriter()
		: isopen_(true)
	{}

	void Append(const std::string & data, const LogWriter::Priority & priority)
	{
		bool expected = true;
		while (!isopen_.compare_exchange_strong(expected, false));
		std::cerr << data << std::endl;
		isopen_ = true;
	}

private:

	std::atomic<bool> isopen_;
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
