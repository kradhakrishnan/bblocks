ifndef OBJDIR
OBJDIR = $(PWD)/../build
endif

STDCPP = /usr/include/c++/4.6.3/

SUBDIR = src/			\
	 src/kmod	 	\
	 test			\

clean: build-teardown build-setup

libsrc: all modules

modules:
	@echo '-->[KMODULE]'
	@make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd)/src/kmod modules
	@make INSTALL_MOD_PATH=$(OBJDIR) -C /lib/modules/$(shell uname -r)/build M=$(PWD)/src/kmod modules_install
	@make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd)/src/kmod clean

build-setup:
	@echo '-->[SETUP]' $(OBJDIR)
	@mkdir -p $(OBJDIR)/ar
	@mkdir -p $(OBJDIR)/src
	@mkdir -p $(OBJDIR)/src/net
	@mkdir -p $(OBJDIR)/src/fs
	@mkdir -p $(OBJDIR)/test

build-doc:
	@echo '-->[DOC]' $(OBJDIR)/doc
	@echo SETUP $(OBJDIR)/doc
	@rm -f -r ../build/doc
	@mkdir $(OBJDIR)/doc
	@doxygen doc/doxygen/Doxyfile

ubuntu-setup:
	@echo '-->[UBUNTU-SETUP]'
	apt-get install build-essential libaio-dev libaio1 libaio1-dbg \
                        libboost-dev libboost-doc libboost-dbg \
                        libboost-program-options-dev zlibc zlib1g-dev \
                        libtcmalloc-minimal0 libtcmalloc-minimal0-dbg \
                        valgrind \

	ln /usr/lib/libtcmalloc_minimal.so.4 /usr/lib/libtcmalloc_minimal.so

build-teardown:
	@echo '-->[CLEAN]' $(OBJDIR)
	@rm -r -f $(OBJDIR)

.DEFAULT_GOAL := libsrc

include MakefileRules
