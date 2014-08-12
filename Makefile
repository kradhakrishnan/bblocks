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

default: all
	@echo '** Copying .sh files'
	@${MKDIR_P} $(OBJDIR)/test/unit/perf
	@${CP} test/unit/perf/*.sh $(OBJDIR)/test/unit/perf
	@chmod +x $(OBJDIR)/test/unit/perf/*.sh

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
	@echo ' DOC		' $(PWD)/doc/doxygen
	@doxygen doc/doxygen/Doxyfile

ubuntu-setup: build-setup
	@scripts/setup-dev-machine

#
# Tests
#

run-test: all run-unit-test run-valgrind-test

run-unit-test: default
	python test/unit/run-unit-test.py -b ../build -u test/unit/default-unit-tests \
					  -o ../build/unit-test.log

run-valgrind-test: default
	python test/unit/run-unit-test.py -v -b ../build -u test/unit/default-unit-tests \
					  -o ../build/unit-test.log

run-flamebox: default
	$(shell cd test/flamebox; \
	        python run-flamebox.py --config flamebox.config --output ../../..)

calc-lcov: default
	$(shell cd ../build; \
		lcov -q --capture --directory . --output-file lcov.dat -b ../bblocks; \
		genhtml lcov.dat --output-directory lcov-html -q)

run-all-test: default
	@scripts/run-checkin-test.sh

#
# Misc
#

tags:
	$(shell cd ..; rm -f tags; ctags -R .)

.DEFAULT_GOAL := default

include MakefileRules
