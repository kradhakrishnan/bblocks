ifndef OBJDIR
OBJDIR = $(PWD)/../build
endif

SUBDIR = src/			\
	 src/kmod	 	\
	 test			\

clean: build-teardown


#
# Unfortunately, I need this flag to pass compilation of boost library
# TODO: Get rid of this please
#
CCFLAGS += -fpermissive

#
# Disabled compiling kernel modules temporarily. Need to enable them.
#

libsrc: all

modules:
	@echo '  KMOD		'
	@make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd)/src/kmod modules
	@make INSTALL_MOD_PATH=$(OBJDIR) -C /lib/modules/$(shell uname -r)/build M=$(PWD)/src/kmod modules_install
	@make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd)/src/kmod clean

build-setup:
	@echo ' SETUP		' $(OBJDIR)
	@mkdir -p $(OBJDIR)

build-teardown:
	@echo ' CLEAN		' $(OBJDIR)
	@rm -r -f $(OBJDIR)

build-doc:
	@echo ' DOC		' $(OBJDIR)/doc
	@echo SETUP $(OBJDIR)/doc
	@rm -f -r ../build/doc
	@mkdir $(OBJDIR)/doc
	@doxygen doc/doxygen/Doxyfile

ubuntu-setup: build-setup
	scripts/setup-dev-machine

#
# Tests
#

run-unit-test:
	python test/unit/run-unit-test.py -b ../build -u test/unit/default-unit-tests \
					  -o ../build/unit-test.log

run-valgrind-test:
	python test/unit/run-unit-test.py -v -b ../build -u test/unit/default-unit-tests \
					  -o ../build/unit-test.log

run-flamebox:
	$(shell cd test/flamebox; \
	        python run-flamebox.py --config flamebox.config --output ../../..)

calc-lcov:
	$(shell cd ../build; \
		lcov -q --capture --directory . --output-file lcov.dat -b ../bblocks; \
		genhtml lcov.dat --output-directory lcov-html -q)

#
# Misc
#

tags:
	$(shell cd ..; rm -f tags; ctags -R .)

.DEFAULT_GOAL := libsrc

include MakefileRules
