# distutils : language = c++

# Test is BROKEN !!
# We need reconsider what needs to be exposed via pyx and via C python module !!

from logger import *
from thread_pool import *

cdef extern from "schd/thread-pool.h" namespace "dh_core":
    cdef cppclass FnPtr1[T]:
        FnPtr1(void (*fn)(T), T)

    cdef cppclass ThreadRoutine:
        void Run()


class Log:

    log = PyLogger()

    @staticmethod
    def Info(message):
        Log.log.Info(message)

cdef class PyUnitTest:

    def __init__(self):
        PyThreadPool.Start()

        Log.Info("PyThreadPool started.")

        self.Start()

        PyThreadPool.Wait()
        PyThreadPool.Shutdown()


cdef Start():
    pass

cdef void Callback(int val):
    print 'PyBasicTest::Callback'
    pass

if __name__ == '__main__':
    t = PyBasicTest()
    Start()
