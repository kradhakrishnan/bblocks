# distutils : language = c++

cdef extern from "buf/buffer.h" namespace "dh_core":
    cdef cppclass IOBuffer:
        IOBuffer()

cdef class PyIOBuffer:
    cdef IOBuffer * thisptr
    def __cinit__(self):
        self.thisptr = new IOBuffer()

