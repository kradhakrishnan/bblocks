# distutils : language = c++

from libcpp.string cimport string

# ................................................................................. cdef extern ....

cdef extern from "logger.h" namespace "dh_core::Logger":
    cdef enum LogType:
        LEVEL_VERBOSE
        LEVEL_DEBUG
        LEVEL_INFO
        LEVEL_WARNING
        LEVEL_ERROR

cdef extern from "logger.h" namespace "dh_core":
    cdef cppclass Logger:
        Logger & Instance()
        void Append(LogType, string)

cdef extern from "logger.h" namespace "dh_core::LogHelper": 
    void InitConsoleLogger()

cdef extern from "logger.h" namespace "dh_core::Logger":
    Logger & Instance()
    void Init()

# .................................................................................... PyLogger ....

cdef class PyLogger:

    cdef Logger * cthis

    def __cinit__(self):
        InitConsoleLogger()
        self.cthis = &(Instance())

    def Info(self, msg):
        self.Append(LEVEL_INFO, msg)

    def Append(self, LogType type, string msg):
        self.cthis.Append(type, msg)
