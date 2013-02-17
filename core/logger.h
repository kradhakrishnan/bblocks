#ifndef _KCOMMON_LOGGER_H_
#define _KCOMMON_LOGGER_H_

#include <vector>
#include <string>
#include <sstream>
#include <streambuf>
#include <stdio.h>

#include "core/util.hpp"

/**
 BUGS/TODO:
 1. Write LogMessage using proper c++ mechanism
 **/
namespace dh_core {

#if !defined(DISABLE_VERBOSE) && defined(DEBUG_BUILD)
#define DEBUG(x) LogMessage(Logger::LEVEL_DEBUG, x.GetPath())
#else
#define DEBUG(x) if (1) {} else LogMessage(Logger::LEVEL_DEBUG, x.GetPath())
#endif

#define ERROR(x) LogMessage(Logger::LEVEL_ERROR, x.GetPath())
#define INFO(x) LogMessage(Logger::LEVEL_INFO, x.GetPath())

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

/**
 */
class Logger : public Singleton<Logger>
{
public:

    friend class Singleton<Logger>;

    enum LogType
    {
        LEVEL_DEBUG = 'd',
        LEVEL_INFO = 'i',
        LEVEL_WARNING = 'w',
        LEVEL_ERROR = 'e',
        LEVEL_NOTICE = 'n',
        LEVEL_FATAL = 'f'
    };

    void AttachWriter(const SharedPtr<LogWriter> & writer)
    {
        ASSERT(!writer_);
        writer_ = writer;
    }

    void Append(const LogType & type, const std::string & msg)
    {
        ASSERT(writer_);
        const LogWriter::Priority p =
            type & (LEVEL_ERROR | LEVEL_NOTICE
                    | LEVEL_FATAL) ? LogWriter::HIGHPRIORITY
                                   : LogWriter::DEFAULT;
        writer_->Append(msg, p);
    }

private:

    Logger() {}

    SharedPtr<LogWriter> writer_;
};

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

/**
 */
class ConsoleLogWriter : public LogWriter
{
    public:

    void Append(const std::string & data, const LogWriter::Priority & priority)
    {
        if (priority == LogWriter::HIGHPRIORITY) {
            std::cerr << data << std::endl;
        } else {
            std::cerr << data << std::endl;
        }
    }
};

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
