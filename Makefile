SRCS =
TARGET =
INCLUDE =

STDCPP = /usr/include/c++/4.6.3/

SUBDIR = core \
         core/test \

-include ${SUBDIR:%=%/Makefile}

ifeq ($(shell uname), Linux)
#
# Linux OS
#

#
# OPT=enable
#
ifdef OPT
    ifeq ($(shell echo $(OPT)), enable)
        # optimized build
        BUILD_CCFLAGS = -O2
    else
        $(error Unknown value for OPT)
    endif
else
    # debug build
    BUILD_CCFLAGS = -g3  -DDEBUG_BUILD
endif

#
# PROFILE=enable
#
ifdef PROFILE
    ifeq ($(shell echo $(PROFILE)), enable)
        # optimized build
        BUILD_CCFLAGS += -pg
        LDFLAGS = -pg
    else
        $(error Unknown value for PROFILE)
    endif
endif

#
# VERBOSE=disable
#
ifdef VERBOSE
    ifeq ($(shell echo $(VERBOSE)), disable)
        # optimized build
        BUILD_CCFLAGS += -DDISABLE_VERBOSE
    else
        $(error Unknown value for VERBOSE)
    endif
endif

#
# VALGRIND=enable
#
ifdef VALGRIND
    ifeq ($(shell echo $(VALGRIND)), enable)
        CCFLAGS += -DVALGRIND_BUILD
    else
        $(error Unknown value for VALGRIND)
    endif
endif

#
# ERRCHECK=enable
#
ifdef ERRCHK
    ifeq ($(ERRCHK), enable)
        CCFLAGS += -DERROR_CHECK
    else
        $(error Unknown value for ERRCHK)
    endif
endif

#
# TCMALLOC
#
ifdef TCMALLOC
    ifeq ($(shell echo $(TCMALLOC)), enable)
        LIBS += -ltcmalloc_minimal
    endif
endif

CC = g++
AR = $(shell which ar)

CCFLAGS += -Wall -Werror -D__STDC_LIMIT_MACROS $(BUILD_CCFLAGS)
LDFLAGS += -L$(OBJDIR)  -L/usr/lib
INCLUDE += -I$(PWD) -Ipublic -I/usr/include/boost -I$(STDCPP)
LIBS    += -lrt -lpthread  -lz

PWD 	:= $(shell pwd)
OBJDIR	:= $(PWD)/../build
OBJS    := ${SRCS:%.cc=$(OBJDIR)/%.o}
DEPS    := ${OBJS:%.o=%.dep}
TOBJS	:= ${TARGET:%.cc=$(OBJDIR)/%.o}
TDEPS	:= ${TOBJS:%.o=%.dep}
TEXE	:= ${TARGET:%.cc=$(OBJDIR)/%}
STATIC	:= ${LIBRARY:%=${OBJDIR}/%/library.a}

all: lib exe
	@echo $(UNAME)

else

#
# Windows OS
#
CC = $(VS)\bin\cl.exe
AR = $(shell ar)

CCFLAGS +=  /Zi /EHsc -DKWARE_WINDOWS
LDFLAGS += /L$(OBJDIR) /L$(VS)/lib
INCLUDE += /I$(BOOST) /I$(WINSDK)/Include /I$(VS)\include /I. /Ipublic /I/usr/include/boost
LIBS    += -lrt -lpthread

PWD 	:= $(shell echo %CD%)
OBJDIR	:= $(PWD)/../build
OBJS    := ${SRCS:.cc=.obj} 
DEPS    := ${SRCS:.cc=.dep} 
TOBJS	:= ${TARGET:%.cc=%.obj}
TDEPS	:= ${TARGET:%.cc=%.dep}
TEXE	:= ${TARGET:%.cc=%}
STATIC	:= ${LIBRARY:%=${OBJDIR}/%/library.a}

all: lib exe

endif

-include $(DEPS)
-include $(TDEPS)

all: lib exe
lib: ${OBJS} $(LIBRARY)
exe: $(TOBJ) $(TEXE)

${TEXE}: ${STATIC} ${TOBJS}
	${CC} ${LDFLAGS} $@.o ${STATIC} -o $@ ${LIBS} 

${LIBRARY}: ${OBJS}
	$(AR) rcs ${OBJDIR}/$@/library.a ${OBJDIR}/$@/*.o 

$(OBJDIR)/%.o: %.cc
	${CC} ${INCLUDE} ${CCFLAGS} -o $@ -c $< 
	${CC} ${INCLUDE} ${CCFLAGS} -MT '$@' -MM $< > ${OBJDIR}/$*.dep

%.obj: %.cc
	${CC} ${INCLUDE} ${CCFLAGS} /Fo${OBJDIR}/$@ $< 
	${CC} ${INCLUDE} ${CCFLAGS} -MM $< > ${OBJDIR}/$*.dep

clean: build-teardown build-setup

build-setup:
	mkdir -p $(OBJDIR)
	mkdir -p $(OBJDIR)/core
	mkdir -p $(OBJDIR)/core/test
	mkdir -p $(OBJDIR)/nfs
	mkdir -p $(OBJDIR)/nfs/test
	mkdir -p $(OBJDIR)/clefdb

ubuntu-setup:
	apt-get install build-essential libaio-dev libaio1 libaio1-dbg \
                        libboost-dev libboost-doc libboost-dbg \
                        zlibc zlib1g-dev \
                        libtcmalloc-minimal4 libtcmalloc-minimal4-dbg \
                        valgrind \

	ln /usr/lib/libtcmalloc_minimal.so.4 /usr/lib/libtcmalloc_minimal.so

build-teardown:
	rm -r -f $(OBJDIR)
