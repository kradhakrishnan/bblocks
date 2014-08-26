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
	@echo " TEST		Unit"
	@python test/unit/run-unit-test.py --build-dir $(OBJDIR) \
					   --unit-test test/unit/default-unit-tests \
					   --output $(OBJDIR)/unit-test.log

run-valgrind-test: default
	@echo " TEST		Valgrind"
	@python test/unit/run-unit-test.py --valgrind --build-dir $(OBJDIR) \
					   --unit-test test/unit/default-unit-tests \
					   --output $(OBJDIR)/unit-test.log

run-helgrind-test: default
	@echo " TEST		Helgrind ** The tests won't succeed if spin=disable is not specified **"
	@python test/unit/run-unit-test.py --helgrind --build-dir $(OBJDIR) \
					   --unit-test test/unit/default-helgrind-test \
					   --output $(OBJDIR)/unit-test.log

run-drd-test: default
	@echo " TEST		DRD ** The tests won't succeed if spin=disable is not specified **"
	@python test/unit/run-unit-test.py --drd --build-dir $(OBJDIR) \
					   --unit-test test/unit/default-helgrind-test \
					   --output $(OBJDIR)/unit-test.log

run-all-test:
	@echo " TEST		ALL"
	@scripts/run-all-test.sh

#
# Flamebox
#
run-flamebox: default
	$(shell cd test/flamebox; \
	        python run-flamebox.py --config flamebox.config --output ../../..)

#
# Code coverage
#
calc-lcov: default
	$(shell cd ../build; \
		lcov -q --capture --directory . --output-file lcov.dat -b ../bblocks; \
		genhtml lcov.dat --output-directory lcov-html -q)

#
# Codeship tests/scripts
#
run-codeship-test: default
	python test/unit/run-unit-test.py -b $(OBJDIR) \
					  -u test/unit/default-codeship-tests \
					  -o $(OBJDIR)/codeship-test.log ; \
	cat $(OBJDIR)/codeship-test.log

#
# Install
#

install: default uninstall
	@echo ' INSTALL	' .h
	@${RM} -r -f $(OBJDIR)/tmp
	@${MKDIR_P} $(OBJDIR)/tmp
	@${CP} --parent `find src -name '*.h'` $(OBJDIR)/tmp
	@sudo ${MV} -f $(OBJDIR)/tmp/src /usr/include/bblocks
	@echo ' INSTALL	' libbblocks.so
	@sudo ${CP} $(OBJDIR)/src/libbblocks.so /usr/lib

uninstall:
	@echo ' UNINSTALL	' .h
	@sudo ${RM} -r -f /usr/include/bblocks
	@echo ' UNINSTALL	' libbblocks.so
	@sudo ${RM} -f /usr/lib/libbblocks.so

#
# Misc
#

tags:
	$(shell cd ..; rm -f tags; ctags -R .)

.DEFAULT_GOAL := default

include MakefileRules
