# distutils : language = c++

# ................................................................................. cdef extern ....

cdef extern from "schd/thread-pool.h" namespace "dh_core":
    cdef cppclass ThreadRoutine:
        void Run()

    cdef cppclass FnPtr1[T]:
        FnPtr1(void (*fn)(T), T)

    cdef cppclass NonBlockingThreadPool:
        NonBlockingThreadPool()
        void Start(int)
        void Shutdown()
        void Wait()
        void Wakeup()
        void Schedule(ThreadRoutine *)

cdef extern from "schd/thread-pool.h" namespace "dh_core::NonBlockingThreadPool":
        NonBlockingThreadPool & Instance()
        void Init()

cdef extern from "schd/schd-helper.h" namespace "dh_core::SysConf":
        int NumCores()

# .............................................................................. PyThreadRoutine ...

cdef object MakePyThreadRoutine(ThreadRoutine * fn):
    r = PyThreadRoutine()
    r.cthis = fn
    return r

cdef AsyncSchedule(PyNonBlockingThreadPool pool, FnPtr1[int] * fn):
    pool.cthis.Schedule(<ThreadRoutine *>fn)

cdef class PyThreadRoutine:

    cdef ThreadRoutine * cthis

    def Run(self):
        self.cthis.Run()

# ..................................................................... PyNonBlockingThreadPool ....

cdef class PyNonBlockingThreadPool:

    cdef NonBlockingThreadPool * cthis

    def __cinit__(self):
        Init()
        self.cthis = &(Instance())

    def __dalloc__(self):
        assert False

    def Start(self, int ncpu = NumCores()):
        self.cthis.Start(ncpu)

    def Shutdown(self):
        self.cthis.Shutdown()

    def Wait(self):
        self.cthis.Wait()

    def Wakeup(self):
        self.cthis.Wakeup()

    def Schedule(self, PyThreadRoutine r):
        self.cthis.Schedule(r.cthis)

# ................................................................................ PyThreadPool ....

cdef class PyThreadPool:

    threadPool = PyNonBlockingThreadPool()

    @staticmethod
    def Start(int ncpu = NumCores()):
        PyThreadPool.threadPool.Start(ncpu)

    @staticmethod
    def Shutdown():
        PyThreadPool.threadPool.Shutdown()

    @staticmethod
    def Wait():
        PyThreadPool.threadPool.Wait()

    @staticmethod
    def Wakeup():
        PyThreadPool.threadPool.Wakeup()

    @staticmethod
    def Schedule(PyThreadRoutine r):
        PyThreadPool.threadPool.Schedule(r)
