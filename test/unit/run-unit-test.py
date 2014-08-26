#!/usr/bin/python

import sys
from optparse import OptionParser
from xml.dom import minidom
import os
import subprocess
import time

sys.path.append("/usr/lib/python2.7/lib-dynload/")


#
# Globals
#
_debug = None
_valgrind = False
_helgrind = False
_drd = False
_valgrindsupp = None
_helgrindsupp = None
_verbose = False

def EnableValgrind():
	global _valgrind
	_valgrind = True

def EnableHelgrind():
    global _helgrind
    _helgrind = True

def EnableDRD():
    global _drd
    _drd = True

#
# Debug - Print and flush for log coherence
#
def InitDebug(filename):
	global _debug
	_debug = open(filename, 'w')

def Debug(str):
	print(str)
	sys.stdout.flush()
	_debug.write("$ %s\n" % str)

def Verbose(str):
	if _verbose:
		print("$ %s\n" % str)

#
# XML parsing helpers
#
def GetElement(node, name):
	e = node.getElementsByTagName(name)
	assert len(e) == 1
	return e[0]

def GetElements(node, name):
	e = node.getElementsByTagName(name)
	return e

def GetAttr(node, name):
	attr = node.attributes[name]
	return attr.value

#
# Exec(command, timeout)
#
def Exec(test, timeout):
	if _valgrind:
	    cmd = [ "timeout",
		    "--kill-after=%d" % timeout,
		    "%ds" % timeout,
		    "valgrind",
		    "--leak-check=full",
		    "--trace-children=yes",
		    "--trace-children-skip=timeout",
		    "--track-fds=yes",
		    "--suppressions=%s" % _valgrindsupp,
		    "--error-exitcode=%d" % 255,
		    "--gen-suppressions=all",
		    "--track-origins=yes",
		    "-v",
		    test ]
	elif _helgrind:
	    cmd = [ "timeout",
		    "--kill-after=%d" % timeout,
		    "%ds" % timeout,
		    "valgrind",
		    "--tool=helgrind",
		    "--error-exitcode=255",
		    "--suppressions=%s" % _helgrindsupp,
		    "--gen-suppressions=all",
		    "-v",
		    test ]
	elif _drd:
	    cmd = [ "timeout",
		    "--kill-after=%d" % timeout,
		    "%ds" % timeout,
		    "valgrind",
		    "--tool=drd",
		    "--error-exitcode=255",
		    "--gen-suppressions=all",
		    "-v",
		    test ]
	else:
	    cmd = [ "timeout",  "--kill-after=%d" % timeout, "%ds" % timeout,
		    test ]

	Verbose("Executing command : %s" % cmd)

	status = subprocess.call(args=cmd, stdin=None, stdout=_debug,
				 stderr=_debug)

	Verbose("exit stauts = %d" % status)

	if status == 0:
		return True

	if status == 124:
		Debug("** TIMEOUT after %d sec **" % timeout)

	if _valgrind and status == 255:
		Debug("** VALGRIND FAILURE **")

	if _helgrind and status == 255:
		Debug("** HELGRIND FAILURE **")

	if _drd and status == 255:
		Debug("** DRD FAILURE **")

	return False

#
# Main
#
oparser = OptionParser()

# -u <filename> or --unit-test <filename>
oparser.add_option("-u", "--unit-test", action="store", type="string", dest="filename")
# -b <directory> or --build-dir <directory>
oparser.add_option("-b", "--build-dir", action="store", type="string", dest="builddir")
# -o <output file>
oparser.add_option("-o", "--output", action="store", type="string", dest="outfile")
# -v or --valgrind
oparser.add_option("-v", "--valgrind", action="store_true", default=False, dest="valgrind")
# --helgrind
oparser.add_option("--helgrind", action="store_true", default=False, dest="helgrind")
# --drd
oparser.add_option("--drd", action="store_true", default=False, dest="drd")

(options, args) = oparser.parse_args()

if not options.filename or not options.outfile or not options.builddir:
	print("Usage: run-unit-test -b <directory> -u <test> -o <output>")
	sys.exit(-1)

InitDebug(options.outfile)

if options.valgrind:
	Verbose("Enabling valgrind")
	EnableValgrind()
	_valgrindsupp = "%s.supp" % options.filename
elif options.helgrind:
	Verbose("Enabling helgrind")
	EnableHelgrind()
        _helgrindsupp = "%s.supp" % options.filename
elif options.drd:
	Verbose("Enableing DRD")
	EnableDRD()

Debug("[ %s ]" % options.filename)

xmldoc = minidom.parse(options.filename)
unittests = GetElement(xmldoc, "unit-tests")
tests = GetElements(unittests, "test")

id = 0
success = 0
failed = 0
failedTests = []

for test in tests:
	name = GetAttr(test, "name")
	cmd = GetAttr(test, "cmd")
	timeout = int(GetAttr(test, "timeout"))

	if options.builddir:
		cmd = "%s/%s" % (options.builddir, cmd)

	id = id + 1
	Verbose("Running %s ... " % name)

	start_time = time.time()
	status = Exec(cmd, timeout)
	elapsedsec = time.time() - start_time

	if status:
		Debug("%d)\t%s\tSUCCESS\t(%.2f sec/%d sec)" % (id, name, elapsedsec, timeout))
		success += 1
	else:
		Debug("%d)\t%s\tFAILED\t(%.2f sec/%d sec)" % (id, name, elapsedsec, timeout))
		failedTests.append(name)
		failed += 1

	total = success + failed


Debug("=======")
Debug("SUMMARY")
Debug("=======")
Debug("Total : %d" % id)
Debug("Success : %d" % success)
Debug("Failure : %d" % failed)
Debug("Failed tests: %s" % failedTests)

_debug.close()

retcode = (total != success)
sys.exit(retcode)
