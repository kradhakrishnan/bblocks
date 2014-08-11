#!/usr/bin/python

import sys
from optparse import OptionParser
from xml.dom import minidom
import os
import subprocess
import time

#
# Globals
#
_debug = None
_valgrind = False
_valgrindsupp = ""
_verbose = False

def EnableValgrind():
	global _valgrind
	_valgrind = True

#
# Debug - Print and flush for log coherence
#
def InitDebug(filename):
	global _debug
	_debug = open(filename, 'w')

def Debug(str):
	print(str)
	sys.stdout.flush()
	_debug.write("%s\n" % str)

def Verbose(str):
	if _verbose:
		print("%s\n" % str)

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
	else:
	    cmd = [ "timeout",  "--kill-after=%d" % timeout, "%ds" % timeout,
		    test ]

	Verbose(">> Executing command : %s" % cmd)

	status = subprocess.call(args=cmd, stdin=None, stdout=_debug,
				 stderr=_debug)

	Verbose(">> exit stauts = %d" % status)

	if status == 0:
		return True

	if status == 124:
		Debug("TIMEOUT after %d sec**" % timeout)

	if _valgrind and status == 255:
		Debug("VALGRIND FAILURE")

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

(options, args) = oparser.parse_args()

if not options.filename or not options.outfile or not options.builddir:
	print("Usage: run-unit-test -b <directory> -u <test> -o <output>")
	sys.exit(-1)

InitDebug(options.outfile)

if options.valgrind:
	Debug("> Enabling valgrind")
	EnableValgrind()

_valgrindsupp = "%s.supp" % options.filename

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
		Debug("%d) %s SUCCESS (%.2f sec/%d sec)" % (id, name, elapsedsec, timeout))
		success += 1
	else:
		Debug("%d) %s FAILED (%.2f sec/%d sec)" % (id, name, elapsedsec, timeout))
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
