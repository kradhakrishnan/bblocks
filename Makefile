ifndef OBJDIR
OBJDIR = $(PWD)/../build
endif

STDCPP = /usr/include/c++/4.6.3/

SUBDIR = core/          	\
         core/test      	\
         core/bmark     	\
	 core/kmod	 	\

clean: build-teardown build-setup

libcore: all modules

modules:
	@echo '-->[KMODULE]'
	@make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd)/core/kmod modules
	@make INSTALL_MOD_PATH=$(OBJDIR) -C /lib/modules/$(shell uname -r)/build M=$(PWD)/core/kmod modules_install
	@make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd)/core/kmod clean

build-setup:
	@echo '-->[SETUP]' $(OBJDIR)
	@mkdir -p $(OBJDIR)/ar
	@mkdir -p $(OBJDIR)/core
	@mkdir -p $(OBJDIR)/core/net
	@mkdir -p $(OBJDIR)/core/fs
	@mkdir -p $(OBJDIR)/core/test
	@mkdir -p $(OBJDIR)/core/bmark

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

.DEFAULT_GOAL := libcore

include MakefileRules
