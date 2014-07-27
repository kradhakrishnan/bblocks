#
# Libraries to build
#
LIBRARY +=  src			            \
	    src/net		            \
	    src/fs		            \

#
# .cc
#
SRCS += src/schd/thread-pool.cc	            \
	src/net/epoll/epoll.cc	            \
	src/net/rpc/rpc-data.cc	            \
	src/net/transport/tcp-linux.cc	    \
	src/net/rpc/rpc.cc		    \
	src/fs/aio-linux.cc	            \

#
# .cc that define main
#
TARGET +=

#
# .pyx
#
# PYX += src/pyx/buffer.pyx               \
#        src/pyx/thread_pool.pyx          \
#        src/pyx/logger.pyx               \

#
# Py module
#
SHARED += src/libbblocks.so
