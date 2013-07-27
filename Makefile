SRCS =
TARGET =
INCLUDE =

STDCPP = /usr/include/c++/4.6.3/

SUBDIR += core/          \
          core/test      \
          core/bmark     \
	  core/kmod	 \
          extentfs       \
          extentfs/test  \

ifndef OBJDIR
OBJDIR = $(PWD)/../build
endif

clean: build-teardown build-setup

libcore: all modules

modules:
	@echo '** Building modules **'
	@make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd)/core/kmod modules
	@make INSTALL_MOD_PATH=$(OBJDIR) -C /lib/modules/$(shell uname -r)/build M=$(PWD)/core/kmod modules_install
	@make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd)/core/kmod clean

build-setup:
	@echo SETUP $(OBJDIR)
	@mkdir -p $(OBJDIR)/core
	@mkdir -p $(OBJDIR)/core/net
	@mkdir -p $(OBJDIR)/core/fs
	@mkdir -p $(OBJDIR)/core/test
	@mkdir -p $(OBJDIR)/core/bmark
	@mkdir -p $(OBJDIR)/extentfs
	@mkdir -p $(OBJDIR)/extentfs/test

build-doc:
	@echo SETUP $(OBJDIR)/doc
	@rm -f -r ../build/doc
	@mkdir $(OBJDIR)/doc
	@doxygen doc/doxygen/Doxyfile

ubuntu-setup:
	apt-get install build-essential libaio-dev libaio1 libaio1-dbg \
                        libboost-dev libboost-doc libboost-dbg \
                        libboost-program-options-dev zlibc zlib1g-dev \
                        libtcmalloc-minimal0 libtcmalloc-minimal0-dbg \
                        valgrind \

	ln /usr/lib/libtcmalloc_minimal.so.4 /usr/lib/libtcmalloc_minimal.so

build-teardown:
	@echo CLEAN $(OBJDIR)
	@rm -r -f $(OBJDIR)

.DEFAULT_GOAL := libcore

include MakefileRules
